// Reuse numeric synthetic frame reader and production calculation test stubs.
#define main calculation_fixture_main
#include "signal_calculation_fixture.cpp"
#undef main
#include "../../src/signal_recording_codec.hpp"
namespace R=FFB::SignalRecording;
struct Mismatch:std::runtime_error {using std::runtime_error::runtime_error;};
static const R::Frame* expectedFrame=nullptr;
static size_t requestIndex=0;
static const R::Request& Match(double kind,double slot,double magnitude,double frequency) {
    if(requestIndex>=expectedFrame->count)throw Mismatch("extra calculated request");
    const auto&q=expectedFrame->requests[requestIndex++];
    if(q.kind!=kind||q.slot!=slot||q.magnitude!=magnitude||q.frequency!=frequency)throw Mismatch("request mismatch");
    return q;
}
static void ObserveConstant(LONG value) {
    const auto&q=Match(1,0,value,0);
    if(q.previousAfter!=FFB::prevConstantLevel && q.previousAfter!=value)
        throw std::runtime_error("invalid output-admission feedback");
    FFB::prevConstantLevel=(LONG)q.previousAfter;
}
static void ObservePeriodic(int slot,float magnitude,float frequency) {
    const auto&q=Match(2,slot,magnitude,frequency);
    if(q.previousAfter!=FFB::prevConstantLevel)throw std::runtime_error("periodic output-state change");
}
static void Replay(const R::Session&s) {
    if(R::session)throw std::runtime_error("recording must be detached before reading");
    FFB::ResetCalculationState(); FFB::useSharedModel=false;
    for(size_t i=0;i<s.count;i++) {
        const auto&f=s.frames[i];if(f.checkpoint)R::Restore(f.before);
        if(R::Snapshot()!=f.before)throw std::runtime_error("unmarked state discontinuity");
        R::Configure(f.config);tick=f.tick;EVWORK_CAR car{};
        car.field_1C4=(float)f.input[0];car.field_8=(uint32_t)f.input[1];car.field_264=(float)f.input[2];car.field_268=(float)f.input[3];
        car.cur_gear_208=(uint32_t)f.input[4];car.field_1D0=(float)f.input[5];car.field_1D4=(float)f.input[6];car.pedal_amount_34=(int)f.input[7];
        expectedFrame=&f;requestIndex=0;
        FFB::CalculateSignals(&car,(float)f.input[8],(DWORD)f.input[9],{ObserveConstant,ObservePeriodic},Clock);
        if(requestIndex!=f.count||R::Snapshot()!=f.after)throw Mismatch("count/post-state mismatch");
    }
}
static void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static void RejectConstant(LONG) {} // Models consumer admission refusal, no native calls.
static void ThrowConstant(LONG) {throw std::runtime_error("synthetic sink interruption");}
static void ChangeThrottle(EVWORK_CAR*car,float&roughness,DWORD&water){car->pedal_amount_34=200;roughness=0;water=0;}
static void Invalid(const R::Bytes&b){bool rejected=false;try{(void)R::Decode(b);}catch(const std::exception&){rejected=true;}Require(rejected,"invalid session accepted");}
static void SelfTest(const char* input,const wchar_t* output) {
    auto frames=Read(input);Require(frames.size()>42,"synthetic state fixture too short");
    const std::string revision="73747cc3585f48c64465340c8c21ee5c06093cf8";
    Require(!R::Begin("C:/private/person"),"private revision accepted");
    FFB::ResetCalculationState();FFB::useSharedModel=false;FFB::ffbLoaded=true;FFB::periodicsActive=false;
    Settings::FFBProfile="legacy";Settings::FFBDiagnosticLog=false;
    Require(R::Begin(revision),"begin failed");
    for(size_t i=0;i<frames.size();i++){
        auto f=frames[i];tick=f.tick;
        if(i==15)FFB::ResetCalculationState(); // Keep the later gear transition observable.
        if(i==30)Settings::FFBSpringStrength=0.57f;
        if(i==40){FFB::periodicsActive=true;FFB::slotRoadTexture=0;FFB::slotTireSlip=1;}
        FFB::CalculateSignals(&f.car,f.roughness,f.water,{i==10?RejectConstant:Constant,Periodic},Clock);
    }
    auto s=R::Take(true);Require(s&&s->complete&&s->count==frames.size()&&s->frames[15].checkpoint&&s->frames[40].checkpoint,"record/checkpoints");
    auto bytes=R::Encode(*s);auto read=R::Decode(bytes);Replay(*read);Replay(*read);R::SaveNew(output,*s);
    bool existingRefused=false;try{R::SaveNew(output,*s);}catch(...){existingRefused=true;}Require(existingRefused,"existing capture overwritten");
    auto bad=std::make_unique<R::Session>(*s);
    bad->complete=false;Invalid(R::Encode(*bad));
    *bad=*s;bad->frames[15].checkpoint=false;Invalid(R::Encode(*bad));
    *bad=*s;bad->frames[0].checkpoint=false;Invalid(R::Encode(*bad));
    *bad=*s;bad->frames[0].tick=100;Invalid(R::Encode(*bad));
    *bad=*s;bad->frames[2].tick=1;Invalid(R::Encode(*bad));
    *bad=*s;bad->frames[0].input[0]=std::numeric_limits<double>::quiet_NaN();Invalid(R::Encode(*bad));
    *bad=*s;bad->frames[0].config[0]=std::numeric_limits<double>::infinity();Invalid(R::Encode(*bad));
    *bad=*s;bad->frames[0].before[5]=-1;Invalid(R::Encode(*bad));
    auto truncated=bytes;truncated.pop_back();Invalid(truncated);
    auto corrupt=bytes;corrupt[48]^=1;Invalid(corrupt);
    auto checksum=bytes;checksum.back()^=1;Invalid(checksum);
    auto version=bytes;version[8]=3;Invalid(version);
    auto magic=bytes;magic[0]='X';Invalid(magic);
    auto extra=bytes;extra.push_back(0);Invalid(extra);
    // A checksum-valid changed expected output is a calculation mismatch, not malformed bytes.
    *bad=*s;bool changed=false;for(size_t i=0;i<bad->count&&!changed;i++)for(size_t r=0;r<bad->frames[i].count;r++)if(bad->frames[i].requests[r].kind==1){bad->frames[i].requests[r].magnitude+=1;changed=true;break;}
    bool differs=false;try{Replay(*R::Decode(R::Encode(*bad)));}catch(const Mismatch&){differs=true;}Require(changed&&differs,"expected output mismatch lost");
    R::SaveNew((std::wstring(output)+L".mismatch").c_str(),*bad);
    R::Configure(s->frames[0].config);FFB::ResetCalculationState();
    Require(R::Begin(revision),"overflow begin");
    auto f=frames[0];for(size_t i=0;i<=R::Capacity;i++){tick=(DWORD)i;FFB::CalculateSignals(&f.car,f.roughness,f.water,{Constant,Periodic},Clock);}
    auto full=R::Take(true);Require(full&&full->failed&&!full->complete&&full->count==R::Capacity,"bounded overflow completion");Invalid(R::Encode(*full));
    Require(R::Begin(revision),"abort begin");auto aborted=R::Take(false);Require(aborted&&!aborted->complete,"explicit incomplete close");Invalid(R::Encode(*aborted));
    Settings::FFBProfile="named-profile";Require(!R::Begin(revision),"named profile accepted");Settings::FFBProfile="legacy";
    FFB::ResetCalculationState();Require(R::Begin(revision),"nonfinite capture begin");
    f.car.field_1C4=std::numeric_limits<float>::quiet_NaN();
    // Recorder validation itself is tested without passing NaN into force arithmetic.
    Require(R::Start(&f.car,Clock)==nullptr,"invalid frame entered buffer");
    auto invalid=R::Take(true);Require(invalid&&invalid->failed&&!invalid->complete,"invalid capture completed");Invalid(R::Encode(*invalid));
    R::Configure(s->frames[0].config);FFB::ResetCalculationState();FFB::periodicsActive=false;f=frames[0];
    for(size_t i=0;i<5;i++){tick=frames[i].tick;auto sample=frames[i];FFB::CalculateSignals(&sample.car,sample.roughness,sample.water,{Constant,Periodic},Clock);}
    Require(R::Begin(revision),"mid-state begin");
    for(size_t i=5;i<15;i++){tick=frames[i].tick;auto sample=frames[i];FFB::CalculateSignals(&sample.car,sample.roughness,sample.water,{Constant,Periodic},Clock);}
    auto mid=R::Take(true);Require(mid&&mid->complete&&mid->frames[0].before[14]==5,"mid-state checkpoint lost");Replay(*R::Decode(R::Encode(*mid)));
    FFB::ResetCalculationState();Require(R::Begin(revision),"wrapped-time begin");
    tick=MAXDWORD;FFB::CalculateSignals(&f.car,f.roughness,f.water,{Constant,Periodic},Clock);
    tick=0;FFB::CalculateSignals(&f.car,f.roughness,f.water,{Constant,Periodic},Clock);
    auto wrapped=R::Take(true);Require(wrapped&&wrapped->failed&&!wrapped->complete,"wrapped capture completed");Invalid(R::Encode(*wrapped));
    FFB::ResetCalculationState();Require(R::Begin(revision),"interrupted begin");
    bool interrupted=false;try{FFB::CalculateSignals(&f.car,f.roughness,f.water,{ThrowConstant,Periodic},Clock);}catch(...){interrupted=true;}
    auto partial=R::Take(true);Require(interrupted&&partial&&partial->failed&&!partial->complete,"interrupted frame completed");Invalid(R::Encode(*partial));
    FFB::ResetCalculationState();FFB::periodicsActive=true;FFB::slotRoadTexture=0;FFB::slotTireSlip=1;
    EVWORK_CAR idle{};idle.cur_gear_208=1;idle.pedal_amount_34=200;
    Require(R::Begin(revision),"idle begin");
    for(int i=0;i<8;i++){tick=i*17;FFB::CalculateSignals(&idle,0,0,{Constant,Periodic},Clock);}
    auto idleRecord=R::Take(true);Require(idleRecord&&idleRecord->complete,"idle recording incomplete");Replay(*R::Decode(R::Encode(*idleRecord)));
    FFB::ResetCalculationState();idle.pedal_amount_34=100;Require(R::Begin(revision),"unstable throttle begin");
    FFB::CalculateSignals(&idle,0,0,{Constant,Periodic},Clock,ChangeThrottle);
    auto unstable=R::Take(true);Require(unstable&&unstable->failed&&!unstable->complete,"unstable throttle completed");Invalid(R::Encode(*unstable));
    std::cout<<"PASS: synthetic record/read/production recalculate; all legacy settings and initial/checkpoint state; constant/periodic observations; reset/config transitions; explicit complete/incomplete close; bounds/checksum/truncation/nonfinite/time/reset refusal; zero native output\n";
}
int main(int argc,char**argv){
    try{spdlog::set_level(spdlog::level::off);
        if(argc==4&&std::string_view(argv[1])=="selftest"){
            std::wstring path;for(const char*p=argv[3];*p;p++){if((unsigned char)*p>127)throw std::runtime_error("fixture output must be ASCII");path+=(wchar_t)*p;}
            SelfTest(argv[2],path.c_str());return 0;
        }
        if(argc==3&&std::string_view(argv[1])=="replay"){auto s=R::ReadFile(argv[2]);Replay(*s);std::cout<<"PASS: recorded calculation requests and state match; observe-only\n";return 0;}
        throw std::runtime_error("usage: signal-recording selftest numeric-input new-session | replay session");
    }catch(const Mismatch& e){std::cerr<<e.what()<<"\n";return 1;}
    catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}
}
