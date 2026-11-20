/* Optional, read-only CPU camera-contract evidence. No product predicates,
   setters, roots, render calls, matrix interpretation, or lifetime changes.
   Included after the existing projection observer. All engine reads occur in
   the existing attached owner-thread checkpoint; flush only consumes cache. */
#define BP_CC_RECORD_LIMIT 5u
#define BP_CC_BYTE_LIMIT 6144u
#define BP_CC_FOOTER_RESERVE 1024u
enum { BP_CC_P, BP_CC_V, BP_CC_C, BP_CC_MATRICES };
enum { BP_CC_MISSING=6, BP_CC_IDENTITY=7 };
typedef struct BpCcBinding {
 const MethodInfo* method;
 unsigned scan,named,valid,invalid;
 bool complete,capped,selected;
 const char* outcome;
} BpCcBinding;
typedef struct BpCcSample {
 Matrix4x4 matrix[BP_CC_MATRICES];
 unsigned status[BP_CC_MATRICES],order[BP_CC_MATRICES],seen;
 int32_t projection_frame; unsigned projection_order;
 char exception_ns[32],exception_name[48]; bool exception_truncated;
} BpCcSample;
static struct {
 BpCcBinding binding[2]; BpCcSample sample[3]; BpIdentity identity,snapshot_identity;
 bool bind_attempted,ready,attempted,flushed,snapshot_valid,identity_valid;
 float near_clip,far_clip; int32_t mask,caller_frame,before_frame,after_frame;
 unsigned phase,initialized,enabled,order,records,bytes;
 unsigned selected_mask,submitted,budget_dropped,format_dropped,writer_dropped;
} bp_cc;
_Static_assert(sizeof(bp_cc)<4096,"Fixed camera-contract cache");

static void bp_cc_bind(void) {
 if(bp_cc.bind_attempted) return;
 bp_cc.bind_attempted=true;
 const BpMethodSpec specs[2]={
  {BT_CAMERA,"get_worldToCameraMatrix",BT_M4,0,{0},false},
  {BT_CAMERA,"get_cullingMatrix",BT_M4,0,{0},false}
 };
 for(unsigned i=0;i<2;++i) {
  BpCcBinding* b=&bp_cc.binding[i];b->outcome="not_available";
  if(!bp_metadata_ready(&bp.api) || !bp.types[BT_CAMERA] || !bp.types[BT_M4]) continue;
  BpLookupEvidence e;
  const MethodInfo* method=bp_find(&bp.api,bp.types,&specs[i],&e);
  b->scan=e.scan_count;b->named=e.named_count;b->valid=e.valid_matches;b->invalid=e.invalid_names;
  b->complete=e.scan_completed;b->capped=e.capped;
  b->selected=method && e.scan_completed && !e.capped && !e.invalid_names && e.valid_matches==1;
  b->method=b->selected?method:NULL;
  b->outcome=e.invalid_names?"name_unreadable":bp_lookup_outcome(&e);
 }
 bp_cc.ready=bp_cc.binding[0].selected && bp_cc.binding[1].selected;
}
static void bp_cc_snapshot(float near_clip,float far_clip,int32_t mask) {
 if(bp_cc.snapshot_valid) return;
 bp_cc.near_clip=near_clip;bp_cc.far_clip=far_clip;bp_cc.mask=mask;
 bp_cc.snapshot_identity=bp.c.identity;bp_cc.snapshot_valid=true;
}
static bool bp_cc_owner(void) {
 return bp.bound && !bp.failed && !bp.c.retiring && !bp.c.quarantined &&
  bp.c.phase==BP_EXCLUDE && bp.c.initialized==3 && bp.c.enabled==3 &&
  GetCurrentThreadId()==bp.owner_thread && InterlockedCompareExchange(&bp_main_depth,0,0)==1 &&
  bp.api.thread_current && bp.api.thread_current();
}
/* Reacquire existing handles before every invocation. No bp_target/bp_get:
   missing evidence cannot unpublish, fail or retire the real producer. */
static Il2CppObject* bp_cc_receiver(unsigned role,Il2CppObject* selected_source) {
 if(role>=3 || !bp.api.gchandle_get_target || !bp.api.object_get_class) return NULL;
 uint32_t h[3]={bp.source_root,bp.eye[0].camera,bp.eye[1].camera};
 Il2CppObject* o[3];
 for(unsigned i=0;i<3;++i) {
  o[i]=h[i]?bp.api.gchandle_get_target(h[i]):NULL;
  if(!o[i] || bp.api.object_get_class(o[i])!=bp.types[BT_CAMERA]) return NULL;
 }
 if(o[0]!=selected_source || o[0]==o[1] || o[0]==o[2] || o[1]==o[2]) return NULL;
 return o[role];
}
static void bp_cc_exception(BpCcSample* s,Il2CppException* exc) {
 if(s->exception_name[0] || s->exception_ns[0]) return;
 BpProjectionSample p={0};bp_projection_exception(&p,exc);
 memcpy(s->exception_ns,p.exception_ns,sizeof(s->exception_ns));
 memcpy(s->exception_name,p.exception_name,sizeof(s->exception_name));
 s->exception_truncated=p.exception_truncated;
}
static void bp_cc_read(unsigned role,unsigned matrix,const MethodInfo* method,Il2CppObject* source) {
 BpCcSample* s=&bp_cc.sample[role];
 if(!bp_cc_owner()) { s->status[matrix]=BP_PR_OWNER;return; }
 if(!bp_identity_equal(&bp_cc.identity,&bp.c.identity)) { s->status[matrix]=BP_CC_IDENTITY;return; }
 if(!method || !bp.api.runtime_invoke || !bp.api.object_unbox) return;
 Il2CppObject* receiver=bp_cc_receiver(role,source);
 if(!receiver) { s->status[matrix]=BP_PR_ROOT;return; }
 s->order[matrix]=++bp_cc.order;
 s->seen|=1u<<matrix;
 Il2CppException* exc=NULL;
 Il2CppObject* boxed=bp.api.runtime_invoke(method,receiver,NULL,&exc);
 if(exc) { s->status[matrix]=BP_PR_EXCEPTION;bp_cc_exception(s,exc);return; }
 void* value=boxed && bp.api.object_get_class(boxed)==bp.types[BT_M4]?bp.api.object_unbox(boxed):NULL;
 if(!value) { s->status[matrix]=BP_PR_BOX;return; }
 memcpy(&s->matrix[matrix],value,sizeof(Matrix4x4));s->status[matrix]=BP_PR_VALUE;
}
static void bp_cc_capture(Il2CppObject* source,int32_t frame) {
 if(bp_cc.attempted) return;
 bp_cc.attempted=true;bp_cc.identity=bp.c.identity;bp_cc.caller_frame=frame;
 bp_cc.before_frame=bp_cc.after_frame=-1;
 bp_cc.phase=(unsigned)bp.c.phase;bp_cc.initialized=bp.c.initialized;bp_cc.enabled=bp.c.enabled;
 for(unsigned r=0;r<3;++r) {
  bp_cc.sample[r].projection_frame=-1;
  for(unsigned m=0;m<BP_CC_MATRICES;++m) bp_cc.sample[r].status[m]=BP_PR_UNAVAILABLE;
 }
 /* Availability is per value. A missing optional culling getter must not
    discard existing own-P evidence or suppress supported source-P/view reads. */
 bp_cc.selected_mask=72u|(bp.method[BM_PROJECTION_GET]?1u:0u)|
  (bp_cc.binding[0].selected?146u:0u)|(bp_cc.binding[1].selected?292u:0u);
 if(!bp_cc_owner()) {
  for(unsigned r=0;r<3;++r) for(unsigned m=0;m<BP_CC_MATRICES;++m) bp_cc.sample[r].status[m]=BP_PR_OWNER;
  return;
 }
 for(unsigned eye=0;eye<2;++eye) {
  const BpProjectionSample* p=&bp_projection.sample[BP_PR_CAPTURE][eye];
  BpCcSample* s=&bp_cc.sample[eye+1];s->status[BP_CC_P]=BP_CC_MISSING;
  if(!p->seen) continue;
  s->seen|=1u<<BP_CC_P;
  s->projection_frame=p->frame;s->projection_order=p->order;
  if(!bp_identity_equal(&p->identity,&bp_cc.identity) || p->phase!=BP_EXCLUDE ||
   p->initialized!=3 || p->enabled!=3 || p->frame_kind!=BP_PR_FRAME_CALLER) {
   s->status[BP_CC_P]=BP_CC_IDENTITY;continue;
  }
  s->status[BP_CC_P]=p->status;
  if(p->status==BP_PR_VALUE) s->matrix[BP_CC_P]=p->actual;
 }
 bp_cc.before_frame=bp_projection_frame();
 bp_cc_read(0,BP_CC_P,bp.method[BM_PROJECTION_GET],source);
 for(unsigned r=0;r<3;++r) {
  bp_cc_read(r,BP_CC_V,bp_cc.binding[0].method,source);
  bp_cc_read(r,BP_CC_C,bp_cc.binding[1].method,source);
 }
 /* No second Time invocation if a callback invalidated the owner context. */
 if(bp_cc_owner()) bp_cc.after_frame=bp_projection_frame();
 bp_cc.identity_valid=bp_cc_owner() && bp_identity_equal(&bp_cc.identity,&bp.c.identity) &&
  bp_cc_receiver(0,source)!=NULL;
}
static bool bp_cc_emit(const char* event,const char* detail,bool footer) {
 unsigned charge=(unsigned)strlen(detail)+384u;
 unsigned limit=BP_CC_BYTE_LIMIT-(footer?0u:BP_CC_FOOTER_RESERVE);
 if(bp_cc.records+(footer?0u:1u)>=BP_CC_RECORD_LIMIT || charge>limit || bp_cc.bytes>limit-charge) {
  ++bp_cc.budget_dropped;return false;
 }
 ++bp_cc.records;bp_cc.bytes+=charge;
 bool wrote=bp_projection_normal_record_tail(event,detail,footer?0u:BP_CC_FOOTER_RESERVE,footer?0u:1u);
 if(!wrote) ++bp_cc.writer_dropped;return wrote;
}
static bool bp_cc_frame_coherent(void) {
 return bp_cc.caller_frame>=0 && bp_cc.before_frame==bp_cc.caller_frame && bp_cc.after_frame==bp_cc.caller_frame &&
  bp_cc.sample[1].projection_frame==bp_cc.caller_frame && bp_cc.sample[2].projection_frame==bp_cc.caller_frame;
}
static void bp_cc_flush(void) {
 if(!bp_cc.attempted || bp_cc.flushed) return;
 bp_cc.flushed=true;
 char detail[1500];unsigned seen=0,values=0,finite=0;
 const BpCcBinding *v=&bp_cc.binding[0],*c=&bp_cc.binding[1];
 int n=telemetry_snprintf(detail,sizeof(detail),
  "\"camera_contract_schema\":1,\"layout\":\"cpu_m0_to_m15_f32_hex\",\"roles\":[\"source\",\"left\",\"right\"],"
  "\"matrices\":[\"projection\",\"worldToCamera\",\"culling\"],\"status\":[\"value\",\"exception\",\"boxed_contract\",\"root_contract\",\"owner\",\"unavailable\",\"missing_sample\",\"identity\"],"
  "\"interpretation\":\"RAW_UNINTERPRETED\",\"wedge_cause\":\"INCONCLUSIVE\",\"ready\":%s,"
  "\"id_order\":\"generation_source_scene_pose_config\",\"optional_columns\":[\"outcome\",\"selected\",\"complete\",\"capped\",\"invalid_names\",\"named\",\"valid\",\"scan\"],"
  "\"optional\":[[\"%s\",%u,%u,%u,%u,%u,%u,%u],[\"%s\",%u,%u,%u,%u,%u,%u,%u]],"
  "\"reuse\":\"own_P_first_capture_order0_reused\",\"absence\":\"UNKNOWN\"",
  bp_cc.ready?"true":"false",v->outcome?v->outcome:"not_available",v->selected,v->complete,v->capped,v->invalid,v->named,v->valid,v->scan,
  c->outcome?c->outcome:"not_available",c->selected,c->complete,c->capped,c->invalid,c->named,c->valid,c->scan);
 if(n>0 && n<(int)sizeof(detail)) (void)bp_cc_emit("base_camera_contract_schema",detail,false);else ++bp_cc.format_dropped;
 for(unsigned r=0;r<3;++r) {
  const BpCcSample* s=&bp_cc.sample[r];char hex[3][129]={{0}},ns[192],name[288];unsigned nf[3]={0};
  seen|=s->seen<<(r*BP_CC_MATRICES);
  for(unsigned m=0;m<BP_CC_MATRICES;++m) if(s->status[m]==BP_PR_VALUE) {
   unsigned bit=1u<<(r*BP_CC_MATRICES+m);values|=bit;
   bp_projection_hex(&s->matrix[m],hex[m]);nf[m]=bp_projection_nonfinite(&s->matrix[m]);
   if(!nf[m]) finite|=bit;
  }
  json_escape(s->exception_ns,ns,sizeof(ns));json_escape(s->exception_name,name,sizeof(name));
  n=telemetry_snprintf(detail,sizeof(detail),
   "\"role\":%u,\"id\":[%llu,%llu,%d,%llu,%llu],\"s\":[%u,%u,%u],\"o\":[%u,%u,%u],"
   "\"pvc\":[\"%s\",\"%s\",\"%s\"],\"nf\":[%u,%u,%u],\"p_origin\":[%d,%d,%u],\"ex\":[\"%s\",\"%s\",%s]",
   r,(unsigned long long)bp_cc.identity.generation,(unsigned long long)bp_cc.identity.source,bp_cc.identity.scene,
   (unsigned long long)bp_cc.identity.pose,(unsigned long long)bp_cc.identity.config,
   s->status[0],s->status[1],s->status[2],s->order[0],s->order[1],s->order[2],hex[0],hex[1],hex[2],nf[0],nf[1],nf[2],
   r?BP_PR_CAPTURE:-1,s->projection_frame,s->projection_order,ns,name,s->exception_truncated?"true":"false");
  if(n>0 && n<(int)sizeof(detail)) { if(bp_cc_emit("base_camera_contract_sample",detail,false)) bp_cc.submitted|=7u<<(r*3); }
  else ++bp_cc.format_dropped;
 }
 bool frame_ok=bp_cc_frame_coherent();
 n=telemetry_snprintf(detail,sizeof(detail),
  "\"id\":[%llu,%llu,%d,%llu,%llu],\"frames\":[%d,%d,%d],\"state\":[%u,%u,%u],"
  "\"identity_valid\":%s,\"frame_coherent\":%s,\"complete_cpu_values\":%s,\"selected_mask\":%u,\"seen_mask\":%u,\"missing_mask\":%u,\"value_mask\":%u,\"finite_mask\":%u,\"submitted_mask\":%u,"
  "\"build_snapshot\":[%s,\"%08X\",\"%08X\",%d],\"snapshot_match\":%s,"
  "\"budget_dropped\":%u,\"format_dropped\":%u,\"writer_dropped_before_footer\":%u,\"charged_before_footer\":%u,\"records_before_footer\":%u,"
  "\"byte_limit\":%u,\"record_limit\":%u,\"absence\":\"UNKNOWN\",\"wedge_cause\":\"INCONCLUSIVE\"",
  (unsigned long long)bp_cc.identity.generation,(unsigned long long)bp_cc.identity.source,bp_cc.identity.scene,
  (unsigned long long)bp_cc.identity.pose,(unsigned long long)bp_cc.identity.config,bp_cc.caller_frame,bp_cc.before_frame,bp_cc.after_frame,
  bp_cc.phase,bp_cc.initialized,bp_cc.enabled,bp_cc.identity_valid?"true":"false",frame_ok?"true":"false",
  bp_cc.identity_valid && frame_ok && finite==511?"true":"false",bp_cc.selected_mask,seen,511u&~seen,values,finite,bp_cc.submitted,
  bp_cc.snapshot_valid?"true":"false",bp_projection_bits(bp_cc.near_clip),bp_projection_bits(bp_cc.far_clip),bp_cc.mask,
  bp_cc.snapshot_valid && bp_identity_equal(&bp_cc.snapshot_identity,&bp_cc.identity)?"true":"false",
  bp_cc.budget_dropped,bp_cc.format_dropped,bp_cc.writer_dropped,bp_cc.bytes,bp_cc.records,BP_CC_BYTE_LIMIT,BP_CC_RECORD_LIMIT);
 if(n>0 && n<(int)sizeof(detail)) (void)bp_cc_emit("base_camera_contract_footer",detail,true);else ++bp_cc.format_dropped;
}
