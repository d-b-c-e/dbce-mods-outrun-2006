#include "dispatch_trace_codec.hpp"
#include <fstream>
#include <iostream>
using namespace DispatchFixture;
static void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void Refuse(const Codec::Bytes& b){bool refused=false;try{Codec::Decode(b);}catch(const std::exception&){refused=true;}Check(refused,"malformed accepted");}
static void Reseal(Codec::Bytes& b){auto h=Codec::Checksum(b,b.size()-8);for(size_t i=0;i<8;i++)b[b.size()-8+i]=uint8_t(h>>(i*8));}
int main(int argc,char** argv){try{
 Check(argc==2,"synthetic output path required");Codec::Recording r;r.sourceSha256[0]=0x12;r.configSha256[31]=0xab;
 const std::array<Observation,6> rows{{{{Kind::Current,0},100,-127},{{Kind::Previous,0},100,127},{{Kind::SwitchNow,0xc0},100,1},{{Kind::SwitchOn,0xc0},100,0},{{Kind::SwitchOn,0x80},117,1},{{Kind::VolumeSwitch,0},117,-1}}};
 Capture c;for(const auto& e:rows)c.Observe(e.query,e.tick,false,[&](Query){return e.result;});r.trace=c.Stop();
 const auto bytes=Codec::Encode(r);auto decoded=Codec::Decode(bytes);Check(Codec::Encode(decoded)==bytes,"canonical bytes");Check(decoded.sourceSha256==r.sourceSha256&&decoded.configSha256==r.configSha256,"declared hashes");
 Playback p;Check(p.Begin(decoded.trace),"decoded playback begin");for(const auto& e:rows){int output=555;Check(p.TryReturn(e.query,e.tick-100,false,output)&&output==e.result,"decoded exact order/edges/currentprevious");}Check(!p.Active(),"decoded exhaustion");
 {std::ofstream f(argv[1],std::ios::binary|std::ios::trunc);Check(bool(f),"fixture output");f.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));Check(bool(f),"fixture write");}
 {std::ifstream f(argv[1],std::ios::binary);Codec::Bytes disk((std::istreambuf_iterator<char>(f)),{});Check(disk==bytes&&Codec::Encode(Codec::Decode(disk))==bytes,"file roundtrip");}
 for(size_t n=0;n<bytes.size();n++)Refuse(Codec::Bytes(bytes.begin(),bytes.begin()+n));
 for(size_t i=0;i<bytes.size();i++){auto bad=bytes;bad[i]^=1;Refuse(bad);}
 auto bad=bytes;bad.push_back(0);Refuse(bad);bad=bytes;bad[7]='2';Reseal(bad);Refuse(bad);
 for(size_t offset:{size_t(8),size_t(76),size_t(80),size_t(84),bytes.size()-16}){bad=bytes;bad[offset]=0xff;Reseal(bad);Refuse(bad);}
 bad=bytes;bad[76+20+12]=99;Reseal(bad);Refuse(bad); // backward tick
 bad=bytes;bad[76+5*20+12]=0x35;bad[76+5*20+13]=8;Reseal(bad);Refuse(bad); // >2000ms span
 r.trace.entries[0].result=std::numeric_limits<int32_t>::min();r.trace.entries[1].result=std::numeric_limits<int32_t>::max();decoded=Codec::Decode(Codec::Encode(r));Check(decoded.trace.entries[0].result==r.trace.entries[0].result&&decoded.trace.entries[1].result==r.trace.entries[1].result,"signed boundaries");
 r.trace.count=Capacity;for(size_t i=0;i<Capacity;i++)r.trace.entries[i]={{Kind::SwitchNow,1},uint32_t(i),int(i)};Check(Codec::Encode(r).size()==Codec::MaxBytes&&Codec::Decode(Codec::Encode(r)).trace.count==Capacity,"exact capacity");r.trace.complete=false;bool refused=false;try{Codec::Encode(r);}catch(...){refused=true;}Check(refused,"incomplete encode");
 std::cout<<"PASS: synthetic file/memory canonical roundtrip; declared hash retention; exact ordered fake playback; every truncation and byte corruption refused; resealed version/count/sequence/kind/argument/footer/time errors refused; signed boundaries/capacity/incomplete checked. No game/device/native writer.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
