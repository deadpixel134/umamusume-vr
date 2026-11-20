/* Direct production observer/normal-room adapter + original bp_call/get/fail.
   Managed invocation/I/O are substitutes; no native Unity cause is simulated. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef struct Matrix4x4 { float m[16]; } Matrix4x4;
#define TELEMETRY_FORMAT_ERROR "\"telemetry_format_error\":true"
static SRWLOCK log_lock=SRWLOCK_INIT;
static volatile LONG telemetry_ready=1;
static struct { unsigned long long attempted,dropped;DWORD bytes[3],records[3]; } telemetry_stats;
static BOOL telemetry_emit_locked(const char*,const char*,const char*,unsigned);
#define BP_FOV_DIAGNOSTIC 1
static bool fov_extra;
static int bp_fov_json(char*,size_t);
#define BP_BACKGROUND_POLICY 1
static int bp_background_json(char*,size_t);
#define BP_PROJECTION_OBSERVE_FIXTURE
#define BP_BINDING_PREFLIGHT_TEST_MAIN preflight_fixture_main
#include "binding_preflight_test.c"
static int bp_fov_json(char* out,size_t capacity) {
 if(fov_extra) {
  /* Worst numeric widths for the production footer fragment. */
  return snprintf(out,capacity,",\"fov\":{\"id\":[18446744073709551615,18446744073709551615,-2147483648,18446744073709551615,18446744073709551615],\"preinit\":\"NOT_SAMPLED\",\"o\":[3,255,255],\"w\":3,\"r\":[3,255,255],\"p\":[3,-2147483648,255,255],\"e\":[3,-2147483648,255,255]}");
 }
 return 0;
}
static int bp_background_json(char* out,size_t capacity) {
 return fov_extra?snprintf(out,capacity,",\"background\":[4,2,\"3DCCCCCD3E4CCCCD4060000000000000\",3,3]"):0;
}

static char projection_records[20][1600],projection_events[20][40];
static unsigned projection_kept,writer_calls,writer_drop_at,reads[2],frame_reads,read_mode[2];
static Il2CppObject pr_camera[2],pr_box[2],pr_frame;
static Matrix4x4 pr_actual[2];
static bool fail_unbox,check_safety,safety_violation;
static const char* safety_error;
enum { PR_OK,PR_THROW,PR_NULL,PR_WRONG,PR_UNBOX };
static BOOL telemetry_emit_locked(const char* evidence,const char* event,const char* detail,unsigned cls) {
 (void)evidence;++writer_calls;
 if(cls || strlen(detail)>=1500 || projection_kept>=20) abort();
 if(check_safety && (!bp.failed || !bp.c.retiring || !bp.c.quarantined || !bp.error || strcmp(bp.error,safety_error))) safety_violation=true;
 if(writer_calls==writer_drop_at) { ++telemetry_stats.dropped;return FALSE; }
 strcpy(projection_records[projection_kept],detail);strcpy(projection_events[projection_kept],event);++projection_kept;
 telemetry_stats.bytes[0]+=(DWORD)strlen(detail)+384u;++telemetry_stats.records[0];return TRUE;
}
static Il2CppObject* pr_invoke(const MethodInfo* m,void* receiver,void** args,Il2CppException** exc) {
 (void)args;*exc=NULL;
 if(m->id==BM_FRAME) { ++frame_reads;return &pr_frame; }
 if(m->id!=BM_PROJECTION_GET) { *exc=&exception;return NULL; }
 int eye=receiver==&pr_camera[0]?0:receiver==&pr_camera[1]?1:-1;
 if(eye<0) { *exc=&exception;return NULL; }
 ++reads[eye];fail_unbox=read_mode[eye]==PR_UNBOX;
 switch(read_mode[eye]) {
 case PR_THROW:*exc=&exception;return NULL;
 case PR_NULL:return NULL;
 case PR_WRONG:return &pr_frame;
 default:return &pr_box[eye];
 }
}
static void* pr_unbox(Il2CppObject* o) {
 if(o==&pr_box[0] || o==&pr_box[1]) return fail_unbox?NULL:(void*)&pr_actual[o==&pr_box[1]?1:0];
 return &o->value;
}
static void pr_reset(void) {
 pf_reset();memset(&bp_projection,0,sizeof(bp_projection));memset(&telemetry_stats,0,sizeof(telemetry_stats));
 memset(projection_records,0,sizeof(projection_records));memset(projection_events,0,sizeof(projection_events));
 memset(reads,0,sizeof(reads));memset(read_mode,0,sizeof(read_mode));
 projection_kept=writer_calls=writer_drop_at=frame_reads=0;fail_unbox=check_safety=safety_violation=false;
 telemetry_ready=1;bp.owner_thread=GetCurrentThreadId();bp.api.runtime_invoke=pr_invoke;bp.api.object_unbox=pr_unbox;
 bp.api.class_get_namespace=f_namespace;bp.method[BM_PROJECTION_GET]=&owned[BM_PROJECTION_GET];bp.method[BM_FRAME]=&owned[BM_FRAME];
 bp.c.phase=BP_EXCLUDE;bp.c.initialized=bp.c.enabled=3;
 bp.c.identity=(BpIdentity){.generation=1,.source=4294919712u,.scene=9,.pose=1,.config=1};
 exception_class.name="Quoted\"Exception\\Name";
 for(unsigned eye=0;eye<2;++eye) {
  pr_camera[eye]=(Il2CppObject){.klass=bp.types[BT_CAMERA]};bp.eye[eye].camera=bp_root(&pr_camera[eye]);
  pr_box[eye]=(Il2CppObject){.klass=bp.types[BT_M4]};
  /* Deliberately asymmetric independent input, not a copy of projection math. */
  for(unsigned i=0;i<16;++i) bp.eye[eye].projection.m[i]=(float)(i*3+eye+1)/17.0f;
  bp.eye[eye].projection.m[10]=1.5f;
  pr_actual[eye]=bp.eye[eye].projection;
 }
 pr_frame=(Il2CppObject){.klass=bp.types[BT_INT],.value=70};bp.stage="unchanged_stage";bp.error=NULL;
}
static void pr_dump(const char* label) {
 for(unsigned i=0;i<projection_kept;++i) printf("JSON {\"case\":\"%s\",\"event\":\"%s\",%s}\n",label,projection_events[i],projection_records[i]);
}
static void pr_all_before(void) {
 bp_projection_checkpoint(BP_PR_SET,0,-1);bp_projection_checkpoint(BP_PR_SET,1,-1);
 bp_projection_pair(BP_PR_INIT,-1);bp_projection_pair(BP_PR_PRE,68);
 bp_projection_checkpoint(BP_PR_EACH,0,-1);bp_projection_checkpoint(BP_PR_EACH,1,-1);
 bp_projection_pair(BP_PR_PAIR,69);
}
static bool pr_original_read(unsigned eye) {
 Matrix4x4 projection;BpEye* e=&bp.eye[eye];bp_projection_arm(eye,72);
 bool ok=bp_get(BM_PROJECTION_GET,bp_target(e->camera),&projection,sizeof(projection));bp_projection_disarm();
 if(!ok) return false;
 /* Runner extracts this exact unchanged product predicate from the source. */
#include "projection_product_predicate.inc"
 return !bp.failed;
}
static int pr_end(const char* label) {
 CHECK(bp_projection.records<=BP_PR_RECORD_LIMIT && bp_projection.bytes<=BP_PR_BYTE_LIMIT);
 CHECK(telemetry_stats.bytes[0]<=40960u-BP_PR_NORMAL_BYTE_RESERVE);
 CHECK(telemetry_stats.records[0]<=160u-BP_PR_NORMAL_RECORD_RESERVE);
 CHECK(!safety_violation);pr_dump(label);++cases;return 0;
}
#ifndef BP_PROJECTION_TEST_MAIN
#define BP_PROJECTION_TEST_MAIN main
#endif
int BP_PROJECTION_TEST_MAIN(void) {
 pr_reset();pr_all_before();CHECK(pr_original_read(0) && pr_original_read(1));bp_projection_finish_capture();
 CHECK(reads[0]==6 && reads[1]==6 && frame_reads==5);CHECK(!bp.failed && !strcmp(bp.stage,"get_projectionMatrix"));
 CHECK(projection_kept==8 && !bp_projection.budget_dropped && !bp_projection.format_dropped && !bp_projection.writer_dropped);
 CHECK(bp_projection.sample[BP_PR_CAPTURE][0].actual.m[8]==pr_actual[0].m[8]);
 unsigned calls=writer_calls;bp_projection_pair(BP_PR_INIT,-1);bp_projection_flush();CHECK(writer_calls==calls && reads[0]==6);
 if(pr_end("equal")) return 1;
 pr_reset();fov_extra=true;pr_all_before();CHECK(pr_original_read(0) && pr_original_read(1));bp_projection_finish_capture();
 CHECK(projection_kept==8 && !bp_projection.budget_dropped && !bp_projection.writer_dropped);
 if(pr_end("fov_shared_budget")) return 1;
 fov_extra=false;

 /* Full raw matrix and max/index support many descriptions; none establishes
    an actual engine cause. Classification labels here are synthetic only. */
 for(unsigned mode=0;mode<8;++mode) {
  pr_reset();pr_all_before();
  switch(mode) {
  case 0:pr_actual[0].m[8]+=0.25f;break;
  case 1:{ Matrix4x4 old=pr_actual[0];for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c) pr_actual[0].m[c*4+r]=old.m[r*4+c];break; }
  case 2:pr_actual[0].m[5]*=-1;break;
  case 3:pr_actual[0].m[10]+=0.5f;pr_actual[0].m[14]-=0.25f;break;
  case 4:pr_actual[0].m[7]+=0.00001f;break;
  case 5:pr_actual[0].m[7]+=0.0002f;break;
  case 6:{ uint32_t u=0x80000000;memcpy(&pr_actual[0].m[3],&u,4);bp.eye[0].projection.m[3]=0;break; }
  default:pr_actual[1].m[0]+=0.125f;break;
  }
  bool pass=pr_original_read(mode==7?1:0);CHECK(pass==(mode==4 || mode==6));
  bp_projection_finish_capture();unsigned eye=mode==7?1:0;BpProjectionSample* s=&bp_projection.sample[BP_PR_CAPTURE][eye];
  unsigned raw,bad;int index;uint64_t max;bp_projection_delta(s,&raw,&bad,&index,&max);
  CHECK(raw!=0 && (bad!=0)==!pass);CHECK(index>=0 && max<=0x7fefffffffffffffULL);
  if(mode==0) CHECK(index==8 && max==0x3fd0000000000000ULL);
  if(mode==3) CHECK(index==10 && max==0x3fe0000000000000ULL);
  if(mode==6) CHECK(index==0 && max==0 && raw==(1u<<3) && bad==0);
  char label[32];snprintf(label,sizeof(label),"synthetic_%u",mode);if(pr_end(label)) return 1;
 }
 for(unsigned mode=0;mode<4;++mode) {
  pr_reset();const uint32_t nonfinite[]={0x7fc12345,0x7f800000,0xff800000,0x7f800001};
  memcpy(&pr_actual[0].m[9],&nonfinite[mode],4);bp_projection_checkpoint(BP_PR_SET,0,-1);
  CHECK(!pr_original_read(0));CHECK(bp.failed && bp.c.quarantined);
  BpProjectionSample* s=&bp_projection.sample[BP_PR_CAPTURE][0];unsigned raw,bad;int index;uint64_t max;
  bp_projection_delta(s,&raw,&bad,&index,&max);CHECK(bad==(1u<<9) && index==-1 && bp_projection_nonfinite(&s->actual)==(1u<<9));
  CHECK(bp_projection_bits(s->actual.m[9])==nonfinite[mode]);
  if(pr_end("nonfinite")) return 1;
 }
 for(unsigned mode=PR_THROW;mode<=PR_UNBOX;++mode) {
  pr_reset();read_mode[0]=mode;bp_projection_checkpoint(BP_PR_SET,0,-1);
  CHECK(!bp.failed && !bp.c.retiring && !bp.error && !strcmp(bp.stage,"unchanged_stage"));
  CHECK(bp_projection.sample[BP_PR_SET][0].status==(mode==PR_THROW?BP_PR_EXCEPTION:BP_PR_BOX));
  safety_error=mode==PR_THROW?"managed_exception_or_aot_unavailable":"boxed_result_contract";check_safety=true;
  CHECK(!pr_original_read(0));CHECK(bp.failed && !strcmp(bp.error,safety_error));
  CHECK(log_count==1 && unpublish_count==1 && bp.c.quarantined && bp.c.retiring);
  CHECK(bp_projection.sample[BP_PR_CAPTURE][0].status==(mode==PR_THROW?BP_PR_EXCEPTION:BP_PR_BOX));
  if(pr_end("read_failure")) return 1;
 }
 pr_reset();bp_projection_checkpoint(BP_PR_SET,0,-1);pr_actual[0].m[8]+=0.5f;
 bp_projection_checkpoint(BP_PR_SET,1,-1);bp_projection_pair(BP_PR_INIT,-1);bp_projection_flush();
 CHECK(bp_projection.sample[BP_PR_SET][0].actual.m[8]!=bp_projection.sample[BP_PR_INIT][0].actual.m[8]);
 if(pr_end("synthetic_right_init_left_change")) return 1;

 pr_reset();bp.eye[0].camera=0;bp_projection_checkpoint(BP_PR_SET,0,-1);
 CHECK(!bp.failed && !reads[0] && bp_projection.sample[BP_PR_SET][0].status==BP_PR_ROOT);bp_projection_flush();
 if(pr_end("root_unknown")) return 1;
 pr_reset();attached=false;bp_projection_pair(BP_PR_INIT,-1);CHECK(!bp.failed && !reads[0] && !reads[1] && !frame_reads);
 CHECK(bp_projection.sample[BP_PR_INIT][0].status==BP_PR_OWNER);bp_projection_flush();if(pr_end("owner_unknown")) return 1;
 pr_reset();bp.method[BM_PROJECTION_GET]=NULL;bp.method[BM_FRAME]=NULL;bp_projection_checkpoint(BP_PR_SET,0,-1);
 CHECK(!bp.failed && bp_projection.sample[BP_PR_SET][0].status==BP_PR_UNAVAILABLE);bp_projection_flush();if(pr_end("missing_unknown")) return 1;
 pr_reset();pr_all_before();writer_drop_at=3;bp_projection_flush();CHECK(bp_projection.writer_dropped==1);if(pr_end("writer_drop")) return 1;
 pr_reset();pr_all_before();telemetry_stats.bytes[0]=40960u-BP_PR_NORMAL_BYTE_RESERVE;bp_projection_flush();
 CHECK(!projection_kept && !writer_calls && bp_projection.writer_dropped==7);if(pr_end("byte_room")) return 1;
 pr_reset();pr_all_before();telemetry_stats.records[0]=160u-BP_PR_NORMAL_RECORD_RESERVE;bp_projection_flush();
 CHECK(!projection_kept && !writer_calls && bp_projection.writer_dropped==7);if(pr_end("record_room")) return 1;
 pr_reset();pr_all_before();telemetry_ready=0;bp_projection_flush();CHECK(!writer_calls && bp_projection.writer_dropped==7);if(pr_end("writer_not_ready")) return 1;
 pr_reset();pr_all_before();format_failure=true;bp_projection_flush();CHECK(!writer_calls && bp_projection.format_dropped==6);format_failure=false;if(pr_end("format_unknown")) return 1;
 pr_reset();pr_all_before();bp_projection.bytes=BP_PR_BYTE_LIMIT-1024;bp_projection_flush();
 CHECK(bp_projection.budget_dropped==6 && projection_kept==1);if(pr_end("budget_footer")) return 1;
 pr_reset();bp_projection_flush();CHECK(!writer_calls && !bp_projection.flushed);++cases;
 pr_reset();CHECK(!bp_projection_normal_record("base_projection_samples","\"a\":nan"));
 CHECK(!bp_projection_normal_record("base_projection_samples",TELEMETRY_FORMAT_ERROR));
 CHECK(!bp_projection_normal_record("base_projection_samples","\"a\":\"\xff\""));
 CHECK(!writer_calls && telemetry_stats.dropped==3 && !bp.failed);++cases;
 pr_reset();pr_all_before();pr_frame.value=-1;bp_projection_arm(0,72);bp_projection_disarm();
 bp_projection_checkpoint(BP_PR_CAPTURE,0,-1);CHECK(bp_projection.sample[BP_PR_CAPTURE][0].frame_kind==BP_PR_FRAME_UNKNOWN);
 bp_projection_flush();if(pr_end("frame_unknown")) return 1;
 pr_reset();bp_projection_checkpoint(BP_PR_SET,0,-1);bp.eye[0].projection.m[8]+=0.25f;pr_actual[0]=bp.eye[0].projection;
 CHECK(pr_original_read(0));bp_projection_finish_capture();
 CHECK(strstr(projection_records[2],"\"x\":\"")!=NULL);if(pr_end("expected_changed")) return 1;
 printf("PASS projection observer: %u cases / %u assertions\n",cases,assertions);return 0;
}
