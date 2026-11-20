/* Consume the already established project interop provider unchanged.
   These helpers run only on the recorded attached Unity owner thread. */
static bool bp_native_provider_gate(void) {
 if(bp.failed) return false;
 if(!get_native_texture) {
  bp.stage="native_texture_provider";bp_fail("owned_native_provider_unavailable");return false;
 }
 return true;
}
static bool bp_component_stop(const char* reason) {
 if(!bp.failed) bp_fail(reason);
 if(GetCurrentThreadId()==bp.owner_thread && bp.api.thread_current && bp.api.thread_current())
  bp_disable_owned_cameras();
 return false;
}
static bool bp_mono_owner(void) {
 if(bp.failed) return false;
 if(!bp.bound || !bp.empty.components_started || GetCurrentThreadId()!=bp.owner_thread || !bp.api.thread_current())
  return bp_component_stop("owned_mono_owner_contract");
 return true;
}
static bool bp_read_false(unsigned method,uint32_t receiver,const char* reason) {
 uint8_t value=1;
 if(!bp_get(method,bp_target(receiver),&value,sizeof(value))) return bp_component_stop(reason);
 if(value) return bp_component_stop(reason);
 return true;
}
static bool bp_apply_xr_optout(BpEye* e) {
 if(!bp_mono_owner()) return false;
 Il2CppObject* additional=bp_target(e->additional);
 if(bp.failed || bp.api.object_get_class(additional)!=bp.types[BT_ADDITIONAL])
  return bp_component_stop("owned_xr_receiver_contract");
 uint8_t no=0;
 if(!bp_set(BM_XR_SET,additional,&no)) return bp_component_stop("owned_xr_optout_invoke");
 return bp_read_false(BM_XR_GET,e->additional,"owned_xr_optout_readback");
}
static bool bp_effective_mono(BpEye* e) {
 if(!bp_mono_owner()) return false;
 Il2CppObject* camera=bp_target(e->camera),*additional=bp_target(e->additional);
 if(bp.failed || bp.api.object_get_class(camera)!=bp.types[BT_CAMERA] ||
  bp.api.object_get_class(additional)!=bp.types[BT_ADDITIONAL])
  return bp_component_stop("owned_mono_receiver_contract");
 return bp_read_false(BM_XR_GET,e->additional,"owned_xr_optout_changed") &&
  bp_read_false(BM_STEREO_ENABLED,e->camera,"owned_stereo_enabled");
}
static bool bp_prepare_own_additional(BpEye* e) {
 if(!bp_mono_owner() || !bp_read_false(BM_ENABLED_GET,e->camera,"own_additional_camera_not_disabled")) return false;
 int32_t base=0,actual=-1;Il2CppObject* additional=bp_target(e->additional);
 if(!bp_set(BM_RENDER_TYPE_SET,additional,&base) ||
  !bp_get(BM_RENDER_TYPE_GET,additional,&actual,sizeof(actual)) || actual!=0 || !bp_stack_empty(additional))
  return bp_component_stop("own_additional_base_stack_contract");
 return bp_apply_xr_optout(e);
}
static bool bp_mono_barrier(void) {
 for(unsigned eye=0;eye<2;++eye) if(!bp_effective_mono(&bp.eye[eye])) return false;
 return true;
}
static bool bp_acquire_owned_native(unsigned eye) {
 if(!bp_mono_owner()) return false;
 if(eye>1 || !bp.backend_verified || !get_native_texture || bp.native[eye] ||
  bp.c.phase!=BP_BUILDING || bp.c.initialized!=3)
  return bp_component_stop("owned_native_prerequisite");
 BpEye* e=&bp.eye[eye]; Il2CppObject* rt=bp_target(e->rt);
 if(bp.failed || bp.api.object_get_class(rt)!=bp.types[BT_RT])
  return bp_component_stop("owned_native_rt_receiver");
 if(!bp_read_false(BM_ENABLED_GET,e->camera,"native_camera_not_disabled")) return false;
 uint8_t created=0;
 if(!bp_get(BM_RT_CREATED,rt,&created,sizeof(created)) || !created ||
  bp_call(BM_TARGET_GET,bp_target(e->camera),NULL)!=rt || !bp_effective_mono(e))
  return bp_component_stop("owned_native_final_rt_contract");
 bp.stage=eye?"owned_native_R":"owned_native_L";
 bp_log("base_native_begin","owner_thread_rooted_final_rt");
 /* No methodPointer/extra MethodInfo/guessed ABI: exactly the existing
    native_texture_icall_fn provider, using only this rooted owned RT. */
 void* borrowed=get_native_texture(rt);
 if(!borrowed) return bp_component_stop("owned_native_provider_null");
 ID3D11Texture2D* owned=NULL;
 HRESULT hr=IUnknown_QueryInterface((IUnknown*)borrowed,&iid_d3d11_texture2d,(void**)&owned);
 if(hr!=S_OK || !owned) return bp_component_stop("owned_native_query_interface");
 bp.native[eye]=owned;
 bp_log("base_native_acquired","owned_rt_reference_retained_until_retire"); return true;
}
static bool bp_enable_mono_pair(void) {
 if(bp.c.phase!=BP_ENABLE_READY || bp.c.initialized!=3 || bp.c.query!=BP_Q_NONE)
  return bp_component_stop("owned_mono_enable_prerequisite");
 if(!bp_mono_barrier()) return false;
 uint8_t yes=1;
 for(unsigned eye=0;eye<2;++eye) {
  uint8_t enabled=0;
  if(!bp_set(BM_ENABLED_SET,bp_target(bp.eye[eye].camera),&yes) ||
   !bp_get(BM_ENABLED_GET,bp_target(bp.eye[eye].camera),&enabled,sizeof(enabled)) || !enabled)
   return bp_component_stop("owned_enable_readback");
  bp.c.enabled|=1u<<eye;
  /* A disabled-only false is never promoted to active mono evidence. */
  if(!bp_effective_mono(&bp.eye[eye])) return false;
  bp_projection_checkpoint(BP_PR_EACH,eye,-1);
 }
 if(!bp_mono_barrier()) return false;
 bp_log("base_mono_verified","enabled_pair_xr_false_stereo_false_state_only");return true;
}
