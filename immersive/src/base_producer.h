/* Approved normal-camera route. Included after existing Unity API globals.
   No native methodPointer, IL2CPP object field offsets or original writes. */
#include "base_producer_state.h"
#include "base_consumer_lease.h"
#include "base_keyed_mutex.h"
#include "base_transport.h"
#include "base_background_policy.h"
#include "base_producer_lookup.h"
#include "base_binding_preflight.h"
#include "base_projection_types.h"
typedef struct BpEye { uint32_t go,camera,additional,data,rt,transform;
 Vec3 position; Quat rotation; Matrix4x4 projection; } BpEye;
typedef struct BpProducer {
 BpControl c; RdApi api; Il2CppClass* types[BT_COUNT]; const MethodInfo* method[BM_COUNT];
 Il2CppObject* (*object_new)(Il2CppClass*);
 Il2CppString* (*string_new)(const char*);
 void (*field_get)(Il2CppObject*,FieldInfo*,void*);
 void (*field_set)(Il2CppObject*,FieldInfo*,void*);
 FieldInfo* initialized_field; FieldInfo* fov_field;
 uint32_t contact_mesh; uint32_t roots[24]; unsigned root_count; uint32_t source_root,source_transform_root;int32_t source_pose_frame;
 BpEye eye[2]; BpEmpty empty; uint32_t tag_root; DWORD owner_thread; const char* stage;
 const char* error; bool bound, failed, disable_done, destroy_called, witness_saved, backend_verified;
 LONG width,height; int32_t eye_mask; float scale; bool post;
 BpBackground background;
 ULONGLONG observed_tick; EyeOptics optics;
 ID3D11Texture2D* native[2]; ID3D11Texture2D* readback[2];
 ID3D11RenderTargetView* sentinel_view[2]; ID3D11Query* fence;
 ID3D11Device* gpu_device; DWORD present_thread; uint64_t last_committed_tick;
 bool preview_near_adjusted;
 uint64_t witness_retry_after;
 unsigned witness_samples;
 BpSourceKind source_kind; bool clean_retired;
} BpProducer;
static BpProducer bp;
/* Shared preview projection policy. A failed tuning attempt leaves the
   authored projection selected for subsequent generations of this session. */
static bool preview_use_authored_clip;
static SRWLOCK bp_lock=SRWLOCK_INIT;
static volatile LONG bp_stop_requested, bp_published_generation, bp_main_depth;
static BpConsumerLease bp_consumer_lease;
static BpTransport bp_transport;
/* Finite state-transition evidence survives normal-stream saturation. No
   per-frame records and no per-generation reset of the process cap. */
static unsigned bp_context_records,bp_context_bytes,bp_context_attempted,bp_context_dropped;
static void bp_context_event_at(const char* stage,const char* reason,int32_t scene,
 BpSourceKind kind,uint64_t source) {
 ++bp_context_attempted;
 if(bp_context_records>=16) { ++bp_context_dropped;return; }
 char detail[640];
 telemetry_snprintf(detail,sizeof(detail),
  "\"stage\":\"%s\",\"reason\":\"%s\",\"generation\":%llu,\"state\":%u,"
  "\"context_kind\":%u,\"scene\":%d,\"source\":%llu,\"quarantined\":%s,"
  "\"receipt_owner\":%ld,\"receipt_generation\":%ld,\"receipt_serial\":%lld",
  stage,reason,(unsigned long long)bp.c.identity.generation,(unsigned)bp.c.phase,
  (unsigned)kind,scene,(unsigned long long)source,bp.c.quarantined?"true":"false",
  (long)bp_transport.owner,(long)bp_transport.generation,(long long)bp_transport_pending(&bp_transport));
 unsigned charge=(unsigned)strlen(detail)+384u;
 if(charge>8192u || bp_context_bytes>8192u-charge) { ++bp_context_dropped;return; }
 ++bp_context_records;bp_context_bytes+=charge;
 write_record("OWN-BASE","owned_context_transition",detail);
}
static void bp_context_event(const char* stage,const char* reason) {
 bp_context_event_at(stage,reason,bp.c.identity.scene,bp.source_kind,bp.c.identity.source);
}
#define BP_CONTEXT_EVENTS 1
#define BP_FOV_DIAGNOSTIC 1
static int bp_fov_json(char*,size_t);
#define BP_BACKGROUND_POLICY 1
static int bp_background_json(char*,size_t);

static void bp_log(const char* event,const char* reason) {
 char detail[768];
 telemetry_snprintf(detail,sizeof(detail),
  "\"status\":\"%s\",\"stage\":\"%s\",\"state\":%u,\"query\":%u,"
  "\"generation\":%llu,\"source\":%llu,\"scene\":%d,\"pose_epoch\":%llu,\"config_epoch\":%llu,"
  "\"initialized_mask\":%u,\"enabled_mask\":%u,\"quarantined\":%s,"
  "\"source_pose_follow\":true,\"dynamic_6dof\":false,\"render_phase_verified\":false",
  reason?reason:"observed",bp.stage?bp.stage:"none",(unsigned)bp.c.phase,(unsigned)bp.c.query,
  (unsigned long long)bp.c.identity.generation,(unsigned long long)bp.c.identity.source,bp.c.identity.scene,
  (unsigned long long)bp.c.identity.pose,(unsigned long long)bp.c.identity.config,
  bp.c.initialized,bp.c.enabled,bp.c.quarantined?"true":"false");
 write_record("OWN-BASE",event,detail);
}
static void bp_unpublish(void) {
 LONG generation=(LONG)bp.c.identity.generation;
 bp_lease_revoke(&bp_consumer_lease,bp.c.identity.generation);
 if(generation) InterlockedCompareExchange(&published_eye_generation,0,generation);
 InterlockedCompareExchange(&bp_published_generation,0,generation);
}
static void bp_consumer_stop(LONG generation) {
 bp_lease_revoke(&bp_consumer_lease,(uint64_t)generation);
 InterlockedCompareExchange(&published_eye_generation,0,generation);
 InterlockedCompareExchange(&bp_published_generation,0,generation);
 InterlockedOr(&bp_stop_requested,3);
 write_record("OWN-BASE","base_consumer_release_failed",
  "\"status\":\"session_experiment_stopped\",\"ownership\":\"unknown_retained\"");
}
static bool bp_type_match(const Il2CppType* t,int type) {
 return bp_lookup_type_match(&bp.api,bp.types,t,type);
}
#include "base_binding_preflight_log.h"
#include "base_projection_normal_log.h"
#include "base_projection_observe.h"
#include "base_camera_contract_observe.h"
#include "base_producer_ownership.h"
static bool bp_stack_empty(Il2CppObject* additional) {
 Il2CppObject* list=bp_call(BM_STACK,additional,NULL);
 if(!list || bp.failed) { bp_fail("own_base_stack_missing"); return false; }
 uint32_t h=bp.api.gchandle_new(list,false);
 if(!h) { bp_fail("stack_root_failed"); return false; }
 /* Only inspect the freshly created own Additional stack; never Clear a list. */
 Il2CppClass* owner=bp.api.object_get_class(list); void* it=NULL;
 const MethodInfo *m,*count_method=NULL; unsigned scanned=0;
 while((m=bp.api.class_get_methods(owner,&it))) {
  if(++scanned>512) break;
  const char* name=bp.api.method_get_name(m); uint32_t impl=0,flags=bp.api.method_get_flags(m,&impl);
  if(name && !strcmp(name,"get_Count") && bp.api.method_get_class(m)==owner &&
   (flags&(7u|0x10u|0x400u))==6u && !bp.api.method_is_generic(m) &&
   bp.api.method_get_param_count(m)==0 && bp_type_match(bp.api.method_get_return_type(m),BT_INT)) {
   if(count_method) { count_method=NULL; break; } count_method=m;
  }
 }
 Il2CppException* exc=NULL; Il2CppObject* boxed=count_method?
  bp.api.runtime_invoke(count_method,bp.api.gchandle_get_target(h),NULL,&exc):NULL;
 bool ok=!exc && boxed && bp.api.object_get_class(boxed)==bp.types[BT_INT] &&
  bp.api.object_unbox(boxed) && *(int32_t*)bp.api.object_unbox(boxed)==0;
 bp.api.gchandle_free(h);
 if(!ok) bp_fail("own_base_stack_not_empty_or_unverified"); return ok;
}
#include "base_producer_mono.h"
#include "base_producer_fov.h"
#include "base_background.h"
#include "base_source_follow.h"
#include "base_story_source.h"
#include "base_story_observe.h"
#include "base_generation_rearm.h"
/* Home stays loaded behind several flat menus. Consult the current Gallop
   view and, for hub views, its active child before leasing either eye. */
static Il2CppClass* home_scene_manager;
static Il2CppClass* home_dialog_manager;
static FieldInfo* home_instance;
static const MethodInfo* home_current_view;
static const MethodInfo* home_front_dialog;
static const MethodInfo* home_ref_getter(Il2CppClass* type,const char* name,bool is_static){
 for(;type;type=bp.api.class_get_parent(type)){
  void* iter=NULL;const MethodInfo* m;
  while((m=bp.api.class_get_methods(type,&iter))){
   if(strcmp(bp.api.method_get_name(m),name) || bp.api.method_get_param_count(m) ||
      bp.api.method_is_generic(m) || bp.api.method_is_inflated(m))continue;
   uint32_t impl=0,flags=bp.api.method_get_flags(m,&impl);
   if(((flags&16)!=0)!=is_static)continue;
   const Il2CppType* result=bp.api.method_get_return_type(m);
   if(result && bp.api.type_get_type(result)==18)return m;
  }
 }
 return NULL;
}
/* The front gacha page can coexist with the retained Home hub child. */
static Il2CppClass* gacha_view_type;
static const MethodInfo* gacha_enabled;
static const MethodInfo* current_view_id;
static int32_t gacha_main_id;
static bool gacha_id_bound;
static bool dress_id_bound;
static int32_t dress_view_id;
static bool current_dress_page(Il2CppObject* manager){
 int32_t id=0;
 return dress_id_bound && geometry_read(current_view_id,manager,NULL,
  bp.api.class_from_type(bp.api.method_get_return_type(current_view_id)),&id,sizeof id) && id==dress_view_id;
}
static void bind_gacha_page(const Il2CppImage* game){
 gacha_view_type=p_class_from_name(game,"Gallop","GachaMainView");
 gacha_enabled=geometry_method(bp.types[BT_BEHAVIOUR],"get_isActiveAndEnabled",bp.types[BT_BOOL],NULL);
 Il2CppClass* define=p_class_from_name(game,"Gallop","SceneDefine");
 Il2CppClass* (*nested)(Il2CppClass*,void**)=(void*)GetProcAddress(GetModuleHandleW(L"GameAssembly.dll"),"il2cpp_class_get_nested_types");
 FieldInfo* (*field)(Il2CppClass*,const char*)=(void*)GetProcAddress(GetModuleHandleW(L"GameAssembly.dll"),"il2cpp_class_get_field_from_name");
 int (*flags)(FieldInfo*)=(void*)GetProcAddress(GetModuleHandleW(L"GameAssembly.dll"),"il2cpp_field_get_flags");
 if(!define || !nested || !field || !flags || !il2cpp_field_get_value_object || !home_scene_manager)return;
 void* iter=NULL;Il2CppClass* enumeration;
 while((enumeration=nested(define,&iter))!=NULL){
  if(strcmp(bp.api.class_get_name(enumeration),"ViewId"))continue;
  FieldInfo* key=field(enumeration,"GachaMain");
  if(!key || !(flags(key)&64))return;
  const MethodInfo* getter=p_class_get_method(home_scene_manager,"GetCurrentViewId",0);
  if(!getter || bp.api.method_get_param_count(getter)!=0 || bp.api.method_is_generic(getter))return;
  Il2CppClass* ret=bp.api.class_from_type(bp.api.method_get_return_type(getter));
  if(ret!=enumeration && ret!=bp.types[BT_INT])return;
  Il2CppObject* value=il2cpp_field_get_value_object(key,NULL);
  if(!value)return;
  Il2CppClass* actual=bp.api.object_get_class(value);
  if(actual!=enumeration && actual!=bp.types[BT_INT])return;
  const Il2CppType* (*base_type)(Il2CppClass*)=(void*)GetProcAddress(GetModuleHandleW(L"GameAssembly.dll"),"il2cpp_class_enum_basetype");
  if(!base_type || bp.api.class_from_type(base_type(enumeration))!=bp.types[BT_INT])return;
  void* bits=bp.api.object_unbox(value);if(!bits)return;
  memcpy(&gacha_main_id,bits,sizeof(gacha_main_id));current_view_id=getter;gacha_id_bound=true;
  FieldInfo* dress_key=field(enumeration,"DressChange");
  if(dress_key && (flags(dress_key)&64)){
   Il2CppObject* literal=il2cpp_field_get_value_object(dress_key,NULL);
   if(literal && bp.api.object_get_class(literal)==actual){
    void* data=bp.api.object_unbox(literal);
    if(data){memcpy(&dress_view_id,data,sizeof dress_view_id);dress_id_bound=true;}
   }
  }
  break;
 }
}
static const char* active_gacha_page(Il2CppObject* manager){
 static ULONGLONG check_after;static const char* result;
 ULONGLONG now=GetTickCount64();
 if(now<check_after)return result;
 check_after=now+150;result=NULL;
 if(gacha_id_bound && manager && bp.api.class_is_assignable_from(home_scene_manager,bp.api.object_get_class(manager))){
  int32_t id=0;
  Il2CppClass* expected=bp.api.class_from_type(bp.api.method_get_return_type(current_view_id));
  if(geometry_read(current_view_id,manager,NULL,expected,&id,sizeof(id)) && id==gacha_main_id)
   return result="GachaMain_view_id";
 }
 if(!gacha_view_type || !gacha_enabled)return NULL;
 Il2CppReflectionType* type=p_type_get_object(p_class_get_type(gacha_view_type));
 Il2CppArray* views=type?unity_find_objects(type,0,0):NULL;
 if(!views || views->max_length>16)return NULL;
 uint32_t root=bp.api.gchandle_new((Il2CppObject*)views,false);if(!root)return NULL;
 for(uintptr_t i=0;i<views->max_length;i++){
  uint8_t enabled=0;
  if(views->vector[i] && geometry_read(gacha_enabled,views->vector[i],NULL,bp.types[BT_BOOL],&enabled,sizeof(enabled)) && enabled){
   result="GachaMainView_active";break;
  }
 }
 bp.api.gchandle_free(root);return result;
}
static Il2CppClass* dress_detail_type;
static void bp_bind_home_view(const Il2CppImage* game){
 dress_detail_type=p_class_from_name(game,"Gallop","DialogTrainedCharacterDetail");
 home_scene_manager=p_class_from_name(game,"Gallop","SceneManager");
 home_dialog_manager=p_class_from_name(game,"Gallop","DialogManager");
 FieldInfo* (*lookup)(Il2CppClass*,const char*)=(void*)GetProcAddress(GetModuleHandleW(L"GameAssembly.dll"),"il2cpp_class_get_field_from_name");
 int (*flags)(FieldInfo*)=(void*)GetProcAddress(GetModuleHandleW(L"GameAssembly.dll"),"il2cpp_field_get_flags");
 if(home_scene_manager && lookup && flags){
  home_instance=lookup(home_scene_manager,"_instance");if(!home_instance)home_instance=lookup(home_scene_manager,"instance");
  if(home_instance && !(flags(home_instance)&16))home_instance=NULL;
 }
 home_current_view=home_ref_getter(home_scene_manager,"GetCurrentViewController",false);
 home_front_dialog=home_ref_getter(home_dialog_manager,"GetForeFrontDialog",true);
 bind_gacha_page(game);
}
/* A foreground dress view owns the UI subtree. Only a displayed render texture
   and its unique active writer can identify the preview camera. Cache numeric
   identities across callbacks, never managed objects. */
static Il2CppClass* dress_raw_image;
static const MethodInfo* dress_texture;
static int32_t dress_host_id,dress_texture_id,leased_host_id,leased_texture_id;
static void bind_dress_ui(const Il2CppImage* ui){
 dress_raw_image=p_class_from_name(ui,"UnityEngine.UI","RawImage");
 if(dress_raw_image) dress_texture=geometry_method(dress_raw_image,"get_texture",bp.types[BT_TEXTURE],NULL);
}
static bool dress_host(Il2CppObject* obj){
 return obj && (component_is(obj,"Gallop","DressChangeViewController") ||
                component_is(obj,"Gallop","DialogTrainedCharacterDetail"));
}
/* The front DialogObject is a wrapper. Only its own active content tree
   can establish that a trained-character detail is actually in front. */
static Il2CppObject* dress_dialog_content(Il2CppObject* obj,bool note,const char** reason){
 if(dress_host(obj))return obj;
 if(!component_is(obj,"Gallop","DialogObject"))return NULL;
 static Il2CppClass* bound_class;static const MethodInfo* base_rect;
 Il2CppClass* klass=bp.api.object_get_class(obj);
 if(klass!=bound_class){
  bound_class=klass;base_rect=home_ref_getter(klass,"get_BaseRectTransform",false);
 }
 Il2CppException* exc=NULL;
 Il2CppObject* rect=base_rect?bp.api.runtime_invoke(base_rect,obj,NULL,&exc):NULL;
 if(exc || !rect || !bp.api.class_is_assignable_from(bp.types[BT_TRANSFORM],bp.api.object_get_class(rect))){
  *reason="dress_dialog_rect_unavailable";return NULL;
 }
 if(note)return rect;
 if(!dress_detail_type || !gacha_enabled)return NULL;
 Il2CppObject* root=component_get_game_object(rect);
 Il2CppReflectionType* type=p_type_get_object(p_class_get_type(dress_detail_type));
 Il2CppArray* details=root && type?gameobject_get_components(root,type,0,1,0,0,NULL):NULL;
 *reason="dialog_without_active_dress_detail";
 if(!details || details->max_length!=1)return NULL;
 Il2CppObject* detail=details->vector[0];uint8_t enabled=0;
 if(!detail || !geometry_read(gacha_enabled,detail,NULL,bp.types[BT_BOOL],&enabled,1) || !enabled)return NULL;
 return detail;
}
static int32_t dress_instance(Il2CppObject* obj){
 int32_t id=0;
 return obj && geometry_read(bp.method[BM_INSTANCE],obj,NULL,bp.types[BT_INT],&id,sizeof id)?id:0;
}
static FieldInfo* dress_instance_reference(Il2CppClass* klass,const char* name){
 HMODULE module=GetModuleHandleW(L"GameAssembly.dll");
 FieldInfo* (*field)(Il2CppClass*,const char*)=(void*)GetProcAddress(module,"il2cpp_class_get_field_from_name");
 int (*flags)(FieldInfo*)=(void*)GetProcAddress(module,"il2cpp_field_get_flags");
 if(!klass || !field || !flags || !il2cpp_field_get_type || !il2cpp_field_get_value_object)return NULL;
 FieldInfo* slot=field(klass,name);
 if(!slot || (flags(slot)&(16|64)))return NULL;
 const Il2CppType* t=il2cpp_field_get_type(slot);
 return t && !bp.api.type_is_byref(t) && bp.api.type_get_type(t)==18?slot:NULL;
}
static const char* dress_display(Il2CppObject* host,bool outgame,bool note){
 bool direct=note;
 if(outgame && (component_is(host,"Gallop","DressChangeViewController") ||
                component_is(host,"Gallop","CharacterNoteMainViewController"))){
  static Il2CppClass* controller_type;static FieldInfo* view_field;
  Il2CppClass* klass=bp.api.object_get_class(host);
  if(controller_type!=klass){
   controller_type=klass;view_field=dress_instance_reference(klass,"_view");
  }
  host=view_field?il2cpp_field_get_value_object(view_field,host):NULL;
  if(!host)return "preview_view_unavailable";
  if(note){
   if(!bp.api.class_is_assignable_from(bp.types[BT_BEHAVIOUR],bp.api.object_get_class(host)))return "note_view_unavailable";
  }else if(!component_is(host,"Gallop","DressChangeView"))return "dress_view_unavailable";
  uint8_t active=0;
  if(!geometry_read(gacha_enabled,host,NULL,bp.types[BT_BOOL],&active,1) || !active)return "dress_view_inactive";
  direct=true;
 }
 if(!dress_texture || !gacha_enabled || !bp.api.class_is_assignable_from(bp.types[BT_COMPONENT],bp.api.object_get_class(host)))return "dress_ui_unbound";
 Il2CppObject* go=component_get_game_object(host);
 Il2CppReflectionType* type=p_type_get_object(p_class_get_type(dress_raw_image));
 Il2CppArray* images=go && type?gameobject_get_components(go,type,0,1,0,0,NULL):NULL;
 if(!images || images->max_length>64)return "dress_image_list_unavailable";
 int32_t target=0;
 for(uintptr_t i=0;i<images->max_length;i++){
  Il2CppObject* image=images->vector[i];uint8_t enabled=0;
  if(!geometry_read(gacha_enabled,image,NULL,bp.types[BT_BOOL],&enabled,1))return "dress_image_unavailable";
  if(!enabled)continue;
  Il2CppException* exc=NULL;Il2CppObject* tex=bp.api.runtime_invoke(dress_texture,image,NULL,&exc);
  if(exc)return "dress_texture_unavailable";
  if(!tex || !bp.api.class_is_assignable_from(bp.types[BT_RT],bp.api.object_get_class(tex)))continue;
  int32_t id=dress_instance(tex);
  if(!id || (target && id!=target))return "dress_texture_ambiguous";
  target=id;
 }
 if(!target && !direct)return "dress_no_display_target";
 dress_host_id=dress_instance(host);dress_texture_id=target;
 return !dress_host_id?"dress_host_unavailable":target?"dress_display_target":"dress_direct_display";
}
static Il2CppObject* preview_model_camera(Il2CppArray* cameras,Scene scene,int32_t* mask,const char** result);
static bool preview_model_allowed;
static Il2CppObject* dress_writer(Il2CppArray* cameras,Scene scene,int32_t* source_mask,const char** result){
 static void (*camera_scene)(Il2CppObject*,Scene*);
 static const MethodInfo* display;
 static bool bound;
 bool direct=dress_texture_id==0;
 if(direct && !bound){
  bound=true;
  camera_scene=(void*)p_resolve_icall("UnityEngine.GameObject::get_scene_Injected(UnityEngine.SceneManagement.Scene&)");
  display=geometry_method(bp.types[BT_CAMERA],"get_targetDisplay",bp.types[BT_INT],NULL);
 }
 *result="dress_writer_unavailable";
 if(direct && (!camera_scene || !display)){*result="dress_display_contract_unavailable";return NULL;}
 unsigned passed=0;
 Il2CppObject* match=NULL;
 for(uintptr_t i=0;cameras && i<cameras->max_length;i++){
  Il2CppObject* camera=cameras->vector[i];uint8_t enabled=0,ortho=1;
  if(!geometry_read(gacha_enabled,camera,NULL,bp.types[BT_BOOL],&enabled,1))return NULL;
  if(!enabled)continue;
  Il2CppException* exc=NULL;Il2CppObject* target=bp.api.runtime_invoke(bp.method[BM_TARGET_GET],camera,NULL,&exc);
  if(exc)return NULL;
  if(direct){
   Il2CppObject* go=component_get_game_object(camera);Scene owner_scene={0};
   if(!go)return NULL;
   camera_scene(go,&owner_scene);
   if(owner_scene.handle!=scene.handle)continue;
   passed|=1;
   if(target)continue;
   passed|=2;
   int32_t index=-1;
   if(!geometry_read(display,camera,NULL,bp.types[BT_INT],&index,sizeof index))return NULL;
   if(index!=0)continue;
   passed|=4;
  }else{
   if(!target)continue;
   int32_t id=dress_instance(target);if(!id)return NULL;
   if(id!=dress_texture_id)continue;
  }
  if(!geometry_read(bp.method[BM_ORTHO],camera,NULL,bp.types[BT_BOOL],&ortho,1))return NULL;
  if(ortho){if(direct)continue;return NULL;}
  passed|=8;
  int32_t mask=0;
  if(!geometry_read(bp.method[BM_CULL_GET],camera,NULL,bp.types[BT_INT],&mask,sizeof mask))return NULL;
  /* A texture-backed preview can render on another layer (observed mask 256).
     For that route the writer must already match the displayed texture ID. */
  if(!mask || (direct && !(mask&(1<<13))))continue;
  if(match){*result="dress_writer_ambiguous";return NULL;}
  match=camera;*source_mask=mask;
 }
 *result=match?(direct?"dress_world_camera_matched":"dress_texture_camera_matched"):
                (!direct?"dress_no_texture_writer":(passed&8)?"dress_no_character_layer":
                 (passed&4)?"dress_no_screen_perspective":(passed&2)?"dress_no_primary_display":
                 (passed&1)?"dress_only_offscreen_cameras":"dress_no_scene_camera");
 return match;
}
static BpSourceKind bp_foreground_source(int32_t scene_handle,bool allow_home){
 static int32_t last_scene;static BpSourceKind last_kind;
 static int last_frame=-1;static bool eligible;static Il2CppClass* last_hub;
 static const MethodInfo* child_getter;static char last_reason[160];
 int frame=-1;
 if(!bp_get(BM_FRAME,NULL,&frame,sizeof(frame)) || frame<0){dress_texture_id=dress_host_id=0;last_frame=-1;return BP_SOURCE_NONE;}
 if(frame==last_frame && scene_handle==last_scene)return last_kind;
 last_scene=scene_handle;
 last_frame=frame;preview_model_allowed=!allow_home;eligible=false;dress_texture_id=dress_host_id=0;Il2CppObject* dress=NULL;Il2CppObject* note_view=NULL;
 const char* reason="view_unavailable";Il2CppException* exc=NULL;
 Il2CppObject* manager=home_instance && il2cpp_field_get_value_object?il2cpp_field_get_value_object(home_instance,NULL):NULL;
 if(manager && home_current_view && bp.api.class_is_assignable_from(home_scene_manager,bp.api.object_get_class(manager))){
  Il2CppObject* view=bp.api.runtime_invoke(home_current_view,manager,NULL,&exc);
  if(view && !exc){
   Il2CppClass* k=bp.api.object_get_class(view);
   if(k!=last_hub){last_hub=k;child_getter=home_ref_getter(k,"get_ChildCurrentController",false);}
   if(child_getter)view=bp.api.runtime_invoke(child_getter,view,NULL,&exc);
   if(view && !exc){k=bp.api.object_get_class(view);reason=bp.api.class_get_name(k);
    if(!allow_home && component_is(view,"Gallop","CharacterNoteMainViewController"))note_view=view;
    if(note_view || dress_host(view) || (strcmp(reason,"HomeViewController") && current_dress_page(manager)))dress=view;
    eligible=allow_home && !strcmp(reason,"HomeViewController") && !strcmp(bp.api.class_get_namespace(k),"Gallop");
   }
  }
 }
 if(eligible){const char* page=active_gacha_page(manager);if(page){eligible=false;reason=page;}}
 if(!exc && home_front_dialog){
  Il2CppObject* dialog=bp.api.runtime_invoke(home_front_dialog,NULL,NULL,&exc);
  if(dialog || exc){
   eligible=false;dress=NULL;reason=dialog?"front_dialog":"dialog_unavailable";
   if(dialog && !exc){
    Il2CppClass* k=bp.api.object_get_class(dialog);
    static Il2CppClass* wrapper;static const MethodInfo* content;
    if(k!=wrapper){wrapper=k;content=home_ref_getter(k,"get_CurrentDialogObj",false);}
    Il2CppObject* obj=dress_host(dialog)?dialog:content?bp.api.runtime_invoke(content,dialog,NULL,&exc):NULL;
    if(obj && !exc){reason=bp.api.class_get_name(bp.api.object_get_class(obj));dress=dress_dialog_content(obj,note_view!=NULL,&reason);}
   }
  }
 }
 if(dress && home_front_dialog && !exc){
  bool character_note_source=note_view!=NULL;
  reason=dress_display(dress,!allow_home,character_note_source);
  /* An info dialog can sit over a texture-backed character preview. Look
     through to the active parent view only when the front has no texture. */
  if(note_view && note_view!=dress && dress_host_id && !dress_texture_id){
   int32_t front_id=dress_host_id;
   dress_host_id=0;
   (void)dress_display(note_view,true,true);
   dress_host_id=front_id;
  }
  if(character_note_source && dress_host_id)reason=dress_texture_id?"note_display_target":"note_direct_display";
 }
 last_kind=dress_host_id?BP_SOURCE_DRESS:eligible?BP_SOURCE_HOME:BP_SOURCE_NONE;
 char stage[160];snprintf(stage,sizeof stage,"%s_view_%s",allow_home?"home":"outgame",reason);
 if(strcmp(last_reason,stage)){
  snprintf(last_reason,sizeof last_reason,"%s",stage);char detail[450];
  telemetry_snprintf(detail,sizeof detail,"\"stage\":\"%s\",\"reason\":\"%s\",\"eligible\":%s",stage,reason,last_kind!=BP_SOURCE_NONE?"true":"false");
  write_record("OWN-BASE","owned_context_transition",detail);
 }
 return last_kind;
}
static void bp_bind(const Il2CppImage* core,const Il2CppImage* game,const Il2CppImage* corlib,
 const Il2CppImage* urp) {
 story_geometry_core=core; bp.api=request_dispatch_api; bp.owner_thread=GetCurrentThreadId();
 HMODULE module=GetModuleHandleW(L"GameAssembly.dll");
 bp.object_new=(Il2CppObject* (*)(Il2CppClass*))(void*)GetProcAddress(module,"il2cpp_object_new");
 bp.string_new=(Il2CppString* (*)(const char*))(void*)GetProcAddress(module,"il2cpp_string_new");
 bp.field_get=(void (*)(Il2CppObject*,FieldInfo*,void*))(void*)GetProcAddress(module,"il2cpp_field_get_value");
 bp.field_set=(void (*)(Il2CppObject*,FieldInfo*,void*))(void*)GetProcAddress(module,"il2cpp_field_set_value");
 if(!bp.object_new || !bp.string_new || !bp.field_get || !bp.field_set || !component_get_game_object) {
  bp_fail("owned_exports"); bp_observe_partial_binding(&bp_binding_preflight); return;
 }
#define BP_REQUIRE(ret,name,args) if(!bp.api.name) { bp.stage="il2cpp_" #name; bp_fail("owned_exports"); bp_observe_partial_binding(&bp_binding_preflight); return; }
 RD_API_LIST(BP_REQUIRE)
#undef BP_REQUIRE
#define BP_TYPE(id,image,ns,name) do { bp.types[id]=(image)?p_class_from_name(image,ns,name):NULL; \
 if(!bp.types[id]) { bp.stage=ns "." name; bp_fail("owned_required_type"); bp_observe_partial_binding(&bp_binding_preflight); return; } } while(0)
 BP_TYPE(BT_BOOL,corlib,"System","Boolean"); BP_TYPE(BT_INT,corlib,"System","Int32");
 BP_TYPE(BT_FLOAT,corlib,"System","Single"); BP_TYPE(BT_STRING,corlib,"System","String");
 BP_TYPE(BT_TYPE,corlib,"System","Type"); BP_TYPE(BT_INTPTR,corlib,"System","IntPtr");
 BP_TYPE(BT_OBJECT,core,"UnityEngine","Object"); BP_TYPE(BT_GO,core,"UnityEngine","GameObject");
 BP_TYPE(BT_COMPONENT,core,"UnityEngine","Component"); BP_TYPE(BT_BEHAVIOUR,core,"UnityEngine","Behaviour");
 BP_TYPE(BT_CAMERA,core,"UnityEngine","Camera"); BP_TYPE(BT_TEXTURE,core,"UnityEngine","Texture");
 BP_TYPE(BT_RT,core,"UnityEngine","RenderTexture"); BP_TYPE(BT_TRANSFORM,core,"UnityEngine","Transform");
 BP_TYPE(BT_V3,core,"UnityEngine","Vector3"); BP_TYPE(BT_Q4,core,"UnityEngine","Quaternion");
 BP_TYPE(BT_M4,core,"UnityEngine","Matrix4x4"); BP_TYPE(BT_RT_FORMAT,core,"UnityEngine","RenderTextureFormat");
 BP_TYPE(BT_COLOR,core,"UnityEngine","Color");
 BP_TYPE(BT_CLEAR_FLAGS,core,"UnityEngine","CameraClearFlags"); BP_TYPE(BT_STEREO_MASK,core,"UnityEngine","StereoTargetEyeMask");
 BP_TYPE(BT_TIME,core,"UnityEngine","Time"); BP_TYPE(BT_DATA,game,"Gallop.RenderPipeline","CameraData");
 BP_TYPE(BT_SYSTEMINFO,core,"UnityEngine","SystemInfo");
 BP_TYPE(BT_GRAPHICS_API,core,"UnityEngine.Rendering","GraphicsDeviceType");
 BP_TYPE(BT_ADDITIONAL,urp,"UnityEngine.Rendering.Universal","UniversalAdditionalCameraData");
 BP_TYPE(BT_RENDER_TYPE,urp,"UnityEngine.Rendering.Universal","CameraRenderType");
#undef BP_TYPE
 if(!bp_bind_required(&bp_binding_preflight)) return;
 typedef FieldInfo* (*fields_fn)(Il2CppClass*,void**);
 fields_fn fields=(fields_fn)(void*)GetProcAddress(module,"il2cpp_class_get_fields");
 const char* (*field_name)(FieldInfo*)=(const char* (*)(FieldInfo*))(void*)GetProcAddress(module,"il2cpp_field_get_name");
 const Il2CppType* (*field_type)(FieldInfo*)=(const Il2CppType* (*)(FieldInfo*))(void*)GetProcAddress(module,"il2cpp_field_get_type");
 int (*field_flags)(FieldInfo*)=(int (*)(FieldInfo*))(void*)GetProcAddress(module,"il2cpp_field_get_flags");
 Il2CppClass* (*field_parent)(FieldInfo*)=(Il2CppClass* (*)(FieldInfo*))(void*)GetProcAddress(module,"il2cpp_field_get_parent");
 bool (*class_is_enum)(const Il2CppClass*)=(bool (*)(const Il2CppClass*))(void*)GetProcAddress(module,"il2cpp_class_is_enum");
 bp.stage="CameraData._isInitialize";
 if(!fields || !field_name || !field_type || !field_flags || !field_parent || !class_is_enum) { bp_fail("owned_field_exports"); return; }
 void* it=NULL; FieldInfo* f; unsigned n=0;
 while((f=fields(bp.types[BT_DATA],&it))) {
  if(++n>256) { bp_fail("owned_field_limit"); return; }
  const char* name=field_name(f);
  if(!name || !name[0]) { bp_fail("owned_field_name_unreadable"); return; }
  if(!strcmp(name,"IsAdjustFieldOfView")) {
   if(bp.fov_field || field_parent(f)!=bp.types[BT_DATA] ||
    (field_flags(f)&0x77)!=6 || !bp_type_match(field_type(f),BT_BOOL) ||
    class_is_enum(bp.types[BT_BOOL])) { bp_fail("owned_fov_field_contract"); return; }
   bp.fov_field=f;
  }
  if(name && !strcmp(name,"_isInitialize") && field_parent(f)==bp.types[BT_DATA] &&
   (field_flags(f)&0x17)==1 && bp_type_match(field_type(f),BT_BOOL)) {
   if(bp.initialized_field) { bp_fail("owned_field_ambiguous"); return; } bp.initialized_field=f;
  }
 }
 if(!bp.initialized_field) { bp_fail("owned_initialized_field_missing"); return; }
 if(!bp.fov_field) { bp_fail("owned_fov_field_missing"); return; }
 if(!bp_native_provider_gate()) return;
 bp_cc_bind();
 bp_bind_home_view(game);
 bp.bound=true; bp_log("base_binding","bound_unallocated");
}
static bool bp_configure_eye(BpEye* e,unsigned index,float near_clip,float far_clip,int32_t mask) {
 uint8_t no=0,yes=1; int32_t base=0;
 Il2CppObject *camera=bp_target(e->camera),*additional=bp_target(e->additional);
 float aspect=(float)bp.width/(float)bp.height,depth=-1000.0f+(float)index;

 if(!bp_set(BM_ENABLED_SET,camera,&no) || !bp_set(BM_TARGET_SET,camera,bp_target(e->rt)) ||
  !bp_set(BM_ASPECT,camera,&aspect) || !bp_set(BM_DEPTH,camera,&depth) ||
  !bp_set(BM_NEAR_SET,camera,&near_clip) || !bp_set(BM_FAR_SET,camera,&far_clip) ||
  !bp_set(BM_CULL_SET,camera,&mask) || !bp_set(BM_MSAA,camera,&no) ||
  !bp_set(BM_RENDER_TYPE_SET,additional,&base) || !bp_set(BM_SHADOWS,additional,&yes)) return false;
 uint8_t post=bp.post?1:0;
 if(!bp_background_apply(e)) return false;
 if(!bp_set(BM_POST,additional,&post) || !bp_stack_empty(additional) || !bp_apply_xr_optout(e)) return false;
 int32_t render_type=-1; if(!bp_get(BM_RENDER_TYPE_GET,additional,&render_type,sizeof(render_type)) || render_type!=0) return false;
 if(bp.source_kind==BP_SOURCE_DRESS && bp.preview_near_adjusted){
  float applied_near=0,applied_far=0;
  if(!bp_get(BM_NEAR_GET,camera,&applied_near,sizeof applied_near) ||
     !bp_get(BM_FAR_GET,camera,&applied_far,sizeof applied_far) ||
     !isfinite(applied_near) || !isfinite(applied_far) || applied_near<=0 || applied_far<=applied_near)return false;
  build_unity_eye_projection(&bp.optics.fov[index],applied_near,applied_far,&e->projection);
  if(e->data){
   char detail[240];
   telemetry_snprintf(detail,sizeof detail,"\"stage\":\"preview_clip_%u\",\"generation\":%llu,\"requested_near\":%.7g,\"applied_near\":%.7g,\"near_m\":%.7g,\"far\":%.7g",
    index,(unsigned long long)bp.c.identity.generation,near_clip,applied_near,applied_near*bp.scale,applied_far);
   write_record("MS-IM-515","owned_context_transition",detail);
  }
 }
 if(!bp_set(BM_POSITION_SET,bp_target(e->transform),&e->position) ||
  !bp_set(BM_ROTATION_SET,bp_target(e->transform),&e->rotation) ||
  !bp_set(BM_PROJECTION,camera,&e->projection)) return false;
 if(e->data) bp_projection_checkpoint(BP_PR_SET,index,-1);
 if(bp_call(BM_TARGET_GET,camera,NULL)!=bp_target(e->rt)) { bp_fail("owned_target_replaced"); return false; }
 return !bp.failed;
}
/* Unknown source policy keeps the established eye camera default. Never
   mutate the original Home camera or attach an authored Live effect to it. */
static bool home_authored_post;
static volatile LONG home_vfx_fields,home_active_fields;
/* Only CameraData attached to a mod-owned eye may receive a private parameter
   reference. A source parameter and all its reference-valued children stay read-only. */
typedef struct { const char* slot; const char* members[5]; } HomeEffectSpec;
static const HomeEffectSpec home_effect_spec[VFX_COUNT]={
    {"IsEnable",{NULL}},
    {"Aura",{NULL}},
    {"BlurOptimized",{"IsEnable"}},
    {"DofDiffuionBloomOverlay",{"IsEnableDof","IsDisableDofTemporary","IsEnableOldDof","IsEnableDiffusion","IsEnableBloom"}},
    {"DofDiffuionBloomOverlay",{"IsEnableDof","IsDisableDofTemporary"}},
    {"DofDiffuionBloomOverlay",{"IsEnableDiffusion"}},
    {"DofDiffuionBloomOverlay",{"IsEnableBloom"}},
    {"TiltShift",{"MaxBlurSize"}},
    {"RadialBlur",{"RadialBlurPower"}},
    {"BgBlur",{"IsEnable"}},
    {"SunShafts",{"IsEnable"}},
    {"IndirectLightShafts",{"IsEnable"}},
    {"TransmittedLight",{"IsEnable"}},
    {"GlobalFog",{"IsDistanceFog","IsHeightFog"}},
    {"ChromaticAberration",{"IsEnable"}},
    {"ToneCurve",{"IsEnable"}},
    {"Exposure",{"IsEnable"}},
    {"ColorCorrection",{"IsEnable"}},
    {"ColorGrading",{"IsEnable"}},
    {"Fluctuation",{"IsEnable"}},
    {"LensDistortion",{"Intensity"}},
    {"Vortex",{"IsEnable"}},
    {"FilmRoll",{"IsEnable"}},
    {"Hatching",{"BlendAlpha"}},
    {"LetterBox",{"IsEnable"}},
    {"RainSplash",{"IsEnable"}},
};
typedef struct {
    Il2CppClass* klass;
    LONG supported;
    FieldInfo* slot[VFX_COUNT];
    FieldInfo* members[VFX_COUNT][5];
} HomeEffectLayout;
typedef struct { uint32_t camera, original, copy; FieldInfo* outer; } HomeEffectLease;
static HomeEffectLayout home_layout[2];
static HomeEffectLease home_effect_lease[2];
static volatile LONG home_vfx_generation,home_vfx_failed;
static int (*home_field_flags)(FieldInfo*);
static void (*home_set_reference)(Il2CppObject*,FieldInfo*,Il2CppObject*);
static const MethodInfo* home_clone_method;

static BOOL home_vfx_api(void){
    if(!home_field_flags || !home_set_reference){
        HMODULE module=GetModuleHandleW(L"GameAssembly.dll");
        home_field_flags=(void*)GetProcAddress(module,"il2cpp_field_get_flags");
        home_set_reference=(void*)GetProcAddress(module,"il2cpp_field_set_value_object");
    }
    return home_field_flags && home_set_reference && il2cpp_field_get_value_object &&
        il2cpp_field_get_value && il2cpp_field_get_type && il2cpp_type_get_type &&
        il2cpp_field_set_value && il2cpp_object_unbox && il2cpp_class_get_parent;
}
static BOOL home_writable_field(FieldInfo* field,int kind){
    if(!field || (home_field_flags(field)&(0x10|0x20|0x40)))return FALSE;
    const Il2CppType* type=il2cpp_field_get_type(field);
    return type && !bp.api.type_is_byref(type) && il2cpp_type_get_type(type)==kind;
}
static BOOL home_float_member(const char* name){
    return !strcmp(name,"MaxBlurSize") || !strcmp(name,"RadialBlurPower") ||
           !strcmp(name,"Intensity") || !strcmp(name,"BlendAlpha");
}
static void home_effect_layout(HomeEffectLayout* layout,Il2CppClass* klass){
    if(layout->klass==klass)return;
    memset(layout,0,sizeof(*layout));layout->klass=klass;
    for(int i=2;i<VFX_COUNT;i++){
        const HomeEffectSpec* spec=&home_effect_spec[i];
        FieldInfo* slot=find_vfx_field(klass,spec->slot);
        if(!home_writable_field(slot,0x11))continue; /* inline value, never a shared child object */
        Il2CppClass* nested=bp.api.class_from_type(il2cpp_field_get_type(slot));
        if(!nested)continue;
        BOOL complete=TRUE;
        for(int n=0;n<5 && spec->members[n];n++){
            FieldInfo* member=find_vfx_field(nested,spec->members[n]);
            if(!home_writable_field(member,home_float_member(spec->members[n])?12:2)){complete=FALSE;break;}
            layout->members[i][n]=member;
        }
        if(complete){layout->slot[i]=slot;layout->supported|=1L<<i;}
    }
}
static void home_release_effect(HomeEffectLease* lease){
    if(lease->camera && lease->original && lease->copy){
        Il2CppObject* camera=bp.api.gchandle_get_target(lease->camera);
        Il2CppObject* clone=bp.api.gchandle_get_target(lease->copy);
        if(camera && clone && il2cpp_field_get_value_object(lease->outer,camera)==clone)
            home_set_reference(camera,lease->outer,bp.api.gchandle_get_target(lease->original));
    }
    if(lease->camera)bp.api.gchandle_free(lease->camera);
    if(lease->original)bp.api.gchandle_free(lease->original);
    if(lease->copy)bp.api.gchandle_free(lease->copy);
    *lease=(HomeEffectLease){0};
}
static void home_reset_effects(void){
    InterlockedExchange(&home_vfx_failed,0);
    InterlockedExchange(&home_vfx_generation,0);
    InterlockedExchange(&home_vfx_fields,0);
    InterlockedExchange(&home_active_fields,0);
    for(int e=0;e<2;e++)home_release_effect(&home_effect_lease[e]);
}
static BOOL home_can_clone(Il2CppClass* klass){
    Il2CppClass* parent=klass;
    while(parent && il2cpp_class_get_parent(parent)){
        if(!strcmp(il2cpp_class_get_namespace(parent),"UnityEngine"))return FALSE;
        parent=il2cpp_class_get_parent(parent);
    }
    if(!parent || strcmp(il2cpp_class_get_namespace(parent),"System") ||
       strcmp(il2cpp_class_get_name(parent),"Object"))return FALSE;
    if(!home_clone_method){
        const MethodInfo* m=p_class_get_method(parent,"MemberwiseClone",0);
        uint32_t impl=0;
        if(!m || bp.api.method_get_class(m)!=parent || bp.api.method_get_param_count(m)!=0 ||
           (bp.api.method_get_flags(m,&impl)&(0x10|0x400)) ||
           bp.api.class_from_type(bp.api.method_get_return_type(m))!=parent)return FALSE;
        home_clone_method=m;
    }
    return TRUE;
}
static BOOL home_prepare_effects(unsigned eye,Il2CppObject* camera,Il2CppObject* original,
        FieldInfo* outer,Il2CppObject* authored,LONG disabled){
    HomeEffectLease* lease=&home_effect_lease[eye];
    Il2CppException* error=NULL;
    Il2CppObject* clone=bp.api.runtime_invoke(home_clone_method,authored,NULL,&error);
    if(error || !clone || clone==original || clone==authored || clone->klass!=original->klass)return FALSE;
    lease->outer=outer;lease->camera=bp.api.gchandle_new(camera,false);
    lease->original=bp.api.gchandle_new(original,false);lease->copy=bp.api.gchandle_new(clone,false);
    if(!lease->camera || !lease->original || !lease->copy)return FALSE;
    HomeEffectLayout* layout=&home_layout[eye];
    for(int i=2;i<VFX_COUNT;i++)if(disabled&(1L<<i)){
        Il2CppObject* boxed=il2cpp_field_get_value_object(layout->slot[i],clone);
        if(!boxed)return FALSE;
        for(int n=0;n<5 && layout->members[i][n];n++){
            uint32_t zero=0; uint8_t force=!strcmp(home_effect_spec[i].members[n],"IsDisableDofTemporary");
            void* value=home_float_member(home_effect_spec[i].members[n])?(void*)&zero:(void*)&force;
            il2cpp_field_set_value(boxed,layout->members[i][n],value);
        }
        void* data=il2cpp_object_unbox(boxed);
        if(!data)return FALSE;
        il2cpp_field_set_value(clone,layout->slot[i],data);
    }
    return TRUE;
}
/* Read the authored state before masking it. A field name alone cannot make a
   dormant effect selectable. Sampling is bounded independently of frame rate. */
static LONG home_active_effects(HomeEffectLayout* layout,Il2CppObject* original){
    FieldInfo* master=find_vfx_field(original->klass,"IsEnable");
    uint8_t enabled=0;
    if(!home_writable_field(master,2))return 0;
    il2cpp_field_get_value(original,master,&enabled);
    if(!enabled)return 0;
    LONG active=0;
    for(int i=2;i<VFX_COUNT;i++)if(layout->supported&(1L<<i)){
        Il2CppObject* boxed=il2cpp_field_get_value_object(layout->slot[i],original);
        if(!boxed)continue;
        BOOL on=FALSE,temporary=FALSE;
        for(int n=0;n<5 && layout->members[i][n];n++){
            const char* name=home_effect_spec[i].members[n];
            if(home_float_member(name)){
                float value=0;il2cpp_field_get_value(boxed,layout->members[i][n],&value);
                on|=isfinite(value) && value!=0;
            }else{
                uint8_t value=0;il2cpp_field_get_value(boxed,layout->members[i][n],&value);
                if(!strcmp(name,"IsDisableDofTemporary"))temporary=value!=0;
                else on|=value!=0;
            }
        }
        if(on && !(i==4 && temporary))active|=1L<<i;
    }
    return active;
}
static void home_update_effects(int32_t frame,Il2CppObject* source_data){
    static int32_t last_frame=-1;static LONG last_generation;
    LONG generation=(LONG)bp.c.identity.generation;
    if(last_frame==frame && last_generation==generation)return;
    last_frame=frame;last_generation=generation;
    for(int e=0;e<2;e++)home_release_effect(&home_effect_lease[e]);
    Il2CppObject* camera[2];Il2CppObject* original[2];FieldInfo* outer[2];
    static ULONGLONG next_sample;
    static LONG available, sampled_generation;
    BOOL sample=generation!=sampled_generation || GetTickCount64()>=next_sample;
    LONG supported=-1,active=-1;
    if(!home_vfx_api())goto unavailable;
    Il2CppObject* authored=NULL;
    if(source_data && source_data->klass==bp.types[BT_DATA]){
        FieldInfo* src_field=find_vfx_field(source_data->klass,"ImageEffectParameter");
        if(home_writable_field(src_field,0x12))authored=il2cpp_field_get_value_object(src_field,source_data);
    }
    for(int e=0;e<2;e++){
        camera[e]=bp_target(bp.eye[e].data);
        if(!camera[e] || camera[e]->klass!=bp.types[BT_DATA])goto unavailable;
        outer[e]=find_vfx_field(camera[e]->klass,"ImageEffectParameter");
        if(!home_writable_field(outer[e],0x12))goto unavailable;
        original[e]=il2cpp_field_get_value_object(outer[e],camera[e]);
        if(!original[e] || !home_can_clone(original[e]->klass))goto unavailable;
        if(authored && authored->klass!=original[e]->klass)goto unavailable;
        home_effect_layout(&home_layout[e],original[e]->klass);
        supported &= home_layout[e].supported;
        if(sample)active &= home_active_effects(&home_layout[e],authored?authored:original[e]);
    }
    if(sample){
        available=active;sampled_generation=generation;next_sample=GetTickCount64()+750;
    }
    LONG disabled=supported & InterlockedCompareExchange(&home_vfx_disabled,0,0);
    static LONG failed_generation,failed_mask;
    BOOL blocked=generation==failed_generation && disabled==failed_mask;
    /* With no requested overrides, keep the initialized eye parameter object. */
    if(disabled && bp.post && !blocked){
        for(int e=0;e<2;e++)if(!home_prepare_effects(e,camera[e],original[e],outer[e],authored?authored:original[e],disabled))goto rejected;
        for(int e=0;e<2;e++){
            Il2CppObject* clone=bp.api.gchandle_get_target(home_effect_lease[e].copy);
            home_set_reference(camera[e],outer[e],clone);
            if(il2cpp_field_get_value_object(outer[e],camera[e])!=clone)goto rejected;
        }
    }
    if(!blocked){failed_generation=0;failed_mask=0;}
    InterlockedExchange(&home_vfx_failed,blocked?(disabled|1):0);
    /* A saved OFF remains reachable after an authored animation ends. */
    LONG visible=supported;
    InterlockedExchange(&home_active_fields,bp.post?available:0);
    static LONG reported_generation,reported_mask,reported_disabled,reported_post,reported_active;
    if(generation!=reported_generation || visible!=reported_mask || disabled!=reported_disabled || bp.post!=reported_post || reported_active!=available){
        char receipt[180];snprintf(receipt,sizeof receipt,"\"generation\":%ld,\"both_eyes\":%ld,\"disabled\":%ld,\"post\":%s,\"authored_source\":%s,\"active_bindings\":%ld",
            generation,visible,disabled,bp.post?"true":"false",authored?"true":"false",available);
        write_record("VFX-126","home_eye_vfx_state",receipt);
        reported_generation=generation;reported_mask=visible;reported_disabled=disabled;reported_post=bp.post;reported_active=available;
    }
    InterlockedExchange(&home_vfx_fields,visible);
    InterlockedExchange(&home_vfx_generation,generation);
    return;
rejected:
    for(int e=0;e<2;e++)home_release_effect(&home_effect_lease[e]);
    failed_generation=generation;failed_mask=disabled;
    InterlockedExchange(&home_vfx_failed,disabled|1);
    InterlockedExchange(&home_vfx_fields,supported & (available|disabled));
    InterlockedExchange(&home_vfx_generation,generation);
    bp_log("home_eye_vfx_rejected","paired_override_reverted");
    return;
unavailable:
    sampled_generation=0;
    home_reset_effects();
}

static volatile LONG home_source_post_status=-1,home_eye_post_status=-1;
static const MethodInfo *home_mask_get,*home_mask_set,*home_trigger_get,*home_trigger_set;
static Il2CppClass* home_layer_mask_class;
static bool home_volume_bound,home_has_mask,home_has_trigger;
static int32_t home_volume_mask;
static void home_copy_volume(Il2CppObject* target,Il2CppObject* trigger){
 if(!target)return;
 Il2CppException* error=NULL;
 if(home_has_mask)bp.api.runtime_invoke(home_mask_set,target,(void*[]){&home_volume_mask},&error);
 error=NULL;
 if(home_has_trigger)bp.api.runtime_invoke(home_trigger_set,target,(void*[]){trigger},&error);
}
static ULONGLONG home_next_post_sample;
static void home_sample_post(Il2CppObject* source,bool first) {
 static const MethodInfo* getter;
 if(first)InterlockedExchange(&home_eye_post_status,-1);
 if(first){home_authored_post=true;home_has_mask=false;home_has_trigger=false;InterlockedExchange(&home_source_post_status,-1);}
 if(!home_volume_bound){
  home_volume_bound=true;
  home_layer_mask_class=p_class_from_name(story_geometry_core,"UnityEngine","LayerMask");
  if(home_layer_mask_class){
   home_mask_get=geometry_method(bp.types[BT_ADDITIONAL],"get_volumeLayerMask",home_layer_mask_class,NULL);
   home_mask_set=geometry_method(bp.types[BT_ADDITIONAL],"set_volumeLayerMask",bp.types[BT_VOID],home_layer_mask_class);
  }
  home_trigger_get=geometry_method(bp.types[BT_ADDITIONAL],"get_volumeTrigger",bp.types[BT_TRANSFORM],NULL);
  home_trigger_set=geometry_method(bp.types[BT_ADDITIONAL],"set_volumeTrigger",bp.types[BT_VOID],bp.types[BT_TRANSFORM]);
 }
 if(!getter)getter=geometry_method(bp.types[BT_ADDITIONAL],"get_renderPostProcessing",bp.types[BT_BOOL],NULL);
 HomeOwnerComponents parts;
 if(collect_home_owner_components(source,&parts)){
  Il2CppException* error=NULL;
  Il2CppObject* data=bp.api.runtime_invoke(bp.method[BM_DATA_ADDITIONAL],parts.camera_data,NULL,&error);
  uint8_t value=0;
  if(!error && data && bp.api.object_get_class(data)==bp.types[BT_ADDITIONAL]){
   if(getter && geometry_read(getter,data,NULL,bp.types[BT_BOOL],&value,sizeof(value))){
    home_authored_post=value!=0;InterlockedExchange(&home_source_post_status,value!=0);
   }
   home_has_mask=home_mask_get && home_mask_set &&
       geometry_read(home_mask_get,data,NULL,home_layer_mask_class,&home_volume_mask,sizeof(home_volume_mask));
   Il2CppObject* trigger=NULL;
   home_has_trigger=home_trigger_get && home_trigger_set;
   if(home_has_trigger){
    trigger=bp.api.runtime_invoke(home_trigger_get,data,NULL,&error);
    home_has_trigger=!error;
    if(!trigger && !error)trigger=component_get_transform(source);
   }
   if(!first)for(unsigned i=0;i<2;i++)home_copy_volume(bp_target(bp.eye[i].additional),trigger);
  }
 }
 if(!first && bp.c.phase==BP_ACTIVE){
  int pair=0;
  for(unsigned i=0;i<2;i++){
   uint8_t on=0;Il2CppObject* additional=bp_target(bp.eye[i].additional);
   if(!additional || !geometry_read(getter,additional,NULL,bp.types[BT_BOOL],&on,sizeof(on))){pair=-1;break;}
   if(on)pair|=1<<i;
  }
  if(InterlockedExchange(&home_eye_post_status,pair)!=pair){
   char detail[100];snprintf(detail,sizeof detail,"\"requested\":%ld,\"eye_mask\":%d",InterlockedCompareExchange(&home_vfx_allowed,0,0),pair);
   write_record("VFX-124","home_eye_post_readback",detail);
  }
 }
 home_next_post_sample=GetTickCount64()+250;
}
static bool home_post_enabled(void){
 return InterlockedCompareExchange(&home_vfx_allowed,0,0)!=0;
}
static void bp_build(Il2CppObject* source,Scene scene,const EyeOptics* optics) {
 /* GetNativeTexturePtr is backend-specific. Establish D3D11 before treating
    its result as COM at all; the later Present also verifies actual device. */
 int32_t backend=-1;
 if(!bp_get(BM_BACKEND,NULL,&backend,sizeof(backend)) || backend!=2) { bp_fail("normal_camera_backend_not_d3d11"); return; }
 bp.backend_verified=true;
 if(!bp_native_provider_gate()) return;
 bp.source_root=bp_root(source); if(bp.failed) return;
 int32_t id=0; if(!bp_get(BM_INSTANCE,source,&id,sizeof(id)) || !id) { bp_fail("source_identity"); return; }
 LONG generation=InterlockedIncrement(&source_generation);
 if(generation<=0) { bp_fail("generation_exhausted"); return; }
 BpIdentity identity={(uint64_t)generation,(uint32_t)id,(uint64_t)generation,1,0,scene.handle,0};
 if(!bp_begin(&bp.c,identity)) return;
 bp.width=InterlockedCompareExchange(&requested_eye_width,0,0);
 bp.height=InterlockedCompareExchange(&requested_eye_height,0,0);
 bp.scale=world_scale;
 if(bp.source_kind==BP_SOURCE_HOME)home_sample_post(source,true);
 bp.post=bp.source_kind==BP_SOURCE_HOME?home_post_enabled():post_processing_enabled!=0; bp.optics=*optics;
 if(!bp_background_snapshot(source)) return;
 uint32_t source_transform=bp_root(bp_call(BM_TRANSFORM,source,NULL));bp.source_transform_root=source_transform;bp.source_pose_frame=-1;
 Vec3 position; Quat rotation; float near_clip=0,far_clip=0; int32_t mask=0; uint8_t ortho=1;
 if(bp.failed || !bp_get(BM_POSITION_GET,bp_target(source_transform),&position,sizeof(position)) ||
  !bp_get(BM_ROTATION_GET,bp_target(source_transform),&rotation,sizeof(rotation)) ||
  !bp_get(BM_NEAR_GET,source,&near_clip,sizeof(near_clip)) ||
  !bp_get(BM_FAR_GET,source,&far_clip,sizeof(far_clip)) || !bp_get(BM_CULL_GET,source,&mask,sizeof(mask)) ||
  !bp_get(BM_ORTHO,source,&ortho,sizeof(ortho))) return;
 if(ortho || !isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z) ||
  !isfinite(rotation.x) || !isfinite(rotation.y) || !isfinite(rotation.z) || !isfinite(rotation.w) ||
  !isfinite(near_clip) || !isfinite(far_clip) || near_clip<=0 || far_clip<=near_clip ||
  !isfinite(bp.scale) || bp.scale<=0 || !isfinite(optics->half_ipd_m) || optics->half_ipd_m<=0) {
  bp_fail("fixed_snapshot_contract"); return;
 }
 bp_cc_snapshot(near_clip,far_clip,mask);
 /* The authored GateCamera narrows to 500 while CourseCamera draws to 2000. */
 if(bp.source_kind==BP_SOURCE_RACE)far_clip=fmaxf(far_clip,2000.0f);
 bp.eye_mask=mask;
 bp.preview_near_adjusted=false;
 if(bp.source_kind==BP_SOURCE_DRESS && !preview_use_authored_clip){
  /* Apply the preview limit in physical metres at the current world scale. */
  const float requested=fminf(near_clip,0.025f/bp.scale);
  bp.preview_near_adjusted=requested<near_clip;
  near_clip=requested;
 }
 else if(bp.source_kind==BP_SOURCE_HOME || bp.source_kind==BP_SOURCE_RACE){
  float authored=near_clip;
  const float near_limit_m=bp.source_kind==BP_SOURCE_RACE ? 0.001f : 0.02f;
  near_clip=fminf(near_clip,near_limit_m/bp.scale);
  char d[144];snprintf(d,sizeof d,"\"source_near\":%.6g,\"eye_near\":%.6g,\"scale\":%.6g",authored,near_clip,bp.scale);
  write_record("OWN-BASE",bp.source_kind==BP_SOURCE_RACE?"race_near_clip":"home_near_clip",d);
 }
 for(unsigned eye=0;eye<2;++eye) {
  const XrFovf* f=&optics->fov[eye];
  if(!isfinite(f->angleLeft) || !isfinite(f->angleRight) || !isfinite(f->angleDown) || !isfinite(f->angleUp) ||
   f->angleLeft>=f->angleRight || f->angleDown>=f->angleUp || f->angleLeft<=-1.56f ||
   f->angleRight>=1.56f || f->angleDown<=-1.56f || f->angleUp>=1.56f) { bp_fail("fixed_fov_contract"); return; }
 }
 BpSourceEyePose initial_pose[2];
 if(!bp_place_source_eyes(position,rotation,bp.scale,optics,initial_pose)) { bp_fail("source_initial_pose_contract");return; }
 uint8_t no=0,yes=1; int32_t one=1,zero=0,depth_bits=24,format=0;
 if(!bp_prepare_empty_pair() || !bp_begin_components()) return;
 for(unsigned eye=0;eye<2 && !bp.failed;++eye) {
  BpEye* e=&bp.eye[eye];
  e->camera=bp_add(e->go,BT_CAMERA); if(bp.failed || !bp_set(BM_ENABLED_SET,bp_target(e->camera),&no)) break;
  e->additional=bp_add(e->go,BT_ADDITIONAL); if(bp.failed) break;
  if(!bp_prepare_own_additional(e)) break;
  e->transform=bp_root(bp_call(BM_TRANSFORM,bp_target(e->camera),NULL)); if(bp.failed) break;
  e->rt=bp_root(bp.object_new(bp.types[BT_RT])); if(bp.failed) break;
  void* rt_args[]={&bp.width,&bp.height,&depth_bits,&format};
  (void)bp_call(BM_RT_CTOR,bp_target(e->rt),rt_args);
  if(bp.failed || !bp_set(BM_RT_AA,bp_target(e->rt),&one) ||
   !bp_set(BM_RT_MIPS,bp_target(e->rt),&no) || !bp_set(BM_RT_AUTOMIPS,bp_target(e->rt),&no)) break;
  uint8_t created=0;
  if(!bp_get(BM_RT_CREATE,bp_target(e->rt),&created,sizeof(created)) || !created ||
   !bp_get(BM_RT_CREATED,bp_target(e->rt),&created,sizeof(created)) || !created) { bp_fail("own_rt_create"); break; }
  e->rotation=initial_pose[eye].rotation;e->position=initial_pose[eye].position;
  build_unity_eye_projection(&optics->fov[eye],near_clip,far_clip,&e->projection);
  if(!bp_configure_eye(e,eye,near_clip,far_clip,mask)) { bp_fail("own_preinit_configuration"); break; }
  /* Activate with Camera disabled. Add CameraData afterwards so Awake's normal
     behavior is observed, not manually invoked or emulated. */
  if(!bp_set(BM_ACTIVE_SET,bp_target(e->go),&yes)) break;
  e->data=bp_add(e->go,BT_DATA); if(bp.failed) break;
  Il2CppArray* owned_components=gameobject_get_components(bp_target(e->go),component_reflection_type,0,0,1,0,NULL);
  uintptr_t component_count=owned_components?owned_components->max_length:0;
  if(!component_count || component_count>16) { bp_fail("own_component_census"); break; }
  for(uintptr_t i=0;i<component_count;++i) {
   Il2CppObject* component=owned_components->vector[i]; Il2CppClass* klass=component?bp.api.object_get_class(component):NULL;
   if(klass && !strcmp(bp.api.class_get_namespace(klass),"UnityEngine") &&
    !strcmp(bp.api.class_get_name(klass),"AudioListener")) { bp_fail("unexpected_owned_audio_listener"); break; }
  }
  if(bp.failed) break;
  uint8_t initialized=0; bp.field_get(bp_target(e->data),bp.initialized_field,&initialized);
  if(!initialized) { (void)bp_call(BM_INITIALIZE,bp_target(e->data),(void*[]){&zero}); }
  if(bp.failed) break;
  bp.field_get(bp_target(e->data),bp.initialized_field,&initialized);
  if(!initialized || bp_call(BM_DATA_CAMERA,bp_target(e->data),NULL)!=bp_target(e->camera) ||
   bp_call(BM_DATA_ADDITIONAL,bp_target(e->data),NULL)!=bp_target(e->additional)) { bp_fail("own_init_identity"); break; }
  uint8_t enabled=1; if(!bp_get(BM_ENABLED_GET,bp_target(e->camera),&enabled,sizeof(enabled)) || enabled) { bp_fail("unexpected_init_camera_enable"); break; }
  if(!bp_configure_eye(e,eye,near_clip,far_clip,mask)) { bp_fail("own_postinit_configuration"); break; }
  if(!bp_compare_untagged(e->go)) break;
  if(!bp_effective_mono(e)) break;
  bp.background.configured|=1u<<eye;
  bp.c.initialized|=1u<<eye;
 }
 /* Both opaque initializations finish before either final native RT is
    acquired; right-eye init must not follow the left-eye native snapshot. */
 if(!bp.failed && bp.c.initialized==3 && !bp_fov_apply()) return;
 if(!bp.failed && bp.c.initialized==3) bp_projection_pair(BP_PR_INIT,-1);
 for(unsigned eye=0;eye<2 && !bp.failed && bp.c.initialized==3;++eye)
  if(!bp_acquire_owned_native(eye)) break;
 if(!bp.failed && !bp_built(&bp.c)) bp_fail("own_build_incomplete");
 if(!bp.failed) bp_log("base_generation_configured","disabled_waiting_present_validation");
}
/* Called under the producer lock on Unity's owner thread, after a fenced eye sample. */
static void bp_race_camera_sample(Il2CppArray* cameras,Il2CppObject* source) {
 static uint64_t last_generation;
 static unsigned reported;
 if(!cameras || bp.c.phase!=BP_ACTIVE || bp.c.retiring || bp.failed ||
    bp.source_kind!=BP_SOURCE_RACE || !bp.witness_samples)return;
 unsigned stage=bp.witness_samples>=10?10:1;
 if(last_generation==bp.c.identity.generation && reported>=stage)return;
 last_generation=bp.c.identity.generation;reported=stage;
 const MethodInfo* name=geometry_method(bp.types[BT_OBJECT],"get_name",bp.types[BT_STRING],NULL);
 Il2CppObject* left=bp.api.gchandle_get_target(bp.eye[0].camera);
 Il2CppObject* right=bp.api.gchandle_get_target(bp.eye[1].camera);
 char rows[1120]="";size_t used=0;unsigned emitted=0;
 /* Prioritize the source and owned eyes before reporting other enabled cameras. */
 for(unsigned pass=0;pass<2;++pass)for(uintptr_t i=0;i<cameras->max_length && emitted<12;++i){
  Il2CppObject* c=cameras->vector[i];if(!c)continue;
  const char* role=c==source?"source":c==left?"left":c==right?"right":"other";
  bool focus=c==source || c==left || c==right;
  if(focus!=(pass==0))continue;
  uint8_t enabled=0;
  if(!focus && (!geometry_read(bp.method[BM_ENABLED_GET],c,NULL,bp.types[BT_BOOL],&enabled,1) || !enabled))continue;
  Il2CppException* exc=NULL;
  Il2CppString* value=name?(Il2CppString*)bp.api.runtime_invoke(name,c,NULL,&exc):NULL;
  char label[27]="";if(!exc && value && bp.api.object_get_class((Il2CppObject*)value)==bp.types[BT_STRING]){
   for(int n=0;n<value->length && n<26;++n){
    unsigned v=value->chars[n];label[n]=(v>=32 && v<127 && v!=34 && v!=92)?(char)v:'_';
   }
  }
  int32_t mask=0;float near_clip=0,far_clip=0;
  bool ok=geometry_read(bp.method[BM_CULL_GET],c,NULL,bp.types[BT_INT],&mask,sizeof mask) &&
   geometry_read(bp.method[BM_NEAR_GET],c,NULL,bp.types[BT_FLOAT],&near_clip,sizeof near_clip) &&
   geometry_read(bp.method[BM_FAR_GET],c,NULL,bp.types[BT_FLOAT],&far_clip,sizeof far_clip) && isfinite(near_clip) && isfinite(far_clip);
  int n=snprintf(rows+used,sizeof rows-used,"%s[\"%s\",\"%s\",%s,%d,%.5g,%.5g]",used?",":"",role,label,ok?"true":"false",mask,ok?near_clip:0,ok?far_clip:0);
  if(n<0 || (size_t)n>=sizeof rows-used){rows[used]=0;break;}
  used+=(size_t)n;++emitted;
 }
 char detail[1450];
 telemetry_snprintf(detail,sizeof detail,"\"stage\":\"sample_%u\",\"generation\":%llu,\"last_sample_frame\":%d,\"initial_mask\":%d,\"enumerated\":%llu,\"columns\":[\"role\",\"name\",\"read_ok\",\"mask\",\"near\",\"far\"],\"cameras\":[%s]",
  stage,(unsigned long long)bp.c.identity.generation,bp.c.pending.frame,bp.eye_mask,(unsigned long long)cameras->max_length,rows);
 write_record("OWN-BASE","race_camera_sample",detail);
}
/* RaceEnvEffect identifies the race pipeline. Its effect-only mask cannot
   supply the world viewpoint. Prefer the current eligible perspective camera
   with the character and scenery layers observed on the authored race view. */
static Il2CppObject* bp_race_world_source(Il2CppArray* cameras,Il2CppObject* effect,int32_t* selected_mask) {
 if(!effect || !cameras)return NULL;
 Il2CppObject* owned[2]={NULL,NULL};
 for(unsigned i=0;i<2;++i)if(bp.eye[i].camera)owned[i]=bp.api.gchandle_get_target(bp.eye[i].camera);
 Il2CppObject* current=bp.source_kind==BP_SOURCE_RACE && bp.source_root?
  bp.api.gchandle_get_target(bp.source_root):NULL;
 Il2CppObject* selected=NULL;
 float best_depth=-INFINITY;
 int32_t best_id=INT32_MAX;
 for(uintptr_t i=0;i<cameras->max_length;++i){
  Il2CppObject* camera=cameras->vector[i];
  if(!camera || camera==effect || camera==owned[0] || camera==owned[1])continue;
  int32_t mask=0;
  if(!geometry_read(bp.method[BM_CULL_GET],camera,NULL,bp.types[BT_INT],&mask,sizeof mask) ||
     !(mask&(1<<13)) || !(mask&((1<<11)|(1<<12))) || (mask&(1<<5)))continue;
  uint8_t enabled=0,ortho=1;
  if(!geometry_read(bp.method[BM_ENABLED_GET],camera,NULL,bp.types[BT_BOOL],&enabled,sizeof enabled) || enabled!=1 ||
     !geometry_read(bp.method[BM_ORTHO],camera,NULL,bp.types[BT_BOOL],&ortho,sizeof ortho) || ortho!=0)continue;
  if(camera==current){*selected_mask=mask;return camera;}
  float depth=camera_get_depth(camera);
  int32_t id=0;
  if(!isfinite(depth) || !geometry_read(bp.method[BM_INSTANCE],camera,NULL,bp.types[BT_INT],&id,sizeof id))continue;
  if(!selected || depth>best_depth || (depth==best_depth && id<best_id)){
   selected=camera;*selected_mask=mask;best_depth=depth;best_id=id;
  }
 }
 return selected;
}
static void bp_main_step(Scene scene,BpSourceKind kind) {
 if(!bp.bound || GetCurrentThreadId()!=bp.owner_thread || !bp.api.thread_current()) return;
 /* Initialize/Awake/property calls can themselves query the active scene.
    Reject re-entry before acquiring the nonrecursive ownership lock. */
 if(InterlockedCompareExchange(&bp_main_depth,1,0)!=0) return;
 /* Main may wait only for a short CPU ownership interval, never for a GPU
    query/mutex. Present uses TryAcquire and cannot re-enter this lock. */
 AcquireSRWLockExclusive(&bp_lock);
 if(kind==BP_SOURCE_HOME || kind==BP_SOURCE_DRESS)
  kind=bp_foreground_source(scene.handle,kind==BP_SOURCE_HOME);
 if(bp.c.phase==BP_RETIRED && (kind==BP_SOURCE_NONE || !bp_rearm())) goto done;
 /* Empty failures have no RT/fence/components. Never send them through the
    full-generation retire adapter, even when a scene/stop changes meanwhile. */
 if(bp.empty.cleanup) { bp_cleanup_empty(); goto done; }
 if(bp.c.attempted && (kind==BP_SOURCE_NONE || kind!=bp.source_kind || scene.handle!=bp.c.identity.scene ||
  (bp.source_kind==BP_SOURCE_DRESS && bp.preview_near_adjusted && preview_use_authored_clip) ||
  (kind==BP_SOURCE_DRESS && (leased_host_id!=dress_host_id || leased_texture_id!=dress_texture_id)) ||
  bp.width!=InterlockedCompareExchange(&requested_eye_width,0,0) ||
  bp.height!=InterlockedCompareExchange(&requested_eye_height,0,0) || bp.scale!=world_scale ||
  (bp.source_kind!=BP_SOURCE_HOME && bp.post!=(post_processing_enabled!=0)) || InterlockedCompareExchange(&bp_stop_requested,0,0))) {
  bool first_retirement=!bp.c.retiring;
  bp_unpublish(); bp_retire(&bp.c,bp.c.quarantined || (InterlockedCompareExchange(&bp_stop_requested,0,0)&2)!=0);
  if(first_retirement) bp_context_event("retire_started","context_source_or_config_changed");
 }
 if(bp.c.retiring) {
  home_reset_effects();
  /* Even an ambiguous initialization gets only a best-effort own disable.
     No cleanup of uncertain registry state, no further allocation or retry. */
  bp_disable_owned_cameras();
  if(bp.c.phase==BP_DESTROY_READY && !bp.destroy_called) {
   bp.destroy_called=true;
   for(unsigned eye=0;eye<2 && !bp.failed;++eye) {
    uint32_t h[]={bp.eye[eye].go,bp.eye[eye].rt};
    for(unsigned i=0;i<2 && !bp.failed;++i) {
     Il2CppObject* o=bp_target(h[i]); bool alive=false;
     if(bp_alive(o,&alive) && alive) (void)bp_call(BM_DESTROY,NULL,(void*[]){o});
    }
   }
   if(story_bake.mesh && !bp.failed){Il2CppObject* owned=bp_target(story_bake.mesh);bool alive=false;if(bp_alive(owned,&alive) && alive)(void)bp_call(BM_DESTROY,NULL,(void*[]){owned});}
   if(bp.contact_mesh && !bp.failed){Il2CppObject* owned=bp_target(bp.contact_mesh);bool alive=false;if(bp_alive(owned,&alive) && alive)(void)bp_call(BM_DESTROY,NULL,(void*[]){owned});}
   if(!bp.failed) { bp.c.phase=BP_DESTROY_PENDING; bp_log("base_destroy_requested","normal_engine_lifecycle"); }
  } else if(bp.c.phase==BP_DESTROY_PENDING) {
   bool any=false;
   for(unsigned eye=0;eye<2 && !bp.failed;++eye) {
    uint32_t h[]={bp.eye[eye].go,bp.eye[eye].rt};
    for(unsigned i=0;i<2 && !bp.failed;++i) { bool alive=true; if(bp_alive(bp_target(h[i]),&alive)) any|=alive; }
   }
   if(story_bake.mesh && !bp.failed){bool alive=false;if(bp_alive(bp_target(story_bake.mesh),&alive))any|=alive;}
   if(bp.contact_mesh && !bp.failed){bool alive=false;if(bp_alive(bp_target(bp.contact_mesh),&alive))any|=alive;}
   if(!bp.failed && bp_destroy_observed(&bp.c,any)) {
    for(unsigned i=0;i<bp.root_count;++i) bp.api.gchandle_free(bp.roots[i]); bp.root_count=0;
    /* The post-disable fence completed before Destroy. COM references also
       stay until the native engine objects are observed destroyed. */
    for(unsigned eye=0;eye<2;++eye) if(bp.native[eye]) { ID3D11Texture2D_Release(bp.native[eye]); bp.native[eye]=NULL; }
    story_release_blocks();story_bake.mesh=0;bp.contact_mesh=0;
    bp.clean_retired=true;
    bp_log("base_generation_retired","native_destroyed_roots_released");
    bp_context_event("retired_clean","native_destroyed_roots_released");
   } else if(bp.c.quarantined) bp_fail("native_destroy_confirmation_limit");
  }
  goto done;
 }
 if(kind==BP_SOURCE_NONE || bp.failed || bp_transport_blocked(&bp_transport)) goto done;
 if(InterlockedCompareExchange(&bp_stop_requested,0,0)) { bp_fail("xr_worker_stopped_before_owned_begin"); goto done; }
 Il2CppObject* source=NULL; Il2CppObject* selected_data=NULL; HomeOwnerComponents components;
 Il2CppArray* cameras=bp_source_cameras(kind);
 uintptr_t count=cameras?cameras->max_length:0;
 if(count>64) { if(bp.c.attempted) { bp_unpublish(); bp_retire(&bp.c,false); } goto done; }
 float best=-INFINITY;
 int32_t dress_mask=0;
 if(kind==BP_SOURCE_DRESS){
  const char* result=NULL;source=dress_writer(cameras,scene,&dress_mask,&result);
  if(!source && preview_model_allowed && (!strcmp(result,"dress_no_texture_writer") ||
     !strcmp(result,"dress_no_scene_camera") || !strcmp(result,"dress_only_offscreen_cameras"))){
   source=preview_model_camera(cameras,scene,&dress_mask,&result);
  }
  static const char* last_result;
  if(!last_result || strcmp(last_result,result)){
   last_result=result;char detail[144];
   telemetry_snprintf(detail,sizeof detail,"\"stage\":\"%s\",\"scene\":%d,\"mask\":%d",result,scene.handle,dress_mask);
   write_record("MS-IM-508","owned_context_transition",detail);
  }
 }
 else for(uintptr_t i=0;i<count;++i) if(kind==BP_SOURCE_HOME?
  collect_home_owner_components(cameras->vector[i],&components):bp_effect_owner(cameras->vector[i],kind)) {
  if(kind==BP_SOURCE_RACE) { source=cameras->vector[i]; break; } /* Context marker only. */
  float depth=camera_get_depth(cameras->vector[i]); if(!source || depth>best) { source=cameras->vector[i]; selected_data=kind==BP_SOURCE_HOME?components.camera_data:NULL; best=depth; }
 }
 int32_t race_mask=0;
 if(kind==BP_SOURCE_RACE){
  source=bp_race_world_source(cameras,source,&race_mask);
  bp_race_camera_sample(cameras,source);
 }
 if(kind==BP_SOURCE_STORY) bp_story_topology(cameras,source,scene);
 /* State changes only: exact Story owner absence is explicit UNKNOWN/PANEL.
    The process cap and this tuple are deliberately not reset by rearm. */
 static int32_t selection_scene=INT32_MIN;static BpSourceKind selection_kind=BP_SOURCE_NONE;
 static Il2CppObject* selection_source;
 if(selection_scene!=scene.handle || selection_kind!=kind || selection_source!=source) {
  selection_scene=scene.handle;selection_kind=kind;selection_source=source;
  bp_context_event_at("selection",source?"eligible_owner":"unknown_or_unsupported_owner",
   scene.handle,kind,0);
 }
 if(bp.c.phase==BP_NEW) {
  EyeOptics optics; LONG w=InterlockedCompareExchange(&requested_eye_width,0,0),h=InterlockedCompareExchange(&requested_eye_height,0,0);
  if(source && w>0 && h>0 && w<=8192 && h<=8192 && snapshot_eye_optics(&optics)) {
   if(kind==BP_SOURCE_DRESS){leased_host_id=dress_host_id;leased_texture_id=dress_texture_id;}
   bp.source_kind=kind;bp_build(source,scene,&optics);
   if(kind==BP_SOURCE_DRESS && !bp.failed && bp.c.identity.generation){
    Il2CppException* exc=NULL;
    Il2CppObject* actual=bp.api.runtime_invoke(bp.method[BM_TARGET_GET],source,NULL,&exc);
    char detail[256];
    telemetry_snprintf(detail,sizeof detail,"\"stage\":\"preview_source_selected\",\"scene\":%d,\"source\":%d,\"host\":%d,\"ui_texture\":%d,\"camera_texture\":%d,\"mask\":%d,\"generation\":%llu",
     scene.handle,dress_instance(source),dress_host_id,dress_texture_id,exc?-1:dress_instance(actual),dress_mask,(unsigned long long)bp.c.identity.generation);
    write_record("MS-IM-514","owned_context_transition",detail);
   }
  }
 } else if(bp.c.attempted) {
  if(!source || source!=bp_target(bp.source_root) || (kind==BP_SOURCE_RACE && race_mask!=bp.eye_mask) ||
     (kind==BP_SOURCE_DRESS && dress_mask!=bp.eye_mask)) {
   bp_unpublish(); bp_retire(&bp.c,false);bp_context_event("retire_started","source_replaced_or_missing");goto done;
  }
  if(kind==BP_SOURCE_HOME && (bp.c.phase==BP_ACTIVE || bp.c.phase==BP_ENABLE_READY)){
   if(GetTickCount64()>=home_next_post_sample)home_sample_post(source,false);
   uint8_t post=home_post_enabled();
   if(bp.post!=(post!=0)){
    for(unsigned e=0;e<2;e++)if(!bp_set(BM_POST,bp_target(bp.eye[e].additional),&post))goto done;
    bp.post=post!=0;
   }
  }
  int32_t frame=-1;
  if(!bp_get(BM_FRAME,NULL,&frame,sizeof(frame)) || frame<0) { bp_fail("frame_observation"); goto done; }
  if(bp.c.phase==BP_ENABLE_READY) {
   if(kind==BP_SOURCE_HOME)home_sample_post(source,false);
   if(!bp_tag_enable_barrier()) goto done;
   if(!bp_fov_check(0,frame)) goto done;
   bp_projection_pair(BP_PR_PRE,frame);
   if(!bp_enable_mono_pair()) goto done;
   int32_t after=-1;
   if(!bp.failed && bp_get(BM_FRAME,NULL,&after,sizeof(after)) && after>=frame && bp_enabled(&bp.c,after)) {
    bp_projection_pair(BP_PR_PAIR,after);
    bp_log("base_cameras_enabled","configuration_frames_excluded");
   }
   else bp_fail("owned_enable_or_frame_contract");
  } else {
   if((bp.c.phase==BP_EXCLUDE || bp.c.phase==BP_ACTIVE) && !bp_mono_barrier()) goto done;
   if(bp.c.phase==BP_ACTIVE && !bp_source_follow(frame)) goto done;
   if(kind==BP_SOURCE_HOME && bp.c.phase==BP_ACTIVE)home_update_effects(frame,selected_data);
   if(bp.c.phase==BP_EXCLUDE && (int64_t)frame-(int64_t)bp.c.enable_frame>1 &&
    !bp_fov_check(1,frame)) goto done;
   for(unsigned eye=0;eye<2 && !bp.failed;++eye) {
    BpEye* e=&bp.eye[eye]; uint8_t enabled=0;
    if((bp.c.phase==BP_EXCLUDE || bp.c.phase==BP_ACTIVE) &&
     (!bp_get(BM_ENABLED_GET,bp_target(e->camera),&enabled,sizeof(enabled)) || !enabled ||
      bp_call(BM_TARGET_GET,bp_target(e->camera),NULL)!=bp_target(e->rt))) bp_fail("owned_camera_disabled_or_target_changed");
    if(bp.c.phase==BP_EXCLUDE && (int64_t)frame-(int64_t)bp.c.enable_frame>1) {
     if(!bp_background_check(e)) break;
     bp.background.observed|=1u<<eye;
     Vec3 position; Quat rotation; Matrix4x4 projection;
     if(!bp_get(BM_POSITION_GET,bp_target(e->transform),&position,sizeof(position)) ||
      !bp_get(BM_ROTATION_GET,bp_target(e->transform),&rotation,sizeof(rotation))) break;
     bp_projection_arm(eye,frame);
     bool projection_read=bp_get(BM_PROJECTION_GET,bp_target(e->camera),&projection,sizeof(projection));
     bp_projection_disarm();
     if(!projection_read) break;
     float dot=rotation.x*e->rotation.x+rotation.y*e->rotation.y+rotation.z*e->rotation.z+rotation.w*e->rotation.w;
     if(!isfinite(dot) || fabsf(fabsf(dot)-1)>0.0001f || fabsf(position.x-e->position.x)>0.001f ||
      fabsf(position.y-e->position.y)>0.001f || fabsf(position.z-e->position.z)>0.001f) bp_fail("owned_fixed_pose_changed_before_capture");
     for(unsigned i=0;i<16;++i) if(!isfinite(projection.m[i]) || fabsf(projection.m[i]-e->projection.m[i])>0.0001f)
      bp_fail("owned_projection_changed_before_capture");
    }
   }
   if(bp.failed) goto done;
   if(bp.c.phase==BP_EXCLUDE && (int64_t)frame-(int64_t)bp.c.enable_frame>1) bp_cc_capture(source,frame);
   (void)bp_observe(&bp.c,&bp.c.identity,frame); bp.observed_tick=GetTickCount64();
   bp_projection_finish_capture();
   bp_cc_flush();
  }
 }
done:
 ReleaseSRWLockExclusive(&bp_lock);
 InterlockedExchange(&bp_main_depth,0);
}






