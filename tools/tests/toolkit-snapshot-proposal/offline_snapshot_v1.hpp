// Proposed fixture-only toolkit contract. Not a production codec or native ABI.
#pragma once
#ifndef DBCE_FORCE_OFFLINE_SNAPSHOT_V1
#error Snapshot proposal requires the explicitly patched fixture model header
#endif
#include "force_profile.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace dbce { namespace force {
using SnapshotConfiguration = std::array<double,31>;
inline SnapshotConfiguration EffectiveValues(const ModelSettings& m,const ShaperSettings& s) {
    return {{m.spring_strength,m.spring_full_speed_mps,m.damper_strength,m.damper_static_fraction,
        m.lateral_weight,m.lateral_force_reference,m.lateral_g_reference,m.trail_floor,m.trail_slip_span,
        m.grip_loss,m.weight_transfer,m.weight_transfer_min,m.weight_transfer_max,m.texture_strength,
        m.texture_hz,m.impact_strength,m.impact_seconds,m.shift_strength,m.shift_seconds,
        double(s.strength),double(s.invert),s.deadzone,s.attack_smoothing,s.decay_smoothing,
        s.soft_saturation,s.slew_per_second,s.output_deadband,s.fade_start_kmh,s.fade_full_kmh,
        s.ramp_seconds,s.peak_limit}};
}
inline bool SnapshotFloat(double value,double low,double high) {
    return std::isfinite(value) && value>=low && value<=high && double(float(value))==value;
}
inline bool SnapshotInteger(double value,double low,double high) {
    return std::isfinite(value) && value>=low && value<=high && std::floor(value)==value;
}
inline bool ValidSnapshotConfiguration(const SnapshotConfiguration& c) {
    // Deliberately bounded fixture eligibility, not a change to production parsing.
    for(double v:c)if(!SnapshotFloat(v,-1000000,1000000))return false;
    for(int i:{0,2,4,13,15,17})if(c[i]<0 || c[i]>100)return false;
    for(int i:{1,5,6,8,14,16,18})if(c[i]<=0)return false;
    for(int i:{3,7,9,21,22,23,24,26,30})if(c[i]<0 || c[i]>1)return false;
    if(c[10]<0 || c[10]>100 || c[11]>c[12])return false;
    if(!SnapshotInteger(c[19],0,100) || !SnapshotInteger(c[20],0,1))return false;
    for(int i:{25,27,28,29})if(c[i]<0)return false;
    return true;
}
class EffectiveConfiguration {
public:
    static EffectiveConfiguration Capture(const Profile& p) {
        auto values=EffectiveValues(p.model,p.shaper);
        if(p.name.empty() || p.version<=0 || !ValidSnapshotConfiguration(values))
            throw std::runtime_error("profile is outside bounded snapshot configuration");
        return EffectiveConfiguration(p.id(),values);
    }
    const std::string& profile_id() const { return id_; }
    const SnapshotConfiguration& values() const { return values_; }
private:
    EffectiveConfiguration(std::string id,SnapshotConfiguration values):id_(std::move(id)),values_(values){}
    const std::string id_;
    const SnapshotConfiguration values_;
};
struct OfflineSnapshot {
    uint32_t version=1;
    // Hash pins LF-normalized unmodified model source, not an authenticated claim.
    std::string model_source="5a0596a0bce15cfb38f35c16540196cd8529b888d49662467a73c8bd01f155db";
    std::string profile_id;
    SnapshotConfiguration configuration{};
    // Model: shift time/emitted, impact time/sign/magnitude, texture phase,
    // last_was_event, last_structural. Shaper: smoothed/last/ramp/started.
    std::array<double,8> model{};
    std::array<double,4> shaper{};
};
struct OfflineSnapshotAccess {
    static bool Valid(const OfflineSnapshot& s,const EffectiveConfiguration& expected) {
        if(s.version!=1 || s.model_source!="5a0596a0bce15cfb38f35c16540196cd8529b888d49662467a73c8bd01f155db" ||
           s.profile_id!=expected.profile_id() || s.configuration!=expected.values() ||
           !ValidSnapshotConfiguration(s.configuration))return false;
        for(double v:s.model)if(!SnapshotFloat(v,-1000000,1000000))return false;
        for(double v:s.shaper)if(!SnapshotFloat(v,-1000000,1000000))return false;
        const auto& m=s.model; const auto& h=s.shaper;
        if(!(m[0]==-1 || SnapshotFloat(m[0],0,s.configuration[18]+0.100001)) || m[1]<0)return false;
        if(!(m[2]==-1 || SnapshotFloat(m[2],0,s.configuration[16]+0.100001)))return false;
        if(!(m[3]==-1 || m[3]==0 || m[3]==1) || m[4]<0 || m[4]>1 || m[5]<0)return false;
        if(!SnapshotInteger(m[6],0,1) || !SnapshotInteger(h[3],0,1))return false;
        if(h[1]<-s.configuration[30] || h[1]>s.configuration[30] ||
           h[2]<0 || h[2]>s.configuration[29]+0.100001)return false;
        if(h[3]==0 && h[2]!=0)return false;
        return true;
    }
    static bool Matches(const Model& m,const Shaper& h,const EffectiveConfiguration& expected) {
        return EffectiveValues(m.settings,h.settings)==expected.values();
    }
    static OfflineSnapshot Capture(const Model& m,const Shaper& h,const EffectiveConfiguration& expected) {
        if(!Matches(m,h,expected))throw std::runtime_error("effective configuration changed");
        OfflineSnapshot s;
        s.profile_id=expected.profile_id(); s.configuration=expected.values();
        s.model={{m.shift_t_,m.shift_emitted_,m.impact_t_,m.impact_sign_,m.impact_mag_,m.texture_phase_,
            double(m.last_was_event),m.last_structural}};
        s.shaper={{h.smoothed_,h.last_,h.ramp_t_,double(h.started_)}};
        if(!Valid(s,expected))throw std::runtime_error("model/shaper state is outside bounded snapshot domain");
        return s;
    }
    static bool Restore(const OfflineSnapshot& s,Model& m,Shaper& h,const EffectiveConfiguration& expected) {
        // Validate every field and both destination configurations before mutation.
        if(!Valid(s,expected) || !Matches(m,h,expected))return false;
        m.shift_t_=float(s.model[0]); m.shift_emitted_=float(s.model[1]);
        m.impact_t_=float(s.model[2]); m.impact_sign_=float(s.model[3]); m.impact_mag_=float(s.model[4]);
        m.texture_phase_=float(s.model[5]); m.last_was_event=s.model[6]!=0; m.last_structural=float(s.model[7]);
        h.smoothed_=float(s.shaper[0]); h.last_=float(s.shaper[1]); h.ramp_t_=float(s.shaper[2]); h.started_=s.shaper[3]!=0;
        return true;
    }
};
inline bool SameSnapshot(const OfflineSnapshot& a,const OfflineSnapshot& b) {
    return a.version==b.version && a.model_source==b.model_source && a.profile_id==b.profile_id &&
        a.configuration==b.configuration && a.model==b.model && a.shaper==b.shaper;
}
}} // namespace dbce::force
