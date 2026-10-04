#pragma once
#include "dispatch_trace_fixture.hpp"
#include <vector>
#include <stdexcept>
#include <limits>
namespace DispatchFixture::Codec {
using Bytes=std::vector<uint8_t>;
// Caller-declared hashes, never authenticated identities or private paths.
struct Recording {std::array<uint8_t,32> sourceSha256{},configSha256{};Trace trace{};};
constexpr size_t MaxBytes=8+4+64+Capacity*20+20;
inline void Require(bool ok){if(!ok)throw std::runtime_error("invalid ordered dispatch recording");}
inline void Put(Bytes& b,uint64_t v,size_t n){for(size_t i=0;i<n;i++)b.push_back(uint8_t(v>>(i*8)));}
inline uint64_t Checksum(const Bytes& b,size_t n){uint64_t h=14695981039346656037ull;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ull;}return h;}
inline Bytes Encode(const Recording& r){
 Require(Valid(r.trace));Bytes b{'O','R','D','I','Q','0','0','1'};Put(b,r.trace.count,4);
 b.insert(b.end(),r.sourceSha256.begin(),r.sourceSha256.end());b.insert(b.end(),r.configSha256.begin(),r.configSha256.end());
 for(size_t i=0;i<r.trace.count;i++){const auto& e=r.trace.entries[i];Put(b,i,4);Put(b,unsigned(e.query.kind),4);Put(b,e.query.argument,4);Put(b,e.tick,4);Put(b,uint32_t(e.result),4);}
 Put(b,0x21444e45,4);Put(b,1,4);Put(b,r.trace.count,4);Put(b,Checksum(b,b.size()),8);Require(b.size()<=MaxBytes);return b;
}
inline Recording Decode(const Bytes& b){
 Require(b.size()>=116&&b.size()<=MaxBytes);size_t p=0;
 auto get=[&](size_t n){Require(n<=b.size()-p);uint64_t v=0;for(size_t i=0;i<n;i++)v|=uint64_t(b[p++])<<(i*8);return v;};
 const std::array<uint8_t,8> magic{'O','R','D','I','Q','0','0','1'};for(auto v:magic)Require(get(1)==v);
 Recording r;const auto count=get(4);Require(count>0&&count<=Capacity&&b.size()==76+count*20+20);r.trace.count=size_t(count);
 for(auto& v:r.sourceSha256)v=uint8_t(get(1));for(auto& v:r.configSha256)v=uint8_t(get(1));
 for(size_t i=0;i<r.trace.count;i++){Require(get(4)==i);const auto kind=get(4);Require(kind<=unsigned(Kind::VolumeSwitch));auto& e=r.trace.entries[i];e.query.kind=Kind(kind);e.query.argument=uint32_t(get(4));e.tick=uint32_t(get(4));const auto bits=get(4);const int64_t signedValue=bits<=0x7fffffffull?int64_t(bits):int64_t(bits)-0x100000000ll;e.result=int(signedValue);}
 Require(get(4)==0x21444e45&&get(4)==1&&get(4)==count);const auto sum=get(8);Require(sum==Checksum(b,b.size()-8)&&p==b.size());r.trace.complete=true;Require(Valid(r.trace));return r;
}
// No filesystem API, live capture hook or runtime playback injection.
}
