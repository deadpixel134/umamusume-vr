/* Callback connection only. No phase certificate, token, producer or render call.
 * Uses official Marshal.GetDelegateForFunctionPointer(IntPtr,Type), non-generic
 * System.Action, pure C void CALLBACK(void). Never fabricates delegate/MethodInfo.
 * All post-registration objects/module/storage are PROCESS-LIFETIME: no supported
 * IL2CPP domain teardown/reload, DLL unload or scheduler cancellation contract.
 */
typedef enum EfStatus {
 EF_NEW,EF_MISSING_API,EF_MISSING_TYPE,EF_METADATA_MISSING,EF_METADATA_AMBIGUOUS,
 EF_METADATA_LIMIT,EF_WRONG_HOST,EF_WRONG_THREAD,EF_ROOT_FAILED,EF_MARSHAL_EXCEPTION,
 EF_MARSHAL_RESULT_REJECTED,EF_REGISTER_EXCEPTION,EF_REGISTERED_PHASE_UNKNOWN,
 EF_CALLBACK_PHASE_UNKNOWN,EF_INLINE_CALLBACK,EF_NESTED_CALLBACK,EF_REPEAT_CALLBACK,
 EF_NESTED_OWN_RENDER,EF_FRAME_EXCEPTION,EF_FRAME_RESULT_REJECTED,EF_DISARMED,EF_LATE_CALLBACK,
 EF_HOST_NOT_ALIVE,EF_HOST_CHECK_EXCEPTION,EF_MODULE_PIN_FAILED
} EfStatus;
typedef struct EfTypes {
 Il2CppClass *marshal,*action,*intptr,*type,*delegate,*mono_behaviour,*scheduler,*host,*time,*int32;
 Il2CppClass *unity_object,*behaviour,*boolean;
} EfTypes;
typedef struct EfSnapshot {
 EfStatus state,event;
 DWORD owner_thread,last_thread;
 LONG attempted,registration_attempted,registration_returned,disarmed,own_render_depth;
 LONG frame_available,frame;
 LONG64 callbacks,wrong_thread,nested,repeated,late,inline_callbacks;
 /* entry_verified/token_count are intentionally absent: no phase proof exists. */
} EfSnapshot;
typedef void (*EfEmit)(const EfSnapshot*);
typedef struct EfBridge {
 RdApi api; EfTypes types;
 const MethodInfo *marshal_method,*schedule_method,*frame_method,*alive_method,*active_method;
 uint32_t host_root,action_root;
 DWORD owner_thread;
 volatile LONG state,attempted,disarmed,registration_attempted,registration_returned;
 volatile LONG registration_inflight,callback_depth,delivered,own_render_depth;
 volatile LONG last_thread,frame_available,frame;
 volatile LONG64 callbacks,wrong_thread,nested,repeated,late,inline_callbacks;
 const char *binding_stage,*exception_name,*exception_namespace;
 DWORD module_pin_error;
 bool module_pinned;
 EfEmit emit;
} EfBridge;
static PVOID volatile ef_process_bridge;
static BOOL (WINAPI *ef_pin_module)(DWORD,LPCWSTR,HMODULE*)=GetModuleHandleExW;
typedef void (CALLBACK *EfNativeCallback)(void);
_Static_assert(sizeof(EfNativeCallback)==sizeof(uintptr_t),"Windows native callback/IntPtr width");

static const char* ef_status_name(EfStatus s) {
 static const char* const names[]={"new","missing_api","missing_type","metadata_missing","metadata_ambiguous",
  "metadata_limit","wrong_host","wrong_thread","root_failed","marshal_exception",
  "marshal_result_rejected","registration_exception","registered_phase_unverified",
  "callback_phase_unverified","inline_callback_rejected","nested_callback_rejected","repeat_callback_rejected",
  "nested_own_render_rejected","frame_exception","frame_result_rejected","disarmed","late_callback_rejected",
  "host_not_alive_or_active","host_liveness_exception","native_module_pin_failed"};
 return (unsigned)s<sizeof(names)/sizeof(names[0])?names[s]:"unknown";
}
static void ef_report(EfBridge* b,EfStatus event) {
 if(!b->emit) return;
 EfSnapshot s={0};
 s.state=(EfStatus)InterlockedCompareExchange(&b->state,0,0); s.event=event;
 s.owner_thread=b->owner_thread; s.last_thread=(DWORD)InterlockedCompareExchange(&b->last_thread,0,0);
#define EF_SNAP(name) s.name=InterlockedCompareExchange(&b->name,0,0)
 EF_SNAP(attempted); EF_SNAP(registration_attempted); EF_SNAP(registration_returned);
 EF_SNAP(disarmed); EF_SNAP(own_render_depth); EF_SNAP(frame_available); EF_SNAP(frame);
#undef EF_SNAP
#define EF_SNAP64(name) s.name=InterlockedCompareExchange64(&b->name,0,0)
 EF_SNAP64(callbacks); EF_SNAP64(wrong_thread); EF_SNAP64(nested);
 EF_SNAP64(repeated); EF_SNAP64(late); EF_SNAP64(inline_callbacks);
#undef EF_SNAP64
 b->emit(&s);
}
static void ef_disarm(EfBridge* b) {
 /* Gate shutdown, NOT cancellation/destruction of the game's coroutine/host.
    Keep action/host roots even after a registration exception (unknown retention). */
 InterlockedExchange(&b->disarmed,1); InterlockedExchange(&b->state,EF_DISARMED);
}
static void ef_exception(EfBridge* b,Il2CppException* exc) {
 Il2CppClass* c=b->api.object_get_class((Il2CppObject*)exc);
 b->exception_name=c?b->api.class_get_name(c):NULL;
 b->exception_namespace=c?b->api.class_get_namespace(c):NULL;
}
static bool ef_type(EfBridge* b,const Il2CppType* t,Il2CppClass* expected,int code) {
 return t && !b->api.type_is_byref(t) && b->api.type_get_type(t)==code &&
  (code==1 || b->api.class_from_type(t)==expected);
}
static const MethodInfo* ef_find(EfBridge* b,Il2CppClass* c,const char* name,bool is_static,
 Il2CppClass* ret,int ret_code,unsigned argc,Il2CppClass** params,const int* codes) {
 if(!c) return NULL;
 const MethodInfo *m,*found=NULL; void* iter=NULL; unsigned count=0;
 while((m=b->api.class_get_methods(c,&iter))) {
  if(++count>512) { InterlockedExchange(&b->state,EF_METADATA_LIMIT); return NULL; }
  const char* n=b->api.method_get_name(m); uint32_t impl=0;
  uint32_t flags=b->api.method_get_flags(m,&impl);
  if(!n || strcmp(n,name) || b->api.method_get_class(m)!=c ||
   b->api.method_get_param_count(m)!=argc || (flags&0x400u) ||
   ((flags&0x10u)!=0)!=is_static || (flags&7u)!=6u || b->api.method_is_generic(m) ||
   b->api.method_is_inflated(m) || !ef_type(b,b->api.method_get_return_type(m),ret,ret_code)) continue;
  bool match=true;
  for(unsigned i=0;i<argc;++i) if(!ef_type(b,b->api.method_get_param(m,i),params[i],codes[i])) match=false;
  if(!match) continue;
  if(found) { InterlockedExchange(&b->state,EF_METADATA_AMBIGUOUS); return NULL; }
  found=m;
 }
 return found;
}
static void ef_deliver(EfBridge* b) {
 if(!b) return;
 InterlockedIncrement64(&b->callbacks);
 DWORD current=GetCurrentThreadId(); InterlockedExchange(&b->last_thread,(LONG)current);
 if(current!=b->owner_thread) InterlockedIncrement64(&b->wrong_thread);
 /* Depth includes reporting: even recursive diagnostic delivery never recurses
    into managed APIs/logging. No foreign/late callback touches managed state. */
 if(InterlockedIncrement(&b->callback_depth)!=1) {
  InterlockedIncrement64(&b->nested); InterlockedExchange(&b->disarmed,1);
  InterlockedExchange(&b->state,EF_NESTED_CALLBACK); InterlockedDecrement(&b->callback_depth); return;
 }
 EfStatus event=EF_CALLBACK_PHASE_UNKNOWN;
 if(InterlockedCompareExchange(&b->disarmed,0,0)) {
  InterlockedIncrement64(&b->late); event=EF_LATE_CALLBACK; goto reject;
 }
 if(current!=b->owner_thread || !b->api.thread_current()) {
  if(current==b->owner_thread) InterlockedIncrement64(&b->wrong_thread);
  InterlockedExchange(&b->disarmed,1);
  event=EF_WRONG_THREAD; goto reject;
 }
 if(InterlockedCompareExchange(&b->registration_inflight,0,0) ||
  !InterlockedCompareExchange(&b->registration_returned,0,0)) {
  InterlockedIncrement64(&b->inline_callbacks); InterlockedExchange(&b->disarmed,1);
  event=EF_INLINE_CALLBACK; goto reject;
 }
 if(InterlockedCompareExchange(&b->own_render_depth,0,0)) {
  InterlockedExchange(&b->disarmed,1); event=EF_NESTED_OWN_RENDER; goto reject;
 }
 if(InterlockedCompareExchange(&b->delivered,1,0)) {
  InterlockedIncrement64(&b->repeated); event=EF_REPEAT_CALLBACK; goto reject;
 }
 {
  Il2CppException* exc=NULL;
  Il2CppObject* frame=b->api.runtime_invoke(b->frame_method,NULL,NULL,&exc);
  if(InterlockedCompareExchange(&b->disarmed,0,0)) { event=(EfStatus)InterlockedCompareExchange(&b->state,0,0); goto reject; }
  if(exc) { ef_exception(b,exc); InterlockedExchange(&b->disarmed,1); event=EF_FRAME_EXCEPTION; goto reject; }
  if(!frame || b->api.object_get_class(frame)!=b->types.int32 || !b->api.object_unbox(frame)) {
   InterlockedExchange(&b->disarmed,1); event=EF_FRAME_RESULT_REJECTED; goto reject;
  }
  int32_t value; memcpy(&value,b->api.object_unbox(frame),sizeof(value));
  if(value<0) { InterlockedExchange(&b->disarmed,1); event=EF_FRAME_RESULT_REJECTED; goto reject; }
  InterlockedExchange(&b->frame,value); InterlockedExchange(&b->frame_available,1);
 }
 /* A frame number and delivery on the registration thread certify NO phase.
    No token/entry=true, re-registration or producer function exists here. */
reject:
 if(event!=EF_LATE_CALLBACK) InterlockedExchange(&b->state,event);
 if(InterlockedCompareExchange64(&b->callbacks,0,0)<=4) ef_report(b,event);
 InterlockedDecrement(&b->callback_depth);
}
static void CALLBACK ef_native_callback(void) {
 ef_deliver((EfBridge*)InterlockedCompareExchangePointer(&ef_process_bridge,NULL,NULL));
}
static bool ef_host_live(EfBridge* b) {
 Il2CppException* exc=NULL;
 Il2CppObject* host=b->api.gchandle_get_target(b->host_root);
 if(!host) { InterlockedExchange(&b->state,EF_HOST_NOT_ALIVE); return false; }
 Il2CppObject* args[]={host};
 Il2CppObject* alive=b->api.runtime_invoke_convert_args(b->alive_method,NULL,args,1,&exc);
 if(exc) { ef_exception(b,exc); InterlockedExchange(&b->state,EF_HOST_CHECK_EXCEPTION); return false; }
 if(!alive || b->api.object_get_class(alive)!=b->types.boolean || !b->api.object_unbox(alive) ||
  !*(const uint8_t*)b->api.object_unbox(alive)) { InterlockedExchange(&b->state,EF_HOST_NOT_ALIVE); return false; }
 host=b->api.gchandle_get_target(b->host_root);
 Il2CppObject* active=host?b->api.runtime_invoke(b->active_method,host,NULL,&exc):NULL;
 if(exc) { ef_exception(b,exc); InterlockedExchange(&b->state,EF_HOST_CHECK_EXCEPTION); return false; }
 if(!active || b->api.object_get_class(active)!=b->types.boolean || !b->api.object_unbox(active) ||
  !*(const uint8_t*)b->api.object_unbox(active)) { InterlockedExchange(&b->state,EF_HOST_NOT_ALIVE); return false; }
 /* This is a point-in-time check, not a promise that a root keeps a Unity
    native object alive or that the host survives until callback delivery. */
 return true;
}

static EfStatus ef_start(EfBridge* b,const RdApi* api,const EfTypes* types,
 Il2CppObject* host,EfEmit emit) {
 if(InterlockedCompareExchange(&b->attempted,1,0)) return (EfStatus)InterlockedCompareExchange(&b->state,0,0);
 if(InterlockedCompareExchange(&b->disarmed,0,0)) return EF_DISARMED;
 b->api=*api; b->types=*types; b->owner_thread=GetCurrentThreadId(); b->emit=emit;
 if(InterlockedCompareExchangePointer(&ef_process_bridge,NULL,NULL) &&
  InterlockedCompareExchangePointer(&ef_process_bridge,NULL,NULL)!=b) { ef_disarm(b); return EF_DISARMED; }
 b->binding_stage="exports";
#define EF_API(name) if(!api->name) { b->binding_stage="il2cpp_" #name; InterlockedExchange(&b->state,EF_MISSING_API); return EF_MISSING_API; }
 EF_API(class_get_methods) EF_API(method_get_name) EF_API(method_get_class)
 EF_API(method_get_param_count) EF_API(method_get_param) EF_API(method_get_return_type)
 EF_API(method_get_flags) EF_API(method_is_generic) EF_API(method_is_inflated)
 EF_API(type_is_byref) EF_API(type_get_type) EF_API(class_from_type)
 EF_API(class_is_assignable_from) EF_API(object_get_class) EF_API(runtime_invoke)
 EF_API(runtime_invoke_convert_args) EF_API(class_get_type) EF_API(type_get_object)
 EF_API(value_box) EF_API(object_unbox) EF_API(gchandle_new) EF_API(gchandle_get_target)
 EF_API(gchandle_free) EF_API(thread_current) EF_API(class_get_name) EF_API(class_get_namespace)
#undef EF_API
 if(!api->thread_current()) { InterlockedExchange(&b->state,EF_WRONG_THREAD); return EF_WRONG_THREAD; }
 b->binding_stage="callback_types";
 if(!types->marshal || !types->action || !types->intptr || !types->type || !types->delegate ||
  !types->mono_behaviour || !types->scheduler || !types->host || !types->time || !types->int32 ||
  !types->unity_object || !types->behaviour || !types->boolean) {
  InterlockedExchange(&b->state,EF_MISSING_TYPE); return EF_MISSING_TYPE;
 }
 /* IntPtr parameter metadata is IL2CPP_TYPE_I (0x18), independent of value_box. */
 Il2CppClass* marshal_params[]={types->intptr,types->type}; const int marshal_codes[]={24,18};
 b->binding_stage="Marshal.GetDelegateForFunctionPointer(IntPtr,Type)";
 b->marshal_method=ef_find(b,types->marshal,"GetDelegateForFunctionPointer",true,types->delegate,18,2,marshal_params,marshal_codes);
 if(!b->marshal_method) goto metadata_failure;
 b->binding_stage="System.Action.Invoke";
 if(!api->class_is_assignable_from(types->delegate,types->action) ||
  !ef_find(b,types->action,"Invoke",false,NULL,1,0,NULL,NULL)) goto metadata_failure;
 Il2CppClass* schedule_params[]={types->mono_behaviour,types->action}; const int schedule_codes[]={18,18};
 b->binding_stage="MonoBehaviourExtension.WaitForEndFrame";
 b->schedule_method=ef_find(b,types->scheduler,"WaitForEndFrame",true,NULL,1,2,schedule_params,schedule_codes);
 if(!b->schedule_method) goto metadata_failure;
 b->binding_stage="Time.get_frameCount";
 b->frame_method=ef_find(b,types->time,"get_frameCount",true,types->int32,8,0,NULL,NULL);
 if(!b->frame_method) goto metadata_failure;
 b->binding_stage="UnityEngine.Object.op_Implicit";
 Il2CppClass* alive_params[]={types->unity_object}; const int alive_codes[]={18};
 b->alive_method=ef_find(b,types->unity_object,"op_Implicit",true,types->boolean,2,1,alive_params,alive_codes);
 if(!b->alive_method) goto metadata_failure;
 b->binding_stage="Behaviour.get_isActiveAndEnabled";
 b->active_method=ef_find(b,types->behaviour,"get_isActiveAndEnabled",false,types->boolean,2,0,NULL,NULL);
 if(!b->active_method) goto metadata_failure;
 b->binding_stage="existing_GameSystem_host";
 if(!host || api->object_get_class(host)!=types->host || !api->class_is_assignable_from(types->mono_behaviour,types->host)) {
  InterlockedExchange(&b->state,EF_WRONG_HOST); return EF_WRONG_HOST;
 }
 b->binding_stage="root_host"; b->host_root=api->gchandle_new(host,false);
 if(!b->host_root || !api->gchandle_get_target(b->host_root)) goto root_failure;
 b->binding_stage="Marshal_arguments";
 uintptr_t address=(uintptr_t)(EfNativeCallback)ef_native_callback;
 uint32_t pointer_root=0,type_root=0;
 b->binding_stage="host_liveness_before_marshal";
 if(!ef_host_live(b)) goto before_registration_cleanup;
 b->binding_stage="pin_native_callback_module";
 HMODULE pinned=NULL;
 if(!ef_pin_module(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
  (LPCWSTR)address,&pinned) || !pinned) {
  b->module_pin_error=GetLastError(); InterlockedExchange(&b->state,EF_MODULE_PIN_FAILED);
  goto before_registration_cleanup;
 }
 b->module_pinned=true; /* Windows PIN is process-lifetime; no FreeLibrary/unpin. */
 Il2CppObject* box=api->value_box(types->intptr,&address);
 if(box) pointer_root=api->gchandle_new(box,false);
 if(!pointer_root || !api->gchandle_get_target(pointer_root)) goto temporary_root_failure;
 Il2CppObject* reflection_type=(Il2CppObject*)api->type_get_object(api->class_get_type(types->action));
 if(reflection_type) type_root=api->gchandle_new(reflection_type,false);
 if(!type_root || !api->gchandle_get_target(type_root)) goto temporary_root_failure;
 {
  Il2CppObject* args[]={api->gchandle_get_target(pointer_root),api->gchandle_get_target(type_root)};
  Il2CppException* exc=NULL; b->binding_stage="Marshal_invoke";
  Il2CppObject* action=api->runtime_invoke_convert_args(b->marshal_method,NULL,args,2,&exc);
  if(exc) { ef_exception(b,exc); InterlockedExchange(&b->state,EF_MARSHAL_EXCEPTION); goto before_registration_cleanup; }
  if(!action || api->object_get_class(action)!=types->action) {
   InterlockedExchange(&b->state,EF_MARSHAL_RESULT_REJECTED); goto before_registration_cleanup;
  }
  b->binding_stage="root_action"; b->action_root=api->gchandle_new(action,false);
  if(!b->action_root || !api->gchandle_get_target(b->action_root)) goto temporary_root_failure;
 }
 b->binding_stage="host_liveness_before_registration";
 if(!ef_host_live(b)) goto before_registration_cleanup;
 if(InterlockedCompareExchange(&b->disarmed,0,0)) {
  InterlockedExchange(&b->state,EF_DISARMED); goto before_registration_cleanup;
 }
 api->gchandle_free(pointer_root); api->gchandle_free(type_root);
 /* One stable process bridge. Action/host roots cannot be freed after this point:
    even a throwing scheduler may have retained the callback before throwing. */
 if(InterlockedCompareExchangePointer(&ef_process_bridge,b,NULL) &&
  InterlockedCompareExchangePointer(&ef_process_bridge,NULL,NULL)!=b) {
  pointer_root=type_root=0; ef_disarm(b); goto before_registration_cleanup;
 }
 b->binding_stage="WaitForEndFrame_registration";
 InterlockedExchange(&b->registration_attempted,1); InterlockedExchange(&b->registration_inflight,1);
 {
  Il2CppException* exc=NULL;
  Il2CppObject* args[]={api->gchandle_get_target(b->host_root),api->gchandle_get_target(b->action_root)};
  (void)api->runtime_invoke_convert_args(b->schedule_method,NULL,args,2,&exc);
  InterlockedExchange(&b->registration_inflight,0);
  if(exc) { ef_exception(b,exc); InterlockedExchange(&b->disarmed,1); InterlockedExchange(&b->state,EF_REGISTER_EXCEPTION); return EF_REGISTER_EXCEPTION; }
 }
 InterlockedExchange(&b->registration_returned,1);
 if(!InterlockedCompareExchange(&b->disarmed,0,0)) InterlockedExchange(&b->state,EF_REGISTERED_PHASE_UNKNOWN);
 return (EfStatus)InterlockedCompareExchange(&b->state,0,0);
temporary_root_failure:
 InterlockedExchange(&b->state,EF_ROOT_FAILED);
before_registration_cleanup:
 if(pointer_root) api->gchandle_free(pointer_root);
 if(type_root) api->gchandle_free(type_root);
 if(b->action_root) { api->gchandle_free(b->action_root); b->action_root=0; }
 if(b->host_root) { api->gchandle_free(b->host_root); b->host_root=0; }
 return (EfStatus)InterlockedCompareExchange(&b->state,0,0);
root_failure:
 if(b->host_root) { api->gchandle_free(b->host_root); b->host_root=0; }
 InterlockedExchange(&b->state,EF_ROOT_FAILED); return EF_ROOT_FAILED;
metadata_failure:
 if(!InterlockedCompareExchange(&b->state,0,0)) InterlockedExchange(&b->state,EF_METADATA_MISSING);
 return (EfStatus)InterlockedCompareExchange(&b->state,0,0);
}
