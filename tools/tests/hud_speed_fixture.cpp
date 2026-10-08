#include "../../src/hud_speed.hpp"
#include "../../lib/toolkit/include/forza_packet.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <cstring>
#include <initializer_list>

static int checks=0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
int main() {
    for (const auto kmh : {0.0f,18.0700684f,36.0f,51.0f,103.0f,124.165169f,360.0f}) {
        dbce::forza::Sled sled{}; dbce::forza::Dash dash{};
        dash.gear=2; dash.speed=-1;
        CHECK(OutRunHudSpeed::TryMetresPerSecond(kmh,dash.speed));
        unsigned char packet[311]{};
        CHECK(dbce::forza::build(dbce::forza::FORZA_FM7_DASH_311,sled,dash,packet,sizeof(packet))==311);
        float decoded=-1; std::memcpy(&decoded,packet+244,sizeof(decoded));
        CHECK(std::fabs(decoded*3.6f-kmh)<0.0001f);
        CHECK(packet[307]==2);
    }
    float output=123;
    for (const auto bad : {-1.0f,-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        CHECK(!OutRunHudSpeed::TryMetresPerSecond(bad,output));
        CHECK(output==123);
    }
    CHECK(OutRunHudSpeed::TryMetresPerSecond(36.0f,output)); CHECK(output==10.0f);
    // Original Dino capture: old normalized *90 estimate yields 82.16 m/s,
    // while the game's displayed peak is 124.165 km/h (34.49 m/s).
    CHECK(OutRunHudSpeed::TryMetresPerSecond(124.165169f,output));
    CHECK(std::fabs(output-34.490325f)<0.00001f);
    CHECK(std::fabs(output-0.91286689f*90.0f)>40.0f);
    std::printf("PASS %d HUD speed / encoded FM7 checks; no sockets or device\n",checks);
}
