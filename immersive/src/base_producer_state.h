/* Normal-camera producer policy, shared by the real managed/Present adapters.
   A frame interval is eligibility, never proof of a render phase or RT write. */
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
/* Same inverse world-scale contract as compose_unity_eye_pose. This is the
   production local eye offset, before rotation into the frozen base pose. */
static inline bool bp_fixed_eye_x(float half_ipd,float scale,float minimum_scale,
 unsigned eye,float* x) {
 if(!x || eye>1 || !isfinite(half_ipd) || half_ipd<=0 || !isfinite(scale) ||
  !isfinite(minimum_scale) || minimum_scale<=0 || scale<minimum_scale) return false;
 float local=(eye?1.0f:-1.0f)*(half_ipd/scale);
 if(!isfinite(local)) return false;
 *x=local; return true;
}
typedef enum BpPhase { BP_NEW, BP_BUILDING, BP_PREPARE, BP_ENABLE_READY,
 BP_EXCLUDE, BP_ACTIVE, BP_RETIRING, BP_DESTROY_READY, BP_DESTROY_PENDING,
 BP_RETIRED, BP_STOPPED } BpPhase;
typedef enum BpQuery { BP_Q_NONE, BP_Q_PREPARE, BP_Q_PAIR, BP_Q_DRAIN } BpQuery;

/* No component/native RT/GPU may exist before the two-eye empty checkpoint.
   Uncertain construction/activation is never eligible for empty Destroy. */
#define BP_DESTROY_CHECK_LIMIT 60u
typedef struct BpEmpty {
 unsigned known, uncertain, tagged, destroy_requested, gone, checks;
 bool components_started, cleanup, finished;
} BpEmpty;
typedef struct BpIdentity {
 uint64_t generation, source, pose, config, capture_tick;
 int32_t scene, frame;
} BpIdentity;
typedef struct BpControl {
 BpPhase phase; BpQuery query;
 BpIdentity identity, pending;
 int32_t enable_frame, observed_frame;
 unsigned initialized, enabled, witness, mutex_owned, destroy_checks;
 bool attempted, retiring, quarantined, disabled, query_failed;
} BpControl;
static inline bool bp_identity_equal(const BpIdentity* a,const BpIdentity* b) {
 return a->generation==b->generation && a->source==b->source && a->scene==b->scene &&
  a->pose==b->pose && a->config==b->config;
}
static inline bool bp_begin(BpControl* c,BpIdentity identity) {
 if(c->attempted || c->phase!=BP_NEW || !identity.generation || !identity.source ||
  !identity.pose || !identity.config) return false;
 c->attempted=true; c->identity=identity; c->phase=BP_BUILDING; return true;
}
static inline void bp_retire(BpControl* c,bool quarantine) {
 bool was_retiring=c->retiring;
 c->retiring=true; c->quarantined|=quarantine;
 if(c->quarantined) c->phase=BP_STOPPED;
 else if(!was_retiring) c->phase=BP_RETIRING;
}
static inline bool bp_built(BpControl* c) {
 if(c->phase!=BP_BUILDING || c->initialized!=3 || c->enabled || c->retiring) return false;
 c->phase=BP_PREPARE; return true;
}
static inline bool bp_arm(BpControl* c,BpQuery q,int32_t frame,uint64_t tick) {
 if(c->query!=BP_Q_NONE || c->query_failed || q==BP_Q_NONE) return false;
 if(q==BP_Q_PREPARE && (c->phase!=BP_PREPARE || c->retiring)) return false;
 if(q==BP_Q_PAIR && (c->phase!=BP_ACTIVE || c->retiring || c->enabled!=3 ||
  frame<=c->enable_frame || (int64_t)frame-(int64_t)c->enable_frame<=1)) return false;
 if(q==BP_Q_DRAIN && (!c->retiring || !c->disabled || c->quarantined)) return false;
 c->query=q; c->pending=c->identity; c->pending.frame=frame; c->pending.capture_tick=tick;
 return true;
}
/* 0 pending; 1 complete; -1 failure. The adapter still owns all resources. */
static inline BpQuery bp_complete(BpControl* c,int result) {
 if(c->query==BP_Q_NONE || result==0) return BP_Q_NONE;
 if(result<0) { c->query_failed=true; bp_retire(c,true); return BP_Q_NONE; }
 BpQuery q=c->query; c->query=BP_Q_NONE;
 if(q==BP_Q_PREPARE && !c->retiring) c->phase=BP_ENABLE_READY;
 if(q==BP_Q_DRAIN && c->disabled && !c->quarantined) c->phase=BP_DESTROY_READY;
 return q;
}
static inline bool bp_enabled(BpControl* c,int32_t frame) {
 if(c->phase!=BP_ENABLE_READY || c->retiring || c->query || c->initialized!=3 || frame<0) return false;
 c->enabled=3; c->enable_frame=frame; c->phase=BP_EXCLUDE; return true;
}
static inline bool bp_observe(BpControl* c,const BpIdentity* identity,int32_t frame) {
 if(!bp_identity_equal(&c->identity,identity)) { bp_retire(c,false); return false; }
 if(c->retiring || frame<0) return false;
 c->observed_frame=frame;
 if(c->phase==BP_EXCLUDE && (int64_t)frame-(int64_t)c->enable_frame>1) c->phase=BP_ACTIVE;
 return c->phase==BP_ACTIVE;
}
static inline bool bp_pair_eligible(const BpControl* c,const BpIdentity* frozen) {
 return c->phase==BP_ACTIVE && !c->retiring && !c->query_failed && c->query==BP_Q_NONE &&
  c->witness==3 && c->enabled==3 && bp_identity_equal(&c->identity,frozen) &&
  frozen->capture_tick && (int64_t)frozen->frame-(int64_t)c->enable_frame>1;
}
static inline bool bp_release_result(BpControl* c,unsigned eye,bool success) {
 if(eye>1 || !(c->mutex_owned&(1u<<eye))) return false;
 if(!success) { bp_retire(c,true); return false; }
 c->mutex_owned&=~(1u<<eye); return true;
}
static inline bool bp_publication_ready(const BpControl* c,const BpIdentity* frozen) {
 return !c->mutex_owned && bp_pair_eligible(c,frozen);
}
static inline bool bp_consumer_current(const BpControl* c,const BpIdentity* cached) {
 return c->phase==BP_ACTIVE && !c->retiring && c->enabled==3 && c->witness==3 &&
  bp_identity_equal(&c->identity,cached);
}
static inline bool bp_destroy_observed(BpControl* c,bool any_native_alive) {
 if(c->phase!=BP_DESTROY_PENDING || c->query!=BP_Q_NONE || c->quarantined || !c->disabled) return false;
 if(!any_native_alive) { c->phase=BP_RETIRED; return true; }
 if(++c->destroy_checks>=BP_DESTROY_CHECK_LIMIT) bp_retire(c,true);
 return false;
}
static inline bool bp_clean_rearm_allowed(const BpControl* c,bool clean,bool failed,bool stopped,
 unsigned roots,bool resources,bool transport_debt,bool transport_broken) {
 return clean && c->phase==BP_RETIRED && c->attempted && c->retiring && c->disabled &&
  !c->quarantined && !c->query_failed && c->query==BP_Q_NONE && !c->mutex_owned &&
  !failed && !stopped && !roots && !resources && !transport_debt && !transport_broken;
}
static inline bool bp_descriptor_ok(unsigned w,unsigned h,unsigned expected_w,unsigned expected_h,
 unsigned mip,unsigned array,unsigned samples,bool format,bool same_device,bool distinct) {
 return w==expected_w && h==expected_h && w && h && w<=8192 && h<=8192 &&
  mip==1 && array==1 && samples==1 && format && same_device && distinct;
}
static inline bool bp_write_witness(uint64_t pixels,uint64_t changed,unsigned channels_range) {
 return pixels && changed>pixels/100 && channels_range>8;
}
