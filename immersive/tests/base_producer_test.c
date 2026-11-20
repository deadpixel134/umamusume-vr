#include <stdio.h>
#include <string.h>
#include "../src/base_producer_state.h"
static unsigned assertions,cases;
#define CHECK(x) do { ++assertions; if(!(x)) { fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x); return 1; } } while(0)
#define CASE(name) do { ++cases; printf("PASS %s\n",name); } while(0)
static BpIdentity identity(void) { return (BpIdentity){9,17,23,41,0,5,0}; }
static BpControl active(void) {
 BpControl c={0}; (void)bp_begin(&c,identity()); c.initialized=3;
 (void)bp_built(&c); (void)bp_arm(&c,BP_Q_PREPARE,0,50); (void)bp_complete(&c,1);
 (void)bp_enabled(&c,100); (void)bp_observe(&c,&c.identity,102); return c;
}
int main(void) {
 const float scales[]={1.0f,2.0f,4.0f};
 const float expected_half[]={0.032f,0.016f,0.008f};
 const float expected_baseline[]={0.064f,0.032f,0.016f};
 for(unsigned i=0;i<3;++i) {
  float left=0,right=0;
  CHECK(bp_fixed_eye_x(0.032f,scales[i],0.55f,0,&left));
  CHECK(bp_fixed_eye_x(0.032f,scales[i],0.55f,1,&right));
  CHECK(fabsf(left+expected_half[i])<0.0000001f);
  CHECK(fabsf(right-expected_half[i])<0.0000001f);
  CHECK(fabsf((right-left)-expected_baseline[i])<0.0000001f);
  CHECK(fabsf(left+right)<0.0000001f);
  CASE(i==0?"production_eye_baseline_worldScale1":i==1?"production_eye_baseline_worldScale2":"production_eye_baseline_worldScale4");
 }
 float x=123.0f;
 CHECK(bp_fixed_eye_x(0.032f,0.55f,0.55f,1,&x)); CHECK(fabsf(x-0.058181818f)<0.0000001f);
 CASE("production_eye_offset_existing_minimum_scale");
 const float invalid_scales[]={0.54f,0.0f,-1.0f,NAN,INFINITY,-INFINITY};
 for(unsigned i=0;i<6;++i) {
  x=123.0f; CHECK(!bp_fixed_eye_x(0.032f,invalid_scales[i],0.55f,0,&x)); CHECK(x==123.0f);
  CASE("production_eye_offset_rejects_invalid_worldScale");
 }
 CHECK(!bp_fixed_eye_x(0.032f,1.0f,0.55f,2,&x));
 CHECK(!bp_fixed_eye_x(0.032f,1.0f,0.55f,0,NULL));
 CHECK(!bp_fixed_eye_x(0.0f,1.0f,0.55f,0,&x));
 CHECK(!bp_fixed_eye_x(NAN,1.0f,0.55f,0,&x));
 CHECK(!bp_fixed_eye_x(INFINITY,1.0f,0.55f,0,&x));
 CASE("production_eye_offset_rejects_bad_eye_output_IPD");
 BpControl c={0}; BpIdentity id=identity();
 id.generation=0; CHECK(!bp_begin(&c,id)); CHECK(!c.attempted); CASE("zero_generation_no_allocation_authority");
 id=identity(); id.source=0; CHECK(!bp_begin(&c,id)); CASE("missing_source_no_allocation");
 id=identity(); CHECK(bp_begin(&c,id)); CHECK(!bp_begin(&c,id)); CASE("one_attempt_per_process");
 c.initialized=1; CHECK(!bp_built(&c)); CHECK(!bp_enabled(&c,100)); bp_retire(&c,true);
 CHECK(c.quarantined && c.phase==BP_STOPPED); CHECK(!bp_arm(&c,BP_Q_DRAIN,0,1));
 CHECK(!bp_begin(&c,id)); CASE("one_eye_init_failure_quarantine_no_destroy_retry");
 memset(&c,0,sizeof(c)); CHECK(bp_begin(&c,id)); c.initialized=3; c.enabled=1;
 CHECK(!bp_built(&c)); CASE("unexpected_init_enable_cannot_be_ready");
 c.enabled=0; CHECK(bp_built(&c)); CHECK(!bp_enabled(&c,100));
 CHECK(bp_arm(&c,BP_Q_PREPARE,0,55)); CHECK(bp_complete(&c,0)==BP_Q_NONE);
 CHECK(c.phase==BP_PREPARE && c.query==BP_Q_PREPARE); CHECK(!bp_enabled(&c,100));
 CASE("pre_enable_descriptor_sentinel_query_pending");
 CHECK(bp_complete(&c,1)==BP_Q_PREPARE); CHECK(c.phase==BP_ENABLE_READY);
 CHECK(!bp_enabled(&c,-1)); CHECK(bp_enabled(&c,100)); CASE("preparation_complete_before_enable");
 CHECK(!bp_observe(&c,&id,100)); CHECK(!bp_arm(&c,BP_Q_PAIR,100,60));
 CHECK(!bp_observe(&c,&id,101)); CHECK(!bp_arm(&c,BP_Q_PAIR,101,60));
 CHECK(bp_observe(&c,&id,102)); CHECK(bp_arm(&c,BP_Q_PAIR,102,60));
 CASE("configure_and_following_frame_excluded_not_render_proof");
 CHECK(!bp_arm(&c,BP_Q_PAIR,103,70)); CHECK(c.pending.frame==102 && c.pending.capture_tick==60);
 CHECK(bp_complete(&c,0)==BP_Q_NONE && c.query==BP_Q_PAIR);
 CHECK(!bp_pair_eligible(&c,&c.pending)); CASE("pending_slot_and_capture_metadata_frozen");
 CHECK(bp_complete(&c,1)==BP_Q_PAIR); CHECK(!bp_pair_eligible(&c,&c.pending));
 c.witness=1; CHECK(!bp_pair_eligible(&c,&c.pending)); c.witness=3;
 CHECK(bp_pair_eligible(&c,&c.pending)); CHECK(c.pending.capture_tick==60);
 CASE("both_eye_write_witness_and_fence_separate_from_tick");
 CHECK(!bp_write_witness(100,0,255)); CHECK(!bp_write_witness(100,100,0));
 CHECK(!bp_write_witness(10000,1,255)); CHECK(bp_write_witness(100,90,20));
 CASE("sentinel_uniform_and_single_pixel_not_scene_acceptance");
 c.mutex_owned=3; CHECK(!bp_publication_ready(&c,&c.pending));
 CHECK(bp_release_result(&c,0,true)); CHECK(!bp_publication_ready(&c,&c.pending));
 CHECK(bp_release_result(&c,1,true)); CHECK(bp_publication_ready(&c,&c.pending));
 CASE("both_releases_before_atomic_commit");
 for(unsigned failed_eye=0;failed_eye<2;++failed_eye) {
  c=active(); c.witness=3; CHECK(bp_arm(&c,BP_Q_PAIR,102,80)); CHECK(bp_complete(&c,1)==BP_Q_PAIR);
  c.mutex_owned=3;
  CHECK(!bp_release_result(&c,failed_eye,false)); CHECK(bp_release_result(&c,1-failed_eye,true));
  CHECK(c.mutex_owned==(1u<<failed_eye)); CHECK(c.quarantined);
  CHECK(!bp_publication_ready(&c,&c.pending)); CHECK(!bp_consumer_current(&c,&id));
  CASE(failed_eye?"right_release_failure_retains_right_rejects_consumer":"left_release_failure_retains_left_rejects_consumer");
 }
 c=active(); c.mutex_owned=3; CHECK(!bp_release_result(&c,0,false)); CHECK(!bp_release_result(&c,1,false));
 CHECK(c.mutex_owned==3 && c.quarantined); CASE("both_release_failures_never_free_unknown_ownership");
 c=active(); CHECK(bp_arm(&c,BP_Q_PAIR,102,90)); c.mutex_owned=3;
 BpIdentity changed=id; ++changed.scene;
 CHECK(!bp_observe(&c,&changed,103)); CHECK(c.retiring && c.query==BP_Q_PAIR);
 CHECK(bp_complete(&c,0)==BP_Q_NONE); CHECK(!bp_pair_eligible(&c,&c.pending));
 CHECK(bp_complete(&c,1)==BP_Q_PAIR); CHECK(!bp_publication_ready(&c,&c.pending));
 CHECK(bp_release_result(&c,0,true) && bp_release_result(&c,1,true));
 CASE("scene_change_drains_pending_without_publication");
 for(unsigned change=0;change<4;++change) {
  c=active(); changed=id;
  if(change==0) ++changed.generation; if(change==1) ++changed.source;
  if(change==2) ++changed.pose; if(change==3) ++changed.config;
  CHECK(!bp_consumer_current(&c,&changed)); CHECK(!bp_observe(&c,&changed,103)); CHECK(c.retiring);
  CASE("generation_source_pose_config_consumer_rejection");
 }
 c=active(); CHECK(bp_arm(&c,BP_Q_PAIR,102,100));
 CHECK(bp_complete(&c,-1)==BP_Q_NONE); CHECK(c.query_failed && c.query==BP_Q_PAIR && c.quarantined);
 CHECK(!bp_arm(&c,BP_Q_DRAIN,0,101)); CHECK(!bp_destroy_observed(&c,false));
 CASE("GPU_query_failure_retains_slot_and_resources");
 c=active(); CHECK(bp_arm(&c,BP_Q_PAIR,102,110)); bp_retire(&c,false);
 CHECK(!bp_arm(&c,BP_Q_DRAIN,0,111)); CHECK(bp_complete(&c,1)==BP_Q_PAIR);
 CHECK(!bp_arm(&c,BP_Q_DRAIN,0,112)); c.disabled=true; c.enabled=0;
 CHECK(bp_arm(&c,BP_Q_DRAIN,0,113)); CHECK(bp_complete(&c,0)==BP_Q_NONE);
 CHECK(c.phase==BP_RETIRING); CHECK(!bp_destroy_observed(&c,false));
 CHECK(bp_complete(&c,1)==BP_Q_DRAIN && c.phase==BP_DESTROY_READY);
 CASE("disable_and_post_disable_drain_before_destroy");
 bp_retire(&c,false); CHECK(c.phase==BP_DESTROY_READY);
 c.phase=BP_DESTROY_PENDING; bp_retire(&c,false); CHECK(c.phase==BP_DESTROY_PENDING);
 CASE("repeated_scene_exit_does_not_reset_retire_progress");
 c.phase=BP_DESTROY_PENDING; CHECK(bp_destroy_observed(&c,false)); CHECK(c.phase==BP_RETIRED);
 CHECK(!bp_begin(&c,id)); CASE("native_destroyed_before_root_release_no_auto_reentry");
 c=active(); bp_retire(&c,false); c.disabled=true; c.enabled=0; c.phase=BP_DESTROY_PENDING;
 for(unsigned n=0;n<59;++n) CHECK(!bp_destroy_observed(&c,true) && !c.quarantined);
 CHECK(!bp_destroy_observed(&c,true)); CHECK(c.quarantined && c.phase==BP_STOPPED);
 CASE("native_destroy_confirmation_bounded_60_no_infinite_poll");
 c=active(); c.witness=3; CHECK(bp_consumer_current(&c,&id)); changed=id; ++changed.scene;
 CHECK(!bp_consumer_current(&c,&changed)); c.enabled=0; CHECK(!bp_consumer_current(&c,&id));
 CASE("consumer_matches_actual_scene_and_enabled_witness");
 CHECK(bp_descriptor_ok(640,720,640,720,1,1,1,true,true,true));
 CHECK(!bp_descriptor_ok(640,720,640,720,1,1,1,true,false,true)); CASE("native_device_mismatch_before_enable");
 CHECK(!bp_descriptor_ok(640,720,640,720,1,1,1,true,true,false)); CASE("left_right_RT_alias_rejected");
 CHECK(!bp_descriptor_ok(640,720,640,720,1,1,4,true,true,true)); CASE("MSAA_not_one_rejected");
 CHECK(!bp_descriptor_ok(640,720,640,720,2,1,1,true,true,true));
 CHECK(!bp_descriptor_ok(640,720,640,720,1,2,1,true,true,true)); CASE("mip_array_subresource_mismatch_rejected");
 CHECK(!bp_descriptor_ok(641,720,640,720,1,1,1,true,true,true));
 CHECK(!bp_descriptor_ok(640,720,640,720,1,1,1,false,true,true)); CASE("extent_format_contract_before_enable");
 c=active(); c.witness=3; CHECK(bp_arm(&c,BP_Q_PAIR,102,777)); CHECK(bp_complete(&c,1)==BP_Q_PAIR);
 BpIdentity frozen=c.pending; ++frozen.config; CHECK(!bp_pair_eligible(&c,&frozen));
 CHECK(c.pending.capture_tick==777); CASE("completed_copy_cannot_refresh_or_relabel_capture");
 c=active(); bp_retire(&c,true); c.disabled=true; CHECK(!bp_arm(&c,BP_Q_DRAIN,0,0));
 CHECK(!bp_pair_eligible(&c,&c.identity)); CHECK(!bp_destroy_observed(&c,false)); CASE("partial_stop_never_claims_safe_recovery");
 printf("RESULT cases=%u assertions=%u real_GPU=NOT_RUN managed_target=UNVERIFIED stereo_fidelity=UNACCEPTED\n",cases,assertions);
 return 0;
}
