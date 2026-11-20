#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct Il2CppClass Il2CppClass;
typedef struct Il2CppType { int code; Il2CppClass* klass; bool byref; } Il2CppType;
/* Intentionally no native pointer/invoker/tail: the binder must use APIs. */
typedef struct MethodInfo {
 const char* name; Il2CppClass* owner; Il2CppType* ret; Il2CppType* params[4];
 unsigned argc,flags,id; bool generic,inflated;
} MethodInfo;
typedef struct Il2CppObject {
 Il2CppClass* klass; const MethodInfo* method; struct Il2CppObject* element;
 Il2CppClass* type_argument; int value; unsigned length;
} Il2CppObject;
typedef struct Il2CppArray { Il2CppObject object; } Il2CppArray;
typedef struct Il2CppReflectionType { Il2CppObject object; } Il2CppReflectionType;
typedef struct Il2CppThread { int unused; } Il2CppThread;
struct Il2CppClass {
 const char* name; Il2CppClass* parent; const MethodInfo* methods[64]; unsigned count;
 Il2CppType type; int rank; Il2CppClass* element;
};
#include "../src/request_dispatch.h"
struct Il2CppException { Il2CppObject object; };
struct Il2CppReflectionMethod { Il2CppObject object; };
static unsigned assertions,cases;
#define CHECK(x) do { ++assertions; if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static Il2CppClass cam,obj,typ,boolean,i32,method_info,runtime_method,array_class,type_array,exception_class;
static Il2CppType void_type,mvar_type,closed_param;
static MethodInfo direct,definition,closed,make,args_method,is_definition,contains,set,get;
static Il2CppObject reflect_def,reflect_closed,object_type,placeholder,zero_box,true_box,false_box;
static Il2CppObject definition_args,closed_args,allocated_array,camera_object,request_object;
static Il2CppException exception;
static Il2CppThread thread;
static Il2CppObject* roots[64];
static unsigned roots_live,root_calls,root_fail_at,make_calls,submit_calls,virtual_calls;
static bool attached,make_exception,submit_exception,open_remaining,wrong_arg,limit_scan,reenter,reflection_null;
static RdBinding* active_binding;
static RdStatus recursive_status;
static uint32_t camera_root,request_root;

static const MethodInfo* f_methods(Il2CppClass* c,void** it) {
 uintptr_t n=(uintptr_t)*it;
 if(limit_scan && c==&cam) { if(n==513) return NULL; *it=(void*)(n+1); return &contains; }
 if(n>=c->count) return NULL; *it=(void*)(n+1); return c->methods[n];
}
static Il2CppClass* f_parent(Il2CppClass* c) { return c->parent; }
static const char* f_class_name(Il2CppClass* c) { return c->name; }
static const char* f_namespace(Il2CppClass* c) { (void)c; return "System"; }
static const char* f_name(const MethodInfo* m) { return m->name; }
static Il2CppClass* f_owner(const MethodInfo* m) { return m->owner; }
static uint32_t f_count(const MethodInfo* m) { return m->argc; }
static const Il2CppType* f_param(const MethodInfo* m,uint32_t n) { return n<m->argc?m->params[n]:NULL; }
static const Il2CppType* f_return(const MethodInfo* m) { return m->ret; }
static uint32_t f_flags(const MethodInfo* m,uint32_t* impl) { *impl=0; return m->flags; }
static bool f_generic(const MethodInfo* m) { return m->generic; }
static bool f_inflated(const MethodInfo* m) { return m->inflated; }
static bool f_byref(const Il2CppType* t) { return t->byref; }
static int f_code(const Il2CppType* t) { return t->code; }
static Il2CppClass* f_from_type(const Il2CppType* t) { return t->klass; }
static Il2CppClass* f_class(Il2CppObject* o) { return o?o->klass:NULL; }
static bool f_assignable(Il2CppClass* c,Il2CppClass* actual) {
 for(;actual;actual=actual->parent) if(actual==c) return true; return false;
}
static Il2CppReflectionMethod* f_reflect(const MethodInfo* m,Il2CppClass* c) {
 return !reflection_null && m==&definition && c==&cam?(Il2CppReflectionMethod*)&reflect_def:NULL;
}
static const MethodInfo* f_unreflect(const Il2CppReflectionMethod* m) {
 return (const Il2CppObject*)m==&reflect_closed?&closed:NULL;
}
static const MethodInfo* f_virtual(Il2CppObject* o,const MethodInfo* m) {
 ++virtual_calls; return f_assignable(m->owner,o->klass)?m:NULL;
}
static Il2CppArray* f_array(Il2CppClass* c,uintptr_t n) {
 if(c!=&typ || n!=1) return NULL; return (Il2CppArray*)&allocated_array;
}
static uint32_t f_length(Il2CppArray* a) { return ((Il2CppObject*)a)->length; }
static int f_rank(const Il2CppClass* c) { return c->rank; }
static Il2CppClass* f_element(Il2CppClass* c) { return c->element; }
static const Il2CppType* f_type(Il2CppClass* c) { return &c->type; }
static Il2CppReflectionType* f_type_object(const Il2CppType* t) {
 return t==&obj.type?(Il2CppReflectionType*)&object_type:NULL;
}
static Il2CppClass* f_system_type(Il2CppReflectionType* t) { return ((Il2CppObject*)t)->type_argument; }
static Il2CppObject* f_box(Il2CppClass* c,void* p) {
 return c==&i32 && *(int32_t*)p==0?&zero_box:NULL;
}
static void* f_unbox(Il2CppObject* o) { return &o->value; }
static uint32_t f_root(Il2CppObject* o,bool pinned) {
 (void)pinned; if(++root_calls==root_fail_at) return 0;
 for(unsigned i=1;i<64;++i) if(!roots[i]) { roots[i]=o; ++roots_live; return i; } return 0;
}
static Il2CppObject* f_target(uint32_t h) { return h<64?roots[h]:NULL; }
static void f_free(uint32_t h) { if(h<64 && roots[h]) { roots[h]=NULL; --roots_live; } }
static Il2CppThread* f_thread(void) { return attached?&thread:NULL; }
static Il2CppObject* f_invoke(const MethodInfo* m,void* receiver,void** params,Il2CppException** exc) {
 *exc=NULL;
 if(params || !receiver || m->argc) { *exc=&exception; return NULL; }
 if(m==&is_definition) return receiver==&reflect_def?&true_box:&false_box;
 if(m==&contains) return open_remaining?&true_box:&false_box;
 if(m==&args_method) return receiver==&reflect_def?&definition_args:&closed_args;
 *exc=&exception; return NULL;
}
static Il2CppObject* f_convert(const MethodInfo* m,void* receiver,Il2CppObject** argv,int argc,Il2CppException** exc) {
 *exc=NULL;
 if(!receiver || !argv || argc!=(int)m->argc) { *exc=&exception; return NULL; }
 if(m==&set) {
  if(argv[0]!=&object_type || argv[1]!=&zero_box || receiver!=&allocated_array) { *exc=&exception; return NULL; }
  allocated_array.element=argv[0]; return NULL;
 }
 if(m==&get) return argv[0]==&zero_box?((Il2CppObject*)receiver)->element:NULL;
 if(m==&make) {
  ++make_calls;
  if(make_exception || receiver!=&reflect_def || argv[0]!=&allocated_array || allocated_array.element!=&object_type) {
   *exc=&exception; return NULL;
  }
  closed_args.element=wrong_arg?&placeholder:&object_type; return &reflect_closed;
 }
 if(m==&direct || m==&closed) {
  if(receiver!=&camera_object || argv[0]!=&request_object) { *exc=&exception; return NULL; }
  ++submit_calls;
  if(reenter) recursive_status=rd_submit_owned(active_binding,camera_root,request_root,7,7,true,true);
  if(submit_exception) *exc=&exception;
  return NULL;
 }
 *exc=&exception; return NULL;
}
static RdApi api;
static RdTypes types;
static void reset(bool use_direct) {
 memset(roots,0,sizeof(roots)); roots_live=root_calls=root_fail_at=make_calls=submit_calls=virtual_calls=0;
 attached=true; make_exception=submit_exception=open_remaining=wrong_arg=limit_scan=reenter=reflection_null=false;
 cam=(Il2CppClass){.name="Camera"}; obj=(Il2CppClass){.name="Object"};
 typ=(Il2CppClass){.name="Type"}; boolean=(Il2CppClass){.name="Boolean"}; i32=(Il2CppClass){.name="Int32"};
 method_info=(Il2CppClass){.name="MethodInfo"}; runtime_method=(Il2CppClass){.name="RuntimeMethodInfo",.parent=&method_info};
 exception_class=(Il2CppClass){.name="NotSupportedException"}; exception.object.klass=&exception_class;
 array_class=(Il2CppClass){.name="Array"}; type_array=(Il2CppClass){.name="Type[]",.parent=&array_class,.rank=1,.element=&typ};
 Il2CppClass* classes[]={&cam,&obj,&typ,&boolean,&i32,&method_info,&runtime_method,&array_class,&type_array};
 for(unsigned n=0;n<9;++n) classes[n]->type=(Il2CppType){.code=18,.klass=classes[n]};
 boolean.type.code=2; i32.type.code=8;
 void_type=(Il2CppType){.code=1}; mvar_type=(Il2CppType){.code=30}; closed_param=obj.type;
 direct=(MethodInfo){.name="SubmitRenderRequestsInternal",.owner=&cam,.ret=&void_type,.params={&obj.type},.argc=1,.flags=1};
 definition=(MethodInfo){.name="SubmitRenderRequest",.owner=&cam,.ret=&void_type,.params={&mvar_type},.argc=1,.flags=6,.generic=true};
 closed=(MethodInfo){.name="SubmitRenderRequest",.owner=&cam,.ret=&void_type,.params={&closed_param},.argc=1,.flags=6,.generic=false,.inflated=true};
 make=(MethodInfo){.name="MakeGenericMethod",.owner=&runtime_method,.ret=&method_info.type,.params={&type_array.type},.argc=1,.flags=0x46};
 args_method=(MethodInfo){.name="GetGenericArguments",.owner=&runtime_method,.ret=&type_array.type,.flags=0x46};
 is_definition=(MethodInfo){.name="get_IsGenericMethodDefinition",.owner=&runtime_method,.ret=&boolean.type,.flags=0x46};
 contains=(MethodInfo){.name="get_ContainsGenericParameters",.owner=&runtime_method,.ret=&boolean.type,.flags=0x46};
 set=(MethodInfo){.name="SetValue",.owner=&array_class,.ret=&void_type,.params={&obj.type,&i32.type},.argc=2};
 get=(MethodInfo){.name="GetValue",.owner=&array_class,.ret=&obj.type,.params={&i32.type},.argc=1};
 cam.methods[0]=use_direct?&direct:&definition; cam.count=1;
 runtime_method.methods[0]=&make; runtime_method.methods[1]=&args_method; runtime_method.methods[2]=&is_definition; runtime_method.methods[3]=&contains; runtime_method.count=4;
 array_class.methods[0]=&set; array_class.methods[1]=&get; array_class.count=2;
 reflect_def=(Il2CppObject){.klass=&runtime_method,.method=&definition};
 reflect_closed=(Il2CppObject){.klass=&runtime_method,.method=&closed};
 object_type=(Il2CppObject){.klass=&typ,.type_argument=&obj}; placeholder=(Il2CppObject){.klass=&typ,.type_argument=&typ};
 zero_box=(Il2CppObject){.klass=&i32}; true_box=(Il2CppObject){.klass=&boolean,.value=1}; false_box=(Il2CppObject){.klass=&boolean};
 definition_args=(Il2CppObject){.klass=&type_array,.length=1,.element=&placeholder};
 closed_args=(Il2CppObject){.klass=&type_array,.length=1,.element=&object_type};
 allocated_array=(Il2CppObject){.klass=&type_array,.length=1};
 camera_object=(Il2CppObject){.klass=&cam}; request_object=(Il2CppObject){.klass=&obj};
 types=(RdTypes){&cam,&obj,&typ,&boolean,&i32,&method_info};
 api=(RdApi){f_methods,f_parent,f_name,f_owner,f_count,f_param,f_return,f_flags,f_generic,f_inflated,f_byref,f_code,f_from_type,f_class,f_assignable,f_invoke,f_convert,f_reflect,f_unreflect,f_virtual,f_array,f_length,f_rank,f_element,f_type,f_type_object,f_system_type,f_box,f_unbox,f_root,f_target,f_free,f_thread,f_class_name,f_namespace};
}
static int bind_case(const char* label,RdStatus expected) {
 RdBinding b={0};
 CHECK(rd_bind(&b,&api,&types)==expected);
 unsigned calls=make_calls,root_count=root_calls;
 CHECK(rd_bind(&b,&api,&types)==expected && make_calls==calls && root_calls==root_count);
 if(expected==RD_BOUND_UNTESTED) CHECK(roots_live==(b.route==RD_REFLECTED_OBJECT?1u:0u));
 else CHECK(!roots_live && !submit_calls && !b.method && b.route==RD_NONE);
 if(expected==RD_REFLECTION_EXCEPTION) CHECK(b.exception_name && !strcmp(b.exception_name,exception_class.name) && !strcmp(b.exception_namespace,"System"));
 rd_release(&b); rd_release(&b); CHECK(!roots_live);
 printf("PASS %s\n",label); ++cases; return 0;
}
static DWORD WINAPI foreign_submit(LPVOID p) {
 recursive_status=rd_submit_owned(p,camera_root,request_root,7,7,true,true); return 0;
}
int main(void) {
 reset(true); CHECK(!bind_case("direct",RD_BOUND_UNTESTED));
 reset(true); api.method_get_object=NULL; CHECK(!bind_case("direct_no_reflection_export",RD_BOUND_UNTESTED));
 reset(true); types.type=types.boolean=types.int32=types.method_info=NULL; CHECK(!bind_case("direct_no_reflection_types",RD_BOUND_UNTESTED));
 reset(true); api.runtime_invoke_convert_args=NULL; CHECK(!bind_case("missing_invoke_export",RD_MISSING_API));
 reset(false); api.method_get_object=NULL; CHECK(!bind_case("missing_reflection_export",RD_MISSING_API));
 reset(false); attached=false; CHECK(!bind_case("detached",RD_THREAD_REJECTED));
 reset(false); types.type=NULL; CHECK(!bind_case("missing_type",RD_MISSING_TYPE));
 reset(false); cam.count=0; CHECK(!bind_case("missing_both_methods",RD_METHOD_MISSING));
 reset(true); cam.methods[1]=&direct; cam.count=2; CHECK(!bind_case("ambiguous_direct",RD_AMBIGUOUS));
 reset(false); limit_scan=true; CHECK(!bind_case("metadata_limit",RD_METADATA_LIMIT));
 reset(false); CHECK(!bind_case("closed_reflection",RD_BOUND_UNTESTED));
 reset(false); reflection_null=true; CHECK(!bind_case("missing_reflection_object",RD_REFLECTION_CONTRACT));
 reset(false); make_exception=true; CHECK(!bind_case("MakeGenericMethod_exception_or_AOT",RD_REFLECTION_EXCEPTION));
 reset(false); make_exception=true; exception_class.name="MissingMethodException"; CHECK(!bind_case("MakeGenericMethod_missing_method_exception",RD_REFLECTION_EXCEPTION));
 reset(false); make_exception=true; exception_class.name="ExecutionEngineException"; CHECK(!bind_case("MakeGenericMethod_execution_engine_exception",RD_REFLECTION_EXCEPTION));
 reset(false); make.name="Unavailable"; CHECK(!bind_case("MakeGenericMethod_unavailable",RD_REFLECTION_CONTRACT));
 reset(false); open_remaining=true; CHECK(!bind_case("contains_open_parameters",RD_CLOSED_REJECTED));
 reset(false); closed.inflated=false; CHECK(!bind_case("noninflated_closed",RD_CLOSED_REJECTED));
 reset(false); closed.generic=true; CHECK(!bind_case("generic_definition_remaining",RD_CLOSED_REJECTED));
 reset(false); closed.owner=&obj; CHECK(!bind_case("wrong_declaring",RD_CLOSED_REJECTED));
 reset(false); closed.ret=&boolean.type; CHECK(!bind_case("wrong_return",RD_CLOSED_REJECTED));
 reset(false); closed.params[0]=&typ.type; CHECK(!bind_case("wrong_param",RD_CLOSED_REJECTED));
 reset(false); closed.params[0]=&mvar_type; CHECK(!bind_case("open_MVAR_param",RD_CLOSED_REJECTED));
 reset(false); closed_param.byref=true; CHECK(!bind_case("byref_param",RD_CLOSED_REJECTED));
 reset(false); closed.flags=0x16; CHECK(!bind_case("static_closed",RD_CLOSED_REJECTED));
 reset(false); closed.flags=1; CHECK(!bind_case("private_closed",RD_CLOSED_REJECTED));
 reset(false); wrong_arg=true; CHECK(!bind_case("wrong_generic_type_argument",RD_CLOSED_REJECTED));
 reset(false); definition_args.length=2; CHECK(!bind_case("wrong_generic_arity",RD_REFLECTION_CONTRACT));
 for(unsigned n=1;n<=7;++n) { reset(false); root_fail_at=n; CHECK(!bind_case("root_failure_unwind",RD_ROOT_FAILED)); }
 for(unsigned route=0;route<2;++route) {
  reset(route==0); RdBinding b={0}; active_binding=&b;
  CHECK(rd_bind(&b,&api,&types)==RD_BOUND_UNTESTED);
  camera_root=f_root(&camera_object,false); request_root=f_root(&request_object,false);
  CHECK(rd_submit_owned(&b,camera_root,request_root,7,7,false,true)==RD_CALL_NOT_AUTHORIZED);
  CHECK(rd_submit_owned(&b,camera_root,request_root,7,7,true,false)==RD_CALL_NOT_AUTHORIZED);
  CHECK(rd_submit_owned(&b,camera_root,request_root,7,8,true,true)==RD_CALL_NOT_AUTHORIZED);
  CHECK(rd_submit_owned(&b,camera_root,request_root,0,0,true,true)==RD_CALL_NOT_AUTHORIZED);
  CHECK(rd_submit_owned(&b,request_root,request_root,7,7,true,true)==RD_ARGUMENT_REJECTED);
  CHECK(rd_submit_owned(&b,0,request_root,7,7,true,true)==RD_ARGUMENT_REJECTED);
  HANDLE t=CreateThread(NULL,0,foreign_submit,&b,0,NULL); CHECK(t);
  CHECK(WaitForSingleObject(t,5000)==WAIT_OBJECT_0); CloseHandle(t);
  CHECK(recursive_status==RD_THREAD_REJECTED && !submit_calls);
  reenter=true;
  CHECK(rd_submit_owned(&b,camera_root,request_root,7,7,true,true)==RD_SUBMIT_RETURNED_UNVERIFIED);
  CHECK(recursive_status==RD_REENTRANT && submit_calls==1 && b.status==RD_BOUND_UNTESTED);
  submit_exception=true;
  CHECK(rd_submit_owned(&b,camera_root,request_root,7,7,true,true)==RD_INVOKE_EXCEPTION);
  CHECK(!b.method && !b.reflection_root && b.route==RD_NONE);
  CHECK(b.exception_name && !strcmp(b.exception_name,"NotSupportedException"));
  CHECK(rd_bind(&b,&api,&types)==RD_INVOKE_EXCEPTION);
  CHECK(rd_submit_owned(&b,camera_root,request_root,7,7,true,true)==RD_INVOKE_EXCEPTION && submit_calls==2);
  rd_release(&b); f_free(camera_root); f_free(request_root); CHECK(!roots_live);
  ++cases; printf("PASS owned_submit_route_%u_gates_exception_reentry\n",route);
 }
 CHECK(!strcmp(rd_status_name(RD_BOUND_UNTESTED),"bound_unexecuted"));
 printf("RESULT cases=%u assertions=%u target_callability=UNVERIFIED renderer=UNVERIFIED gpu=UNVERIFIED\n",cases,assertions);
 return 0;
}
