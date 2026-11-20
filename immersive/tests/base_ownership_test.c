/* Reuse only exported-API substitutes; the old request suite is not run.
   This fixture executes the production owned invocation/preflight/disposition
   adapter. Unity native ctor/CompareTag/Destroy behavior remains unverified. */
#define main request_fixture_main
#include "request_dispatch_test.c"
#undef main
#include ".tmp/umamusume-localify/src/il2cpp/il2cpp-blob.h"
#include ".tmp/umamusume-localify/src/il2cpp/il2cpp-tabledefs.h"
#include "../src/base_producer_state.h"
#include "../src/base_producer_lookup.h"
#include "../src/base_binding_preflight.h"
#include "../src/base_projection_types.h"
typedef Il2CppObject Il2CppString;
typedef struct BpEye { uint32_t go,camera,additional,data,rt;
#ifdef BP_PROJECTION_OBSERVE_FIXTURE
 Matrix4x4 projection;
#endif
} BpEye;
/* The production functions use this exact field contract. This small fixture
   owner omits unrelated D3D11/pose fields, not managed object-layout fields. */
static struct {
 BpControl c; RdApi api; Il2CppClass* types[BT_COUNT]; const MethodInfo* method[BM_COUNT];
 Il2CppObject* (*object_new)(Il2CppClass*); Il2CppString* (*string_new)(const char*);
 uint32_t roots[24]; unsigned root_count; BpEye eye[2]; BpEmpty empty; uint32_t tag_root,source_root;
 DWORD owner_thread; const char *stage,*error; bool failed,bound,disable_done;
#ifdef BP_NATIVE_MONO_FIXTURE
 ID3D11Texture2D* native[2]; bool backend_verified;
#endif
} bp;
static unsigned log_count,unpublish_count;
static void bp_log(const char* event,const char* reason) { (void)event; (void)reason; ++log_count; }
static void bp_unpublish(void) { ++unpublish_count; }
#ifdef BP_BINDING_LOG_FIXTURE
#include "../src/base_binding_preflight_log.h"
#else
static void bp_emit_preflight(const BpPreflight* p) { (void)p; }
#endif
#ifdef BP_PROJECTION_OBSERVE_FIXTURE
#include "../src/base_projection_normal_log.h"
#include "../src/base_projection_observe.h"
#else
static inline void bp_projection_flush(void) { }
static inline void bp_projection_tap_exception(unsigned id,Il2CppException* exc) { (void)id;(void)exc; }
static inline void bp_projection_tap_box(unsigned id) { (void)id; }
static inline void bp_projection_tap_value(unsigned id,const void* value,size_t size) { (void)id;(void)value;(void)size; }
#endif
#include "../src/base_producer_ownership.h"

enum { TAG_TRUE, TAG_FALSE, TAG_EXCEPTION, TAG_NULL, TAG_WRONG_BOX, TAG_UNBOX_FAIL };
static Il2CppClass go_class,string_class,component_class,camera_class;
static Il2CppObject gos[2],strings[8],bool_boxes[2],bad_box,component,reflection;
static MethodInfo owned[BM_COUNT],extra;
static unsigned allocations,string_allocations,compare_calls[2],destroys[2],adds,ctor_calls[2];
static unsigned tag_mode[2],ctor_failure,active_failure,active_exception,destroy_failure,method_limit;
static bool native_alive[2],active[2],order_violation,root_violation,string_failure;
static bool is_rooted(Il2CppObject* o) {
 for(unsigned i=1;i<64;++i) if(roots[i]==o) return true;
 return false;
}
static int eye_of(void* receiver) { return receiver==&gos[0]?0:receiver==&gos[1]?1:-1; }
static Il2CppObject* allocate_go(Il2CppClass* c) {
 if(c!=&go_class || allocations>=2) return NULL;
 unsigned eye=allocations++; gos[eye]=(Il2CppObject){.klass=c}; native_alive[eye]=active[eye]=true;
 return &gos[eye];
}
static Il2CppString* allocate_string(const char* s) {
 if(string_failure || string_allocations>=8) return NULL;
 Il2CppObject* o=&strings[string_allocations++];
 *o=(Il2CppObject){.klass=&string_class,.value=!strcmp(s,"Untagged")?42:7}; return o;
}
static Il2CppObject* box_bool(Il2CppClass* c,void* value) {
 return c==&boolean?&bool_boxes[*(uint8_t*)value?1:0]:NULL;
}
static void* unbox_owned(Il2CppObject* o) { return o==&bad_box?NULL:&o->value; }
static Il2CppReflectionType* reflect_type(const Il2CppType* t) {
 reflection=(Il2CppObject){.klass=&typ,.type_argument=t->klass}; return (Il2CppReflectionType*)&reflection;
}
static const MethodInfo* metadata_methods(Il2CppClass* c,void** iterator) {
 if(method_limit && c==&go_class) {
  uintptr_t i=(uintptr_t)*iterator; if(i>=513) return NULL; *iterator=(void*)(i+1);
  return i==0?&owned[BM_COMPARE_TAG]:&extra;
 }
 return f_methods(c,iterator);
}
static Il2CppObject* invoke_owned(const MethodInfo* m,void* receiver,Il2CppObject** args,int argc,Il2CppException** exc) {
 *exc=NULL; int eye=eye_of(receiver);
 if(receiver && !is_rooted(receiver)) root_violation=true;
 for(int i=0;i<argc;++i) if(!args[i] || !is_rooted(args[i])) root_violation=true;
 switch(m->id) {
 case BM_GO_CTOR:
  if(eye<0) { *exc=&exception; break; }
  ++ctor_calls[eye]; if(ctor_failure==(unsigned)eye+1) *exc=&exception; break;
 case BM_ACTIVE_SET:
  if(eye<0 || argc!=1) { *exc=&exception; break; }
  if(active_exception==(unsigned)eye+1) { *exc=&exception; break; }
  active[eye]=args[0]->value!=0; break;
 case BM_ACTIVE_GET:
  return &bool_boxes[active[eye] || active_failure==(unsigned)eye+1?1:0];
 case BM_NAME: break;
 case BM_COMPARE_TAG:
  if(eye<0 || argc!=1 || args[0]->klass!=&string_class || args[0]->value!=42) { *exc=&exception; break; }
  ++compare_calls[eye];
  switch(tag_mode[eye]) {
  case TAG_FALSE:return &bool_boxes[0];
  case TAG_EXCEPTION:*exc=&exception;return NULL;
  case TAG_NULL:return NULL;
  case TAG_WRONG_BOX:return &zero_box;
  case TAG_UNBOX_FAIL:return &bad_box;
  default:return &bool_boxes[1];
  }
 case BM_ALIVE:
  eye=eye_of(args[0]); if(eye<0) { *exc=&exception; break; }
  return &bool_boxes[native_alive[eye]?1:0];
 case BM_DESTROY:
  eye=eye_of(args[0]); if(eye<0) { *exc=&exception; break; }
  ++destroys[eye]; if(destroy_failure==(unsigned)eye+1) *exc=&exception; break;
 case BM_ADD:
  ++adds;
  if(bp.empty.known!=3 || bp.empty.tagged!=3 || !bp.empty.components_started || active[0] || active[1]) order_violation=true;
  component=(Il2CppObject){.klass=args[0]->type_argument}; return &component;
 default:*exc=&exception;break;
 }
 return NULL;
}
static Il2CppObject* invoke_zero(const MethodInfo* m,void* receiver,void** args,Il2CppException** exc) {
 if(args || m->argc) { *exc=&exception;return NULL; }
 return invoke_owned(m,receiver,NULL,0,exc);
}
static void ownership_reset(void) {
 (void)bp_disable_owned_cameras; /* Shared retire entry, exercised by mono fixture. */
 reset(true); memset(&bp,0,sizeof(bp)); memset(gos,0,sizeof(gos)); memset(strings,0,sizeof(strings));
 memset(&bp_binding_preflight,0,sizeof(bp_binding_preflight));
 memset(compare_calls,0,sizeof(compare_calls)); memset(destroys,0,sizeof(destroys)); memset(ctor_calls,0,sizeof(ctor_calls));
 memset(tag_mode,0,sizeof(tag_mode)); memset(native_alive,0,sizeof(native_alive)); memset(active,0,sizeof(active));
 allocations=string_allocations=adds=log_count=unpublish_count=0;
 ctor_failure=active_failure=active_exception=destroy_failure=method_limit=0;
 order_violation=root_violation=string_failure=false;
 go_class=(Il2CppClass){.name="GameObject"}; string_class=(Il2CppClass){.name="String"};
 camera_class=(Il2CppClass){.name="Camera"}; component_class=(Il2CppClass){.name="Component"};
 string_class.type=(Il2CppType){.code=IL2CPP_TYPE_STRING,.klass=&string_class};
 go_class.type=(Il2CppType){.code=IL2CPP_TYPE_CLASS,.klass=&go_class};
 camera_class.type=(Il2CppType){.code=IL2CPP_TYPE_CLASS,.klass=&camera_class};
 component_class.type=(Il2CppType){.code=IL2CPP_TYPE_CLASS,.klass=&component_class};
 boolean.type.code=IL2CPP_TYPE_BOOLEAN;
 bool_boxes[0]=(Il2CppObject){.klass=&boolean}; bool_boxes[1]=(Il2CppObject){.klass=&boolean,.value=1};
 bad_box=(Il2CppObject){.klass=&boolean,.value=1};
 bp.api=api; bp.api.runtime_invoke=invoke_zero; bp.api.runtime_invoke_convert_args=invoke_owned;
 bp.api.value_box=box_bool; bp.api.object_unbox=unbox_owned; bp.api.type_get_object=reflect_type;
 bp.api.class_get_methods=metadata_methods;
 bp.types[BT_GO]=&go_class; bp.types[BT_STRING]=&string_class; bp.types[BT_BOOL]=&boolean;
 bp.types[BT_OBJECT]=&obj; bp.types[BT_CAMERA]=&camera_class; bp.types[BT_COMPONENT]=&component_class;
 bp.object_new=allocate_go; bp.string_new=allocate_string;
 bp.owner_thread=GetCurrentThreadId(); bp.bound=true;
 bp.c=(BpControl){.phase=BP_BUILDING,.attempted=true};
 for(unsigned i=0;i<BM_COUNT;++i) {
  owned[i]=(MethodInfo){.id=i,.name=bp_specs[i].name,.argc=bp_specs[i].argc,.flags=METHOD_ATTRIBUTE_PUBLIC};
  bp.method[i]=&owned[i];
 }
 owned[BM_COMPARE_TAG].owner=&go_class; owned[BM_COMPARE_TAG].ret=&boolean.type;
 owned[BM_COMPARE_TAG].params[0]=&string_class.type;
 go_class.methods[0]=&owned[BM_COMPARE_TAG]; go_class.count=1;
 extra=owned[BM_COMPARE_TAG]; extra.name="Other";
}
static int end_case(const char* label) {
 CHECK(!root_violation && !order_violation); CHECK(!submit_calls && !make_calls);
 ++cases; printf("PASS %s\n",label); return 0;
}
static int tag_failure_case(unsigned eye,unsigned mode) {
 ownership_reset(); tag_mode[eye]=mode;
 CHECK(!bp_prepare_empty_pair()); CHECK(bp.failed && bp.empty.cleanup && !bp.empty.components_started);
 CHECK(!adds && bp.empty.known==(eye?3u:1u) && !bp.empty.uncertain);
 CHECK(!bp_begin_components()); CHECK(!bp_add(bp.eye[0].go,BT_CAMERA)); CHECK(!adds);
 unsigned live=roots_live;
 bp_cleanup_empty(); CHECK(destroys[0]==1 && destroys[1]==(eye?1u:0u)); CHECK(roots_live==live && !bp.empty.finished);
 bp_cleanup_empty(); CHECK(destroys[0]==1 && destroys[1]==(eye?1u:0u) && roots_live==live);
 native_alive[0]=native_alive[1]=false;
 bp_cleanup_empty(); CHECK(bp.empty.finished && bp.c.phase==BP_RETIRED && !roots_live);
 CHECK(!bp_prepare_empty_pair()); CHECK(allocations==eye+1);
 char label[64]; snprintf(label,sizeof(label),"eye%u_tag_failure_mode%u_actual_adapter",eye,mode);
 return end_case(label);
}
#ifndef BP_OWNERSHIP_TEST_MAIN
#define BP_OWNERSHIP_TEST_MAIN main
#endif
int BP_OWNERSHIP_TEST_MAIN(void) {
 cases=assertions=0;
 CHECK(IL2CPP_TYPE_STRING==0x0e && IL2CPP_TYPE_BOOLEAN==0x02 && METHOD_ATTRIBUTE_PUBLIC==6);
 ownership_reset(); CHECK(bp_bind_owned_method(BM_COMPARE_TAG));
 CHECK(bp_prepare_empty_pair() && !adds && allocations==2 && bp.empty.known==3 && bp.empty.tagged==3);
 CHECK(!active[0] && !active[1] && ctor_calls[0]==1 && ctor_calls[1]==1);
 CHECK(bp_begin_components() && bp_add(bp.eye[0].go,BT_CAMERA) && bp_add(bp.eye[1].go,BT_CAMERA));
 CHECK(adds==2 && bp_tag_enable_barrier() && compare_calls[0]==2 && compare_calls[1]==2);
 CHECK(!end_case("both_empty_before_real_AddComponent_adapter"));

 ownership_reset(); go_class.count=0; bp.bound=false;
 CHECK(!bp_bind_owned_method(BM_COMPARE_TAG) && !bp_prepare_empty_pair() && !allocations);
 CHECK(!end_case("missing_CompareTag_allocations0"));
 ownership_reset(); bp.bound=false;
 CHECK(!bp_prepare_empty_pair() && !allocations); CHECK(!end_case("all_binding_not_completed_allocations0"));
 for(unsigned id=BM_ALIVE;id<=BM_DESTROY;++id) {
  ownership_reset(); bp.method[id]=NULL; CHECK(!bp_prepare_empty_pair() && !allocations);
  CHECK(!end_case("Destroy_or_native_liveness_required_before_allocation"));
 }
 for(unsigned mode=0;mode<8;++mode) {
  ownership_reset(); Il2CppType wrong=boolean.type;
  switch(mode) {
  case 0:owned[BM_COMPARE_TAG].flags|=METHOD_ATTRIBUTE_STATIC;break;
  case 1:owned[BM_COMPARE_TAG].ret=&void_type;break;
  case 2:wrong.byref=true;owned[BM_COMPARE_TAG].ret=&wrong;break;
  case 3:wrong.klass=&obj;owned[BM_COMPARE_TAG].ret=&wrong;break;
  case 4:wrong=string_class.type;wrong.code=IL2CPP_TYPE_CLASS;owned[BM_COMPARE_TAG].params[0]=&wrong;break;
  case 5:wrong=string_class.type;wrong.byref=true;owned[BM_COMPARE_TAG].params[0]=&wrong;break;
  case 6:extra=owned[BM_COMPARE_TAG];go_class.methods[1]=&extra;go_class.count=2;break;
  default:method_limit=1;break;
  }
  CHECK(!bp_bind_owned_method(BM_COMPARE_TAG) && !bp_prepare_empty_pair() && !allocations);
  CHECK(!end_case("invalid_or_ambiguous_or_incomplete_metadata_allocations0"));
 }
 ownership_reset(); extra.name=NULL;go_class.methods[1]=&extra;go_class.count=2;
 CHECK(!bp_bind_owned_method(BM_COMPARE_TAG) && !bp_prepare_empty_pair() && !allocations);
 CHECK(!strcmp(bp.error,"owned_tag_metadata_names_incomplete")); CHECK(!end_case("unreadable_name_cannot_prove_unique_allocations0"));
 for(unsigned eye=0;eye<2;++eye) for(unsigned mode=TAG_FALSE;mode<=TAG_UNBOX_FAIL;++mode) CHECK(!tag_failure_case(eye,mode));

 for(unsigned eye=0;eye<2;++eye) {
  ownership_reset(); ctor_failure=eye+1;
  CHECK(!bp_prepare_empty_pair() && bp.empty.uncertain==(1u<<eye) && !adds);
  bp_cleanup_empty(); CHECK(destroys[eye]==0 && roots_live>0);
  native_alive[0]=native_alive[1]=false;bp_cleanup_empty();
  CHECK(bp.empty.finished && bp.c.quarantined && roots_live>0 && bp.c.phase==BP_STOPPED);
  CHECK(!end_case("ctor_exception_unknown_never_Destroy_or_free"));
  ownership_reset(); active_failure=eye+1;
  CHECK(!bp_prepare_empty_pair() && bp.empty.uncertain==(1u<<eye) && !adds);
  bp_cleanup_empty(); CHECK(!destroys[eye] && roots_live>0);
  CHECK(!end_case("inactive_not_proved_no_speculative_Destroy"));
  ownership_reset(); active_exception=eye+1;
  CHECK(!bp_prepare_empty_pair() && bp.empty.uncertain==(1u<<eye) && !adds);
  bp_cleanup_empty();CHECK(!destroys[eye] && roots_live>0);
  CHECK(!end_case("SetActive_exception_unknown_no_speculative_Destroy"));
 }
 ownership_reset();tag_mode[1]=TAG_FALSE;CHECK(!bp_prepare_empty_pair());
 unsigned staggered_roots=roots_live;bp_cleanup_empty();native_alive[0]=false;bp_cleanup_empty();
 CHECK(bp.empty.gone==1 && !bp.empty.finished && roots_live==staggered_roots);
 CHECK(destroys[0]==1 && destroys[1]==1);
 native_alive[1]=false;bp_cleanup_empty();CHECK(bp.empty.finished && !roots_live);
 CHECK(!end_case("staggered_native_gone_all_roots_retained_until_both"));
 ownership_reset(); tag_mode[1]=TAG_FALSE; CHECK(!bp_prepare_empty_pair());
 unsigned retained=roots_live;
 for(unsigned i=0;i<BP_DESTROY_CHECK_LIMIT;++i) bp_cleanup_empty();
 CHECK(bp.empty.finished && bp.empty.checks==BP_DESTROY_CHECK_LIMIT && bp.c.quarantined);
 CHECK(roots_live==retained && destroys[0]==1 && destroys[1]==1);
 native_alive[0]=native_alive[1]=false;bp_cleanup_empty();
 CHECK(roots_live==retained && !bp_prepare_empty_pair() && allocations==2);
 CHECK(!end_case("bounded_native_confirmation_no_late_free_or_retry"));
 ownership_reset(); tag_mode[1]=TAG_FALSE; CHECK(!bp_prepare_empty_pair());destroy_failure=1;
 bp_cleanup_empty();CHECK(destroys[0]==1 && destroys[1]==1 && bp.empty.uncertain==1);
 native_alive[1]=false;bp_cleanup_empty();CHECK(bp.empty.finished && roots_live>0);
 bp_cleanup_empty();CHECK(destroys[0]==1);CHECK(!end_case("Destroy_exception_each_once_other_eye_independent_roots_retained"));

 ownership_reset();CHECK(bp_prepare_empty_pair() && bp_begin_components());
 tag_mode[0]=TAG_FALSE;CHECK(!bp_compare_untagged(bp.eye[0].go));
 CHECK(bp.c.quarantined && !bp.empty.cleanup && bp.empty.components_started);
 bp_cleanup_empty();CHECK(!destroys[0] && !destroys[1] && roots_live>0);
 CHECK(!end_case("post_init_tag_failure_uses_existing_partial_not_empty"));
 ownership_reset();CHECK(bp_prepare_empty_pair() && bp_begin_components());tag_mode[1]=TAG_FALSE;
 CHECK(!bp_tag_enable_barrier() && !bp.empty.cleanup && bp.c.quarantined);
 CHECK(!end_case("both_enable_queries_precede_enable_right_failure"));
 ownership_reset();CHECK(!bp_begin_components() && !adds); CHECK(!end_case("cannot_begin_components_before_two_eye_checkpoint"));
 ownership_reset();bp.owner_thread=0;CHECK(!bp_prepare_empty_pair() && !allocations);CHECK(!end_case("foreign_thread_allocations0"));
 ownership_reset();attached=false;CHECK(!bp_prepare_empty_pair() && !allocations);CHECK(!end_case("detached_allocations0"));
 ownership_reset();string_failure=true;CHECK(!bp_prepare_empty_pair() && !allocations);CHECK(!end_case("Untagged_string_failure_allocations0"));
 ownership_reset();CHECK(bp_prepare_empty_pair());gos[0].klass=&obj;
 CHECK(!bp_compare_untagged(bp.eye[0].go) && compare_calls[0]==1);
 CHECK(!end_case("wrong_receiver_class_no_query_invoke"));
 ownership_reset();CHECK(bp_prepare_empty_pair());strings[0].klass=&obj;
 CHECK(!bp_compare_untagged(bp.eye[0].go) && compare_calls[0]==1);
 CHECK(!end_case("wrong_rooted_string_class_no_query_invoke"));
 ownership_reset();root_fail_at=2;CHECK(!bp_prepare_empty_pair() && allocations==1 && bp.empty.uncertain==1);
 bp_cleanup_empty();CHECK(!destroys[0] && roots_live>0 && bp.empty.finished);
 CHECK(!end_case("GO_root_failure_unknown_no_Destroy_or_unproven_free"));
 printf("RESULT cases=%u assertions=%u actual_owned_adapter=YES native_Unity_runtime=UNVERIFIED full_build_eye_adapter=NOT_EXECUTED\n",cases,assertions);
 return 0;
}
