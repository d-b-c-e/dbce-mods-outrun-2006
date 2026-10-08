// Included at file scope after the producer and codec. All processing happens
// on the existing game thread. No timer, input injection or game-memory writer.
namespace FFB {
namespace SignalCaptureRuntime {
namespace C=OutRunSignalCapture;
namespace R=SignalRecording;
static std::wstring root, leasePath, directory, dataPath, outcomePath;
static std::string proxySha;
static C::Request request;
static bool resolved=false, faulted=false, finalized=false;
static std::uint32_t update=0, pausedCalls=0;
static ULONGLONG started=0;
static EVWORK_CAR* producerCar=nullptr;
static int producerStage=-1, producerId=-1, producerKind=-1;

static bool Legacy() {return !useSharedModel&&(Settings::FFBProfile.empty()||!_stricmp(Settings::FFBProfile.c_str(),"legacy"));}
static bool NetworkActive() {
    if(!Game::SumoNet_CurNetDriver||!*Game::SumoNet_CurNetDriver)return false;
    const auto* driver=*Game::SumoNet_CurNetDriver;
    return driver->vftable==reinterpret_cast<std::uintptr_t>(Module::exe_ptr(0x627EB8-0x400000)) ||
        driver->vftable==reinterpret_cast<std::uintptr_t>(Module::exe_ptr(0x627BC8-0x400000)) || driver->is_in_lobby_5;
}
static void Log(const char* text) noexcept {try{spdlog::info("{}",text);}catch(...) {}}
static bool LeaseMatches() {
    WIN32_FILE_ATTRIBUTE_DATA a{};FILETIME now{};
    if(!GetFileAttributesExW(leasePath.c_str(),GetFileExInfoStandard,&a))return false;
    GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER time{},written{};time.LowPart=now.dwLowDateTime;time.HighPart=now.dwHighDateTime;
    written.LowPart=a.ftLastWriteTime.dwLowDateTime;written.HighPart=a.ftLastWriteTime.dwHighDateTime;
    return time.QuadPart>=written.QuadPart && time.QuadPart-written.QuadPart<2ull*60*60*10000000 &&
        C::LeaseText(leasePath)==request.lease;
}
// First detach the producer buffer, so exceptions/disk failures cannot leave a
// recorder running or obstruct cleanup. This also works after the lifecycle gate
// has drained and stopped on exit; there is then no active frame to race.
static void Finish(bool complete,const char* reason) noexcept {
    if(!R::session)return;
    if(R::current){R::session->failed=true;return;} // only a game-thread boundary may finish
    auto captured=std::move(R::session);
    captured->complete=complete&&!captured->failed&&captured->count>0;
    try {
        auto bytes=R::Encode(*captured);
        const bool saved=C::WriteNew(dataPath,bytes.data(),bytes.size());
        std::string text=std::string("schema=")+C::Schema+"\nid="+request.id+
            "\noutcome="+(saved&&captured->complete?"complete":"incomplete")+
            "\nreason="+reason+"\nrows="+std::to_string(captured->count)+
            "\npausedCarCalls="+std::to_string(pausedCalls)+"\ndataFile="+(saved?"signals.osig":"none")+
            "\nmodel=outrun.legacy@1\nroute=constant-fallback-software\nadmission=virtual\nphysicalOutput=false"+
            "\nnativeLoaded=false\ntelemetryDelivery=false\nsourceCommit="+request.source+
            "\nsourceIdentity=caller-package-provenance\nproxySha256="+request.proxySha+
            "\ngameSha256="+OutRunLifecycle::ExactDiskSha256+
            "\nnominalArithmeticHz=60\nspeedUnits=raw-field-1c4-unqualified\ngameplayReplay=false\n";
        if(!C::WriteNew(outcomePath,text))Log("SignalCapture: OUTCOME WRITE FAILED; no complete receipt");
        Log(saved&&captured->complete?"SignalCapture: complete; software requests only":"SignalCapture: incomplete; software requests only");
    }catch(...){Log("SignalCapture: save failed; no complete receipt");}
}
static void Fault(const char* why) noexcept {faulted=true;Finish(false,why);Log(why);}
static void Refuse(const std::wstring& path,const std::string& why) {
    MoveFileExW(path.c_str(),(root+L"\\request.refused.txt").c_str(),MOVEFILE_REPLACE_EXISTING);
    Log(("SignalCapture: request refused: "+why).c_str());
}
static void TryArm() {
    const auto path=root+L"\\request.txt";
    if(!C::Exists(path))return;
    const auto lease=C::LeaseText(leasePath);
    if(proxySha.empty())proxySha=C::FileSha256(Module::DllPath.c_str());
    C::Request next;
    auto why=C::Parse(C::ReadSmall(path),(long long)std::time(nullptr),OutRunLifecycle::ExactDiskSha256,proxySha,lease,next);
    if(why.empty()&&(!ConsumerLifecycle::hostVerified.load()||NetworkActive()))why="unverified host or network active";
    if(why.empty()&&!Legacy())why="only the existing legacy profile is supported";
    if(why.empty()&&(ffbLoaded||initialized||periodicsActive))why="native state present in software process";
    if(!why.empty()){Refuse(path,why);return;}
    request=next;
    if(!LeaseMatches()){Refuse(path,"expired rig lease");return;}
    directory=root+L"\\"+std::wstring(next.id.begin(),next.id.end());
    dataPath=directory+L"\\signals.osig";outcomePath=directory+L"\\outcome.txt";
    if(!CreateDirectoryW(directory.c_str(),nullptr)){Refuse(path,"result directory exists or unavailable");return;}
    if(!MoveFileExW(path.c_str(),(directory+L"\\request.txt").c_str(),0)){Refuse(path,"request claim failed");return;}
    if(!R::Begin(next.source,true)) {
        C::WriteNew(outcomePath,std::string("schema=")+C::Schema+"\noutcome=incomplete\nreason=buffer unavailable\n");return;
    }
    pausedCalls=0;started=GetTickCount64();
    Log("SignalCapture: armed; software legacy constant route; native and motion output blocked until exit");
}
static void Poll() {
    if(finalized||faulted||OutRunSignalMute::StartupMode()!=OutRunSignalMute::Mode::LegacyConstant)return;
    if(!resolved) {
        resolved=true;wchar_t base[MAX_PATH]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",base,MAX_PATH);
        if(!n||n>=MAX_PATH){Fault("SignalCapture: LOCALAPPDATA unavailable");return;}
        root=std::wstring(base)+L"\\Dbce\\StagePlayback\\outrun-force";
        leasePath=std::wstring(base)+L"\\dbce\\test-slot.txt";
    }
    ++update;
    if(R::session) {
        if(R::session->failed){Finish(false,"producer validity or bound failed");return;}
        if(!ConsumerLifecycle::hostVerified.load()||!Legacy()||NetworkActive()){Fault("SignalCapture: host, profile or offline eligibility changed");return;}
        if(update%60==0&&!LeaseMatches()){Fault("SignalCapture: rig lease lost");return;}
        if(GetTickCount64()-started>=ULONGLONG(request.seconds)*1000){Finish(true,"duration ended");return;}
        if(update%60==0&&C::Exists(directory+L"\\stop.txt")){Finish(false,"external stop before duration");return;}
    }else if(update%60==0)TryArm();
}
static void VirtualConstant(LONG value) {
    // This updates only the producer's duplicate-suppression state. No accepted
    // hardware command is claimed: v3 explicitly names virtual admission.
    if(value<-10000||value>10000||!std::isfinite(Settings::FFBGlobalStrength))throw std::runtime_error("invalid virtual request");
    prevConstantLevel=value;
}
static void VirtualPeriodic(int,float,float) {throw std::runtime_error("unexpected periodic request in constant route");}
static void Pause() {prevConstantLevel=prevStructLevel=0;warmupFrames=0;if(R::session)++pausedCalls;}
static void Produce(EVWORK_CAR* car) {
    if(faulted||finalized||OutRunSignalMute::StartupMode()!=OutRunSignalMute::Mode::LegacyConstant)return;
    if(!ConsumerLifecycle::hostVerified.load()||NetworkActive()){Pause();return;}
    if(!car||car!=Game::pl_car())return;
    if(!Legacy()){Fault("SignalCapture: unsupported selected model");return;}
    if(Overlay::IsActive||Overlay::WheelSettingsVisible||Overlay::IsBindingDialogActive||
        !Game::current_mode||*Game::current_mode!=STATE_GAME){Pause();return;}
    if(!Game::stg_stage_num||!Game::app_time||!Game::power_on_timer||!Game::sprani_num_ticks||!Game::game_mode){
        Fault("SignalCapture: game context unavailable");return;
    }
    int stage=int(*Game::stg_stage_num);
    if(producerCar!=car||producerStage!=stage||producerId!=car->car_id_10||producerKind!=car->car_kind_11) {
        if(producerCar&&R::session){Finish(false,"local car or stage changed");}
        ResetCalculationState();producerCar=car;producerStage=stage;producerId=car->car_id_10;producerKind=car->car_kind_11;
    }
    // Never pretend a native module or periodic capability exists. The explicit
    // route uses the legacy model's existing constant-force vibration fallback.
    if(ffbLoaded||initialized||useSharedModel||periodicsActive||slotRoadTexture!=-1||slotTireSlip!=-1){Fault("SignalCapture: native state present");return;}
    float roughness=0;DWORD water=0;SampleSurface(car,roughness,water);
    std::array<double,10> input={car->field_1C4,double(car->field_8),car->field_264,car->field_268,
        double(car->cur_gear_208),car->field_1D0,car->field_1D4,double(car->pedal_amount_34),roughness,double(water)};
    const auto config=R::Configuration();
    if(!R::SafeInput(input)||!R::Finite(config)){Fault("SignalCapture: invalid force input or setting");return;}
    // Bounds prevent invalid arithmetic on a corrupt/unreviewed configuration;
    // nothing is clamped or substituted in the captured settings.
    for(double value:config)if(value<0||value>100){Fault("SignalCapture: force setting outside diagnostic bounds");return;}
    for(int i:{0,2,3,5,6})if(std::abs(input[i])>10000){Fault("SignalCapture: force input outside diagnostic bounds");return;}
    R::pendingContext={double(update),double(*Game::app_time),double(*Game::power_on_timer),double(*Game::sprani_num_ticks),
        double(*Game::current_mode),double(*Game::game_mode),double(stage),double(car->car_id_10),double(car->car_kind_11),double(car->manu_transmission_enable_13)};
    for(double v:R::pendingContext)if(!R::Integral(v,0,4294967295.0)){Fault("SignalCapture: invalid game context");return;}
    CalculateSignals(car,roughness,water,{VirtualConstant,VirtualPeriodic},GetTickCount,nullptr,true);
}
}
void SignalCaptureUpdate() noexcept {
    if(!OutRunSignalMute::BlocksOutput())return;
    try{ConsumerLifecycle::Gate::Lease lease(ConsumerLifecycle::Runtime(),true);if(lease)SignalCaptureRuntime::Poll();}
    catch(...){SignalCaptureRuntime::Fault("SignalCapture: update exception");}
}
void ProcessMutedSignals(EVWORK_CAR* car) {
    try{SignalCaptureRuntime::Produce(car);}catch(...){SignalCaptureRuntime::Fault("SignalCapture: producer exception");}
}
void FinalizeSignalCapture() noexcept {
    SignalCaptureRuntime::finalized=true;
    SignalCaptureRuntime::Finish(false,"game exited before duration");
}
}
