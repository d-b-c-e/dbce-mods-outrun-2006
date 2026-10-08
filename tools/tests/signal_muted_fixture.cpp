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
    Check(argc==3,"usage: muted-test scenario output-root");
    std::string mode=argv[1];SetEnvironmentVariableW(L"DBCE_OUTRUN_SIGNAL_MUTE",mode=="invalid"?L"invalid":L"legacy");
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
    if(mode=="invalid") {FFB::SignalCaptureUpdate();FFB::ProcessMutedSignals(nullptr);Check(!R::session&&loadAttempts==0,"invalid mode captured/loaded");std::cout<<"PASS invalid mode stays muted without calculating\n";return 0;}
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
    const int rowCount=mode=="legacy"?360:mode=="empty"?0:mode=="overflow"?8193:3;
    for(int i=0;i<rowCount;i++) {
        FFB::SignalCaptureUpdate();app++;power++;
        if(i==70){Overlay::WheelSettingsVisible=true;FFB::Update(&car);Overlay::WheelSettingsVisible=false;}
        car.field_1D0=i<180?.2f:-.3f;
        FFB::Update(&car);
    }
    if(mode=="overflow")FFB::SignalCaptureUpdate();
    else Check(R::session&&R::session->count==size_t(rowCount)&&!R::session->failed,"original muted capture incomplete");
    Check(!FFB::ffbLoaded&&!FFB::initialized&&!Telemetry::initialized&&!loadAttempts,"hardware path used");
    const auto result=root/std::wstring(id.begin(),id.end());
    if(mode!="legacy") {
        if(mode=="stop") {Check(C::WriteNew((result/L"stop.txt").wstring(),"stop\n"),"stop create");M::update=59;FFB::SignalCaptureUpdate();}
        else if(mode=="lease") {std::ofstream(base/L"dbce/test-slot.txt",std::ios::trunc)<<"other owner";M::update=59;FFB::SignalCaptureUpdate();}
        else if(mode=="stale-lease") {std::filesystem::last_write_time(base/L"dbce/test-slot.txt",std::filesystem::file_time_type::clock::now()-std::chrono::hours(3));M::update=59;FFB::SignalCaptureUpdate();}
        else if(mode=="model") {Settings::FFBProfile="arcade";FFB::SignalCaptureUpdate();}
        else if(mode=="car") {car.car_id_10++;FFB::Update(&car);}
        else if(mode=="invalid-input") {car.field_1C4=std::numeric_limits<float>::quiet_NaN();FFB::Update(&car);}
        else if(mode=="exit") FFB::FinalizeSignalCapture();
        else if(mode=="mid-frame") {
            R::current=&R::session->frames[0];M::Fault("synthetic mid-frame fault");
            Check(R::session&&R::session->failed&&!C::Exists((result/L"outcome.txt").wstring()),"mid-frame fault claimed completion");
            R::current=nullptr;FFB::FinalizeSignalCapture();
        } else if(mode=="empty"||mode=="write-failure") {
            if(mode=="write-failure")Check(C::WriteNew((result/L"signals.osig").wstring(),"existing evidence"),"file collision create");
            M::started=GetTickCount64()-10001;FFB::SignalCaptureUpdate();
        } else Check(mode=="overflow","unknown scenario");
        Check(!R::session,"interrupted capture did not detach");
        const auto outcome=C::ReadSmall((result/L"outcome.txt").wstring());
        Check(outcome.find("outcome=incomplete")!=std::string::npos&&outcome.find("outcome=complete")==std::string::npos,"interrupted capture claimed success");
        if(mode=="write-failure")Check(C::ReadSmall((result/L"signals.osig").wstring())=="existing evidence","overwrote prior evidence");
        else {std::ifstream in(result/L"signals.osig",std::ios::binary);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)),{});Invalid(bytes);}
        FFB::FinalizeSignalCapture();Check(!productionConstants&&!productionPeriodics&&!loadAttempts,"failure escaped mute");
        std::cout<<"PASS incomplete and output-muted: "<<mode<<"\n";return 0;
    }
    bool nonzero=false;for(size_t i=0;i<R::session->count;i++)for(size_t j=0;j<R::session->frames[i].count;j++)nonzero|=R::session->frames[i].requests[j].magnitude!=0;
    Check(nonzero,"producer was silenced with output");
    M::started=GetTickCount64()-10001;FFB::SignalCaptureUpdate();Check(!R::session,"runtime did not close");
    const auto data=(root/std::wstring(id.begin(),id.end())/L"signals.osig");
    auto captured=R::ReadFile(data.string().c_str());Check(captured->softwareOnly&&captured->count==360,"v3 read");Replay(*captured);
    auto bad=std::make_unique<R::Session>(*captured);bad->frames[0].before[18]=1;Invalid(R::Encode(*bad));
    *bad=*captured;bad->frames[2].context[0]=bad->frames[1].context[0];Invalid(R::Encode(*bad));
    const auto outcome=C::ReadSmall((root/std::wstring(id.begin(),id.end())/L"outcome.txt").wstring());
    Check(outcome.find("outcome=complete")!=std::string::npos&&outcome.find("admission=virtual")!=std::string::npos,"complete receipt missing");
    Check(C::WriteNew((root/L"request.txt").wstring(),RequestText(std::string(32,'c'),game,proxy,source,lease)),"second request create");
    for(int i=0;i<60;i++)FFB::SignalCaptureUpdate();
    Check(!R::session&&C::Exists((root/L"request.txt").wstring()),"second capture armed in same process");
    FFB::FinalizeSignalCapture();Check(loadAttempts==0,"loader called");
    std::cout<<"PASS process-cached mute, loader/sink/telemetry guards, bound/hash/lease/expiry request, original Update producer with FFB Off, pause checkpoint, 360-row v3 exact replay and false-native refusal; no device\n";
    return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
}
