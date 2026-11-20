#define COBJMACROS
#include <d3d11.h>
#define boolean fixture_boolean_class
#define BP_NATIVE_MONO_FIXTURE
#define BP_BINDING_PREFLIGHT_TEST_MAIN preflight_fixture_main
#include "binding_preflight_test.c"
#undef boolean
/* Existing provider's exact project declaration; no new invocation ABI. */
typedef void* (*native_texture_icall_fn)(Il2CppObject*);
static native_texture_icall_fn get_native_texture;
static const GUID iid_d3d11_texture2d={0x6f15aaf2,0xd208,0x4e89,{0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};
static bool empty_stack=true;
static unsigned stack_checks;
static bool bp_stack_empty(Il2CppObject* additional) { (void)additional;++stack_checks;return empty_stack; }
#include "../src/base_producer_mono.h"

enum { MO_OK,MO_TRUE,MO_EXCEPTION,MO_NULL,MO_WRONG_CLASS,MO_UNBOX_FAIL };
static Il2CppObject mo_camera[2],mo_additional[2],mo_rt[2],mo_data[2],mo_boxes[2],mo_enum;
static bool mo_enabled[2],mo_xr[2],mo_stereo[2],mo_created[2],mo_target_alias;
static unsigned mo_xr_mode[2],mo_stereo_mode[2],mo_set_exception,mo_ignore_set;
static unsigned mo_on_enable,mo_provider_null,mo_provider_calls,mo_qi_calls,mo_releases;
static unsigned mo_xr_sets[2],mo_active_checks[2],mo_inactive_checks[2],mo_enable_writes,mo_base_sets;
static unsigned mo_qi_mode;static bool mo_wrong_rt,mo_order_error;
typedef struct MoCom { IUnknown iface; ULONG refs; unsigned eye; } MoCom;
static MoCom mo_com[2];
static HRESULT STDMETHODCALLTYPE mo_qi(IUnknown* self,REFIID iid,void** out) {
 ++mo_qi_calls;*out=NULL;
 if(memcmp(iid,&iid_d3d11_texture2d,sizeof(GUID))) return E_NOINTERFACE;
 if(mo_qi_mode==1) return E_NOINTERFACE;
 if(mo_qi_mode==2) return S_OK; /* A malformed success must still fail closed. */
 MoCom* c=(MoCom*)self;++c->refs;*out=self;return S_OK;
}
static ULONG STDMETHODCALLTYPE mo_addref(IUnknown* self) { return ++((MoCom*)self)->refs; }
static ULONG STDMETHODCALLTYPE mo_release(IUnknown* self) { ++mo_releases;return --((MoCom*)self)->refs; }
static IUnknownVtbl mo_vtable={mo_qi,mo_addref,mo_release};
static int mo_eye(void* o,const Il2CppObject* objects) { return o==&objects[0]?0:o==&objects[1]?1:-1; }
static void* mo_provider(Il2CppObject* rt) {
 ++mo_provider_calls;int eye=mo_eye(rt,mo_rt);
 if(eye<0 || !is_rooted(rt) || GetCurrentThreadId()!=bp.owner_thread || !attached) {mo_wrong_rt=true;return NULL;}
 return mo_provider_null==(unsigned)eye+1?NULL:&mo_com[eye].iface;
}
static Il2CppObject* mo_box(Il2CppClass* klass,void* value) {
 if(klass==&pf_classes[BT_BOOL]) return &mo_boxes[*(uint8_t*)value?1:0];
 if(klass==&pf_classes[BT_RENDER_TYPE]) {mo_enum=(Il2CppObject){.klass=klass,.value=*(int32_t*)value};return &mo_enum;}
 return NULL;
}
static Il2CppObject* mo_result(bool value,unsigned mode,Il2CppException** exc) {
 switch(mode) {
 case MO_TRUE:return &mo_boxes[1];case MO_EXCEPTION:*exc=&exception;return NULL;
 case MO_NULL:return NULL;case MO_WRONG_CLASS:return &zero_box;case MO_UNBOX_FAIL:return &bad_box;
 default:return &mo_boxes[value?1:0];
 }
}
static Il2CppObject* mo_invoke(const MethodInfo* m,void* receiver,Il2CppObject** args,int argc,Il2CppException** exc) {
 ++pf_invocations;*exc=NULL;int camera=mo_eye(receiver,mo_camera),additional=mo_eye(receiver,mo_additional),rt=mo_eye(receiver,mo_rt);
 if(receiver && !is_rooted(receiver)) root_violation=true;
 for(int i=0;i<argc;++i) if(!is_rooted(args[i])) root_violation=true;
 switch(m->id) {
 case BM_XR_SET:
  if(additional<0 || argc!=1 || args[0]->klass!=&pf_classes[BT_BOOL] || args[0]->value) {mo_order_error=true;*exc=&exception;return NULL;}
  ++mo_xr_sets[additional];
  if(mo_set_exception==(unsigned)additional+1) {*exc=&exception;return NULL;}
  if(mo_ignore_set!=(unsigned)additional+1) mo_xr[additional]=false;return NULL;
 case BM_XR_GET:return mo_result(mo_xr[additional],mo_xr_mode[additional],exc);
 case BM_STEREO_ENABLED:
  if(mo_enabled[camera]) ++mo_active_checks[camera];else ++mo_inactive_checks[camera];
  return mo_result(mo_stereo[camera],mo_stereo_mode[camera],exc);
 case BM_ENABLED_GET:return mo_result(mo_enabled[camera],MO_OK,exc);
 case BM_ENABLED_SET:
  if(camera<0) {*exc=&exception;return NULL;}
  mo_enabled[camera]=args[0]->value!=0;
  if(mo_enabled[camera]) {++mo_enable_writes;if(mo_on_enable==(unsigned)camera+1) mo_stereo[camera]=true;}
  return NULL;
 case BM_RENDER_TYPE_SET:++mo_base_sets;return NULL;
 case BM_RENDER_TYPE_GET:mo_enum=(Il2CppObject){.klass=&pf_classes[BT_RENDER_TYPE]};return &mo_enum;
 case BM_RT_CREATED:return mo_result(mo_created[rt],MO_OK,exc);
 case BM_TARGET_GET:return mo_target_alias?&mo_rt[1-camera]:&mo_rt[camera];
 case BM_ALIVE:return mo_result(mo_eye(args[0],mo_camera)>=0,MO_OK,exc);
 default:*exc=&exception;return NULL;
 }
}
static Il2CppObject* mo_invoke_zero(const MethodInfo* m,void* receiver,void** args,Il2CppException** exc) {
 if(args || m->argc) {*exc=&exception;return NULL;}return mo_invoke(m,receiver,NULL,0,exc);
}
static void mo_reset(bool objects) {
 pf_reset();get_native_texture=mo_provider;
 mo_provider_calls=mo_qi_calls=mo_releases=mo_enable_writes=mo_base_sets=stack_checks=0;
 mo_on_enable=mo_provider_null=mo_qi_mode=mo_set_exception=mo_ignore_set=0;
 mo_target_alias=mo_wrong_rt=mo_order_error=false;empty_stack=true;
 memset(mo_xr_mode,0,sizeof(mo_xr_mode));memset(mo_stereo_mode,0,sizeof(mo_stereo_mode));
 memset(mo_xr_sets,0,sizeof(mo_xr_sets));memset(mo_active_checks,0,sizeof(mo_active_checks));memset(mo_inactive_checks,0,sizeof(mo_inactive_checks));
 for(unsigned eye=0;eye<2;++eye) {
  mo_enabled[eye]=mo_stereo[eye]=false;mo_xr[eye]=mo_created[eye]=true;
  mo_com[eye]=(MoCom){.iface={&mo_vtable},.refs=1,.eye=eye};
 }
 mo_boxes[0]=(Il2CppObject){.klass=&pf_classes[BT_BOOL]};mo_boxes[1]=(Il2CppObject){.klass=&pf_classes[BT_BOOL],.value=1};
 bad_box.klass=&pf_classes[BT_BOOL];
 if(!objects) return;
 if(!bp_bind_required(NULL) || !bp_native_provider_gate()) abort();
 bp.bound=bp.backend_verified=true;bp.empty.components_started=true;
 bp.empty.known=bp.empty.tagged=3;bp.c.phase=BP_BUILDING;bp.c.initialized=3;
 for(unsigned eye=0;eye<2;++eye) {
  mo_camera[eye]=(Il2CppObject){.klass=&pf_classes[BT_CAMERA]};
  mo_additional[eye]=(Il2CppObject){.klass=&pf_classes[BT_ADDITIONAL]};
  mo_rt[eye]=(Il2CppObject){.klass=&pf_classes[BT_RT]};mo_data[eye]=(Il2CppObject){.klass=&pf_classes[BT_DATA]};
  bp.eye[eye].camera=bp_root(&mo_camera[eye]);bp.eye[eye].additional=bp_root(&mo_additional[eye]);
  bp.eye[eye].rt=bp_root(&mo_rt[eye]);bp.eye[eye].data=bp_root(&mo_data[eye]);
 }
 bp.api.runtime_invoke=mo_invoke_zero;bp.api.runtime_invoke_convert_args=mo_invoke;bp.api.value_box=mo_box;
}
static int mo_end(const char* label) {
 CHECK(!allocations && !ctor_calls[0] && !ctor_calls[1] && !make_calls && !submit_calls);
 CHECK(!root_violation && !mo_wrong_rt && !mo_order_error);
 CHECK(!destroys[0] && !destroys[1] && !bp.empty.cleanup);
 ++cases;printf("PASS %s\n",label);return 0;
}
static int mo_new_binding(unsigned id,unsigned mode) {
 mo_reset(false);Il2CppType wrong=pf_classes[BT_BOOL].type;
 bool setter=id==BM_XR_SET;
 switch(mode) {
 case 0:owned[id].name="Unavailable";break;
 case 1:owned[id].owner=&pf_classes[BT_RT];break;
 case 2:owned[id].flags|=METHOD_ATTRIBUTE_STATIC;break;
 case 3:owned[id].generic=true;break;
 case 4:owned[id].inflated=true;break;
 case 5:wrong.byref=true;if(setter) owned[id].params[0]=&wrong;else owned[id].ret=&wrong;break;
 case 6:wrong.code=IL2CPP_TYPE_I4;if(setter) owned[id].params[0]=&wrong;else owned[id].ret=&wrong;break;
 case 7:wrong.klass=&pf_classes[BT_INT];if(setter) owned[id].params[0]=&wrong;else owned[id].ret=&wrong;break;
 case 8:owned[id].flags|=METHOD_ATTRIBUTE_ABSTRACT;break;
 case 9:owned[id].argc=setter?0:1;break;
 case 10:pf_extra[0]=owned[id];owned[id].owner->methods[owned[id].owner->count++]=&pf_extra[0];break;
 case 11:pf_caps=id==BM_STEREO_ENABLED?2:1;break;
 case 12:pf_extra[0]=owned[id];pf_extra[0].name=NULL;owned[id].owner->methods[owned[id].owner->count++]=&pf_extra[0];break;
 case 13:wrong.code=IL2CPP_TYPE_I4;owned[id].ret=&wrong;break;
 default:wrong.code=IL2CPP_TYPE_VOID;wrong.byref=true;owned[id].ret=&wrong;break;
 }
 CHECK(!bp_bind_required(&bp_binding_preflight));
 CHECK(!bp_prepare_empty_pair() && !allocations && !pf_invocations && !mo_provider_calls && !mo_qi_calls);
 CHECK(bp_binding_preflight.row[id].observed && !bp_binding_preflight.row[id].selected);
 if(mode!=11 && mode!=12) CHECK(bp_binding_preflight.first_failure==id);
 char label[96];snprintf(label,sizeof(label),"new_spec%u_mode%u_exact_gate_allocation0",id,mode);return mo_end(label);
}
int main(void) {
 cases=assertions=0;
 CHECK(IL2CPP_TYPE_BOOLEAN==2 && IL2CPP_TYPE_VOID==1 && BM_COUNT==50);
 mo_reset(false);get_native_texture=NULL;CHECK(!bp_native_provider_gate() && !bp_prepare_empty_pair());
 CHECK(!allocations && !mo_provider_calls && !mo_qi_calls);CHECK(!mo_end("provider_missing_allocation0"));
 const unsigned ids[]={BM_XR_SET,BM_XR_GET,BM_STEREO_ENABLED};
 for(unsigned i=0;i<3;++i) for(unsigned mode=0;mode<15;++mode) CHECK(!mo_new_binding(ids[i],mode));

 mo_reset(true);CHECK(bp_prepare_own_additional(&bp.eye[0]) && bp_prepare_own_additional(&bp.eye[1]));
 CHECK(mo_base_sets==2 && stack_checks==2 && !mo_xr[0] && !mo_xr[1]);
 mo_xr[0]=true;CHECK(bp_apply_xr_optout(&bp.eye[0]) && !mo_xr[0] && mo_xr_sets[0]==2);
 CHECK(bp_effective_mono(&bp.eye[0]) && bp_acquire_owned_native(0) && bp_acquire_owned_native(1));
 CHECK(mo_provider_calls==2 && mo_qi_calls==2 && mo_com[0].refs==2 && mo_com[1].refs==2);
 CHECK(bp.native[0] && bp.native[1] && bp.native[0]!=bp.native[1]);
 bp.c.phase=BP_ENABLE_READY;CHECK(bp_enable_mono_pair() && bp.c.enabled==3);
 CHECK(mo_active_checks[0]>0 && mo_active_checks[1]>0 && mo_inactive_checks[0]>0);
 bp.c.phase=BP_ACTIVE;CHECK(bp_mono_barrier());
 CHECK(mo_com[0].refs==2 && !mo_releases);CHECK(!mo_end("normal_typed_false_init_reset_native_QI_ref_prepostenable_active_checks"));
 for(unsigned which=0;which<2;++which) for(unsigned mode=MO_TRUE;mode<=MO_UNBOX_FAIL;++mode) {
  mo_reset(true);mo_xr[0]=mo_xr[1]=false;
  if(which) mo_stereo_mode[1]=mode;else mo_xr_mode[1]=mode;
  unsigned live=roots_live;CHECK(!bp_mono_barrier() && bp.failed && bp.c.quarantined);
  CHECK(!mo_enabled[0] && !mo_enabled[1] && bp.disable_done && roots_live==live);
  CHECK(!mo_provider_calls && !mo_qi_calls);CHECK(!mo_end("typed_true_exception_null_wrongbool_unboxfail_partial_stop"));
 }
 mo_reset(true);mo_set_exception=1;CHECK(!bp_apply_xr_optout(&bp.eye[0]) && bp.c.quarantined);
 CHECK(!mo_end("XR_setter_exception_partial_not_empty"));
 mo_reset(true);mo_ignore_set=1;CHECK(!bp_apply_xr_optout(&bp.eye[0]) && mo_xr[0]);
 CHECK(!mo_end("setter_return_without_actual_false_is_not_success"));
 mo_reset(true);empty_stack=false;CHECK(!bp_prepare_own_additional(&bp.eye[0]) && !mo_xr_sets[0]);
 CHECK(!mo_end("Base_empty_stack_gate_precedes_XR_write"));
 for(unsigned eye=0;eye<2;++eye) {
  mo_reset(true);mo_xr[0]=mo_xr[1]=false;mo_on_enable=eye+1;bp.c.phase=BP_ENABLE_READY;
  CHECK(!bp_enable_mono_pair() && bp.c.quarantined && !bp.c.enabled);
  CHECK(!mo_enabled[0] && !mo_enabled[1] && mo_active_checks[eye]>0);
  CHECK(!mo_end("post_enable_stereo_true_immediate_own_disable"));
 }
 mo_reset(true);mo_xr[0]=mo_xr[1]=false;
 CHECK(bp_mono_barrier() && !bp_enable_mono_pair() && !mo_enable_writes);
 CHECK(!mo_end("disabled_false_not_enable_ready_without_original_fence_state"));
 mo_reset(true);mo_xr[0]=mo_xr[1]=false;bp.c.phase=BP_ENABLE_READY;CHECK(bp_enable_mono_pair());
 bp.c.phase=BP_ACTIVE;mo_stereo[0]=true;unsigned published_stops=unpublish_count;
 CHECK(!bp_mono_barrier() && unpublish_count>published_stops && !bp.c.enabled && bp.c.quarantined);
 CHECK(!mo_end("eligible_stereo_change_unpublishes_and_disables"));
 mo_reset(true);mo_xr[0]=mo_xr[1]=false;bp.c.phase=BP_ENABLE_READY;CHECK(bp_enable_mono_pair());
 bp.c.phase=BP_ACTIVE;mo_xr[1]=true;CHECK(!bp_mono_barrier() && !bp.c.enabled && bp.c.quarantined);
 CHECK(!mo_end("eligible_XR_flag_change_unpublishes_and_disables"));
 for(unsigned mode=0;mode<12;++mode) {
  mo_reset(true);mo_xr[0]=mo_xr[1]=false;
  switch(mode) {
  case 0:mo_provider_null=1;break;
  case 1:mo_qi_mode=1;break;
  case 2:mo_qi_mode=2;break;
  case 3:bp.owner_thread=0;break;
  case 4:attached=false;break;
  case 5:bp.backend_verified=false;break;
  case 6:mo_enabled[0]=true;break;
  case 7:mo_created[0]=false;break;
  case 8:mo_target_alias=true;break;
  case 9:bp.c.initialized=1;break;
  case 10:bp.eye[0].rt=0;break;
  default:mo_rt[0].klass=&pf_classes[BT_TEXTURE];break;
  }
  CHECK(!bp_acquire_owned_native(0) && !bp.native[0] && bp.c.quarantined);
  if(!mode) CHECK(mo_provider_calls==1 && !mo_qi_calls);
  else if(mode<=2) CHECK(mo_provider_calls==1 && mo_qi_calls==1);
  else CHECK(!mo_provider_calls && !mo_qi_calls);
  if(mode==3 || mode==4) CHECK(!pf_invocations); /* No foreign-thread disable either. */
  CHECK(!mo_releases);CHECK(!mo_end("native_NULL_QI_failure_owner_backend_disabled_created_target_guards"));
 }
 mo_reset(true);mo_xr[0]=mo_xr[1]=false;CHECK(bp_acquire_owned_native(0));mo_provider_null=2;
 CHECK(!bp_acquire_owned_native(1) && bp.native[0] && mo_com[0].refs==2 && !mo_releases);
 CHECK(!mo_end("second_eye_native_failure_retains_first_COM_and_roots"));
 mo_reset(true);mo_xr[0]=mo_xr[1]=false;CHECK(bp_acquire_owned_native(0));
 CHECK(!bp_acquire_owned_native(0) && mo_provider_calls==1 && mo_com[0].refs==2);
 CHECK(!mo_end("no_second_acquisition_overwriting_owned_COM"));
 mo_reset(true);
 CHECK(!bp_descriptor_ok(100,100,100,100,1,1,1,true,false,true));
 CHECK(!bp_descriptor_ok(100,100,100,100,1,1,1,true,true,false));
 CHECK(!bp_descriptor_ok(100,100,100,100,1,1,2,true,true,true));
 CHECK(bp_descriptor_ok(100,100,100,100,1,1,1,true,true,true));
 CHECK(!mo_end("existing_Present_device_alias_MSAA_gate_retained"));
 printf("RESULT cases=%u assertions=%u production_native_mono_helpers=YES target_native_Unity=UNVERIFIED full_bp_build=NOT_EXECUTED\n",cases,assertions);
 return 0;
}
