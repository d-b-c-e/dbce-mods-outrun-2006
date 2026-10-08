#pragma once
// Fixed numeric layout; never serializes C++ struct padding,
// pointers, device GUIDs, strings, file paths or raw game memory.
#include <fstream>
#include <stdexcept>
#include <cstring>
namespace FFB::SignalRecording {
using Bytes=std::vector<unsigned char>;
constexpr size_t MaxBytes=9*1024*1024;
static void Put(Bytes& b,uint64_t v,size_t n){for(size_t i=0;i<n;i++)b.push_back((unsigned char)(v>>(i*8)));}
static uint64_t Checksum(const Bytes& b){uint64_t h=14695981039346656037ull;for(auto v:b){h^=v;h*=1099511628211ull;}return h;}
static void Number(Bytes&b,double d){static_assert(sizeof(double)==8);uint64_t v;std::memcpy(&v,&d,8);Put(b,v,8);}
template<size_t N>static void Numbers(Bytes&b,const std::array<double,N>&a){for(double d:a)Number(b,d);}
static Bytes Encode(const Session& s){
    if(s.count>s.Limit())throw std::runtime_error("frame count bound");
    Bytes b; b.reserve(60+s.count*(s.softwareOnly?1136:1056)); for(char c:std::string_view(s.softwareOnly?"DBCEORS3":"DBCEORR2"))b.push_back(c);
    Put(b,s.softwareOnly?3:2,4);Put(b,60,4);for(auto v:s.source)b.push_back(v);Put(b,s.count,4);
    for(size_t i=0;i<s.count;i++){const auto&f=s.frames[i];Put(b,i,4);Put(b,f.tick,4);Put(b,f.checkpoint?1:0,4);
        Numbers(b,f.before);Numbers(b,f.after);Numbers(b,f.config);Numbers(b,f.input);Put(b,f.count,4);
        for(const auto&r:f.requests){Number(b,r.kind);Number(b,r.slot);Number(b,r.magnitude);Number(b,r.frequency);Number(b,r.previousAfter);}
        if(s.softwareOnly)Numbers(b,f.context);}
    Put(b,0x454e4421,4);Put(b,s.complete&&!s.failed?1:0,4);Put(b,s.count,4);Put(b,Checksum(b),8);
    if(b.size()>(s.softwareOnly?MaxBytes:256*1024))throw std::runtime_error("session size bound");return b;
}
static void SaveNew(const wchar_t* path,const Session& s){
    auto b=Encode(s);HANDLE h=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("new output unavailable");
    DWORD n=0;bool ok=WriteFile(h,b.data(),(DWORD)b.size(),&n,nullptr)!=FALSE&&n==b.size();CloseHandle(h);
    if(!ok)throw std::runtime_error("incomplete file write");
}
static bool Integer(double d,double min,double max){return std::isfinite(d)&&d>=min&&d<=max&&std::floor(d)==d;}
static bool ValidState(const State&s){
    if(!SafeState(s))return false;
    for(int i:{0,1,2})if(!Integer(s[i],0,4294967295.0))return false;
    for(int i:{5,6,7,11,12,14,15})if(!Integer(s[i],0,2147483646))return false;
    for(int i:{9,10})if(!Integer(s[i],-10000,10000))return false;
    for(int i:{18,19})if(!Integer(s[i],0,1))return false;
    for(int i:{20,21})if(!Integer(s[i],-1,3))return false;
    // Floats must roundtrip exactly: no hidden precision lost by restoration.
    for(size_t i=0;i<s.size();i++)if(i==3||i==4||i==8||i==13||i==16||i==17||i>=22)
        if(double((float)s[i])!=s[i])return false;
    return true;
}
static std::unique_ptr<Session> Decode(const Bytes&b){
    if(b.size()>MaxBytes||b.size()<60)throw std::runtime_error("session byte bound");
    size_t pos=0;auto get=[&](size_t n){if(n>8||pos+n>b.size())throw std::runtime_error("truncated session");
        uint64_t v=0;for(size_t i=0;i<n;i++)v|=uint64_t(b[pos++])<<(i*8);return v;};
    std::string magic;for(int i=0;i<8;i++)magic+=(char)get(1);
    const bool softwareOnly=magic=="DBCEORS3";
    if(!softwareOnly && magic!="DBCEORR2")throw std::runtime_error("unsupported magic");
    if(get(4)!=(softwareOnly?3:2)||get(4)!=60)throw std::runtime_error("unsupported version/rate");
    if(!softwareOnly && b.size()>256*1024)throw std::runtime_error("legacy byte bound");
    auto s=std::make_unique<Session>();s->softwareOnly=softwareOnly;for(auto&v:s->source)v=(unsigned char)get(1);
    s->count=(size_t)get(4);if(!s->count||s->count>s->Limit())throw std::runtime_error("frame count bound");
    auto number=[&](){auto bits=get(8);double d;std::memcpy(&d,&bits,8);if(!std::isfinite(d)||std::abs(d)>1e12)throw std::runtime_error("nonfinite/range channel");return d;};
    auto numbers=[&](auto& a){for(auto& d:a)d=number();};
    for(size_t i=0;i<s->count;i++){auto&f=s->frames[i];if(get(4)!=i)throw std::runtime_error("sequence mismatch");
        f.tick=(DWORD)get(4);auto reset=get(4);if(reset>1||(!i&&!reset))throw std::runtime_error("checkpoint marker");f.checkpoint=reset!=0;
        numbers(f.before);numbers(f.after);numbers(f.config);numbers(f.input);f.count=(size_t)get(4);
        if(f.count>3||!ValidState(f.before)||!ValidState(f.after)||!Integer(f.config[11],0,1))throw std::runtime_error("state/config/request bound");
        if(softwareOnly)for(const auto*state:{&f.before,&f.after})
            if((*state)[18]!=0||(*state)[19]!=0||(*state)[20]!=-1||(*state)[21]!=-1)
                throw std::runtime_error("software-only session claims native state");
        for(size_t c=0;c<f.config.size();c++)if(c!=11&&double((float)f.config[c])!=f.config[c])throw std::runtime_error("config precision");
        if(!Integer(f.input[1],0,4294967295.0)||!Integer(f.input[4],0,6)||!Integer(f.input[7],0,255)||
           !Integer(f.input[9],0,1)||f.input[0]<0||f.input[8]<0||f.input[8]>1)throw std::runtime_error("input validity");
        for(int c:{0,2,3,5,6,8})if(double((float)f.input[c])!=f.input[c])throw std::runtime_error("input precision");
        if(i&&(f.tick<s->frames[i-1].tick||(!f.checkpoint&&f.before!=s->frames[i-1].after)))throw std::runtime_error("time/reset continuity");
        for(size_t r=0;r<3;r++){auto&q=f.requests[r];q={number(),number(),number(),number(),number()};
            if(r>=f.count){if(q.kind||q.slot||q.magnitude||q.frequency||q.previousAfter)throw std::runtime_error("unused request data");continue;}
            if(!Integer(q.previousAfter,-10000,10000))throw std::runtime_error("output state");
            if(q.kind==1){if(q.slot||q.frequency||!Integer(q.magnitude,-10000,10000))throw std::runtime_error("constant request");}
            else if(q.kind==2){if(!Integer(q.slot,-1,3)||q.magnitude<0||q.frequency<1||q.frequency>100||
                double((float)q.magnitude)!=q.magnitude||double((float)q.frequency)!=q.frequency)throw std::runtime_error("periodic request");}
            else throw std::runtime_error("request kind");
            if(softwareOnly && (q.kind!=1||q.previousAfter!=q.magnitude))throw std::runtime_error("software-only sink contract");
        }
        if(softwareOnly){numbers(f.context);for(double v:f.context)if(!Integer(v,0,4294967295.0))throw std::runtime_error("game context");
            if(i&&f.context[0]<=s->frames[i-1].context[0])throw std::runtime_error("game update order");}
    }
    if(get(4)!=0x454e4421||get(4)!=1||get(4)!=s->count)throw std::runtime_error("incomplete/footer mismatch");
    Bytes content(b.begin(),b.begin()+pos);auto checksum=get(8);
    if(pos!=b.size()||checksum!=Checksum(content))throw std::runtime_error("checksum/trailing bytes");
    s->complete=true;return s;
}
static std::unique_ptr<Session> ReadFile(const char* path){
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("session unreadable");
    file.seekg(0,std::ios::end);auto size=file.tellg();if(size<0||size>MaxBytes)throw std::runtime_error("file bound");
    file.seekg(0);Bytes b((size_t)size);if(!file.read((char*)b.data(),size))throw std::runtime_error("session read failed");return Decode(b);
}
}
