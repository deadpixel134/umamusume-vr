/* Execute the actual optional selector, raw observer, and normal writer.
   Reuse the existing managed/I/O substitutes; their earlier suites are not run.
   No simulated renderer contract or matrix-volume interpretation. */
#define BP_PROJECTION_TEST_MAIN projection_fixture_main
#include "projection_observe_test.c"
static volatile LONG bp_main_depth;
#include "../src/base_camera_contract_observe.h"
_Static_assert(BM_COUNT==53,"Observer must not extend required bindings");
static MethodInfo cc_getter[2],cc_duplicate;
static Il2CppObject cc_source,cc_source_box,cc_box[3][2];
static Matrix4x4 cc_values[3][3];
static unsigned cc_calls[3][3],cc_modes[3][3];
static bool cc_bad_unbox,cc_frame_change,cc_identity_change,cc_scan_capped;
static const MethodInfo* cc_metadata(Il2CppClass* owner,void** it) {
 if(cc_scan_capped && owner==bp.types[BT_CAMERA]) {
  uintptr_t n=(uintptr_t)*it;if(n>=513) return NULL;*it=(void*)(n+1);
  return n<2?&cc_getter[n]:&extra;
 }
 return pf_methods(owner,it);
}
static void* cc_unbox(Il2CppObject* boxed) {
 if(boxed==&cc_source_box) return cc_bad_unbox?NULL:&cc_values[0][0];
 for(unsigned r=0;r<3;++r) for(unsigned m=0;m<2;++m)
  if(boxed==&cc_box[r][m]) return cc_bad_unbox?NULL:&cc_values[r][m+1];
 return pr_unbox(boxed);
}
static Il2CppObject* cc_invoke(const MethodInfo* method,void* receiver,void** args,Il2CppException** exc) {
 if(method->id==BM_FRAME) {
  if(cc_frame_change && frame_reads==1) pr_frame.value=73;
  return pr_invoke(method,receiver,args,exc);
 }
 int role=receiver==&cc_source?0:receiver==&pr_camera[0]?1:receiver==&pr_camera[1]?2:-1;
 int matrix=method->id==BM_COUNT?BP_CC_V:method->id==BM_COUNT+1?BP_CC_C:method->id==BM_PROJECTION_GET?BP_CC_P:-1;
 if(role<0 || matrix<0 || (role && matrix==BP_CC_P)) return pr_invoke(method,receiver,args,exc);
 *exc=NULL;++cc_calls[role][matrix];unsigned mode=cc_modes[role][matrix];cc_bad_unbox=mode==PR_UNBOX;
 if(cc_identity_change) { ++bp.c.identity.generation;cc_identity_change=false; }
 if(mode==PR_THROW) { *exc=&exception;return NULL; }
 if(mode==PR_NULL) return NULL;
 if(mode==PR_WRONG) return &pr_frame;
 return matrix==BP_CC_P?&cc_source_box:&cc_box[role][matrix-1];
}
static void cc_reset(void) {
 pr_reset();memset(&bp_cc,0,sizeof(bp_cc));memset(cc_calls,0,sizeof(cc_calls));memset(cc_modes,0,sizeof(cc_modes));
 cc_bad_unbox=cc_frame_change=cc_identity_change=cc_scan_capped=false;bp_main_depth=1;bp.bound=true;
 bp.api.runtime_invoke=cc_invoke;bp.api.object_unbox=cc_unbox;bp.api.class_get_methods=cc_metadata;
 cc_source=(Il2CppObject){.klass=bp.types[BT_CAMERA]};cc_source_box=(Il2CppObject){.klass=bp.types[BT_M4]};
 bp.source_root=bp_root(&cc_source);
 for(unsigned r=0;r<3;++r) for(unsigned m=0;m<3;++m) {
  for(unsigned i=0;i<16;++i) cc_values[r][m].m[i]=(float)(r*100+m*20+i)/7.0f;
  if(m) cc_box[r][m-1]=(Il2CppObject){.klass=bp.types[BT_M4]};
 }
 Il2CppClass* camera=bp.types[BT_CAMERA];
 cc_getter[0]=(MethodInfo){.name="get_worldToCameraMatrix",.owner=camera,.ret=&bp.types[BT_M4]->type,.flags=6,.id=BM_COUNT};
 cc_getter[1]=(MethodInfo){.name="get_cullingMatrix",.owner=camera,.ret=&bp.types[BT_M4]->type,.flags=6,.id=BM_COUNT+1};
 camera->methods[camera->count++]=&cc_getter[0];camera->methods[camera->count++]=&cc_getter[1];
 pr_frame.value=72;
}
static int cc_seed(void) {
 CHECK(pr_original_read(0) && pr_original_read(1));
 bp_cc_snapshot(0.1f,100.0f,42);return 0;
}
static int cc_end(const char* label) {
 CHECK(bp_cc.records<=5 && bp_cc.bytes<=6144 && projection_kept<=5);
 CHECK(telemetry_stats.bytes[0]<=40960u-BP_PR_NORMAL_BYTE_RESERVE);
 CHECK(telemetry_stats.records[0]<=160u-BP_PR_NORMAL_RECORD_RESERVE);
 CHECK(!telemetry_stats.bytes[1] && !telemetry_stats.bytes[2] && !telemetry_stats.records[1] && !telemetry_stats.records[2]);
 for(unsigned i=0;i<projection_kept;++i) if(!strcmp(projection_events[i],"base_camera_contract_footer"))
  CHECK(strlen(projection_records[i])+384u<=BP_CC_FOOTER_RESERVE);
 pr_dump(label);++cases;return 0;
}
static int cc_run(const char* label) {
 BpControl original=bp.c;const char *stage=bp.stage,*error=bp.error;
 const MethodInfo* required[BM_COUNT];memcpy(required,bp.method,sizeof(required));unsigned root_before=root_calls;
 bp_cc_capture(&cc_source,72);bp_cc_flush();
 CHECK(!memcmp(&original,&bp.c,sizeof(original)) && bp.stage==stage && bp.error==error && !bp.failed);
 CHECK(!memcmp(required,bp.method,sizeof(required)) && root_before==root_calls && !log_count && !unpublish_count);
 CHECK(reads[0]==1 && reads[1]==1 && frame_reads<=2 && bp_cc.order<=7);
 unsigned calls=writer_calls,order=bp_cc.order;bp_cc_capture(&cc_source,72);bp_cc_flush();
 CHECK(calls==writer_calls && order==bp_cc.order);
 return cc_end(label);
}
int main(void) {
 cc_reset();CHECK(!cc_seed());bp_cc_bind();CHECK(bp_cc.ready && !pf_invocations);
 CHECK(!cc_run("complete"));CHECK(projection_kept==5 && bp_cc.submitted==511 && bp_cc.identity_valid && bp_cc_frame_coherent());
 CHECK(frame_reads==2 && bp_cc.order==7 && !bp_cc.budget_dropped && !bp_cc.writer_dropped);
 for(unsigned r=0;r<3;++r) for(unsigned m=0;m<3;++m) {
  CHECK(cc_calls[r][m]==((!r || m)?1u:0u));
  const Matrix4x4* expected=m || !r?&cc_values[r][m]:&pr_actual[r-1];
  CHECK(!memcmp(expected,&bp_cc.sample[r].matrix[m],sizeof(Matrix4x4)));
 }
 /* Optional preflight failure must not become a required producer failure. */
 for(unsigned mode=0;mode<12;++mode) {
  cc_reset();CHECK(!cc_seed());Il2CppType wrong=bp.types[BT_M4]->type;
  switch(mode) {
  case 0:cc_getter[0].name="missing";break;
  case 1:cc_getter[0].owner=bp.types[BT_RT];break;
  case 2:cc_getter[0].flags=0x16;break;
  case 3:cc_getter[0].flags=1;break;
  case 4:cc_getter[0].generic=true;break;
  case 5:cc_getter[0].inflated=true;break;
  case 6:wrong.byref=true;cc_getter[0].ret=&wrong;break;
  case 7:wrong.klass=bp.types[BT_V3];cc_getter[0].ret=&wrong;break;
  case 8:cc_getter[0].argc=1;break;
  case 9:cc_duplicate=cc_getter[0];bp.types[BT_CAMERA]->methods[bp.types[BT_CAMERA]->count++]=&cc_duplicate;break;
  case 10:cc_scan_capped=true;break;
  default:cc_getter[1].name=NULL;break;
  }
  bp_cc_bind();CHECK(!bp_cc.ready && bp.bound && !bp.failed && !pf_invocations);
  CHECK(!cc_run("optional_partial"));CHECK(frame_reads==2 && bp_cc.order<=4);
  CHECK(bp_cc.selected_mask!=511 && bp_cc.sample[1].status[0]==BP_PR_VALUE && bp_cc.sample[2].status[0]==BP_PR_VALUE);
 }
 cc_reset();CHECK(!cc_seed());cc_getter[1].name="missing_culling";bp_cc_bind();
 CHECK(!bp_cc.ready && bp_cc.binding[0].selected && !bp_cc.binding[1].selected);
 CHECK(!cc_run("culling_absent_pv"));CHECK(bp_cc.selected_mask==219 && bp_cc.order==4 && frame_reads==2);
 for(unsigned r=0;r<3;++r) CHECK(bp_cc.sample[r].status[BP_CC_C]==BP_PR_UNAVAILABLE && cc_calls[r][BP_CC_C]==0);
 cc_reset();CHECK(!cc_seed());bp.api.method_get_return_type=NULL;bp_cc_bind();CHECK(!bp_cc.ready);
 CHECK(!cc_run("metadata_export_unavailable"));
 /* Getter failures and nonfinite bits remain evidence-only. */
 for(unsigned mode=PR_THROW;mode<=PR_UNBOX;++mode) {
  cc_reset();CHECK(!cc_seed());bp_cc_bind();cc_modes[1][BP_CC_C]=mode;
  CHECK(!cc_run("getter_failure"));CHECK(bp_cc.sample[1].status[BP_CC_C]==(mode==PR_THROW?BP_PR_EXCEPTION:BP_PR_BOX));
 }
 cc_reset();CHECK(!cc_seed());bp_cc_bind();
 uint32_t nanbits=0x7fc12345u,negzero=0x80000000u;
 memcpy(&cc_values[0][BP_CC_C].m[9],&nanbits,4);memcpy(&cc_values[1][BP_CC_V].m[3],&negzero,4);
 CHECK(!cc_run("raw_nonfinite_signedzero"));
 CHECK(bp_projection_bits(bp_cc.sample[0].matrix[BP_CC_C].m[9])==nanbits);
 CHECK(bp_projection_bits(bp_cc.sample[1].matrix[BP_CC_V].m[3])==negzero);
 cc_reset();CHECK(!cc_seed());bp_cc_bind();cc_frame_change=true;CHECK(!cc_run("frame_changed"));CHECK(!bp_cc_frame_coherent());
 cc_reset();CHECK(!cc_seed());bp_cc_bind();bp_projection.sample[BP_PR_CAPTURE][1].frame=71;
 CHECK(!cc_run("reused_frame_changed"));CHECK(!bp_cc_frame_coherent());
 cc_reset();CHECK(!cc_seed());bp_cc_bind();bp_projection.sample[BP_PR_CAPTURE][0].seen=false;
 CHECK(!cc_run("reused_missing"));CHECK(bp_cc.sample[1].status[0]==BP_CC_MISSING);
 cc_reset();CHECK(!cc_seed());bp_cc_bind();bp.source_root=bp.eye[0].camera;
 CHECK(!cc_run("source_alias"));CHECK(!bp_cc.identity_valid && !bp_cc.order);
 cc_reset();CHECK(!cc_seed());bp_cc_bind();bp.source_root=63;
 CHECK(!cc_run("source_missing"));CHECK(!bp_cc.identity_valid && !bp_cc.order);
 cc_reset();CHECK(!cc_seed());bp_cc_bind();attached=false;CHECK(!cc_run("owner_unavailable"));CHECK(!frame_reads);
 cc_reset();CHECK(!cc_seed());bp_cc_bind();bp_main_depth=0;CHECK(!cc_run("owner_depth"));CHECK(!frame_reads);
 cc_reset();CHECK(!cc_seed());bp_cc_bind();
 cc_identity_change=true;bp_cc_capture(&cc_source,72);CHECK(!bp_cc.identity_valid && bp_cc.order==1 && frame_reads==2);
 CHECK(bp_cc.sample[0].status[1]==BP_CC_IDENTITY);bp_cc_flush();CHECK(!cc_end("identity_changed"));
 cc_reset();CHECK(!cc_seed());bp_cc_bind();writer_drop_at=2;CHECK(!cc_run("writer_drop"));CHECK(bp_cc.writer_dropped==1);
 cc_reset();CHECK(!cc_seed());bp_cc_bind();telemetry_stats.bytes[0]=40960u-BP_PR_NORMAL_BYTE_RESERVE;
 CHECK(!cc_run("normal_byte_reserve"));CHECK(!writer_calls && bp_cc.writer_dropped==5);
 cc_reset();CHECK(!cc_seed());bp_cc_bind();telemetry_stats.records[0]=160u-BP_PR_NORMAL_RECORD_RESERVE;
 CHECK(!cc_run("normal_record_reserve"));CHECK(!writer_calls && bp_cc.writer_dropped==5);
 cc_reset();CHECK(!cc_seed());bp_cc_bind();bp_cc.bytes=BP_CC_BYTE_LIMIT-BP_CC_FOOTER_RESERVE;
 CHECK(!cc_run("footer_reserve"));CHECK(projection_kept==1 && bp_cc.budget_dropped==4);
 cc_reset();CHECK(!cc_seed());bp_cc_bind();format_failure=true;
 CHECK(!cc_run("format_failure"));CHECK(!writer_calls && bp_cc.format_dropped==5);format_failure=false;
 cc_reset();CHECK(!cc_seed());bp_cc_bind();telemetry_ready=0;
 CHECK(!cc_run("logger_unavailable"));CHECK(!writer_calls && bp_cc.writer_dropped==5);
 /* Recorded initial normal-byte position. Nonfooter admission leaves the
    existing footer slack inside the same logger transaction; skipped rows
    stay absent from submitted_mask even though CPU values were acquired. */
 cc_reset();CHECK(!cc_seed());cc_getter[1].name="missing_culling";bp_cc_bind();telemetry_stats.bytes[0]=35048;
 CHECK(!cc_run("recorded_global_35048"));CHECK(bp_cc.writer_dropped>0);
 CHECK(projection_kept>=1 && !strcmp(projection_events[projection_kept-1],"base_camera_contract_footer"));
 unsigned submitted=0;
 for(unsigned i=0;i<projection_kept;++i) if(!strcmp(projection_events[i],"base_camera_contract_sample")) {
  unsigned role=99;CHECK(sscanf(projection_records[i],"\"role\":%u",&role)==1 && role<3);submitted|=7u<<(role*3);
 }
 CHECK(bp_cc.submitted==submitted && bp_cc.selected_mask==219 && bp_cc.order==4);
 cc_reset();CHECK(!cc_seed());bp_cc_bind();telemetry_stats.records[0]=154;
 CHECK(!cc_run("global_record_tail"));CHECK(projection_kept==2 && bp_cc.writer_dropped==3 && !bp_cc.submitted);
 CHECK(!strcmp(projection_events[1],"base_camera_contract_footer"));
 cc_reset();CHECK(!cc_seed());bp_cc_bind();telemetry_stats.bytes[0]=37888-400;
 CHECK(!cc_run("insufficient_initial_room"));CHECK(!projection_kept && bp_cc.writer_dropped==5);
 cc_reset();CHECK(!cc_seed());bp_cc_bind();telemetry_stats.bytes[0]=35048;writer_drop_at=2;
 CHECK(!cc_run("global_tail_sink_failure"));CHECK(bp_cc.writer_dropped>0 && !bp.failed);
 cc_reset();
 CHECK(!bp_projection_normal_record_tail("base_camera_contract_sample","\"x\":0",UINT32_MAX,0));
 CHECK(!bp_projection_normal_record_tail("base_camera_contract_sample","\"x\":0",0,UINT32_MAX));
 CHECK(!writer_calls && !bp.failed);++cases;
 cc_reset();CHECK(!cc_seed());bp_cc_bind();
 CHECK(!bp_cc_emit("base_camera_contract_sample","\"p\":nan",false));
 CHECK(!bp_cc_emit("base_camera_contract_sample",TELEMETRY_FORMAT_ERROR,false));
 CHECK(!writer_calls && !bp.failed && !telemetry_stats.bytes[1]);++cases;
 /* Longest identifiers/escaped exception metadata still fit the local and
    normal budgets. This does not represent actual Unity exception content. */
 cc_reset();CHECK(!cc_seed());bp_cc_bind();
 bp.c.identity=(BpIdentity){.generation=UINT64_MAX,.source=UINT64_MAX,.scene=INT32_MIN,.pose=UINT64_MAX,.config=UINT64_MAX};
 for(unsigned eye=0;eye<2;++eye) bp_projection.sample[BP_PR_CAPTURE][eye].identity=bp.c.identity;
 bp_cc.snapshot_identity=bp.c.identity;bp_cc.mask=INT32_MIN;
 pr_frame.value=INT32_MAX;
 for(unsigned eye=0;eye<2;++eye) bp_projection.sample[BP_PR_CAPTURE][eye].frame=INT32_MAX;
 bp_cc_capture(&cc_source,INT32_MAX);
 for(unsigned r=0;r<3;++r) {
  memset(bp_cc.sample[r].exception_ns,1,sizeof(bp_cc.sample[r].exception_ns)-1);
  memset(bp_cc.sample[r].exception_name,2,sizeof(bp_cc.sample[r].exception_name)-1);
  bp_cc.sample[r].exception_truncated=true;
  bp_cc.sample[r].status[BP_CC_C]=BP_PR_EXCEPTION;
 }
 bp_cc_flush();CHECK(!cc_end("max_encoded_metadata"));
 CHECK(projection_kept==5 && !bp_cc.budget_dropped && !bp_cc.format_dropped && !bp_cc.writer_dropped);
 printf("PASS camera contract observer: %u cases / %u assertions\n",cases,assertions);return 0;
}
