// Reuses production-linked profile fixture stubs and memory-only sinks.
#define main ProfileFixtureMain
#include "signal_profile_fixture.cpp"
#undef main
#include "toolkit-snapshot-proposal/offline_snapshot_v1.hpp"
#include <limits>
namespace S = dbce::force;
static unsigned replayCases=0,invalidCases=0;
static bool sawShift=false,sawImpact=false,sawTexture=false,sawRamp=false,sawSmoothing=false,sawHistory=false;
static EVWORK_CAR SyntheticCar(unsigned i) {
    EVWORK_CAR car{};
    car.field_1C4=i<5?0.03f:(i<50?0.8f:0.5f);
    car.field_1D0=i<30?0.24f:-0.31f; car.field_1D4=i<30?0.012f:-0.013f;
    car.field_264=i<30?15.0f:-12.0f; car.field_268=2.0f;
    car.cur_gear_208=i<20?2:(i<55?3:4);
    car.field_8=i>=50 && i<55?4096:0; car.pedal_amount_34=180;
    return car;
}
static void Attach(S::Model& m,S::Shaper& h,const S::Profile& p,bool periodic) {
    FFB::sharedModel=&m; FFB::sharedShaper=&h; FFB::sharedProfile=p;
    FFB::useSharedModel=true; FFB::ffbLoaded=true; FFB::periodicsActive=periodic;
    FFB::slotRoadTexture=0; FFB::slotTireSlip=1; Settings::FFBProfile=p.id();
    Settings::FFBDiagnosticLog=false; Settings::TelemetryEnabled=false;
}
static std::string Continue(unsigned first,unsigned last) {
    trace.str(""); trace.clear(); trace.imbue(std::locale::classic()); clockReads=0;
    for(unsigned i=first;i<last;++i) {
        tick=i*17; rejectConstant=i>=26 && i<29;
        auto car=SyntheticCar(i);
        FFB::CalculateSignals(&car,0.83f,i>=35 && i<40,{Constant,Periodic},Clock);
    }
    Require(clockReads==0,"snapshot fixture sampled clock while recorder disabled");
    return trace.str();
}
static void ReplayCase(const S::Profile& p,bool periodic,unsigned checkpoint,bool restart,bool reset) {
    const auto configuration=S::EffectiveConfiguration::Capture(p);
    S::Model original(p.model); S::Shaper originalShaper(p.shaper);
    Attach(original,originalShaper,p,periodic); FFB::ResetCalculationState();
    Continue(0,checkpoint);
    if(restart)originalShaper.restart_ramp();
    if(reset)FFB::ResetCalculationState();
    const auto shared=S::OfflineSnapshotAccess::Capture(original,originalShaper,configuration);
    const auto pc=FFB::SignalRecording::Snapshot();
    sawShift=sawShift || shared.model[0]>=0;
    sawImpact=sawImpact || shared.model[2]>=0;
    sawTexture=sawTexture || shared.model[5]>0;
    sawRamp=sawRamp || (shared.shaper[2]>0 && shared.shaper[2]<p.shaper.ramp_seconds);
    sawSmoothing=sawSmoothing || shared.shaper[0]!=0;
    sawHistory=sawHistory || pc[5]>6;
    const auto expected=Continue(checkpoint,96);
    const auto finalShared=S::OfflineSnapshotAccess::Capture(original,originalShaper,configuration);
    const auto finalPc=FFB::SignalRecording::Snapshot();
    S::Model restored(p.model); S::Shaper restoredShaper(p.shaper);
    Attach(restored,restoredShaper,p,periodic); FFB::ResetCalculationState();
    Require(S::OfflineSnapshotAccess::Restore(shared,restored,restoredShaper,configuration),"valid snapshot restore rejected");
    Require(S::SameSnapshot(shared,S::OfflineSnapshotAccess::Capture(restored,restoredShaper,configuration)),"restore did not reproduce exact private state");
    FFB::SignalRecording::Restore(pc);
    Require(expected==Continue(checkpoint,96),"populated-history replay request mismatch");
    Require(finalPc==FFB::SignalRecording::Snapshot(),"PC final state mismatch");
    Require(S::SameSnapshot(finalShared,S::OfflineSnapshotAccess::Capture(restored,restoredShaper,configuration)),"private model/shaper final state mismatch");
    FFB::sharedModel=nullptr; FFB::sharedShaper=nullptr; FFB::useSharedModel=false;
    ++replayCases;
}
static void InvalidCases(const S::Profile& p) {
    const auto configuration=S::EffectiveConfiguration::Capture(p);
    S::Model model(p.model); S::Shaper shaper(p.shaper);
    Attach(model,shaper,p,false); FFB::ResetCalculationState(); Continue(0,22);
    const auto good=S::OfflineSnapshotAccess::Capture(model,shaper,configuration);
    auto reject=[&](const S::OfflineSnapshot& bad) {
        Require(!S::OfflineSnapshotAccess::Restore(bad,model,shaper,configuration),"invalid snapshot admitted");
        Require(S::SameSnapshot(good,S::OfflineSnapshotAccess::Capture(model,shaper,configuration)),"failed restore partially mutated destination");
        ++invalidCases;
    };
    auto bad=good; bad.version=2; reject(bad);
    bad=good; bad.model_source="unreviewed"; reject(bad);
    bad=good; bad.profile_id="different@1"; reject(bad);
    for(size_t i=0;i<bad.configuration.size();++i) {
        bad=good; bad.configuration[i]=std::numeric_limits<double>::quiet_NaN(); reject(bad);
        bad=good; bad.configuration[i]=1000001; reject(bad);
    }
    for(size_t i=0;i<bad.model.size();++i) {
        bad=good; bad.model[i]=std::numeric_limits<double>::infinity(); reject(bad);
        bad=good; bad.model[i]=1000001; reject(bad);
    }
    for(size_t i=0;i<bad.shaper.size();++i) {
        bad=good; bad.shaper[i]=std::numeric_limits<double>::quiet_NaN(); reject(bad);
        bad=good; bad.shaper[i]=-1000001; reject(bad);
    }
    for(const auto& mutation:std::array<std::pair<size_t,double>,7>{{{0,-0.5},{1,-1},{2,-0.5},{3,0.5},{4,1.1},{5,-1},{6,0.5}}}){
        bad=good; bad.model[mutation.first]=mutation.second; reject(bad);
    }
    bad=good; bad.shaper[1]=1.1; reject(bad);
    bad=good; bad.shaper[2]=-1; reject(bad);
    bad=good; bad.shaper[3]=0.5; reject(bad);
    bad=good; bad.shaper[2]=0.1f; bad.shaper[3]=0; reject(bad);
    bad=good; bad.model[7]=0.1; reject(bad); // non-binary32-exact numeric channel
    auto other=p; other.name="same-settings-other-profile";
    const auto otherConfiguration=S::EffectiveConfiguration::Capture(other);
    Require(!S::OfflineSnapshotAccess::Restore(good,model,shaper,otherConfiguration),"cross-profile equal-settings restore accepted");
    ++invalidCases;
    const auto immutable=configuration.values(); auto changed=p; changed.model.spring_strength+=0.1f;
    Require(immutable==configuration.values(),"captured effective configuration mutated");
    model.settings=changed.model;
    Require(!S::OfflineSnapshotAccess::Restore(good,model,shaper,configuration),"destination config mutation accepted");
    bool captureRefused=false; try{S::OfflineSnapshotAccess::Capture(model,shaper,configuration);}catch(...){captureRefused=true;}
    Require(captureRefused,"mutated configuration capture accepted"); model.settings=p.model;
    Require(S::SameSnapshot(good,S::OfflineSnapshotAccess::Capture(model,shaper,configuration)),"config mismatch restore mutated state");
    invalidCases+=2;
    for(int channel=0;channel<3;++channel) {
        auto invalid=p;
        if(channel==0)invalid.model.texture_hz=std::numeric_limits<float>::quiet_NaN();
        if(channel==1)invalid.shaper.strength=101;
        if(channel==2)invalid.shaper.peak_limit=1.1f;
        bool refused=false;try{S::EffectiveConfiguration::Capture(invalid);}catch(...){refused=true;}
        Require(refused,"invalid effective configuration accepted"); ++invalidCases;
    }
    FFB::sharedModel=nullptr; FFB::sharedShaper=nullptr; FFB::useSharedModel=false;
}
int main(int argc,char** argv) {
    try {
        Require(argc==2,"usage: snapshot-fixture synthetic-profile-directory");
        spdlog::set_level(spdlog::level::off);
        std::string text,why;
        Require(S::detail::read_file(std::string(argv[1])+"/force-profiles.ini",text),"shipped synthetic fixture profiles unreadable");
        auto ids=S::list_profile_ids(text); ids.push_back("synthetic-owner@1");
        for(const auto& id:ids) {
            S::Profile p; Require(S::load_profile_dir(argv[1],id,p,&why),why.c_str());
            for(bool periodic:{false,true})for(unsigned checkpoint:{1u,7u,21u,35u,51u,61u,95u})
                for(unsigned mode=0;mode<3;++mode)ReplayCase(p,periodic,checkpoint,mode==1,mode==2);
            InvalidCases(p);
        }
        // Synthetic profile makes texture, slew and long ramp explicitly active.
        S::Profile stress; stress.name="synthetic-state"; stress.version=1;
        stress.model.spring_strength=0.8f; stress.model.texture_strength=0.3f;
        stress.model.shift_strength=0.2f; stress.model.impact_strength=0.4f;
        stress.shaper.slew_per_second=0.5f; stress.shaper.ramp_seconds=2.0f;
        for(unsigned checkpoint:{1u,21u,51u})for(unsigned mode=0;mode<3;++mode)
            ReplayCase(stress,false,checkpoint,mode==1,mode==2);
        InvalidCases(stress);
        Require(replayCases==303,"snapshot replay coverage count");
        Require(sawShift && sawImpact && sawTexture && sawRamp && sawSmoothing && sawHistory,
            "snapshot cases did not populate every required event/phase/ramp/history channel");
        std::cout<<"PASS: "<<replayCases<<" production-linked exact populated-history snapshot replays; "<<invalidCases
            <<" invalid/mismatched configuration/state/version refusals; atomic restore, reset/restart, eight synthetic/shipped profiles; zero native output\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}
}
