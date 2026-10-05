// Offline adapter over the existing strict codec and actual production replay.
// No duplicated force model, new recorder or native force/device calls.
#define OUTRUN_RECORDING_NO_MAIN
#include "signal_recording_fixture.cpp"
#include <bcrypt.h>
#pragma comment(lib,"bcrypt.lib")
struct CommandSummary {
    size_t frames=0, checkpoints=0, configChanges=0, constants=0, nonzeroConstants=0, capConstants=0;
    size_t constantFrames=0, nonzeroConstantFrames=0, feedbackDiffers=0;
    size_t periodic=0, periodicFrames=0, requestPairs=0, positiveTickPairs=0, equalTickPairs=0;
    double minConstant=0,maxConstant=0,peakAbs=0,sumAbs=0,maxDelta=0,maxRate=0;
    double minAmplitude=0,maxAmplitude=0,minHz=0,maxHz=0;
    uint64_t tickSpan=0;
};
static CommandSummary Summarize(const R::Session& s) {
    Require(s.complete&&!s.failed&&s.count>0&&s.count<=R::Capacity,"summary requires bounded complete recording");
    CommandSummary m; m.frames=s.count; m.tickSpan=uint64_t(s.frames[s.count-1].tick)-s.frames[0].tick;
    bool havePrevious=false; double previous=0; DWORD previousTick=0;
    for(size_t i=0;i<s.count;i++) {
        const auto& f=s.frames[i];m.checkpoints+=f.checkpoint?1:0;
        const bool changed=i&&f.config!=s.frames[i-1].config;m.configChanges+=changed?1:0;
        // No slew pair spans a reset/selection/state checkpoint or config change.
        if(f.checkpoint||changed)havePrevious=false;
        bool hasConstant=false,hasNonzero=false,hasPeriodic=false;
        for(size_t n=0;n<f.count;n++) {
            const auto& q=f.requests[n];
            if(q.kind==1) {
                if(!m.constants){m.minConstant=m.maxConstant=q.magnitude;}
                m.minConstant=std::min(m.minConstant,q.magnitude);m.maxConstant=std::max(m.maxConstant,q.magnitude);
                m.peakAbs=std::max(m.peakAbs,std::abs(q.magnitude));m.sumAbs+=std::abs(q.magnitude);
                ++m.constants;m.nonzeroConstants+=q.magnitude!=0;m.capConstants+=std::abs(q.magnitude)==10000;
                m.feedbackDiffers+=q.previousAfter!=q.magnitude;hasConstant=true;hasNonzero|=q.magnitude!=0;
                if(havePrevious) {
                    ++m.requestPairs;const double delta=std::abs(q.magnitude-previous);m.maxDelta=std::max(m.maxDelta,delta);
                    if(f.tick>previousTick){++m.positiveTickPairs;m.maxRate=std::max(m.maxRate,delta*1000.0/double(uint64_t(f.tick)-previousTick));}
                    else ++m.equalTickPairs;
                }
                previous=q.magnitude;previousTick=f.tick;havePrevious=true;
            } else if(q.kind==2) {
                if(!m.periodic){m.minAmplitude=m.maxAmplitude=q.magnitude;m.minHz=m.maxHz=q.frequency;}
                m.minAmplitude=std::min(m.minAmplitude,q.magnitude);m.maxAmplitude=std::max(m.maxAmplitude,q.magnitude);
                m.minHz=std::min(m.minHz,q.frequency);m.maxHz=std::max(m.maxHz,q.frequency);
                ++m.periodic;hasPeriodic=true;
            }
        }
        m.constantFrames+=hasConstant;m.nonzeroConstantFrames+=hasNonzero;m.periodicFrames+=hasPeriodic;
    }
    return m;
}
static std::string Sha256(const R::Bytes& bytes) {
    unsigned char hash[32]{};
    Require(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,const_cast<unsigned char*>(bytes.data()),(ULONG)bytes.size(),hash,32)>=0,"SHA256 failed");
    std::ostringstream out;out<<std::hex<<std::setfill('0');for(unsigned char b:hash)out<<std::setw(2)<<unsigned(b);return out.str();
}
static std::string DeclaredRevision(const R::Session& s) {
    std::ostringstream out;out<<std::hex<<std::setfill('0');for(unsigned char b:s.source)out<<std::setw(2)<<unsigned(b);return out.str();
}
static void Value(bool supported,double number) {if(supported)std::cout<<number;else std::cout<<"null";}
static void WriteSummary(const R::Session& s,const CommandSummary& m) {
    std::cout.imbue(std::locale::classic());std::cout<<std::setprecision(17);
    std::cout<<"{\n  \"schema\": \"dbce.outrun2006.command-summary\",\n  \"version\": 1,\n"
        "  \"metric_definitions\": \"dbce.outrun2006.command-metrics@1\",\n"
        "  \"recording_format\": \"experimental-DBCEORR2-v2\",\n"
        "  \"recording_sha256\": \""<<Sha256(R::Encode(s))<<"\",\n"
        "  \"declared_calculation_revision\": \""<<DeclaredRevision(s)<<"\",\n"
        "  \"declared_revision_authenticated\": false,\n  \"production_recalculation_verified\": true,\n"
        "  \"physical_output\": false,\n  \"domain\": \"legacy-calculation-requests-before-master-strength-and-native-encoding\",\n"
        "  \"frame_count\": "<<m.frames<<",\n  \"checkpoint_count\": "<<m.checkpoints<<",\n"
        "  \"configuration_change_count\": "<<m.configChanges<<",\n  \"sample_tick_span_ms\": "<<m.tickSpan<<",\n"
        "  \"fixed60_calculation_step_sum_seconds\": "<<double(m.frames)/60.0<<",\n  \"constant\": {\n"
        "    \"units\": \"pre-actuator-native-range-request\",\n    \"request_count\": "<<m.constants<<",\n"
        "    \"minimum_signed_request\": ";Value(m.constants,m.minConstant);
    std::cout<<",\n    \"maximum_signed_request\": ";Value(m.constants,m.maxConstant);
    std::cout<<",\n    \"peak_absolute_request\": ";Value(m.constants,m.peakAbs);
    std::cout<<",\n    \"mean_absolute_request\": ";Value(m.constants,m.constants?m.sumAbs/m.constants:0);
    std::cout<<",\n    \"request_cap_absolute_units\": 10000,\n    \"requests_at_cap\": "<<m.capConstants<<",\n    \"request_cap_fraction\": ";Value(m.constants,m.constants?double(m.capConstants)/m.constants:0);
    std::cout<<",\n    \"nonzero_requests\": "<<m.nonzeroConstants<<",\n    \"nonzero_request_fraction\": ";Value(m.constants,m.constants?double(m.nonzeroConstants)/m.constants:0);
    std::cout<<",\n    \"frames_with_request\": "<<m.constantFrames<<",\n    \"frames_with_nonzero_request\": "<<m.nonzeroConstantFrames<<",\n"
        "    \"request_frame_activity_fraction\": "<<double(m.constantFrames)/m.frames<<",\n"
        "    \"nonzero_request_frame_activity_fraction\": "<<double(m.nonzeroConstantFrames)/m.frames<<",\n"
        "    \"requests_with_feedback_different_from_requested_value\": "<<m.feedbackDiffers<<",\n"
        "    \"within_segment_request_pairs\": "<<m.requestPairs<<",\n    \"maximum_absolute_request_delta\": ";Value(m.requestPairs,m.maxDelta);
    std::cout<<",\n    \"positive_tick_request_pairs\": "<<m.positiveTickPairs<<",\n    \"equal_tick_pairs_without_rate\": "<<m.equalTickPairs<<",\n"
        "    \"maximum_request_pair_average_delta_units_per_sample_tick_second\": ";Value(m.positiveTickPairs,m.maxRate);
    std::cout<<"\n  },\n  \"periodic\": {\n    \"units\": \"pre-clamp-amplitude-parameter-and-requested-Hz\",\n"
        "    \"request_count\": "<<m.periodic<<",\n    \"frames_with_request\": "<<m.periodicFrames<<",\n    \"minimum_amplitude_parameter\": ";Value(m.periodic,m.minAmplitude);
    std::cout<<",\n    \"maximum_amplitude_parameter\": ";Value(m.periodic,m.maxAmplitude);
    std::cout<<",\n    \"minimum_requested_frequency_hz\": ";Value(m.periodic,m.minHz);
    std::cout<<",\n    \"maximum_requested_frequency_hz\": ";Value(m.periodic,m.maxHz);
    std::cout<<"\n  },\n  \"unsupported\": {\n"
        "    \"post_strength_output_envelope\": null,\n    \"time_weighted_output_cap_occupancy\": null,\n"
        "    \"effect_duty_or_nonzero_hold_duration\": null,\n    \"physical_torque_nm\": null,\n"
        "    \"instantaneous_actuator_slew\": null,\n    \"shutdown_zero_or_release_success\": null,\n"
        "    \"cross_game_feel_equivalence\": null\n  }\n}\n";
}
static void MetricTests() {
    auto s=std::make_unique<R::Session>();s->complete=true;s->count=3;
    for(size_t i=0;i<3;i++){auto& f=s->frames[i];f.tick=(DWORD)(i*10);f.checkpoint=i==0;f.count=1;f.requests[0]={1,0,double(int(i)*10000-10000),0,0};}
    auto checked=R::Decode(R::Encode(*s));auto m=Summarize(*checked);
    Require(m.constants==3&&m.capConstants==2&&m.nonzeroConstants==2&&m.peakAbs==10000&&m.sumAbs==20000&&m.tickSpan==20&&m.requestPairs==2&&m.maxDelta==10000&&m.maxRate==1000000,"magnitude/cap/activity/duration/rate math");
    s->frames[1].requests[0].magnitude=10000;s->frames[2].requests[0].magnitude=-10000;
    m=Summarize(*R::Decode(R::Encode(*s)));Require(m.maxDelta==20000&&m.maxRate==2000000,"signed reversal delta");
    s->frames[1].checkpoint=true;m=Summarize(*R::Decode(R::Encode(*s)));Require(m.requestPairs==1,"checkpoint pair exclusion");
    s->frames[1].checkpoint=false;s->frames[1].config[12]=s->frames[2].config[12]=0.5;
    m=Summarize(*R::Decode(R::Encode(*s)));Require(m.requestPairs==1&&m.configChanges==1,"configuration pair exclusion");
    s->frames[1].config[12]=s->frames[2].config[12]=0;s->frames[1].tick=s->frames[2].tick=0;
    m=Summarize(*R::Decode(R::Encode(*s)));Require(m.equalTickPairs==2&&m.positiveTickPairs==0&&m.tickSpan==0,"equal tick refusal for rate");
    for(auto& f:s->frames){f.count=0;f.requests={};}
    m=Summarize(*R::Decode(R::Encode(*s)));Require(!m.constants&&!m.periodic&&!m.requestPairs,"empty channels must not invent magnitudes/rates");
    s->frames[0].count=1;s->frames[0].requests[0]={2,0,1.25,25,0};
    m=Summarize(*R::Decode(R::Encode(*s)));Require(m.periodic==1&&m.maxAmplitude==1.25&&!m.constants,"periodic raw parameter must not be clamped or merged into constant");
    s->complete=false;bool refused=false;try{Summarize(*s);}catch(...){refused=true;}Require(refused,"incomplete summary accepted");
    std::cout<<"PASS: eight numeric-only metric cases; checkpoint/config segmentation, equal ticks, absent channels, unmerged periodic units, incomplete refusal\n";
}
int main(int argc,char** argv) {
    try {
        spdlog::set_level(spdlog::level::off);
        if(argc==2&&std::string_view(argv[1])=="selftest"){MetricTests();return 0;}
        if(argc!=4||std::string_view(argv[1])!="summary")throw std::runtime_error("usage: command-summary selftest | summary recording expected-declared-revision");
        auto s=R::ReadFile(argv[2]);const std::string expected=argv[3];
        if(expected.size()!=40||expected.find_first_not_of("0123456789abcdef")!=std::string::npos||DeclaredRevision(*s)!=expected)
            throw std::runtime_error("declared provenance mismatch");
        Replay(*s);WriteSummary(*s,Summarize(*s));return 0;
    }catch(const Mismatch& e){std::cerr<<e.what()<<"\n";return 1;}
    catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}
}
