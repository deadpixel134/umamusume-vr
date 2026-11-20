/* Clean full-generation retirement is distinct from failed-empty BP_RETIRED.
   Never reset bindings, process counters/capture state, transport or XR state. */
static bool bp_rearm(void) {
 bool resources=bp.native[0] || bp.native[1] || bp.readback[0] || bp.readback[1] ||
  bp.sentinel_view[0] || bp.sentinel_view[1] || bp.fence || bp.gpu_device;
 if(!bp_clean_rearm_allowed(&bp.c,bp.clean_retired,bp.failed,
  InterlockedCompareExchange(&bp_stop_requested,0,0)!=0,bp.root_count,resources,
  bp_transport_pending(&bp_transport)!=0,bp_transport_blocked(&bp_transport))) return false;
 bp_context_event("rearmed","clean_old_generation_fresh_source_required");
 ZeroMemory(&bp.c,sizeof(bp.c));ZeroMemory(bp.roots,sizeof(bp.roots));
 ZeroMemory(bp.eye,sizeof(bp.eye));ZeroMemory(&bp.empty,sizeof(bp.empty));
 ZeroMemory(&bp.background,sizeof(bp.background));ZeroMemory(&bp.optics,sizeof(bp.optics));
 ZeroMemory(&bp_fov,sizeof(bp_fov));
 bp.source_root=bp.source_transform_root=bp.tag_root=0;bp.source_pose_frame=-1;
 bp.stage=bp.error=NULL;bp.failed=bp.disable_done=bp.destroy_called=bp.witness_saved=false;
 bp.backend_verified=bp.clean_retired=false;bp.source_kind=BP_SOURCE_NONE;
 bp.width=bp.height=0;bp.scale=0;bp.post=false;bp.observed_tick=0;
 bp.witness_retry_after=0;bp.witness_samples=0;
 bp.present_thread=0;bp.last_committed_tick=0;
 return true;
}
