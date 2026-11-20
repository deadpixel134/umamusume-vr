/* Reuse the existing exported-API fixture, not a parallel bridge implementation.
   Each reset below represents a fresh process with no real queued callbacks. */
#define main request_fixture_main
#include "request_dispatch_test.c"
#undef main
#include "../src/end_frame_bridge.h"

static Il2CppClass marshal_class,action_class,intptr_class,delegate_class,mono_class,
 scheduler_class,host_class,time_class,unity_class,behaviour_class;
static MethodInfo marshal_method,invoke_method,schedule_method,frame_method,alive_method,active_method;
static Il2CppObject host_object,action_object,action_type,bad_action,frame_box;
static struct { Il2CppObject object; uintptr_t address; } pointer_box;
static EfTypes ef_types;
static EfBridge bridge;
static EfNativeCallback captured_callback;
static EfSnapshot last_snapshot;
static unsigned marshal_calls,schedule_calls,frame_calls,callback_reports;
static bool marshal_throws,registration_throws,inline_delivery,disarm_during_registration,
 host_alive,host_active,host_throws,destroy_in_marshal,bad_marshal_result,frame_throws,
 bad_frame,nested_frame,ef_meta_limit,disarm_in_marshal;
static uint32_t lost_root;
static bool pin_failure;
static unsigned pin_calls;
static BOOL WINAPI cb_pin(DWORD flags,LPCWSTR address,HMODULE* module) {
 ++pin_calls;
 if(pin_failure || flags!=(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN) ||
  (uintptr_t)address!=(uintptr_t)(EfNativeCallback)ef_native_callback) {
  *module=NULL; SetLastError(ERROR_ACCESS_DENIED); return FALSE;
 }
 *module=(HMODULE)(uintptr_t)1; return TRUE;
}

static const MethodInfo* cb_methods(Il2CppClass* c,void** it) {
 if(ef_meta_limit && c==&marshal_class) {
  uintptr_t n=(uintptr_t)*it; if(n==513) return NULL;
  *it=(void*)(n+1); return &contains;
 }
 return f_methods(c,it);
}
static Il2CppObject* cb_target(uint32_t h) { return h && h==lost_root?NULL:f_target(h); }
static Il2CppObject* cb_box(Il2CppClass* c,void* data) {
 if(c==&intptr_class) { memcpy(&pointer_box.address,data,sizeof(uintptr_t)); return &pointer_box.object; }
 return f_box(c,data);
}
static Il2CppReflectionType* cb_type_object(const Il2CppType* t) {
 return t==&action_class.type?(Il2CppReflectionType*)&action_type:f_type_object(t);
}
static Il2CppObject* cb_invoke(const MethodInfo* m,void* receiver,void** params,Il2CppException** exc) {
 *exc=NULL;
 if(m==&frame_method) {
  ++frame_calls;
  if(receiver || params || frame_throws) { *exc=&exception; return NULL; }
  if(nested_frame) { nested_frame=false; captured_callback(); }
  return bad_frame?&true_box:&frame_box;
 }
 if(m==&active_method) {
  if(receiver!=&host_object || params || host_throws) { *exc=&exception; return NULL; }
  return host_active?&true_box:&false_box;
 }
 return f_invoke(m,receiver,params,exc);
}
static Il2CppObject* cb_convert(const MethodInfo* m,void* receiver,Il2CppObject** argv,int argc,Il2CppException** exc) {
 *exc=NULL;
 if(m==&alive_method) {
  if(receiver || argc!=1 || argv[0]!=&host_object || host_throws) { *exc=&exception; return NULL; }
  return host_alive?&true_box:&false_box;
 }
 if(m==&marshal_method) {
  ++marshal_calls;
  if(receiver || argc!=2 || argv[0]!=&pointer_box.object || argv[1]!=&action_type || marshal_throws) { *exc=&exception; return NULL; }
  captured_callback=(EfNativeCallback)pointer_box.address;
  if(destroy_in_marshal) host_alive=false;
  if(disarm_in_marshal) ef_disarm(&bridge);
  return bad_marshal_result?&bad_action:&action_object;
 }
 if(m==&schedule_method) {
  ++schedule_calls;
  if(receiver || argc!=2 || argv[0]!=&host_object || argv[1]!=&action_object) { *exc=&exception; return NULL; }
  if(disarm_during_registration) ef_disarm(&bridge);
  if(inline_delivery) captured_callback();
  if(registration_throws) *exc=&exception;
  return NULL;
 }
 return f_convert(m,receiver,argv,argc,exc);
}
static void cb_emit(const EfSnapshot* s) { ++callback_reports; last_snapshot=*s; }
static void cb_reset(void) {
 reset(true);
 memset(&bridge,0,sizeof(bridge)); memset(&last_snapshot,0,sizeof(last_snapshot));
 InterlockedExchangePointer(&ef_process_bridge,NULL); captured_callback=NULL;
 marshal_calls=schedule_calls=frame_calls=callback_reports=0; lost_root=0;
 pin_calls=0; pin_failure=false; ef_pin_module=cb_pin;
 marshal_throws=registration_throws=inline_delivery=disarm_during_registration=host_throws=
 destroy_in_marshal=bad_marshal_result=frame_throws=bad_frame=nested_frame=ef_meta_limit=disarm_in_marshal=false;
 host_alive=host_active=true;
 marshal_class=(Il2CppClass){.name="Marshal"}; action_class=(Il2CppClass){.name="Action",.parent=&delegate_class};
 intptr_class=(Il2CppClass){.name="IntPtr"}; delegate_class=(Il2CppClass){.name="Delegate"};
 unity_class=(Il2CppClass){.name="Object"}; behaviour_class=(Il2CppClass){.name="Behaviour",.parent=&unity_class};
 mono_class=(Il2CppClass){.name="MonoBehaviour",.parent=&behaviour_class};
 scheduler_class=(Il2CppClass){.name="MonoBehaviourExtension"};
 host_class=(Il2CppClass){.name="GameSystem",.parent=&mono_class}; time_class=(Il2CppClass){.name="Time"};
 Il2CppClass* classes[]={&marshal_class,&action_class,&intptr_class,&delegate_class,&unity_class,
  &behaviour_class,&mono_class,&scheduler_class,&host_class,&time_class};
 for(unsigned i=0;i<10;++i) classes[i]->type=(Il2CppType){.code=18,.klass=classes[i]};
 intptr_class.type.code=24;
 marshal_method=(MethodInfo){.name="GetDelegateForFunctionPointer",.owner=&marshal_class,
  .ret=&delegate_class.type,.params={&intptr_class.type,&typ.type},.argc=2,.flags=0x16};
 invoke_method=(MethodInfo){.name="Invoke",.owner=&action_class,.ret=&void_type,.flags=0x46};
 schedule_method=(MethodInfo){.name="WaitForEndFrame",.owner=&scheduler_class,.ret=&void_type,
  .params={&mono_class.type,&action_class.type},.argc=2,.flags=0x16};
 frame_method=(MethodInfo){.name="get_frameCount",.owner=&time_class,.ret=&i32.type,.flags=0x16};
 alive_method=(MethodInfo){.name="op_Implicit",.owner=&unity_class,.ret=&boolean.type,
  .params={&unity_class.type},.argc=1,.flags=0x16};
 active_method=(MethodInfo){.name="get_isActiveAndEnabled",.owner=&behaviour_class,.ret=&boolean.type,.flags=6};
 marshal_class.methods[0]=&marshal_method; marshal_class.count=1;
 action_class.methods[0]=&invoke_method; action_class.count=1;
 scheduler_class.methods[0]=&schedule_method; scheduler_class.count=1;
 time_class.methods[0]=&frame_method; time_class.count=1;
 unity_class.methods[0]=&alive_method; unity_class.count=1;
 behaviour_class.methods[0]=&active_method; behaviour_class.count=1;
 host_object=(Il2CppObject){.klass=&host_class}; action_object=(Il2CppObject){.klass=&action_class};
 action_type=(Il2CppObject){.klass=&typ,.type_argument=&action_class};
 bad_action=(Il2CppObject){.klass=&delegate_class}; frame_box=(Il2CppObject){.klass=&i32,.value=123};
 pointer_box.object=(Il2CppObject){.klass=&intptr_class}; pointer_box.address=0;
 ef_types=(EfTypes){&marshal_class,&action_class,&intptr_class,&typ,&delegate_class,&mono_class,
  &scheduler_class,&host_class,&time_class,&i32,&unity_class,&behaviour_class,&boolean};
 api.class_get_methods=cb_methods; api.value_box=cb_box; api.type_get_object=cb_type_object;
 api.runtime_invoke=cb_invoke; api.runtime_invoke_convert_args=cb_convert; api.gchandle_get_target=cb_target;
}
static int cb_start_case(const char* name,EfStatus status,unsigned retained) {
 CHECK(ef_start(&bridge,&api,&ef_types,&host_object,cb_emit)==status);
 CHECK(roots_live==retained);
 unsigned made=root_calls,marshaled=marshal_calls,scheduled=schedule_calls;
 CHECK(ef_start(&bridge,&api,&ef_types,&host_object,cb_emit)==status);
 CHECK(root_calls==made && marshal_calls==marshaled && schedule_calls==scheduled);
 CHECK(strcmp(ef_status_name(status),"unknown"));
 printf("PASS %s\n",name); ++cases; return 0;
}
static DWORD WINAPI cb_foreign(LPVOID unused) { (void)unused; captured_callback(); return 0; }
int main(void) {
 cases=assertions=0;
 cb_reset(); ef_disarm(&bridge); CHECK(!cb_start_case("disarm_before_start_no_registration",EF_DISARMED,0));
 cb_reset(); api.runtime_invoke_convert_args=NULL; CHECK(!cb_start_case("missing_export",EF_MISSING_API,0));
 cb_reset(); ef_types.action=NULL; CHECK(!cb_start_case("missing_type",EF_MISSING_TYPE,0));
 cb_reset(); scheduler_class.count=0; CHECK(!cb_start_case("missing_scheduler",EF_METADATA_MISSING,0));
 cb_reset(); schedule_method.flags=6; CHECK(!cb_start_case("wrong_scheduler_instance",EF_METADATA_MISSING,0));
 cb_reset(); schedule_method.ret=&obj.type; CHECK(!cb_start_case("wrong_scheduler_return",EF_METADATA_MISSING,0));
 cb_reset(); schedule_method.params[1]=&obj.type; CHECK(!cb_start_case("wrong_action_parameter",EF_METADATA_MISSING,0));
 cb_reset(); invoke_method.ret=&boolean.type; CHECK(!cb_start_case("Action_Invoke_not_void",EF_METADATA_MISSING,0));
 cb_reset(); invoke_method.generic=true; CHECK(!cb_start_case("Action_Invoke_generic",EF_METADATA_MISSING,0));
 cb_reset(); marshal_method.generic=true; CHECK(!cb_start_case("Marshal_generic_rejected",EF_METADATA_MISSING,0));
 cb_reset(); CHECK(!cb_start_case("Marshal_native_int_IntPtr_binding",EF_REGISTERED_PHASE_UNKNOWN,2));
 CHECK(bridge.marshal_method==&marshal_method && marshal_calls==1 && schedule_calls==1 && captured_callback);
 CHECK(!bridge.frame_available && !frame_calls);
 cb_reset(); marshal_method.params[0]=&i32.type; CHECK(!cb_start_case("Marshal_I4_rejected",EF_METADATA_MISSING,0));
 CHECK(!marshal_calls && !schedule_calls);
 Il2CppType wrong_intptr;
 cb_reset(); wrong_intptr=intptr_class.type; wrong_intptr.klass=&obj; marshal_method.params[0]=&wrong_intptr;
 CHECK(!cb_start_case("Marshal_native_int_wrong_class_rejected",EF_METADATA_MISSING,0));
 CHECK(!marshal_calls && !schedule_calls);
 cb_reset(); wrong_intptr=intptr_class.type; wrong_intptr.code=17; marshal_method.params[0]=&wrong_intptr;
 CHECK(!cb_start_case("Marshal_VALUETYPE_IntPtr_rejected",EF_METADATA_MISSING,0));
 CHECK(!marshal_calls && !schedule_calls);
 cb_reset(); wrong_intptr=intptr_class.type; wrong_intptr.byref=true; marshal_method.params[0]=&wrong_intptr;
 CHECK(!cb_start_case("Marshal_byref_native_int_rejected",EF_METADATA_MISSING,0));
 CHECK(!marshal_calls && !schedule_calls);
 cb_reset(); marshal_method.params[1]=&obj.type; CHECK(!cb_start_case("Marshal_not_Type",EF_METADATA_MISSING,0));
 cb_reset(); marshal_method.ret=&obj.type; CHECK(!cb_start_case("Marshal_wrong_return",EF_METADATA_MISSING,0));
 cb_reset(); marshal_class.methods[1]=&marshal_method; marshal_class.count=2; CHECK(!cb_start_case("ambiguous_metadata",EF_METADATA_AMBIGUOUS,0));
 cb_reset(); ef_meta_limit=true; CHECK(!cb_start_case("metadata_bound",EF_METADATA_LIMIT,0));
 cb_reset(); host_object.klass=&mono_class; CHECK(!cb_start_case("wrong_host_class",EF_WRONG_HOST,0));
 cb_reset(); host_class.parent=&obj; CHECK(!cb_start_case("host_not_MonoBehaviour",EF_WRONG_HOST,0));
 cb_reset(); attached=false; CHECK(!cb_start_case("not_attached",EF_WRONG_THREAD,0));
 cb_reset(); host_alive=false; CHECK(!cb_start_case("Unity_native_host_destroyed",EF_HOST_NOT_ALIVE,0));
 cb_reset(); host_active=false; CHECK(!cb_start_case("host_inactive",EF_HOST_NOT_ALIVE,0));
 cb_reset(); host_throws=true; CHECK(!cb_start_case("host_check_exception",EF_HOST_CHECK_EXCEPTION,0));
 cb_reset(); marshal_throws=true; CHECK(!cb_start_case("Marshal_exception_AOT_unverified",EF_MARSHAL_EXCEPTION,0));
 cb_reset(); bad_marshal_result=true; CHECK(!cb_start_case("Marshal_wrong_delegate",EF_MARSHAL_RESULT_REJECTED,0));
 cb_reset(); destroy_in_marshal=true; CHECK(!cb_start_case("host_dies_before_registration",EF_HOST_NOT_ALIVE,0));
 cb_reset(); pin_failure=true; CHECK(!cb_start_case("module_pin_failure_no_Marshal_registration",EF_MODULE_PIN_FAILED,0));
 CHECK(!marshal_calls && !schedule_calls && bridge.module_pin_error==ERROR_ACCESS_DENIED);
 cb_reset(); disarm_in_marshal=true; CHECK(!cb_start_case("disarm_before_registration_unwinds",EF_DISARMED,0));
 CHECK(!schedule_calls && !InterlockedCompareExchangePointer(&ef_process_bridge,NULL,NULL));
 for(unsigned n=1;n<=4;++n) { cb_reset(); root_fail_at=n; CHECK(!cb_start_case("root_allocation_unwind",EF_ROOT_FAILED,0)); }
 for(unsigned n=1;n<=4;++n) { cb_reset(); lost_root=n; CHECK(!cb_start_case("root_target_unwind",EF_ROOT_FAILED,0)); }
 cb_reset(); registration_throws=true; CHECK(!cb_start_case("registration_exception_roots_retained",EF_REGISTER_EXCEPTION,2));
 CHECK(captured_callback && bridge.registration_attempted && !bridge.registration_returned);
 captured_callback(); CHECK(bridge.late==1 && !frame_calls && roots_live==2);
 cb_reset(); inline_delivery=true; CHECK(!cb_start_case("inline_callback_disarms",EF_INLINE_CALLBACK,2));
 CHECK(bridge.inline_callbacks==1 && bridge.disarmed && !frame_calls);
 captured_callback(); CHECK(bridge.late==1 && roots_live==2);
 cb_reset(); inline_delivery=registration_throws=true; CHECK(!cb_start_case("inline_then_registration_exception",EF_REGISTER_EXCEPTION,2));
 CHECK(bridge.inline_callbacks==1 && bridge.disarmed);
 cb_reset(); disarm_during_registration=true; CHECK(!cb_start_case("disarm_with_registration_pending",EF_DISARMED,2));
 captured_callback(); CHECK(bridge.late==1 && !frame_calls && roots_live==2);
 cb_reset(); CHECK(!cb_start_case("registered_phase_unknown",EF_REGISTERED_PHASE_UNKNOWN,2));
 CHECK(pin_calls==1 && bridge.module_pinned);
 captured_callback(); CHECK(bridge.state==EF_CALLBACK_PHASE_UNKNOWN && bridge.frame_available && bridge.frame==123);
 CHECK(bridge.callbacks==1 && frame_calls==1 && !bridge.disarmed);
 captured_callback(); CHECK(bridge.repeated==1 && frame_calls==1 && last_snapshot.event==EF_REPEAT_CALLBACK);
 ef_disarm(&bridge); captured_callback(); CHECK(bridge.late==1 && last_snapshot.event==EF_LATE_CALLBACK && roots_live==2);
 cb_reset(); CHECK(!cb_start_case("wrong_thread_delivery",EF_REGISTERED_PHASE_UNKNOWN,2));
 HANDLE t=CreateThread(NULL,0,cb_foreign,NULL,0,NULL); CHECK(t && WaitForSingleObject(t,5000)==WAIT_OBJECT_0); CloseHandle(t);
 CHECK(bridge.wrong_thread==1 && bridge.disarmed && !frame_calls && roots_live==2);
 cb_reset(); CHECK(!cb_start_case("nested_delivery_during_frame_getter",EF_REGISTERED_PHASE_UNKNOWN,2));
 nested_frame=true; captured_callback(); CHECK(bridge.nested==1 && bridge.disarmed && bridge.state==EF_NESTED_CALLBACK && !bridge.frame_available);
 cb_reset(); CHECK(!cb_start_case("own_render_depth_rejected",EF_REGISTERED_PHASE_UNKNOWN,2));
 InterlockedExchange(&bridge.own_render_depth,1); captured_callback(); CHECK(bridge.state==EF_NESTED_OWN_RENDER && !frame_calls);
 cb_reset(); CHECK(!cb_start_case("frame_exception",EF_REGISTERED_PHASE_UNKNOWN,2)); frame_throws=true; captured_callback();
 CHECK(bridge.state==EF_FRAME_EXCEPTION && bridge.disarmed && !bridge.frame_available);
 cb_reset(); CHECK(!cb_start_case("frame_wrong_type",EF_REGISTERED_PHASE_UNKNOWN,2)); bad_frame=true; captured_callback();
 CHECK(bridge.state==EF_FRAME_RESULT_REJECTED && bridge.disarmed);
 cb_reset(); CHECK(!cb_start_case("disarm_before_delivery",EF_REGISTERED_PHASE_UNKNOWN,2)); ef_disarm(&bridge); captured_callback();
 CHECK(bridge.late==1 && !frame_calls && bridge.state==EF_DISARMED && roots_live==2);
 printf("RESULT cases=%u assertions=%u phase=UNVERIFIED tokens=0 producer_calls=0 target_AOT=UNVERIFIED unload=UNSUPPORTED\n",cases,assertions);
 return 0;
}
