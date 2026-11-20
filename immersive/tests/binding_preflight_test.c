/* Execute the production binding driver + diagnostic metadata path + JSON
   formatter. Reused ownership/request test mains are compiled, not executed. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
static bool format_failure;
static int telemetry_snprintf(char* out,size_t capacity,const char* format,...) {
 if(format_failure) { if(capacity) out[0]=0;return -1; }
 va_list ap; va_start(ap,format); int n=vsnprintf(out,capacity,format,ap); va_end(ap); return n;
}
/* Existing telemetry escaping is unchanged; this seam exercises ASCII and
   escaped metadata text in the actual new producer formatter. */
#ifdef BP_PROJECTION_OBSERVE_FIXTURE
/* Exact existing parser/escape functions extracted by the focused runner. */
#include "projection_telemetry_json.inc"
#else
static void json_escape(const char* in,char* out,size_t cap) {
 size_t n=0; if(!in) in="";
 for(;*in && n+7<cap;++in) {
  unsigned char c=(unsigned char)*in;
  if(c=='"' || c=='\\') { out[n++]='\\';out[n++]=(char)c; }
  else if(c<32) { int k=snprintf(out+n,cap-n,"\\u%04x",c);n+=(size_t)k; }
  else out[n++]=(char)c;
 }
 out[n]=0;
}
#endif
static void write_record(const char*,const char*,const char*);
#define BP_BINDING_LOG_FIXTURE
#define BP_OWNERSHIP_TEST_MAIN ownership_fixture_main
#include "base_ownership_test.c"
static Il2CppClass pf_classes[BT_COUNT];
static MethodInfo pf_extra[8];
static unsigned pf_caps,pf_scans[BT_COUNT],pf_method_reads,pf_invocations,record_calls,record_kept,record_bytes;
static bool drop_records;
static char records[BP_PREFLIGHT_RECORD_LIMIT][1800],events[BP_PREFLIGHT_RECORD_LIMIT][40];
static void write_record(const char* evidence,const char* event,const char* detail) {
 (void)evidence; ++record_calls;
 if(drop_records) return;
 if(record_kept>=BP_PREFLIGHT_RECORD_LIMIT || strlen(detail)>=sizeof(records[0])) { fprintf(stderr,"unbounded record\n"); abort(); }
 strcpy(records[record_kept],detail); strcpy(events[record_kept],event); ++record_kept;
 record_bytes+=(unsigned)strlen(detail)+384u;
}
static int expected_type_code(unsigned type) {
 switch(type) {
 case BT_VOID:return IL2CPP_TYPE_VOID;
 case BT_BOOL:return IL2CPP_TYPE_BOOLEAN;
 case BT_INT:return IL2CPP_TYPE_I4;
 case BT_FLOAT:return IL2CPP_TYPE_R4;
 case BT_STRING:return IL2CPP_TYPE_STRING;
 case BT_INTPTR:return IL2CPP_TYPE_I;
 case BT_V3:case BT_Q4:case BT_M4:case BT_RT_FORMAT:case BT_RENDER_TYPE:
 case BT_STEREO_MASK:case BT_CLEAR_FLAGS:case BT_GRAPHICS_API:case BT_COLOR:return IL2CPP_TYPE_VALUETYPE;
 case BT_LIST:return IL2CPP_TYPE_GENERICINST;
 default:return IL2CPP_TYPE_CLASS;
 }
}
static const char* pf_namespace(Il2CppClass* c) {
 return c==&pf_classes[BT_LIST]?"System.Collections.Generic":"UnityEngine";
}
static const MethodInfo* pf_methods(Il2CppClass* c,void** it) {
 unsigned type=(unsigned)(c-pf_classes); if(type>=BT_COUNT) return NULL;
 if(!*it) ++pf_scans[type]; ++pf_method_reads;
 if(pf_caps && type==(pf_caps==2?BT_CAMERA:BT_ADDITIONAL)) {
  uintptr_t n=(uintptr_t)*it; if(n>=513) return NULL; *it=(void*)(n+1);
  return n==0?&owned[pf_caps==2?BM_STEREO_ENABLED:BM_XR_SET]:&extra;
 }
 return f_methods(c,it);
}
static Il2CppObject* pf_invoke(const MethodInfo* m,void* o,void** args,Il2CppException** exc) {
 ++pf_invocations;return invoke_zero(m,o,args,exc);
}
static Il2CppObject* pf_convert(const MethodInfo* m,void* o,Il2CppObject** args,int argc,Il2CppException** exc) {
 ++pf_invocations;return invoke_owned(m,o,args,argc,exc);
}
static void pf_reset(void) {
 ownership_reset(); memset(pf_classes,0,sizeof(pf_classes)); memset(bp.method,0,sizeof(bp.method));
 memset(pf_scans,0,sizeof(pf_scans)); memset(records,0,sizeof(records)); memset(events,0,sizeof(events));
 pf_caps=pf_method_reads=pf_invocations=record_calls=record_kept=record_bytes=0;
 drop_records=format_failure=false;bp.bound=false;
 const char* names[BT_COUNT]={"Void","Boolean","Int32","Single","String","Type","IntPtr",
  "Object","GameObject","Component","Behaviour","Camera","Texture","RenderTexture","Transform",
  "Vector3","Quaternion","Matrix4x4","UniversalAdditionalCameraData","CameraData","RenderTextureFormat",
  "CameraRenderType","StereoTargetEyeMask","CameraClearFlags","Time","SystemInfo","GraphicsDeviceType","List`1","Color"};
 for(unsigned type=0;type<BT_COUNT;++type) {
  pf_classes[type]=(Il2CppClass){.name=names[type]};
  pf_classes[type].type=(Il2CppType){.code=expected_type_code(type),.klass=&pf_classes[type]};
  bp.types[type]=&pf_classes[type];
 }
 for(unsigned id=0;id<BM_COUNT;++id) {
  const BpMethodSpec* s=&bp_specs[id]; Il2CppClass* owner=&pf_classes[s->owner];
  owned[id]=(MethodInfo){.name=s->name,.id=id,.owner=owner,.ret=&pf_classes[s->ret].type,
   .argc=s->argc,.flags=METHOD_ATTRIBUTE_PUBLIC|(s->is_static?METHOD_ATTRIBUTE_STATIC:0)};
  for(unsigned arg=0;arg<s->argc;++arg) owned[id].params[arg]=&pf_classes[s->arg[arg]].type;
  owner->methods[owner->count++]=&owned[id];
 }
 extra=owned[BM_XR_SET];extra.name="Other";
 bp.api.class_get_methods=pf_methods;bp.api.class_get_namespace=pf_namespace;
 bp.api.runtime_invoke=pf_invoke;bp.api.runtime_invoke_convert_args=pf_convert;
}
static int pf_end(const char* label) {
 CHECK(!allocations && !ctor_calls[0] && !ctor_calls[1] && !root_calls && !make_calls && !submit_calls && !pf_invocations);
 CHECK(record_calls<=BP_PREFLIGHT_RECORD_LIMIT && record_bytes<=BP_PREFLIGHT_BYTE_LIMIT);
 if(record_kept) {
  CHECK(!strcmp(events[0],"base_binding_header"));
  CHECK(!strcmp(events[record_kept-1],"base_binding_footer"));
  for(unsigned i=0;i<record_kept;++i) printf("RECORD {\"event\":\"%s\",\"detail\":{%s}}\n",events[i],records[i]);
 }
 ++cases;printf("PASS %s\n",label);return 0;
}
static int xr_set_case(unsigned mode) {
 pf_reset();Il2CppType wrong=pf_classes[BT_BOOL].type;
 switch(mode) {
 case 0:break;
 case 1:wrong.code=IL2CPP_TYPE_VALUETYPE;owned[BM_XR_SET].params[0]=&wrong;break;
 case 2:wrong.klass=&pf_classes[BT_INT];owned[BM_XR_SET].params[0]=&wrong;break;
 case 3:owned[BM_XR_SET].owner=&pf_classes[BT_RT];break;
 case 4:owned[BM_XR_SET].name="OtherName";break;
 case 5:pf_extra[0]=owned[BM_XR_SET];pf_classes[BT_ADDITIONAL].methods[pf_classes[BT_ADDITIONAL].count++]=&pf_extra[0];break;
 default:pf_caps=1;break;
 }
 bool ok=bp_bind_required(&bp_binding_preflight); const BpBindingRow* r=&bp_binding_preflight.row[BM_XR_SET];
 CHECK(ok==(mode==0));CHECK(bp_binding_preflight.complete && bp_binding_preflight.observed==BM_COUNT);
 unsigned owner_specs=0;for(unsigned id=0;id<BM_COUNT;++id) owner_specs+=bp_specs[id].owner==BT_ADDITIONAL;
 CHECK(pf_scans[BT_ADDITIONAL]==owner_specs); /* Prefix is not re-enumerated. */
 unsigned scans=0;for(unsigned type=0;type<BT_COUNT;++type) scans+=pf_scans[type];
 CHECK(scans==BM_COUNT); /* Original prefix + only the unvisited suffix. */
 if(mode!=5) CHECK(strstr(records[record_kept-1],"\"rows_submitted\":53") && strstr(records[record_kept-1],"\"rows_omitted\":0"));
 CHECK(strstr(records[1],"set_allowXRRendering") && strstr(records[2],"get_allowXRRendering") && strstr(records[3],"get_stereoEnabled"));
 if(!mode) {
  CHECK(r->selected && r->complete && r->valid==1 && !r->detail_kept && !bp.failed);
  for(unsigned id=0;id<BM_COUNT;++id) CHECK(bp.method[id]==&owned[id]);
 } else {
  CHECK(bp.failed && bp_binding_preflight.first_failure==BM_XR_SET && !strcmp(bp.stage,"set_allowXRRendering"));
  CHECK(!bp.method[BM_XR_SET] && bp_binding_preflight.row[BM_BACKEND].observed);
  for(unsigned id=BM_XR_SET+1;id<BM_COUNT;++id) CHECK(!bp.method[id]);
  CHECK(!bp_prepare_empty_pair() && !allocations);
  if(mode==1) CHECK(bp_binding_preflight.detail[0].lookup.candidate[0].parameter.code==IL2CPP_TYPE_VALUETYPE &&
   bp_binding_preflight.detail[0].lookup.candidate[0].rejection==22);
  if(mode==2) CHECK(bp_binding_preflight.detail[0].lookup.candidate[0].rejection==23 &&
   !bp_binding_preflight.detail[0].lookup.candidate[0].parameter.klass.exact);
  if(mode==3) CHECK(bp_binding_preflight.detail[0].lookup.candidate[0].rejection==1 &&
   !bp_binding_preflight.detail[0].lookup.candidate[0].owner.exact);
  if(mode==4) CHECK(!r->named && !strcmp(r->result,"name_absent"));
  if(mode==5) CHECK(r->valid==2 && !r->complete && !strcmp(r->result,"ambiguous"));
  if(mode==6) CHECK(r->capped && !r->complete && !strcmp(r->result,"incomplete"));
 }
 char label[80];snprintf(label,sizeof(label),"xr_set_spec_mode%u_actual_driver_and_diagnostics",mode);return pf_end(label);
}
#ifndef BP_BINDING_PREFLIGHT_TEST_MAIN
#define BP_BINDING_PREFLIGHT_TEST_MAIN main
#endif
int BP_BINDING_PREFLIGHT_TEST_MAIN(void) {
 cases=assertions=0;
 CHECK(IL2CPP_TYPE_I==0x18 && IL2CPP_TYPE_VALUETYPE==0x11);
 CHECK(bp_specs[BM_XR_SET].owner==BT_ADDITIONAL && bp_specs[BM_XR_SET].ret==BT_VOID && bp_specs[BM_XR_SET].argc==1 && bp_specs[BM_XR_SET].arg[0]==BT_BOOL);
 for(unsigned mode=0;mode<7;++mode) CHECK(!xr_set_case(mode));
 for(unsigned id=BM_CLEAR_GET;id<=BM_BG_SET;++id) for(unsigned mode=0;mode<2;++mode) {
  pf_reset();Il2CppType wrong=pf_classes[id==BM_CLEAR_GET?BT_CLEAR_FLAGS:BT_COLOR].type;
  if(mode==0) wrong.byref=true;else wrong.klass=&pf_classes[BT_M4];
  if(id==BM_BG_SET) owned[id].params[0]=&wrong;else owned[id].ret=&wrong;
  CHECK(!bp_bind_required(&bp_binding_preflight) && bp_binding_preflight.first_failure==id);
  CHECK(!pf_end("background_exact_type_rejects_byref_or_other_struct"));
 }
 pf_reset();owned[BM_NAME].flags=METHOD_ATTRIBUTE_PRIVATE;owned[BM_XR_SET].flags=METHOD_ATTRIBUTE_PRIVATE;
 owned[BM_TARGET_SET].flags=METHOD_ATTRIBUTE_PRIVATE;owned[BM_TARGET_GET].flags=METHOD_ATTRIBUTE_PRIVATE;
 CHECK(!bp_bind_required(&bp_binding_preflight));
 CHECK(bp_binding_preflight.first_failure==BM_NAME && !strcmp(bp.stage,"set_name"));
 CHECK(bp_binding_preflight.failed==4 && bp_binding_preflight.details_omitted==3);
 CHECK(bp_binding_preflight.detail[0].used && bp_binding_preflight.detail[0].id==BM_XR_SET);
 CHECK(bp_binding_preflight.row[BM_COMPARE_TAG].selected && !bp_binding_preflight.row[BM_COMPARE_TAG].detail_kept);
 for(unsigned i=0;i<record_kept;++i) if(!strcmp(events[i],"base_binding_candidate")) {
  CHECK(strstr(records[i],"\"spec\":18,")!=NULL);break;
 }
 CHECK(!pf_end("first_failure_fixed_later_summaries_and_native_detail_priority"));

 for(unsigned observation=0;observation<2;++observation) {
  pf_reset();owned[BM_XR_SET].flags=METHOD_ATTRIBUTE_PRIVATE;drop_records=true;
  CHECK(!bp_bind_required(observation?&bp_binding_preflight:NULL));
  CHECK(bp.failed && !bp.bound && !strcmp(bp.stage,"set_allowXRRendering"));
  CHECK(!strcmp(bp.error,"owned_exact_method_missing_or_ambiguous"));
  for(unsigned id=0;id<BM_COUNT;++id) CHECK(bp.method[id]==(id<BM_XR_SET?&owned[id]:NULL));
  CHECK(!bp_prepare_empty_pair() && !allocations && !ctor_calls[0]);
  if(observation) CHECK(bp_binding_preflight.observed==BM_COUNT && !record_kept && record_calls>0);
  else CHECK(!record_calls);
  CHECK(!pf_end(observation?"observation_on_dropped_logs_same_first_failure_allocation0":"observation_off_same_first_failure_allocation0"));
 }
 pf_reset();bp.types[BT_BOOL]=NULL;bp.stage="System.Boolean";bp_fail("owned_required_type");
 bp_observe_partial_binding(&bp_binding_preflight);
 CHECK(bp_binding_preflight.blocked_before_methods && bp_binding_preflight.row[BM_XR_SET].observed);
 CHECK(!bp_binding_preflight.row[BM_XR_SET].available && !strcmp(bp_binding_preflight.row[BM_XR_SET].result,"not_available"));
 CHECK(!pf_scans[BT_TEXTURE] && !strcmp(bp.stage,"System.Boolean"));
 unsigned reads=pf_method_reads,calls=record_calls;bp_observe_partial_binding(&bp_binding_preflight);
 CHECK(reads==pf_method_reads && calls==record_calls);
 for(unsigned id=0;id<BM_COUNT;++id) CHECK(!bp.method[id]);
 CHECK(!pf_end("missing_type_no_resolve_no_partial_commit_attempt_once"));
 pf_reset();bp.api.class_get_methods=NULL;bp.stage="il2cpp_class_get_methods";bp_fail("owned_exports");
 bp_observe_partial_binding(&bp_binding_preflight);
 CHECK(bp_binding_preflight.unavailable==BM_COUNT && !pf_method_reads);
 CHECK(!pf_end("metadata_unavailable_no_unsafe_query"));
 pf_reset();owned[BM_XR_SET].flags=METHOD_ATTRIBUTE_PRIVATE;format_failure=true;
 CHECK(!bp_bind_required(&bp_binding_preflight) && !record_kept && !record_calls);
 CHECK(bp.failed && !strcmp(bp.stage,"set_allowXRRendering") && bp_binding_preflight.observed==BM_COUNT);
 CHECK(!pf_end("format_failure_never_changes_production_selection"));

 pf_reset();
 const unsigned ids[]={BM_GO_CTOR,BM_ACTIVE_SET,BM_XR_SET};
 for(unsigned k=0;k<3;++k) {
  unsigned id=ids[k];owned[id].flags=METHOD_ATTRIBUTE_PRIVATE;Il2CppClass* c=owned[id].owner;
  for(unsigned n=0;n<2;++n) {pf_extra[k*2+n]=owned[id];c->methods[c->count++]=&pf_extra[k*2+n];}
 }
 CHECK(!bp_bind_required(&bp_binding_preflight));
 CHECK(bp_binding_preflight.detail[0].lookup.captured==3 && bp_binding_preflight.first_failure==BM_GO_CTOR);
 CHECK(strstr(records[record_kept-1],"\"omission_is_not_absence\":true")!=NULL);
 CHECK(record_bytes<=BP_PREFLIGHT_BYTE_LIMIT && !pf_end("bounded_multi_candidate_record_byte_budget"));

 pf_reset();owned[BM_XR_SET].flags=METHOD_ATTRIBUTE_PRIVATE;CHECK(!bp_bind_required(&bp_binding_preflight));
 memset(records,0,sizeof(records));memset(events,0,sizeof(events));record_calls=record_kept=record_bytes=0;
 for(unsigned id=0;id<BM_COUNT;++id) {
  memset(bp_binding_preflight.row[id].owner.ns,1,sizeof(bp_binding_preflight.row[id].owner.ns)-1);
  memset(bp_binding_preflight.row[id].owner.name,2,sizeof(bp_binding_preflight.row[id].owner.name)-1);
 }
 bp_emit_preflight(&bp_binding_preflight);
 CHECK(record_bytes<=BP_PREFLIGHT_BYTE_LIMIT);
 CHECK(strstr(records[record_kept-1],"\"budget_skipped\":0")==NULL || strstr(records[record_kept-1],"\"rows_omitted\":0")==NULL);
 CHECK(!pf_end("escaped_metadata_excess_omissions_explicit_never_absence"));
 printf("RESULT cases=%u assertions=%u production_binding_driver=YES metadata_only=YES native_target=UNVERIFIED full_suite=NOT_RUN specs=%u\n",cases,assertions,(unsigned)BM_COUNT);
 return 0;
}


