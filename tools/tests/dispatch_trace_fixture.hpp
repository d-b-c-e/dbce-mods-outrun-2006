#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
namespace DispatchFixture {
// Test-only protocol over existing return interfaces; not installed in hooks.
enum class Kind:uint8_t {Current,Previous,SwitchOn,SwitchNow,VolumeSwitch};
struct Query {Kind kind;uint32_t argument;};
struct Observation {Query query{};uint32_t tick=0;int result=0;};
constexpr size_t Capacity=128;constexpr uint32_t MaxSpanMs=2000;
struct Trace {std::array<Observation,Capacity> entries{};size_t count=0;bool complete=false;};
inline bool ValidQuery(Query q){return unsigned(q.kind)<=unsigned(Kind::VolumeSwitch)&&
 ((q.kind==Kind::SwitchOn||q.kind==Kind::SwitchNow)?q.argument!=0:q.argument<=2);}
inline bool Valid(const Trace& t){
 if(!t.complete||!t.count||t.count>Capacity)return false;
 for(size_t i=0;i<t.count;i++)if(!ValidQuery(t.entries[i].query)||
  (i&&t.entries[i].tick<t.entries[i-1].tick)||
  uint64_t(t.entries[i].tick)-t.entries[0].tick>MaxSpanMs)return false;
 return true;
}
class Capture {
 Trace trace{};bool active=true,failed=false;
public:
 template<class ReturnPath> int Observe(Query q,uint32_t tick,bool blocked,ReturnPath&& path){
  // Always preserve the existing path result; capture failure cannot inject it.
  const int result=path(q);
  if(!active)return result;
  if(blocked||!ValidQuery(q)||trace.count==Capacity||
   (trace.count&&(tick<trace.entries[trace.count-1].tick||uint64_t(tick)-trace.entries[0].tick>MaxSpanMs))){Cancel();return result;}
  trace.entries[trace.count++]={q,tick,result};return result;
 }
 void Cancel(){active=false;failed=true;}
 Trace Stop(){active=false;trace.complete=!failed&&trace.count>0;return trace;}
 size_t Count()const{return trace.count;}
};
// Owns a bounded copy: caller mutations cannot change active replay.
class Playback {
 Trace trace{};size_t cursor=0;bool active=false;
public:
 bool Begin(const Trace& input){Cancel();if(!Valid(input))return false;trace=input;cursor=0;active=true;return true;}
 void Cancel(){active=false;}
 bool Active()const{return active;}
 bool TryReturn(Query q,uint32_t relativeMs,bool blocked,int& result){
  // Refusal leaves result untouched; caller must choose existing safe path.
  if(!active)return false;
  if(blocked||relativeMs>MaxSpanMs||cursor==trace.count){Cancel();return false;}
  const auto& e=trace.entries[cursor];
  if(!ValidQuery(q)||q.kind!=e.query.kind||q.argument!=e.query.argument||
   relativeMs!=e.tick-trace.entries[0].tick){Cancel();return false;}
  result=e.result;++cursor;if(cursor==trace.count)Cancel();return true;
 }
};
}
