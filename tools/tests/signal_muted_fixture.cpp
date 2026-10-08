// Actual producer/update/mute/codec with synthetic game memory. Every loader
// attempt is intercepted; no game, native library, input or device is opened.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
static int loadAttempts=0;
static HMODULE WINAPI RefuseLoad(LPCWSTR) {++loadAttempts;return nullptr;}
#define LoadLibraryW RefuseLoad
#define OUTRUN_MUTED_FIXTURE
#define OUTRUN_RECORDING_NO_MAIN
#include "signal_recording_fixture.cpp"
#undef LoadLibraryW
namespace C=OutRunSignalCapture;
namespace M=FFB::SignalCaptureRuntime;
static void Check(bool value,const char* why) {Require(value,why);}
static std::string RequestText(std::string id,std::string game,std::string proxy,std::string source,std::string lease) {
    return "action=record-legacy-muted\nid="+id+"\nseconds=10\nexpiresUnix="+std::to_string(std::time(nullptr)+300)+
        "\ngameSha256="+game+"\nproxySha256="+proxy+"\nsourceCommit="+source+"\nleaseToken="+lease+"\n";
}
int main(int argc,char**argv) {
 try {
    spdlog::set_level(spdlog::level::off);
    Check(argc==3,"usage: muted-test legacy|invalid output-root");
    std::string mode=argv[1];SetEnvironmentVariableW(L"DBCE_OUTRUN_SIGNAL_MUTE",mode=="legacy"?L"legacy":L"invalid");
    Check(OutRunSignalMute::BlocksOutput(),"mute missing");
    SetEnvironmentVariableW(L"DBCE_OUTRUN_SIGNAL_MUTE",nullptr);
    Check(OutRunSignalMute::BlocksOutput(),"cached mute switched off");
    Check(!FFB::LoadApi()&&!FFB::DeferredInit(),"mute allowed native setup");
    FFB::RefreshUiDevices(true);Check(loadAttempts==0,"mute attempted native library load");
    FFB::ffbLoaded=FFB::initialized=true;FFB::ffb.SetDeviceForcesXY=ProductionConstant;FFB::ffb.UpdatePeriodicEffect=ProductionPeriodic;
    productionConstants=productionPeriodics=0;FFB::SetConstantForce(5000);FFB::UpdatePeriodic(0,0.5f,25);
    Check(!productionConstants&&!productionPeriodics,"native output while muted");
    FFB::ffbLoaded=FFB::initialized=false;
    Settings::TelemetryEnabled=true;Check(!Telemetry::Init(),"motion telemetry initialized under mute");Telemetry::Write(nullptr,true);
    if(mode!="legacy") {FFB::SignalCaptureUpdate();FFB::ProcessMutedSignals(nullptr);Check(!R::session&&loadAttempts==0,"invalid mode captured/loaded");std::cout<<"PASS invalid mode stays muted without calculating\n";return 0;}
    const auto base=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(base);
    SetEnvironmentVariableW(L"LOCALAPPDATA",base.c_str());
    const auto root=base/L"Dbce/StagePlayback/outrun-force";std::filesystem::create_directories(root);
    std::filesystem::create_directories(base/L"dbce");
    const std::string lease="fixture owner lease=00112233445566778899aabbccddeeff";
    Check(C::WriteNew((base/L"dbce/test-slot.txt").wstring(),lease+"\r\n"),"lease create");
    Module::DllPath=base/L"synthetic-proxy.bin";Check(C::WriteNew(Module::DllPath.wstring(),"synthetic source bytes"),"fake module create");
    const std::string proxy=C::FileSha256(Module::DllPath.c_str()),source(40,'a'),id(32,'b'),game=OutRunLifecycle::ExactDiskSha256;
    const std::string text=RequestText(id,game,proxy,source,lease);C::Request r;
    auto parse=[&](std::string_view value,std::string_view p,std::string_view l){return C::Parse(value,(long long)std::time(nullptr),game,p,l,r);};
    Check(parse(text,proxy,lease).empty(),"valid request");
    Check(!parse(text,game,lease).empty(),"wrong proxy accepted");Check(!parse(text,proxy,"wrong").empty(),"wrong lease accepted");
    Check(!parse(text+"id="+id+"\n",proxy,lease).empty(),"duplicate accepted");
    Check(!parse(text+"extra=1\n",proxy,lease).empty(),"extra key accepted");
    Check(!parse(std::string(2049,'x'),proxy,lease).empty(),"oversize accepted");
    auto expired=text;auto pos=expired.find("expiresUnix=");auto end=expired.find('\n',pos);expired.replace(pos,end-pos,"expiresUnix=1");Check(!parse(expired,proxy,lease).empty(),"expired accepted");
    Check(C::WriteNew((root/L"request.txt").wstring(),text),"request create");
    ConsumerLifecycle::hostVerified=true;
    Settings::FFBProfile="legacy";Settings::FFBDiagnosticLog=false;Settings::DirectInputFFB=false;
    FFB::periodicsActive=false;FFB::slotRoadTexture=FFB::slotTireSlip=-1;
    std::vector<unsigned char> image(0x400000);Module::ExeHandle=reinterpret_cast<HMODULE>(image.data());
    EVWORK_CAR car{};car.field_1C4=.8f;car.field_1D0=.2f;car.field_1D4=.01f;car.field_264=15;car.field_268=2;car.cur_gear_208=2;car.pedal_amount_34=180;
    Game::event(8)->event_data_8=(std::uint32_t)&car;
    GameState state=STATE_GAME;GameStage stage=(GameStage)0;int app=0,power=0,ticks=1,gameMode=0;
    Game::current_mode=&state;Game::stg_stage_num=&stage;Game::app_time=&app;Game::power_on_timer=&power;Game::sprani_num_ticks=&ticks;Game::game_mode=&gameMode;
    for(int i=0;i<60;i++)FFB::SignalCaptureUpdate();Check(R::session&&R::session->softwareOnly,"runtime did not arm");
    for(int i=0;i<360;i++) {
        FFB::SignalCaptureUpdate();app++;power++;
        if(i==70){Overlay::WheelSettingsVisible=true;FFB::Update(&car);Overlay::WheelSettingsVisible=false;}
        car.field_1D0=i<180?.2f:-.3f;
        FFB::Update(&car);
    }
    Check(R::session&&R::session->count==360&&!R::session->failed,"original muted capture incomplete");
    Check(!FFB::ffbLoaded&&!FFB::initialized&&!Telemetry::initialized&&!loadAttempts,"hardware path used");
    bool nonzero=false;for(size_t i=0;i<R::session->count;i++)for(size_t j=0;j<R::session->frames[i].count;j++)nonzero|=R::session->frames[i].requests[j].magnitude!=0;
    Check(nonzero,"producer was silenced with output");
    M::started=GetTickCount64()-10001;FFB::SignalCaptureUpdate();Check(!R::session,"runtime did not close");
    const auto data=(root/std::wstring(id.begin(),id.end())/L"signals.osig");
    auto captured=R::ReadFile(data.string().c_str());Check(captured->softwareOnly&&captured->count==360,"v3 read");Replay(*captured);
    auto bad=std::make_unique<R::Session>(*captured);bad->frames[0].before[18]=1;Invalid(R::Encode(*bad));
    *bad=*captured;bad->frames[2].context[0]=bad->frames[1].context[0];Invalid(R::Encode(*bad));
    const auto outcome=C::ReadSmall((root/std::wstring(id.begin(),id.end())/L"outcome.txt").wstring());
    Check(outcome.find("outcome=complete")!=std::string::npos&&outcome.find("admission=virtual")!=std::string::npos,"complete receipt missing");
    FFB::FinalizeSignalCapture();Check(loadAttempts==0,"loader called");
    std::cout<<"PASS process-cached mute, loader/sink/telemetry guards, bound/hash/lease/expiry request, original Update producer with FFB Off, pause checkpoint, 360-row v3 exact replay and false-native refusal; no device\n";
    return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
}
