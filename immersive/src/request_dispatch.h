/* Request binding only, not a stereo producer or proof of target callability.
 * ABI: pinned Unity 2022.3 il2cpp-api-functions.h. No MethodInfo layout access.
 * Managed contract: Camera.bindings.cs 5b13d21... L413/452 and documented
 * MethodInfo.MakeGenericMethod(Type[]), Array.SetValue/GetValue APIs.
 * Parameterized reflection uses runtime_invoke_convert_args (boxed references
 * and boxed Int32), avoiding native guessing of a generic/instance icall ABI.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
typedef struct Il2CppException Il2CppException;
typedef struct Il2CppReflectionMethod Il2CppReflectionMethod;

#define RD_API_LIST(X) \
 X(const MethodInfo*,class_get_methods,(Il2CppClass*,void**)) \
 X(Il2CppClass*,class_get_parent,(Il2CppClass*)) \
 X(const char*,method_get_name,(const MethodInfo*)) \
 X(Il2CppClass*,method_get_class,(const MethodInfo*)) \
 X(uint32_t,method_get_param_count,(const MethodInfo*)) \
 X(const Il2CppType*,method_get_param,(const MethodInfo*,uint32_t)) \
 X(const Il2CppType*,method_get_return_type,(const MethodInfo*)) \
 X(uint32_t,method_get_flags,(const MethodInfo*,uint32_t*)) \
 X(bool,method_is_generic,(const MethodInfo*)) \
 X(bool,method_is_inflated,(const MethodInfo*)) \
 X(bool,type_is_byref,(const Il2CppType*)) \
 X(int,type_get_type,(const Il2CppType*)) \
 X(Il2CppClass*,class_from_type,(const Il2CppType*)) \
 X(Il2CppClass*,object_get_class,(Il2CppObject*)) \
 X(bool,class_is_assignable_from,(Il2CppClass*,Il2CppClass*)) \
 X(Il2CppObject*,runtime_invoke,(const MethodInfo*,void*,void**,Il2CppException**)) \
 X(Il2CppObject*,runtime_invoke_convert_args,(const MethodInfo*,void*,Il2CppObject**,int,Il2CppException**)) \
 X(Il2CppReflectionMethod*,method_get_object,(const MethodInfo*,Il2CppClass*)) \
 X(const MethodInfo*,method_get_from_reflection,(const Il2CppReflectionMethod*)) \
 X(const MethodInfo*,object_get_virtual_method,(Il2CppObject*,const MethodInfo*)) \
 X(Il2CppArray*,array_new,(Il2CppClass*,uintptr_t)) \
 X(uint32_t,array_length,(Il2CppArray*)) \
 X(int,class_get_rank,(const Il2CppClass*)) \
 X(Il2CppClass*,class_get_element_class,(Il2CppClass*)) \
 X(const Il2CppType*,class_get_type,(Il2CppClass*)) \
 X(Il2CppReflectionType*,type_get_object,(const Il2CppType*)) \
 X(Il2CppClass*,class_from_system_type,(Il2CppReflectionType*)) \
 X(Il2CppObject*,value_box,(Il2CppClass*,void*)) \
 X(void*,object_unbox,(Il2CppObject*)) \
 X(uint32_t,gchandle_new,(Il2CppObject*,bool)) \
 X(Il2CppObject*,gchandle_get_target,(uint32_t)) \
 X(void,gchandle_free,(uint32_t)) \
 X(Il2CppThread*,thread_current,(void)) \
 X(const char*,class_get_name,(Il2CppClass*)) \
 X(const char*,class_get_namespace,(Il2CppClass*))
typedef struct RdApi {
#define RD_FIELD(ret,name,args) ret (*name) args;
 RD_API_LIST(RD_FIELD)
#undef RD_FIELD
} RdApi;
typedef enum RdStatus {
 RD_UNBOUND, RD_BOUND_UNTESTED, RD_MISSING_API, RD_MISSING_TYPE,
 RD_METHOD_MISSING, RD_AMBIGUOUS, RD_METADATA_LIMIT, RD_ROOT_FAILED,
 RD_REFLECTION_CONTRACT, RD_REFLECTION_EXCEPTION, RD_CLOSED_REJECTED,
 RD_THREAD_REJECTED, RD_CALL_NOT_AUTHORIZED, RD_REENTRANT,
 RD_ARGUMENT_REJECTED, RD_INVOKE_EXCEPTION, RD_SUBMIT_RETURNED_UNVERIFIED
} RdStatus;
typedef enum RdRoute { RD_NONE, RD_DIRECT_OBJECT, RD_REFLECTED_OBJECT } RdRoute;
typedef struct RdTypes {
 Il2CppClass *camera,*object,*type,*boolean,*int32,*method_info;
} RdTypes;
typedef struct RdBinding {
 RdApi api; RdTypes types;
 const MethodInfo* method;
 uint32_t reflection_root;
 DWORD thread_id;
 RdStatus status;
 RdRoute route;
 const char* stage;
 const char *exception_name,*exception_namespace;
 bool attempted, in_submit;
} RdBinding;
typedef enum RdKind { RK_VOID,RK_OBJECT,RK_TYPE_ARRAY,RK_METHODINFO,RK_BOOL,RK_INT32,RK_MVAR } RdKind;
typedef struct RdTemps { uint32_t handles[8]; unsigned count; } RdTemps;

static const char* rd_status_name(RdStatus s) {
 static const char* const names[]={"unbound","bound_unexecuted","missing_api","missing_type",
  "method_missing","ambiguous","metadata_limit","root_failed","reflection_contract",
  "reflection_exception","closed_contract_rejected","thread_rejected","call_not_authorized",
  "reentrant","argument_rejected","invoke_exception_or_aot_unavailable","submit_returned_unverified"};
 return (unsigned)s<sizeof(names)/sizeof(names[0])?names[s]:"unknown";
}
static void rd_release(RdBinding* b) {
 if (b->reflection_root && b->api.gchandle_free) b->api.gchandle_free(b->reflection_root);
 b->reflection_root=0; b->method=NULL; b->route=RD_NONE;
 /* attempted deliberately survives: no automatic reflection retry/cache growth. */
 b->status=RD_UNBOUND;
}
static void rd_exception(RdBinding* b,Il2CppException* exc) {
 /* Copy only metadata identities; never retain/dereference exception layout.
    An exception type is evidence, not an automatic diagnosis of AOT/renderer. */
 Il2CppClass* c=b->api.object_get_class((Il2CppObject*)exc);
 b->exception_name=c && b->api.class_get_name?b->api.class_get_name(c):NULL;
 b->exception_namespace=c && b->api.class_get_namespace?b->api.class_get_namespace(c):NULL;
}
static uint32_t rd_root(RdBinding* b,RdTemps* t,Il2CppObject* obj) {
 if (!obj || t->count==8) { b->status=RD_ROOT_FAILED; return 0; }
 uint32_t h=b->api.gchandle_new(obj,false);
 if (!h || !b->api.gchandle_get_target(h)) {
  if(h) b->api.gchandle_free(h); b->status=RD_ROOT_FAILED; return 0;
 }
 t->handles[t->count++]=h; return h;
}
static bool rd_type(RdBinding* b,const Il2CppType* t,RdKind k) {
 RdApi* a=&b->api;
 if (!t || a->type_is_byref(t)) return false;
 if (k==RK_VOID) return a->type_get_type(t)==1;
 if (k==RK_MVAR) return a->type_get_type(t)==30;
 if (a->type_get_type(t)==30 || a->type_get_type(t)==19) return false;
 Il2CppClass* c=a->class_from_type(t);
 if (k==RK_TYPE_ARRAY) return c && a->class_get_rank(c)==1 && a->class_get_element_class(c)==b->types.type;
 Il2CppClass* expected=k==RK_OBJECT?b->types.object:k==RK_BOOL?b->types.boolean:
  k==RK_INT32?b->types.int32:b->types.method_info;
 return c && c==expected;
}
static bool rd_signature(RdBinding* b,const MethodInfo* m,Il2CppClass* owner,
 const char* name,RdKind ret,unsigned argc,const RdKind* args) {
 RdApi* a=&b->api; uint32_t impl=0;
 if (!m || a->method_get_class(m)!=owner || !a->method_get_name(m) ||
  strcmp(a->method_get_name(m),name) || a->method_get_param_count(m)!=argc ||
  (a->method_get_flags(m,&impl)&(0x10u|0x400u)) || !rd_type(b,a->method_get_return_type(m),ret)) return false;
 /* ECMA flags from the pinned il2cpp-tabledefs.h; the official Camera
    binding has private internal submit and public generic submit. */
 uint32_t access=a->method_get_flags(m,&impl)&0x7u;
 if ((!strcmp(name,"SubmitRenderRequestsInternal") && access!=0x1u) ||
     (!strcmp(name,"SubmitRenderRequest") && access!=0x6u)) return false;
 for (unsigned i=0;i<argc;++i) if (!rd_type(b,a->method_get_param(m,i),args[i])) return false;
 return true;
}
static const MethodInfo* rd_find(RdBinding* b,Il2CppClass* owner,const char* name,
 RdKind ret,unsigned argc,const RdKind* args,bool generic,bool parents) {
 RdApi* a=&b->api;
 for (unsigned depth=0;owner && depth<8;++depth) {
  void* iter=NULL; const MethodInfo *m,*found=NULL; unsigned count=0;
  while ((m=a->class_get_methods(owner,&iter))) {
   if (++count>512) { b->status=RD_METADATA_LIMIT; return NULL; }
   if (rd_signature(b,m,owner,name,ret,argc,args) && a->method_is_generic(m)==generic &&
       !a->method_is_inflated(m)) {
    if(found) { b->status=RD_AMBIGUOUS; return NULL; } found=m;
   }
  }
  if(found) return found;
  if(!parents) break;
  owner=a->class_get_parent(owner);
 }
 if (owner && parents) b->status=RD_METADATA_LIMIT;
 return NULL;
}
static Il2CppObject* rd_reflect_call(RdBinding* b,Il2CppObject* receiver,const char* name,
 RdKind ret,unsigned argc,const RdKind* kinds,Il2CppObject** args) {
 if (!receiver) { b->status=RD_REFLECTION_CONTRACT; return NULL; }
 const MethodInfo* m=rd_find(b,b->api.object_get_class(receiver),name,ret,argc,kinds,false,true);
 if(!m) { if(b->status==RD_UNBOUND) b->status=RD_REFLECTION_CONTRACT; return NULL; }
 uint32_t impl=0;
 if(b->api.method_get_flags(m,&impl)&0x40u) m=b->api.object_get_virtual_method(receiver,m);
 if(!m || !rd_signature(b,m,b->api.method_get_class(m),name,ret,argc,kinds) ||
  b->api.method_is_generic(m) || b->api.method_is_inflated(m) ||
  !b->api.class_is_assignable_from(b->api.method_get_class(m),b->api.object_get_class(receiver))) {
  b->status=RD_REFLECTION_CONTRACT; return NULL;
 }
 Il2CppException* exc=NULL;
 Il2CppObject* result=argc?b->api.runtime_invoke_convert_args(m,receiver,args,(int)argc,&exc):
  b->api.runtime_invoke(m,receiver,NULL,&exc);
 if(exc) { rd_exception(b,exc); b->status=RD_REFLECTION_EXCEPTION; return NULL; }
 return result;
}
static bool rd_bool(RdBinding* b,Il2CppObject* receiver,const char* property,bool* value) {
 b->stage=property;
 Il2CppObject* obj=rd_reflect_call(b,receiver,property,RK_BOOL,0,NULL,NULL);
 if (!obj || b->status!=RD_UNBOUND || b->api.object_get_class(obj)!=b->types.boolean) {
  if(b->status==RD_UNBOUND) b->status=RD_REFLECTION_CONTRACT; return false;
 }
 void* payload=b->api.object_unbox(obj);
 if(!payload) { b->status=RD_REFLECTION_CONTRACT; return false; }
 *value=*(const uint8_t*)payload!=0; return true;
}
static bool rd_generic_arg(RdBinding* b,RdTemps* t,Il2CppObject* reflection,
 uint32_t index,bool require_object) {
 b->stage="GetGenericArguments";
 Il2CppObject* array=rd_reflect_call(b,reflection,"GetGenericArguments",RK_TYPE_ARRAY,0,NULL,NULL);
 if(!array || b->status!=RD_UNBOUND || b->api.class_get_rank(b->api.object_get_class(array))!=1 ||
  b->api.class_get_element_class(b->api.object_get_class(array))!=b->types.type ||
  b->api.array_length((Il2CppArray*)array)!=1) {
  if(b->status==RD_UNBOUND) b->status=RD_REFLECTION_CONTRACT; return false;
 }
 uint32_t h=rd_root(b,t,array); if(!h) return false;
 if(require_object) {
  RdKind k=RK_INT32; Il2CppObject* args[]={b->api.gchandle_get_target(index)};
  Il2CppObject* type=rd_reflect_call(b,b->api.gchandle_get_target(h),"GetValue",RK_OBJECT,1,&k,args);
  if(!type || b->status!=RD_UNBOUND || !b->api.class_is_assignable_from(b->types.type,b->api.object_get_class(type)) ||
   b->api.class_from_system_type((Il2CppReflectionType*)type)!=b->types.object) {
   if(b->status==RD_UNBOUND) b->status=RD_CLOSED_REJECTED; return false;
  }
 }
 return true;
}
static RdStatus rd_bind(RdBinding* b,const RdApi* api,const RdTypes* types) {
 if(b->attempted) return b->status;
 b->attempted=true; b->api=*api; b->types=*types; b->thread_id=GetCurrentThreadId();
 b->status=RD_UNBOUND; b->stage="exports";
 /* Binding is bounded to once per loaded IL2CPP lifetime. All exports are
    optional to the accepted Live/PANEL adapter, never its readiness floor. */
#define RD_REQUIRE(name) if(!api->name) { b->stage="il2cpp_" #name; return b->status=RD_MISSING_API; }
 RD_REQUIRE(class_get_methods) RD_REQUIRE(method_get_name) RD_REQUIRE(method_get_class)
 RD_REQUIRE(method_get_param_count) RD_REQUIRE(method_get_param) RD_REQUIRE(method_get_return_type)
 RD_REQUIRE(method_get_flags) RD_REQUIRE(method_is_generic) RD_REQUIRE(method_is_inflated)
 RD_REQUIRE(type_is_byref) RD_REQUIRE(type_get_type) RD_REQUIRE(class_from_type)
 RD_REQUIRE(object_get_class) RD_REQUIRE(class_is_assignable_from) RD_REQUIRE(runtime_invoke_convert_args)
 RD_REQUIRE(gchandle_get_target) RD_REQUIRE(thread_current)
#undef RD_REQUIRE
 if(!api->thread_current()) return b->status=RD_THREAD_REJECTED;
 if(!types->camera || !types->object) {
  b->stage=types->camera?"System.Object":"UnityEngine.Camera";
  return b->status=RD_MISSING_TYPE;
 }
 RdKind object=RK_OBJECT;
 b->stage="SubmitRenderRequestsInternal";
 b->method=rd_find(b,types->camera,"SubmitRenderRequestsInternal",RK_VOID,1,&object,false,false);
 if(b->status!=RD_UNBOUND) return b->status;
 if(b->method) { b->route=RD_DIRECT_OBJECT; return b->status=RD_BOUND_UNTESTED; }
 b->stage="reflection_exports";
#define RD_CHECK(ret,name,args) if(!api->name) { b->stage="il2cpp_" #name; return b->status=RD_MISSING_API; }
 RD_API_LIST(RD_CHECK)
#undef RD_CHECK
 if(!types->type || !types->boolean || !types->int32 || !types->method_info) {
  b->stage=!types->type?"System.Type":!types->boolean?"System.Boolean":!types->int32?"System.Int32":"System.Reflection.MethodInfo";
  return b->status=RD_MISSING_TYPE;
 }
 b->stage="SubmitRenderRequest_definition";
 RdKind mvar=RK_MVAR;
 const MethodInfo* definition=rd_find(b,types->camera,"SubmitRenderRequest",RK_VOID,1,&mvar,true,false);
 if(!definition) { if(b->status==RD_UNBOUND) b->status=RD_METHOD_MISSING; return b->status; }
 RdTemps temps={0};
 b->stage="method_get_object";
 Il2CppObject* reflection=(Il2CppObject*)api->method_get_object(definition,types->camera);
 if(!reflection || !api->class_is_assignable_from(types->method_info,api->object_get_class(reflection))) {
  b->status=RD_REFLECTION_CONTRACT; goto done;
 }
 uint32_t original=rd_root(b,&temps,reflection); if(!original) goto done;
 bool is_definition=false;
 if(!rd_bool(b,api->gchandle_get_target(original),"get_IsGenericMethodDefinition",&is_definition) || !is_definition) {
  if(b->status==RD_UNBOUND) b->status=RD_REFLECTION_CONTRACT; goto done;
 }
 int32_t zero=0;
 b->stage="boxed_index";
 uint32_t index=rd_root(b,&temps,api->value_box(types->int32,&zero)); if(!index) goto done;
 if(!rd_generic_arg(b,&temps,api->gchandle_get_target(original),index,false)) goto done;
 b->stage="typeof_object";
 uint32_t argument=rd_root(b,&temps,(Il2CppObject*)api->type_get_object(api->class_get_type(types->object)));
 if(!argument) goto done;
 b->stage="Type_array";
 uint32_t array=rd_root(b,&temps,(Il2CppObject*)api->array_new(types->type,1)); if(!array) goto done;
 RdKind set_kinds[]={RK_OBJECT,RK_INT32};
 Il2CppObject* set_args[]={api->gchandle_get_target(argument),api->gchandle_get_target(index)};
 b->stage="Type_array_SetValue";
 (void)rd_reflect_call(b,api->gchandle_get_target(array),"SetValue",RK_VOID,2,set_kinds,set_args);
 if(b->status!=RD_UNBOUND) goto done;
 RdKind type_array=RK_TYPE_ARRAY; Il2CppObject* make_args[]={api->gchandle_get_target(array)};
 b->stage="MakeGenericMethod";
 reflection=rd_reflect_call(b,api->gchandle_get_target(original),"MakeGenericMethod",RK_METHODINFO,1,&type_array,make_args);
 if(!reflection || b->status!=RD_UNBOUND || !api->class_is_assignable_from(types->method_info,api->object_get_class(reflection))) {
  if(b->status==RD_UNBOUND) b->status=RD_REFLECTION_CONTRACT; goto done;
 }
 uint32_t closed=rd_root(b,&temps,reflection); if(!closed) goto done;
 bool contains=true;
 if(!rd_bool(b,api->gchandle_get_target(closed),"get_ContainsGenericParameters",&contains) || contains) {
  if(b->status==RD_UNBOUND) b->status=RD_CLOSED_REJECTED; goto done;
 }
 if(!rd_generic_arg(b,&temps,api->gchandle_get_target(closed),index,true)) goto done;
 b->stage="closed_MethodInfo";
 const MethodInfo* method=api->method_get_from_reflection((Il2CppReflectionMethod*)api->gchandle_get_target(closed));
 if(!rd_signature(b,method,types->camera,"SubmitRenderRequest",RK_VOID,1,&object) ||
  api->method_is_generic(method) || !api->method_is_inflated(method)) {
  b->status=RD_CLOSED_REJECTED; goto done;
 }
 b->method=method; b->route=RD_REFLECTED_OBJECT; b->reflection_root=closed;
 b->status=RD_BOUND_UNTESTED;
done:
 for(unsigned i=0;i<temps.count;++i) if(temps.handles[i]!=b->reflection_root) api->gchandle_free(temps.handles[i]);
 return b->status;
}

/* Future owned-eye adapter boundary. Caller supplies strong owned roots and a
 * current nonzero generation, plus separately evidenced support/engine entry.
 * No current Home/Live call site invokes it. A void return is not renderer/GPU
 * completion, and managed exceptions (including AOT failures) poison this binding.
 */
static inline RdStatus rd_submit_owned(RdBinding* b,uint32_t camera_root,uint32_t request_root,
 uint64_t generation,uint64_t current_generation,bool entry_verified,bool support_verified) {
 if(GetCurrentThreadId()!=b->thread_id) return RD_THREAD_REJECTED;
 if(b->status!=RD_BOUND_UNTESTED || !b->method) return b->status;
 if(!b->api.thread_current()) return RD_THREAD_REJECTED;
 if(b->in_submit) return RD_REENTRANT;
 if(!entry_verified || !support_verified || !generation || generation!=current_generation) return RD_CALL_NOT_AUTHORIZED;
 Il2CppObject* camera=b->api.gchandle_get_target(camera_root);
 Il2CppObject* request=b->api.gchandle_get_target(request_root);
 if(!camera || !request || camera==request ||
  !b->api.class_is_assignable_from(b->types.camera,b->api.object_get_class(camera))) return RD_ARGUMENT_REJECTED;
 b->in_submit=true; Il2CppException* exc=NULL; Il2CppObject* args[]={request};
 (void)b->api.runtime_invoke_convert_args(b->method,camera,args,1,&exc);
 b->in_submit=false;
 if(exc) { rd_exception(b,exc); rd_release(b); b->stage="submit_runtime_invoke"; return b->status=RD_INVOKE_EXCEPTION; }
 return RD_SUBMIT_RETURNED_UNVERIFIED;
}
