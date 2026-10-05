#include "dispatch_trace_fixture.hpp"
#include <iostream>
#include <stdexcept>
using namespace DispatchFixture;
static void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
 const std::array<Observation,8> source{{
  {{Kind::Current,0},100,-42},{{Kind::Previous,0},100,-17},
  {{Kind::Current,1},100,201},{{Kind::Current,2},100,31},
  {{Kind::SwitchNow,0xc0},100,1},{{Kind::SwitchOn,0xc0},100,0},
  {{Kind::SwitchOn,0x80},117,1},{{Kind::VolumeSwitch,0},117,-1}}};
 Capture capture;size_t calls=0;
 for(const auto& e:source){auto path=[&](Query q){Check(q.kind==e.query.kind&&q.argument==e.query.argument,"query forwarded intact");++calls;return e.result;};Check(capture.Observe(e.query,e.tick,false,path)==e.result,"return unchanged");}
 auto trace=capture.Stop();Check(Valid(trace)&&calls==8,"ordered capture complete");
 Playback replay;Check(replay.Begin(trace),"valid begin");trace.entries[0].result=999;
 for(const auto& e:source){int result=777;Check(replay.TryReturn(e.query,e.tick-100,false,result)&&result==e.result,"exact order/current/previous/chord/edge preserved");}
 Check(!replay.Active(),"exhaustion stops");int result=777;Check(!replay.TryReturn(source[0].query,0,false,result)&&result==777,"no repeat/last held value");
 trace.entries[0].result=-42;
 for(int mode=0;mode<4;mode++){Check(replay.Begin(trace),"fresh start");result=777;auto q=source[0].query;if(mode==1)q.kind=Kind::Previous;if(mode==2)q.argument=1;Check(!replay.TryReturn(q,mode==3?1:0,mode==0,result)&&result==777&&!replay.Active(),"focus/UI/query/order/time refusal");}
 Check(replay.Begin(trace),"cancel start");replay.Cancel();Check(!replay.TryReturn(source[0].query,0,false,result),"explicit cancellation");
 auto bad=trace;bad.complete=false;Check(!replay.Begin(bad),"incomplete");bad=trace;bad.count=Capacity+1;Check(!replay.Begin(bad),"count bound");bad=trace;bad.entries[1].tick=99;Check(!replay.Begin(bad),"backward tick");bad=trace;bad.entries[7].tick=2101;Check(!replay.Begin(bad),"duration bound");bad=trace;bad.entries[0].query.argument=3;Check(!replay.Begin(bad),"unsupported analog channel");
 Capture full;calls=0;auto path=[&](Query){++calls;return 5;};for(size_t i=0;i<Capacity+1;i++)Check(full.Observe({Kind::SwitchNow,1},100,false,path)==5,"overflow observe-only");Check(full.Count()==Capacity&&!full.Stop().complete&&calls==Capacity+1,"query bound and no path suppression");
 for(int mode=0;mode<4;mode++){Capture c;c.Observe({Kind::Current,0},100,false,path);c.Observe({Kind::Current,0},mode==1?99:mode==2?2101:100,mode==0,path);if(mode==3)c.Cancel();Check(!c.Stop().complete,"capture focus/UI/time/cancel");}
 // A repeated millisecond is a poll group, not a proven game update/frame.
 Check(source[0].tick==source[5].tick&&source[0].query.kind!=source[5].query.kind,"same-tick query distinctions");
 std::cout<<"PASS: bounded ordered capture; unchanged callback results; immutable replay copy; current/previous/chord/edge distinctions; equal-ms groups; count/time/malformed/cancel/focus/UI/exhaustion refusal. Memory-only fake interface; no game/device/native output.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
