/* Cache only fixed lifecycle checkpoints; flush once without any engine reads.
   Float/double bits are hexadecimal IEEE754 words in CPU m[0..15] order.
   Absent/failed/dropped evidence is UNKNOWN, never proof of an unchanged value. */
typedef struct BpProjectionSample {
 Matrix4x4 expected,actual; BpIdentity identity;
 unsigned status,order,phase,initialized,enabled,frame_kind;
 int32_t frame; bool seen,exception_truncated;
 char exception_ns[32],exception_name[48];
} BpProjectionSample;
static struct {
 BpProjectionSample sample[BP_PR_STAGES][2];
 unsigned order,armed_eye,records,bytes,budget_dropped,format_dropped,writer_dropped;
 bool armed,flushed; int32_t armed_frame;
} bp_projection;

static void bp_projection_text(char* out,size_t cap,const char* in,bool* truncated) {
 size_t n=0; if(!in) in="<unavailable>";
 while(in[n] && n+1<cap) { out[n]=in[n];++n; }
 out[n]=0; if(in[n]) *truncated=true;
}
static void bp_projection_exception(BpProjectionSample* s,Il2CppException* exc) {
 s->status=BP_PR_EXCEPTION;
 Il2CppClass* c=bp.api.object_get_class((Il2CppObject*)exc);
 bp_projection_text(s->exception_ns,sizeof(s->exception_ns),c?bp.api.class_get_namespace(c):NULL,&s->exception_truncated);
 bp_projection_text(s->exception_name,sizeof(s->exception_name),c?bp.api.class_get_name(c):NULL,&s->exception_truncated);
}
static BpProjectionSample* bp_projection_begin(unsigned stage,unsigned eye,int32_t frame,unsigned frame_kind) {
 if(stage>=BP_PR_STAGES || eye>1 || bp_projection.flushed) return NULL;
 BpProjectionSample* s=&bp_projection.sample[stage][eye]; if(s->seen) return NULL;
 s->seen=true;s->order=++bp_projection.order;s->identity=bp.c.identity;
 s->phase=(unsigned)bp.c.phase;s->initialized=bp.c.initialized;s->enabled=bp.c.enabled;
 s->expected=bp.eye[eye].projection;s->frame=frame;s->frame_kind=frame_kind;
 s->status=BP_PR_UNAVAILABLE;return s;
}
/* Direct use of already-bound getter, not bp_call/get: observation failures
   cannot change bp.stage/error/failed, retire, publication or mono policy. */
static void bp_projection_read(BpProjectionSample* s,unsigned eye) {
 if(!s) return;
 if(GetCurrentThreadId()!=bp.owner_thread || !bp.api.thread_current()) { s->status=BP_PR_OWNER;return; }
 if(!bp.method[BM_PROJECTION_GET]) return;
 Il2CppObject* camera=bp.eye[eye].camera?bp.api.gchandle_get_target(bp.eye[eye].camera):NULL;
 if(!camera || bp.api.object_get_class(camera)!=bp.types[BT_CAMERA]) { s->status=BP_PR_ROOT;return; }
 Il2CppException* exc=NULL;
 Il2CppObject* o=bp.api.runtime_invoke(bp.method[BM_PROJECTION_GET],camera,NULL,&exc);
 if(exc) { bp_projection_exception(s,exc);return; }
 void* value=o && bp.api.object_get_class(o)==bp.types[BT_M4]?bp.api.object_unbox(o):NULL;
 if(!value) { s->status=BP_PR_BOX;return; }
 memcpy(&s->actual,value,sizeof(s->actual));s->status=BP_PR_VALUE;
}
static int32_t bp_projection_frame(void) {
 if(GetCurrentThreadId()!=bp.owner_thread || !bp.api.thread_current() || !bp.method[BM_FRAME]) return -1;
 Il2CppException* exc=NULL;
 Il2CppObject* o=bp.api.runtime_invoke(bp.method[BM_FRAME],NULL,NULL,&exc);
 void* value=!exc && o && bp.api.object_get_class(o)==bp.types[BT_INT]?bp.api.object_unbox(o):NULL;
 int32_t frame=-1;if(value) memcpy(&frame,value,sizeof(frame));return frame;
}
static void bp_projection_checkpoint(unsigned stage,unsigned eye,int32_t frame) {
 BpProjectionSample* s=bp_projection_begin(stage,eye,frame,frame<0?BP_PR_FRAME_UNKNOWN:BP_PR_FRAME_CALLER);
 bp_projection_read(s,eye);
 if(s && frame<0 && s->status!=BP_PR_OWNER) {
  s->frame=bp_projection_frame();s->frame_kind=s->frame<0?BP_PR_FRAME_UNKNOWN:BP_PR_FRAME_AFTER_READ;
 }
}
static void bp_projection_pair(unsigned stage,int32_t frame) {
 BpProjectionSample* s[2];
 for(unsigned eye=0;eye<2;++eye) {
  s[eye]=bp_projection_begin(stage,eye,frame,frame<0?BP_PR_FRAME_UNKNOWN:BP_PR_FRAME_CALLER);
  bp_projection_read(s[eye],eye);
 }
 if(frame<0 && (s[0] || s[1])) {
  int32_t after=bp_projection_frame();
  for(unsigned eye=0;eye<2;++eye) if(s[eye]) { s[eye]->frame=after;s[eye]->frame_kind=after<0?BP_PR_FRAME_UNKNOWN:BP_PR_FRAME_AFTER_READ; }
 }
}
static void bp_projection_arm(unsigned eye,int32_t frame) {
 bp_projection.armed=eye<2 && !bp_projection.flushed && !bp_projection.sample[BP_PR_CAPTURE][eye].seen;
 bp_projection.armed_eye=eye;bp_projection.armed_frame=frame;
}
static void bp_projection_disarm(void) { bp_projection.armed=false; }
static BpProjectionSample* bp_projection_tap(unsigned id) {
 if(id!=BM_PROJECTION_GET || !bp_projection.armed) return NULL;
 return bp_projection_begin(BP_PR_CAPTURE,bp_projection.armed_eye,bp_projection.armed_frame,BP_PR_FRAME_CALLER);
}
static void bp_projection_tap_exception(unsigned id,Il2CppException* exc) {
 BpProjectionSample* s=bp_projection_tap(id);if(s) bp_projection_exception(s,exc);
}
static void bp_projection_tap_box(unsigned id) {
 BpProjectionSample* s=bp_projection_tap(id);if(s) s->status=BP_PR_BOX;
}
static void bp_projection_tap_value(unsigned id,const void* value,size_t size) {
 BpProjectionSample* s=bp_projection_tap(id);
 if(s && size==sizeof(s->actual)) { memcpy(&s->actual,value,size);s->status=BP_PR_VALUE; }
}
static uint32_t bp_projection_bits(float value) { uint32_t u;memcpy(&u,&value,sizeof(u));return u; }
static void bp_projection_hex(const Matrix4x4* m,char out[129]) {
 static const char hex[]="0123456789ABCDEF";
 for(unsigned i=0;i<16;++i) {
  uint32_t u=bp_projection_bits(m->m[i]);
  for(unsigned n=0;n<8;++n) out[i*8+n]=hex[(u>>(28-4*n))&15];
 } out[128]=0;
}
static bool bp_projection_finite(uint32_t bits) { return (bits&0x7f800000u)!=0x7f800000u; }
static unsigned bp_projection_nonfinite(const Matrix4x4* m) {
 unsigned mask=0;for(unsigned i=0;i<16;++i) if(!bp_projection_finite(bp_projection_bits(m->m[i]))) mask|=1u<<i;return mask;
}
/* Pure diagnostic math. Never consumed by the product projection predicate. */
static void bp_projection_delta(const BpProjectionSample* s,unsigned* rawdiff,unsigned* predicate,int* index,uint64_t* maxbits) {
 double max=0;*rawdiff=*predicate=0;*index=0;
 for(unsigned i=0;i<16;++i) {
  uint32_t a=bp_projection_bits(s->actual.m[i]),e=bp_projection_bits(s->expected.m[i]);
  if(a!=e) *rawdiff|=1u<<i;
  if(!bp_projection_finite(a)) { *predicate|=1u<<i;continue; }
  if(!bp_projection_finite(e)) {
   if(!(e&0x007fffffu)) *predicate|=1u<<i;
   continue;
  }
  double d=fabs((double)s->actual.m[i]-(double)s->expected.m[i]);
  if(d>max) { max=d;*index=(int)i; }
  /* Same float absolute predicate, avoiding nonfinite-input arithmetic. */
  if(fabsf(s->actual.m[i]-s->expected.m[i])>0.0001f) *predicate|=1u<<i;
 }
 memcpy(maxbits,&max,sizeof(max));
 if(bp_projection_nonfinite(&s->actual) || bp_projection_nonfinite(&s->expected)) *index=-1;
}
static bool bp_projection_emit(const char* event,const char* detail,bool footer) {
 unsigned charge=(unsigned)strlen(detail)+384u;
 if(bp_projection.records+(footer?0u:1u)>=BP_PR_RECORD_LIMIT ||
  charge>BP_PR_BYTE_LIMIT-(footer?0u:1024u) || bp_projection.bytes>BP_PR_BYTE_LIMIT-(footer?0u:1024u)-charge) {
  ++bp_projection.budget_dropped;return false;
 }
 ++bp_projection.records;bp_projection.bytes+=charge;
 bool wrote=bp_projection_normal_record(event,detail);
 if(!wrote) ++bp_projection.writer_dropped;return wrote;
}
/* Sample contains a full identity; stages may not be silently joined across it.
   e is the exact first expected matrix per eye from the schema record unless
   x is present (a changed C-side expected value at this checkpoint). */
static bool bp_projection_row(const BpProjectionSample* s,unsigned eye,const Matrix4x4* baseline,char* out,size_t cap) {
 char actual[129]="",expected[129],override[145]="",ns[192],name[288],maxhex[24]="null";
 unsigned rawdiff=0,predicate=0;int index=-1;uint64_t bits=0;
 if(s->status==BP_PR_VALUE) {
  bp_projection_hex(&s->actual,actual);bp_projection_delta(s,&rawdiff,&predicate,&index,&bits);
  if(index>=0) telemetry_snprintf(maxhex,sizeof(maxhex),"\"%016llX\"",(unsigned long long)bits);
 }
 if(memcmp(&s->expected,baseline,sizeof(*baseline))) {
  bp_projection_hex(&s->expected,expected);telemetry_snprintf(override,sizeof(override),",\"x\":\"%s\"",expected);
 }
 json_escape(s->exception_ns,ns,sizeof(ns));json_escape(s->exception_name,name,sizeof(name));
 int n=telemetry_snprintf(out,cap,
  "{\"eye\":%u,\"o\":%u,\"id\":[%llu,%llu,%d,%llu,%llu],\"s\":%u,\"f\":%d,\"fk\":%u,\"p\":[%u,%u,%u],"
  "\"a\":\"%s\",\"nf\":[%u,%u],\"raw\":%u,\"bad\":%u,\"max\":%s,\"at\":%d,\"ex\":[\"%s\",\"%s\",%s]%s}",
  eye,s->order,(unsigned long long)s->identity.generation,(unsigned long long)s->identity.source,s->identity.scene,
  (unsigned long long)s->identity.pose,(unsigned long long)s->identity.config,s->status,s->frame,s->frame_kind,
  s->phase,s->initialized,s->enabled,actual,bp_projection_nonfinite(&s->expected),s->status==BP_PR_VALUE?bp_projection_nonfinite(&s->actual):0,
  rawdiff,predicate,maxhex,index,ns,name,s->exception_truncated?"true":"false",override);
 return n>0 && n<(int)cap;
}
static void bp_projection_flush(void) {
 if(bp_projection.flushed || !bp_projection.order) return;
 bp_projection.flushed=true;Matrix4x4 baseline[2]={0};unsigned seen=0,values=0,submitted=0,expected_mask=0;
 for(unsigned eye=0;eye<2;++eye) for(unsigned stage=0;stage<BP_PR_STAGES;++stage) {
  BpProjectionSample* s=&bp_projection.sample[stage][eye];
  if(s->seen) { baseline[eye]=s->expected;expected_mask|=1u<<eye;break; }
 }
 char e0[129],e1[129],detail[1500];bp_projection_hex(&baseline[0],e0);bp_projection_hex(&baseline[1],e1);
 int n=telemetry_snprintf(detail,sizeof(detail),
  "\"projection_schema\":1,\"layout\":\"cpu_m0_to_m15_f32_hex\",\"max_encoding\":\"f64_hex_or_null\","
  "\"id_order\":\"generation_source_scene_pose_config\",\"stages\":[\"final_set\",\"both_init\",\"pre_enable\",\"each_enabled\",\"pair_enabled\",\"first_capture\"],"
  "\"status\":[\"value\",\"exception\",\"boxed_contract\",\"root_contract\",\"owner\",\"unavailable\"],"
  "\"frame_kind\":[\"product_snapshot\",\"after_read\",\"unknown\"],\"epsilon_f32\":\"38D1B717\",\"expected_mask\":%u,\"e\":[\"%s\",\"%s\"],\"absence\":\"UNKNOWN\"",expected_mask,e0,e1);
 if(n>0 && n<(int)sizeof(detail)) (void)bp_projection_emit("base_projection_schema",detail,false);
 else ++bp_projection.format_dropped;
 /* Baseline and first failing/capture value precede supporting stages. */
 const unsigned priority[]={BP_PR_SET,BP_PR_CAPTURE,BP_PR_INIT,BP_PR_PRE,BP_PR_PAIR,BP_PR_EACH};
 for(unsigned k=0;k<BP_PR_STAGES;++k) {
  unsigned stage=priority[k];char row[2][1024]={{0}};bool ok=true;unsigned mask=0;
  for(unsigned eye=0;eye<2;++eye) {
   BpProjectionSample* s=&bp_projection.sample[stage][eye];unsigned bit=1u<<(stage*2+eye);
   if(!s->seen) continue;seen|=bit;mask|=bit;if(s->status==BP_PR_VALUE) values|=bit;
   if(!bp_projection_row(s,eye,&baseline[eye],row[eye],sizeof(row[eye]))) ok=false;
  }
  if(!mask) continue;
  n=ok?telemetry_snprintf(detail,sizeof(detail),"\"stage_id\":%u,\"samples\":[%s%s%s]",stage,row[0],row[0][0]&&row[1][0]?",":"",row[1]):-1;
  if(n<=0 || n>=(int)sizeof(detail)) { ++bp_projection.format_dropped;continue; }
  if(bp_projection_emit("base_projection_samples",detail,false)) submitted|=mask;
 }
 n=telemetry_snprintf(detail,sizeof(detail),
  "\"seen_mask\":%u,\"value_mask\":%u,\"submitted_mask\":%u,\"missing_mask\":%u,\"budget_dropped\":%u,\"format_dropped\":%u,\"writer_dropped_before_footer\":%u,"
  "\"charged_before_footer\":%u,\"records_before_footer\":%u,\"byte_limit\":%u,\"record_limit\":%u,\"absence\":\"UNKNOWN\"",
  seen,values,submitted,((1u<<(BP_PR_STAGES*2))-1u)&~seen,bp_projection.budget_dropped,bp_projection.format_dropped,
  bp_projection.writer_dropped,bp_projection.bytes,bp_projection.records,BP_PR_BYTE_LIMIT,BP_PR_RECORD_LIMIT);
#ifdef BP_FOV_DIAGNOSTIC
 /* Share the reserved footer: no extra record/envelope and no displacement
    of matrix samples or the separate original stop/disable reserve. */
 if(n>0 && n<(int)sizeof(detail)) {
  int extra=bp_fov_json(detail+n,sizeof(detail)-(size_t)n);
  if(extra<0 || extra>=(int)(sizeof(detail)-(size_t)n)) n=-1;else n+=extra;
 }
#endif
#ifdef BP_BACKGROUND_POLICY
 if(n>0 && n<(int)sizeof(detail)) {
  int extra=bp_background_json(detail+n,sizeof(detail)-(size_t)n);
  if(extra<0 || extra>=(int)(sizeof(detail)-(size_t)n)) n=-1;else n+=extra;
 }
#endif
 if(n>0 && n<(int)sizeof(detail)) (void)bp_projection_emit("base_projection_footer",detail,true);
}
static void bp_projection_finish_capture(void) {
 if(bp_projection.sample[BP_PR_CAPTURE][0].seen || bp_projection.sample[BP_PR_CAPTURE][1].seen) bp_projection_flush();
}
