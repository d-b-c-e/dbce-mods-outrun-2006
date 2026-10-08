// Included inside FFB. No runtime enable setting: explicit owner-controlled
// Begin/Take API only. Producers append to preallocated memory; never file I/O.
namespace SignalRecording {
constexpr size_t Capacity=128, StateCount=46, ConfigCount=13, InputCount=10;
using State=std::array<double,StateCount>;
using Config=std::array<double,ConfigCount>;
struct Request { double kind=0, slot=0, magnitude=0, frequency=0, previousAfter=0; };
struct Frame { DWORD tick=0; bool checkpoint=false; State before{},after{}; Config config{};
    std::array<double,InputCount> input{}; std::array<Request,3> requests{}; size_t count=0; };
struct Session { std::array<unsigned char,20> source{}; std::array<Frame,Capacity> frames{};
    size_t count=0; bool complete=false, failed=false; };
static std::unique_ptr<Session> session;
static Frame* current=nullptr;
static State Snapshot() {
    State s={double(sharedPrevGear),double(prevGear),double(prevCollisionFlags),prevSpeed,smoothedLateral,
        double(speedHistoryIdx),double(latHistoryIdx),double(crashImpulseTimer),crashImpulseForce,
        double(prevConstantLevel),double(prevStructLevel),double(gearShiftTimer),double(splashTimer),splashAmp,
        double(warmupFrames),double(updateCounter),roadPhase,slipPhase,double(ffbLoaded),double(periodicsActive),
        double(slotRoadTexture),double(slotTireSlip)};
    size_t i=22; for(float v:speedHistory)s[i++]=v; for(float v:latHistory)s[i++]=v; return s;
}
static Config Configuration() {
    return OutRunForceObservation::ReadConfiguration();
}
template<size_t N> static bool Finite(const std::array<double,N>& values) {
    for(double v:values)if(!std::isfinite(v)||std::abs(v)>1e12)return false; return true;
}
static bool Integral(double v,double min,double max){return std::isfinite(v)&&v>=min&&v<=max&&std::floor(v)==v;}
static bool SafeState(const State& s){
    if(!Finite(s))return false;
    for(int i:{0,1,2})if(!Integral(s[i],0,4294967295.0))return false;
    for(int i:{5,6,7,11,12,14,15})if(!Integral(s[i],0,2147483646))return false;
    for(int i:{9,10})if(!Integral(s[i],-10000,10000))return false;
    for(int i:{18,19})if(!Integral(s[i],0,1))return false;
    for(int i:{20,21})if(!Integral(s[i],-1,3))return false;
    return true;
}
static bool SafeInput(const std::array<double,InputCount>& input){
    return Finite(input)&&input[0]>=0&&Integral(input[1],0,4294967295.0)&&Integral(input[4],0,6)&&
        Integral(input[7],0,255)&&input[8]>=0&&input[8]<=1&&Integral(input[9],0,1);
}
static bool Begin(std::string_view revision) {
    ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime());
    if(!lease || session || revision.size()!=40 || useSharedModel ||
       (!Settings::FFBProfile.empty() && _stricmp(Settings::FFBProfile.c_str(),"legacy")))return false;
    auto next=std::unique_ptr<Session>(new(std::nothrow) Session);
    if(!next)return false;
    auto hex=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;};
    for(size_t i=0;i<20;i++){int a=hex(revision[i*2]),b=hex(revision[i*2+1]);if(a<0||b<0)return false;next->source[i]=(unsigned char)(a*16+b);}
    session=std::move(next); return true;
}
static std::unique_ptr<Session> Take(bool complete) {
    ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime(),true);
    if(!lease || current || !session)return {};
    session->complete=complete && !session->failed && session->count>0;
    return std::move(session);
}
static Frame* Start(EVWORK_CAR* car,DWORD (WINAPI *clock)()) {
    if(!session || session->failed)return nullptr;
    if(current || session->count==Capacity || useSharedModel ||
       (!Settings::FFBProfile.empty() && _stricmp(Settings::FFBProfile.c_str(),"legacy"))){session->failed=true;return nullptr;}
    Frame& f=session->frames[session->count]; f=Frame{}; f.tick=clock(); f.before=Snapshot(); f.config=Configuration();
    f.input={car->field_1C4,double(car->field_8),car->field_264,car->field_268,double(car->cur_gear_208),
        car->field_1D0,car->field_1D4,double(car->pedal_amount_34),0,0};
    f.checkpoint=session->count==0 || f.before!=session->frames[session->count-1].after;
    if(!SafeState(f.before)||!Finite(f.config)||!SafeInput(f.input)||
       (session->count && f.tick<session->frames[session->count-1].tick)){session->failed=true;return nullptr;}
    current=&f; return &f;
}
static void Surface(float roughness,DWORD water) {if(current){current->input[8]=roughness;current->input[9]=water;}}
static void Inputs(float speed,uint32_t flags,float lateral1,float lateral2,uint32_t gear,float steer,float rate) {
    if(current){auto&v=current->input;v[0]=speed;v[1]=flags;v[2]=lateral1;v[3]=lateral2;v[4]=gear;v[5]=steer;v[6]=rate;}
}
static void Throttle(float normalized) {
    if(current && normalized!=std::clamp((float)current->input[7]/255.0f,0.0f,1.0f))session->failed=true;
}
static void IdleSelection(float speed,bool entered) {
    if(current && speed<0.05f && entered!=(current->input[7]>0))session->failed=true;
}
static void Observe(double kind,double slot,double magnitude,double frequency) {
    if(!current)return;
    if(current->count==3){session->failed=true;return;}
    current->requests[current->count++]={kind,slot,magnitude,frequency,double(prevConstantLevel)};
}
static void End(Frame* f) {
    if(!f)return;
    if(current!=f){session->failed=true;return;}
    f->after=Snapshot();
    if(!SafeState(f->after)||!SafeInput(f->input)||f->config!=Configuration()||useSharedModel||
       (!Settings::FFBProfile.empty() && _stricmp(Settings::FFBProfile.c_str(),"legacy")))session->failed=true;
    for(size_t i=0;i<f->count;i++){const auto&r=f->requests[i];
        if(!Finite(std::array<double,5>{r.kind,r.slot,r.magnitude,r.frequency,r.previousAfter}))session->failed=true;}
    current=nullptr; ++session->count;
}
struct Scope {
    Frame* frame; int exceptions=0;
    Scope(EVWORK_CAR* car,DWORD (WINAPI *clock)()):frame(Start(car,clock)) {if(frame)exceptions=std::uncaught_exceptions();}
    ~Scope(){if(frame){if(std::uncaught_exceptions()>exceptions)session->failed=true;End(frame);}}
};
#ifdef OUTRUN_OFFLINE_SIGNALS
static void Restore(const State& s) {
    sharedPrevGear=(uint32_t)s[0];prevGear=(uint32_t)s[1];prevCollisionFlags=(uint32_t)s[2];prevSpeed=(float)s[3];smoothedLateral=(float)s[4];
    speedHistoryIdx=(int)s[5];latHistoryIdx=(int)s[6];crashImpulseTimer=(int)s[7];crashImpulseForce=(float)s[8];
    prevConstantLevel=(LONG)s[9];prevStructLevel=(LONG)s[10];gearShiftTimer=(int)s[11];splashTimer=(int)s[12];splashAmp=(float)s[13];
    warmupFrames=(int)s[14];updateCounter=(int)s[15];roadPhase=(float)s[16];slipPhase=(float)s[17];ffbLoaded=s[18]!=0;periodicsActive=s[19]!=0;
    slotRoadTexture=(int)s[20];slotTireSlip=(int)s[21];size_t i=22;for(float&v:speedHistory)v=(float)s[i++];for(float&v:latHistory)v=(float)s[i++];
}
static void Configure(const Config& c) {
    Settings::FFBLateralDeadzone=(float)c[0];Settings::FFBGripLoss=(float)c[1];Settings::FFBWallImpact=(float)c[2];Settings::FFBRoadTexture=(float)c[3];
    Settings::FFBTireSlip=(float)c[4];Settings::FFBEngineIdle=(float)c[5];Settings::FFBSpringStrength=(float)c[6];Settings::FFBDamperStrength=(float)c[7];
    Settings::FFBSteeringWeight=(float)c[8];Settings::FFBWeightTransfer=(float)c[9];Settings::FFBGearShift=(float)c[10];
    Settings::FFBInvertForce=c[11]!=0;Settings::FFBGlobalStrength=(float)c[12];Settings::FFBDiagnosticLog=false;
}
#endif
} // namespace SignalRecording
