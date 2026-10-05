// Executes the actual PC calculation and vendored model/shaper, memory sinks only.
#define OUTRUN_OFFLINE_SIGNALS
#ifndef OUTRUN_PROFILE_HOOKS_SOURCE
#define OUTRUN_PROFILE_HOOKS_SOURCE "../../src/hooks_dinputffb.cpp"
#endif
#include OUTRUN_PROFILE_HOOKS_SOURCE
#include <iostream>
#include <iomanip>
#include <sstream>
#include <locale>
float VibrationLeftMotor=0, VibrationRightMotor=0;
double __cdecl sub_1149C0(unsigned int,int,DWORD*) { std::abort(); }
Hook::Hook() {}
namespace DInputRemap {
IDirectInputDevice8A* GetPrimaryDevice() { std::abort(); }
bool IsPrimaryInitialized() { std::abort(); }
bool GetPrimaryDeviceGuid(GUID*) { std::abort(); }
int GetTelemetryAccel() { std::abort(); }
int GetTelemetryBrake() { std::abort(); }
UiSnapshot ReadUiSnapshot() { std::abort(); }
}
static void Require(bool ok,const char* reason) { if(!ok)throw std::runtime_error(reason); }
static DWORD tick=0;
static unsigned clockReads=0, constants=0, periodics=0, events=0;
static bool rejectConstant=false;
static std::ostringstream trace;
static DWORD WINAPI Clock() { ++clockReads; return tick; }
static void Constant(LONG value) {
    ++constants; trace<<"constant,"<<value<<","<<rejectConstant<<"\n";
    if(!rejectConstant)FFB::prevConstantLevel=value;
}
static void Periodic(int slot,float magnitude,float hz) {
    ++periodics; trace<<"periodic,"<<slot<<","<<std::hexfloat<<magnitude<<","<<hz<<"\n";
}
static std::string Run(const dbce::force::Profile& profile,bool periodic,bool irregular,bool middleReset) {
    dbce::force::Model model(profile.model);
    dbce::force::Shaper shaper(profile.shaper);
    FFB::sharedModel=&model; FFB::sharedShaper=&shaper; FFB::sharedProfile=profile;
    FFB::useSharedModel=true; FFB::ffbLoaded=true; // Availability only; no module loaded.
    FFB::periodicsActive=periodic; FFB::slotRoadTexture=0; FFB::slotTireSlip=1;
    Settings::FFBProfile=profile.id(); Settings::FFBDiagnosticLog=false; Settings::TelemetryEnabled=false;
    FFB::ResetCalculationState(); trace.str(""); trace.clear(); trace.imbue(std::locale::classic());
    clockReads=constants=periodics=events=0;
    for(unsigned i=0;i<96;++i) {
        if(middleReset && i==48)FFB::ResetCalculationState();
        tick=irregular ? (i<32?0:0xfffffff0u) : i*17;
        rejectConstant=(i>=27 && i<31);
        EVWORK_CAR car{};
        car.field_1C4=i<8?0.0f:(i<60?0.8f:0.5f);
        car.field_1D0=i<32?0.2f:-0.3f; car.field_1D4=i<32?0.01f:-0.015f;
        car.field_264=i<32?15.0f:-12.0f; car.field_268=2.0f;
        car.cur_gear_208=i<20?2:(i<70?3:4);
        car.field_8=i>=60 && i<65?4096:0; car.pedal_amount_34=180;
        trace<<"frame,"<<i<<"\n";
        FFB::CalculateSignals(&car,0.8f,i>=40 && i<45,{Constant,Periodic},Clock);
        events+=model.last_was_event?1:0;
        trace<<"state,"<<FFB::sharedPrevGear<<","<<FFB::prevConstantLevel<<","<<FFB::warmupFrames
             <<","<<FFB::updateCounter<<","<<std::hexfloat<<model.last_structural<<","<<model.last_was_event<<"\n";
    }
    FFB::sharedModel=nullptr; FFB::sharedShaper=nullptr; FFB::useSharedModel=false;
    Require(constants>0,"no named-profile constant requests");
    Require(periodic ? periodics==48 : periodics==0,"periodic branch coverage");
    Require(clockReads==0,"disabled recorder/diagnostics sampled clock");
    if(profile.model.shift_strength>0 || profile.model.impact_strength>0)Require(events>0,"missing profile events");
    return trace.str();
}
#ifdef OUTRUN_PROFILE_RECORDING_GUARDS
static void RecordingGuards(const dbce::force::Profile& profile) {
    constexpr auto revision="69041401cf58113daa7036e9e0b588d7cc38099c";
    Settings::FFBProfile=profile.id(); FFB::useSharedModel=false;
    Require(!FFB::SignalRecording::Begin(revision),"named setting admitted to legacy recording");
    Settings::FFBProfile="legacy"; FFB::useSharedModel=true;
    Require(!FFB::SignalRecording::Begin(revision),"active shared model admitted to legacy recording");
    FFB::useSharedModel=false; FFB::ResetCalculationState();
    Require(FFB::SignalRecording::Begin(revision),"legacy recording setup failed");
    Settings::FFBProfile=profile.id();
    EVWORK_CAR car{}; car.cur_gear_208=2; tick=0; clockReads=0;
    FFB::CalculateSignals(&car,0,0,{Constant,Periodic},Clock);
    auto capture=FFB::SignalRecording::Take(true);
    Require(capture && !capture->complete && capture->failed && capture->count==0 && clockReads==0,
        "between-frame profile change must invalidate before sampling");
}
#endif
int main(int argc,char** argv) {
    try {
        Require(argc==2,"usage: signal-profile synthetic-profile-directory");
        spdlog::set_level(spdlog::level::off);
        std::string text,why;
        Require(dbce::force::detail::read_file(std::string(argv[1])+"/force-profiles.ini",text),"profile file unreadable");
        auto ids=dbce::force::list_profile_ids(text); Require(ids.size()==6,"expected six pinned shipped profiles");
        ids.push_back("synthetic-owner@1");
        for(const auto& id:ids) {
            dbce::force::Profile profile;
            Require(dbce::force::load_profile_dir(argv[1],id,profile,&why),why.c_str());
            if(id=="synthetic-owner@1") {
                Require(profile.model.spring_strength==0.73f && profile.shaper.invert && profile.shaper.strength==37,
                    "synthetic inherited user profile did not load");
            }
            for(bool periodic:{false,true})for(bool reset:{false,true}) {
                auto result=Run(profile,periodic,false,reset);
                Require(result==Run(profile,periodic,false,reset),"fresh model/reset repeatability failed");
                Require(result==Run(profile,periodic,true,reset),"fixed60Hz changed with equal/jumped clock");
                std::cout<<"profile,"<<id<<",periodic,"<<periodic<<",middleReset,"<<reset<<"\n"<<result;
            }
#ifdef OUTRUN_PROFILE_RECORDING_GUARDS
            RecordingGuards(profile);
#endif
        }
        dbce::force::Profile missing;
        Require(!dbce::force::load_profile_dir(argv[1],"absent@1",missing,&why),"missing profile accepted");
        std::cerr<<"PASS: seven profiles, 28 cases, 84 production runs; memory-only, no clock/native output\n";
#ifdef OUTRUN_PROFILE_RECORDING_GUARDS
        std::cerr<<"PASS: named/shared recording refusal and between-frame profile invalidation; recording remains legacy-only\n";
#endif
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}
}
