/* Actual owned managed adapter and the bounded known-empty checkpoint.
   Shared directly with the focused exported-API fixture; no Unity layout ABI. */
static void bp_fail(const char* reason) {
 bp_unpublish(); bp.error=reason;
 bp_retire(&bp.c,true);
 if(!bp.empty.components_started && (bp.empty.known || bp.empty.uncertain)) bp.empty.cleanup=true;
 if(!bp.failed) { bp.failed=true; bp_projection_flush(); bp_log("base_experiment_stopped",reason);
#ifdef BP_CONTEXT_EVENTS
  bp_context_event("stopped",reason);
#endif
 }
}
static Il2CppObject* bp_target(uint32_t h) {
 Il2CppObject* o=h?bp.api.gchandle_get_target(h):NULL;
 if(!o) bp_fail("root_target_missing"); return o;
}
static uint32_t bp_root(Il2CppObject* o) {
 if(!o || bp.root_count>=24) { bp_fail("owned_root_capacity_or_null"); return 0; }
 uint32_t h=bp.api.gchandle_new(o,false);
 if(!h || !bp.api.gchandle_get_target(h)) {
  if(h) bp.api.gchandle_free(h); bp_fail("owned_root_failed"); return 0;
 }
 bp.roots[bp.root_count++]=h; return h;
}
/* Every boxed/reference argument is rooted for the entire invocation. Return
   values are consumed/rooted before another managed call. No native ABI guess. */
static Il2CppObject* bp_call(unsigned id,Il2CppObject* receiver,void** values) {
 const BpMethodSpec* s=&bp_specs[id]; bp.stage=s->name;
 Il2CppObject* args[4]={0}; uint32_t handles[4]={0}; Il2CppObject* result=NULL;
 if(bp.failed || !bp.method[id]) return NULL;
 for(unsigned i=0;i<s->argc;++i) {
  int type=s->arg[i]; int code=bp_type_code(type);
  args[i]=(code==18 || code==14)?(Il2CppObject*)values[i]:bp.api.value_box(bp.types[type],values[i]);
  if(args[i]) {
   handles[i]=bp.api.gchandle_new(args[i],false);
   if(!handles[i] || !(args[i]=bp.api.gchandle_get_target(handles[i]))) { bp_fail("argument_root_failed"); goto done; }
  } else { bp_fail("argument_null_or_box_failed"); goto done; }
 }
 {
  Il2CppException* exc=NULL;
  result=s->argc?bp.api.runtime_invoke_convert_args(bp.method[id],receiver,args,(int)s->argc,&exc):
   bp.api.runtime_invoke(bp.method[id],receiver,NULL,&exc);
  if(exc) { bp_projection_tap_exception(id,exc); bp_fail("managed_exception_or_aot_unavailable"); result=NULL; }
 }
done:
 for(unsigned i=0;i<s->argc;++i) if(handles[i]) bp.api.gchandle_free(handles[i]);
 return result;
}
static bool bp_get(unsigned id,Il2CppObject* receiver,void* out,size_t size) {
 Il2CppObject* o=bp_call(id,receiver,NULL); int type=bp_specs[id].ret;
 if(bp.failed) return false;
 if(!o || bp.api.object_get_class(o)!=bp.types[type] || !bp.api.object_unbox(o)) {
  bp_projection_tap_box(id);
  bp_fail("boxed_result_contract"); return false;
 }
 memcpy(out,bp.api.object_unbox(o),size); bp_projection_tap_value(id,out,size); return true;
}
static bool bp_set(unsigned id,Il2CppObject* receiver,void* value) {
 void* args[]={value}; (void)bp_call(id,receiver,args); return !bp.failed;
}
static bool bp_alive(Il2CppObject* o,bool* alive) {
 void* args[]={o}; Il2CppObject* boxed=bp_call(BM_ALIVE,NULL,args);
 if(bp.failed) return false;
 if(!boxed || bp.api.object_get_class(boxed)!=bp.types[BT_BOOL] || !bp.api.object_unbox(boxed)) {
  bp_fail("native_liveness_result"); return false;
 }
 *alive=*(uint8_t*)bp.api.object_unbox(boxed)!=0; return true;
}
static uint32_t bp_add(uint32_t go,int type) {
 if(!bp.empty.components_started) { bp_fail("components_before_empty_checkpoint"); return 0; }
 Il2CppReflectionType* t=bp.api.type_get_object(bp.api.class_get_type(bp.types[type]));
 void* args[]={t}; Il2CppObject* o=bp_call(BM_ADD,bp_target(go),args);
 if(bp.failed) return 0;
 if(!o || bp.api.object_get_class(o)!=bp.types[type]) { bp_fail("owned_component_class"); return 0; }
 return bp_root(o);
}

/* Same selector/first-failure contract. The optional snapshot is observational;
   later diagnostics never commit to bp.method or change bp.stage/bp.error. */
static bool bp_bind_owned_method_evidence(unsigned id,BpLookupEvidence* observed) {
 bp.stage=bp_specs[id].name;
 BpLookupEvidence tag_evidence;
 BpLookupEvidence* evidence=observed?observed:bp_requires_readable_names(id)?&tag_evidence:NULL;
 bp.method[id]=bp_find(&bp.api,bp.types,&bp_specs[id],evidence);
 if(bp_requires_readable_names(id) && evidence->invalid_names) {
  bp.method[id]=NULL;
  bp_fail(id==BM_COMPARE_TAG?"owned_tag_metadata_names_incomplete":"owned_mono_metadata_names_incomplete"); return false;
 }
 if(!bp.method[id]) { bp_fail("owned_exact_method_missing_or_ambiguous"); return false; }
 return true;
}
static inline bool bp_bind_owned_method(unsigned id) { return bp_bind_owned_method_evidence(id,NULL); }
static BpPreflight bp_binding_preflight;
static inline bool bp_bind_required(BpPreflight* observation) {
 /* One observation attempt per process, including pre-method type failure.
    This guard changes only evidence work, not the product binding result. */
 BpPreflight* p=observation && !observation->used?observation:NULL;
 if(p) { memset(p,0,sizeof(*p)); p->used=true; p->first_failure=BM_COUNT; }
 for(unsigned id=0;id<BM_COUNT;++id) {
  BpLookupEvidence e;
  bool ok=bp_bind_owned_method_evidence(id,p?&e:NULL);
  if(p) bp_preflight_note(p,&bp.api,bp.types,id,&e,ok);
  if(!ok) {
   if(p) {
    p->first_failure=id;
    bp_preflight_remaining(p,&bp.api,bp.types,id+1);
    bp_emit_preflight(p);
   }
   return false;
  }
 }
 if(p) { p->complete=true; bp_emit_preflight(p); }
 return true;
}
static inline void bp_observe_partial_binding(BpPreflight* p) {
 if(p->used) return;
 memset(p,0,sizeof(*p)); p->used=true; p->first_failure=BM_COUNT; p->blocked_before_methods=true;
 bp_preflight_remaining(p,&bp.api,bp.types,0); bp_emit_preflight(p);
}
static bool bp_compare_untagged(uint32_t go) {
 if(bp.failed) return false;
 if(GetCurrentThreadId()!=bp.owner_thread || !bp.api.thread_current()) {
  bp_fail("owned_tag_owner_thread"); return false;
 }
 Il2CppObject* receiver=bp_target(go),*tag=bp_target(bp.tag_root);
 if(bp.failed) return false;
 if(bp.api.object_get_class(receiver)!=bp.types[BT_GO] || bp.api.object_get_class(tag)!=bp.types[BT_STRING]) {
  bp_fail("owned_tag_receiver_or_argument"); return false;
 }
 Il2CppObject* boxed=bp_call(BM_COMPARE_TAG,receiver,(void*[]){tag});
 if(bp.failed) return false;
 void* value=boxed && bp.api.object_get_class(boxed)==bp.types[BT_BOOL]?bp.api.object_unbox(boxed):NULL;
 if(!value) { bp_fail("owned_tag_boxed_boolean_contract"); return false; }
 if(!*(uint8_t*)value) { bp_fail("owned_tag_not_untagged"); return false; }
 return true;
}
static bool bp_prepare_empty_pair(void) {
 if(bp.failed) return false;
 /* bound is set only after *all* method/field/export gates in bp_bind.
    The explicit destruction prerequisites also guard this shared entry. */
 if(!bp.bound || !bp.method[BM_COMPARE_TAG] || !bp.method[BM_DESTROY] || !bp.method[BM_ALIVE] ||
  GetCurrentThreadId()!=bp.owner_thread || !bp.api.thread_current() ||
  bp.empty.known || bp.empty.uncertain || bp.empty.components_started) {
  bp_fail("empty_checkpoint_prerequisite"); return false;
 }
 Il2CppString* tag=bp.string_new("Untagged");
 if(!tag) { bp_fail("owned_tag_string_allocation"); return false; }
 bp.tag_root=bp_root((Il2CppObject*)tag); if(bp.failed) return false;
 for(unsigned eye=0;eye<2;++eye) {
  unsigned bit=1u<<eye;
  /* Until normal ctor and inactive readback both return, native state is
     uncertain. Never infer the default tag or initial activation state. */
  bp.empty.uncertain|=bit;
  bp.eye[eye].go=bp_root(bp.object_new(bp.types[BT_GO])); if(bp.failed) return false;
  Il2CppObject* receiver=bp_target(bp.eye[eye].go);
  (void)bp_call(BM_GO_CTOR,receiver,NULL); if(bp.failed) return false;
  uint8_t no=0,active=1;
  if(!bp_set(BM_ACTIVE_SET,receiver,&no) || !bp_get(BM_ACTIVE_GET,receiver,&active,sizeof(active))) return false;
  if(active) { bp_fail("own_go_not_inactive"); return false; }
  bp.empty.known|=bit; bp.empty.uncertain&=~bit;
  Il2CppString* name=bp.string_new(eye?"UmaVR_OwnBase_R":"UmaVR_OwnBase_L");
  if(!name) { bp_fail("owned_name_string_allocation"); return false; }
  if(!bp_set(BM_NAME,receiver,name) || !bp_compare_untagged(bp.eye[eye].go)) return false;
  bp.empty.tagged|=bit;
 }
 bp_log("base_empty_checkpoint","both_inactive_untagged_no_components"); return true;
}
static bool bp_begin_components(void) {
 if(bp.failed) return false;
 if(bp.empty.known!=3 || bp.empty.tagged!=3 || bp.empty.uncertain || bp.empty.cleanup ||
  bp.empty.components_started) { bp_fail("components_before_empty_checkpoint"); return false; }
 /* Mark before the first AddComponent attempt: its failure/exception can
    have entered native lifecycle and may never use empty cleanup. */
 bp.empty.components_started=true; return true;
}
static bool bp_tag_enable_barrier(void) {
 if(!bp.empty.components_started) { bp_fail("tag_enable_before_components"); return false; }
 /* Both checks precede either enabled=true write. Not a per-frame monitor. */
 for(unsigned eye=0;eye<2;++eye) if(!bp_compare_untagged(bp.eye[eye].go)) return false;
 return true;
}
static void bp_cleanup_empty(void) {
 if(!bp.empty.cleanup || bp.empty.finished || bp.empty.components_started) return;
 bool previous_failure=bp.failed; bp.failed=false;
 for(unsigned eye=0;eye<2;++eye) {
  unsigned bit=1u<<eye;
  if(!(bp.empty.known&bit) || (bp.empty.gone&bit) || (bp.empty.uncertain&bit)) continue;
  Il2CppObject* o=bp_target(bp.eye[eye].go); bool alive=true;
  if(!bp.failed && bp_alive(o,&alive)) {
   if(!alive) bp.empty.gone|=bit;
   else if(!(bp.empty.destroy_requested&bit)) {
    /* Set before invocation: an exception may have scheduled destruction. */
    bp.empty.destroy_requested|=bit;
    (void)bp_call(BM_DESTROY,NULL,(void*[]){o});
   }
  }
  if(bp.failed) { bp.empty.uncertain|=bit; bp.failed=false; }
 }
 bp.failed|=previous_failure;
 unsigned pending=bp.empty.known & ~bp.empty.gone & ~bp.empty.uncertain;
 if(!pending) {
  bp.empty.finished=true;
  if(!bp.empty.uncertain) {
   for(unsigned i=0;i<bp.root_count;++i) bp.api.gchandle_free(bp.roots[i]);
   bp.root_count=0; bp.c.phase=BP_RETIRED;
   bp_log("base_empty_retired","native_destroyed_roots_released_no_retry");
  } else bp_log("base_empty_quarantined","uncertain_native_state_roots_retained_no_retry");
 } else if(++bp.empty.checks>=BP_DESTROY_CHECK_LIMIT) {
  bp.empty.finished=true; bp_log("base_empty_quarantined","native_confirmation_limit_roots_retained_no_retry");
 }
}

static void bp_disable_owned_cameras(void) {
  if(!bp.disable_done) {
   bool previous_failure=bp.failed; bp.failed=false;
   uint8_t no=0; bool ok=true;
   for(unsigned eye=0;eye<2;++eye) if(bp.eye[eye].camera) {
    Il2CppObject* camera=bp_target(bp.eye[eye].camera); bool alive=false;
    if(!camera || !bp_alive(camera,&alive) || (alive && !bp_set(BM_ENABLED_SET,camera,&no))) ok=false;
   }
   bp.failed|=previous_failure; bp.disable_done=true; bp.c.disabled=ok; bp.c.enabled=0;
   if(!ok) bp_fail("owned_disable_failed");
   bp_log("base_retire_disabled",bp.c.quarantined?"partial_quarantined":"await_present_drain");
  }
}

