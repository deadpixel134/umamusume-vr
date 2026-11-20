/* Execute the production intervention adapter with fault-injected owners.
   This does not execute Unity or establish the game's field-consumer contract. */
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "../src/base_producer_state.h"
enum { BT_GO,BT_DATA,BT_CAMERA,BT_ADDITIONAL,BT_COUNT };
enum { BM_DATA_CAMERA,BM_DATA_ADDITIONAL,BM_ENABLED_GET };
typedef struct Il2CppClass { int id; } Il2CppClass;
typedef struct Il2CppObject { Il2CppClass* klass; unsigned eye; bool alive; } Il2CppObject;
typedef struct FieldInfo { int id; } FieldInfo;
typedef struct BpEye { uint32_t go,data,camera,additional; } BpEye;
static struct {
 BpControl c; BpEye eye[2]; uint32_t source_root; bool failed,bound;
 DWORD owner_thread; FieldInfo* fov_field;
#ifdef BP_BACKGROUND_FIXTURE
 BpBackground background;
#endif
 void (*field_get)(Il2CppObject*,FieldInfo*,void*);
 void (*field_set)(Il2CppObject*,FieldInfo*,void*);
 Il2CppClass* types[BT_COUNT];
 struct { Il2CppClass* (*object_get_class)(Il2CppObject*); } api;
} bp;
static LONG bp_main_depth;
static Il2CppClass classes[BT_COUNT];
static Il2CppObject objects[2][4],source,source_go;
static Il2CppObject* roots[16];
static FieldInfo field;
static uint8_t values[2];
static unsigned reads[2],writes[2],ignore_write,checks,cases,all_reads,order_bad,source_access;
static bool attached,wrong_go,wrong_relation,enabled[2],dead;
static char emitted[640];
static const char* failure;
#define CHECK(x) do { ++checks; if(!(x)) { printf("FAIL line %d: %s (%s)\n",__LINE__,#x,failure?failure:""); return 1; } } while(0)
static bool bp_component_stop(const char* why) { bp.failed=true;failure=why;bp_retire(&bp.c,true);return false; }
static bool bp_mono_owner(void) {
 return bp.bound && bp.owner_thread==GetCurrentThreadId() && attached ? true : bp_component_stop("owner");
}
static Il2CppObject* bp_target(uint32_t h) {
 if(h>=16 || !roots[h]) {bp_component_stop("root");return NULL;} return roots[h];
}
static Il2CppClass* object_class(Il2CppObject* o) { return o->klass; }
static bool bp_alive(Il2CppObject* o,bool* alive) { *alive=o->alive && !dead;return true; }
static Il2CppObject* component_get_game_object(Il2CppObject* o) {
 if(o==&source) return &source_go;
 return wrong_go?&source_go:&objects[o->eye][BT_GO];
}
static Il2CppObject* bp_call(unsigned method,Il2CppObject* o,void* args) {
 (void)args;
 if(wrong_relation) return &source;
 return &objects[o->eye][method==BM_DATA_CAMERA?BT_CAMERA:BT_ADDITIONAL];
}
static bool bp_read_false(unsigned method,uint32_t h,const char* reason) {
 (void)method;Il2CppObject* o=bp_target(h);if(!o) return false;
 return enabled[o->eye]?bp_component_stop(reason):true;
}
static void get_field(Il2CppObject* o,FieldInfo* f,void* out) {
 if(o==&source || f!=&field) {++source_access;return;}
 ++reads[o->eye];++all_reads;*(uint8_t*)out=values[o->eye];
}
static void set_field(Il2CppObject* o,FieldInfo* f,void* value) {
 if(o==&source || f!=&field) {++source_access;return;}
 if(!reads[0] || !reads[1] || *(uint8_t*)value!=0) ++order_bad;
 ++writes[o->eye];if(ignore_write!=o->eye+1) values[o->eye]=*(uint8_t*)value;
}
static int telemetry_snprintf(char* out,size_t cap,const char* fmt,...) {
 va_list args;va_start(args,fmt);int n=vsnprintf(out,cap,fmt,args);va_end(args);return n;
}
#include "../src/base_producer_fov.h"
static void reset(void) {
 ++cases;memset(&bp,0,sizeof(bp));memset(&bp_fov,0,sizeof(bp_fov));memset(roots,0,sizeof(roots));
 memset(reads,0,sizeof(reads));memset(writes,0,sizeof(writes));memset(enabled,0,sizeof(enabled));
 ignore_write=all_reads=order_bad=source_access=0;wrong_go=wrong_relation=dead=false;failure=NULL;
 attached=true;bp_main_depth=1;bp.bound=true;bp.owner_thread=GetCurrentThreadId();
 bp.c.phase=BP_BUILDING;bp.c.initialized=3;bp.c.identity=(BpIdentity){1,2,3,4,0,5,0};
 bp.field_get=get_field;bp.field_set=set_field;bp.fov_field=&field;bp.api.object_get_class=object_class;
 for(unsigned e=0;e<2;++e) {
  values[e]=1;
  for(unsigned i=0;i<4;++i) {
   bp.types[i]=&classes[i];objects[e][i]=(Il2CppObject){&classes[i],e,true};roots[1+e*4+i]=&objects[e][i];
  }
  bp.eye[e]=(BpEye){1+e*4,2+e*4,3+e*4,4+e*4};
 }
 source=(Il2CppObject){&classes[BT_CAMERA],0,true};source_go=(Il2CppObject){&classes[BT_GO],0,true};
 roots[9]=&source;bp.source_root=9;
}
int main(void) {
 reset();CHECK(bp_fov_apply());CHECK(writes[0]==1 && writes[1]==1 && !order_bad && !source_access);
 CHECK(bp_fov.original_mask==3 && bp_fov.original[0]==1 && bp_fov.original[1]==1);
 CHECK(bp_fov.readback_mask==3 && !values[0] && !values[1]);
 bp.c.phase=BP_ENABLE_READY;CHECK(bp_fov_check(0,100));
 bp.c.phase=BP_EXCLUDE;bp.c.enabled=3;enabled[0]=enabled[1]=true;CHECK(bp_fov_check(1,103));
 unsigned before=all_reads;CHECK(bp_fov_check(1,104) && all_reads==before);
 CHECK(bp_fov_json(emitted,sizeof(emitted))>0);
 CHECK(strstr(emitted,"\"o\":[3,1,1]") && strstr(emitted,"\"e\":[3,103,0,0]"));
 printf("{ %s }\n",emitted+1);
 CHECK(!bp_fov_apply() && writes[0]==1 && writes[1]==1);
 for(unsigned eye=0;eye<2;++eye) {
  reset();values[eye]=0;CHECK(!bp_fov_apply() && reads[0]==1 && reads[1]==1 && !writes[0] && !writes[1]);
  reset();values[eye]=2;CHECK(!bp_fov_apply() && !writes[0] && !writes[1]);
  reset();ignore_write=eye+1;CHECK(!bp_fov_apply() && writes[0]==1 && writes[1]==eye);
  CHECK(bp.c.quarantined && !bp.c.enabled);
  reset();CHECK(bp_fov_apply());values[eye]=1;bp.c.phase=BP_ENABLE_READY;
  CHECK(!bp_fov_check(0,1) && writes[0]==1 && writes[1]==1);
  reset();CHECK(bp_fov_apply());bp.c.phase=BP_ENABLE_READY;CHECK(bp_fov_check(0,1));
  values[eye]=1;bp.c.phase=BP_EXCLUDE;bp.c.enabled=3;
  CHECK(!bp_fov_check(1,3) && writes[0]==1 && writes[1]==1);
 }
 for(unsigned mode=0;mode<15;++mode) {
  reset();
  switch(mode) {
   case 0:attached=false;break;case 1:bp.owner_thread++;break;case 2:bp_main_depth=0;break;
   case 3:bp.c.retiring=true;break;case 4:bp.c.quarantined=true;break;case 5:bp.c.initialized=1;break;
   case 6:bp.c.phase=BP_ACTIVE;break;case 7:enabled[1]=true;break;
   case 8:bp.eye[1].data=bp.eye[0].data;break;case 9:bp.eye[0].camera=bp.source_root;break;
   case 10:wrong_go=true;break;case 11:wrong_relation=true;break;case 12:dead=true;break;
   case 13:roots[bp.eye[1].data]=NULL;break;case 14:bp.field_set=NULL;break;
  }
  CHECK(!bp_fov_apply() && !writes[0] && !writes[1] && !source_access);
 }
 reset();CHECK(!bp_fov_check(0,0) && !all_reads);
 printf("PASS %u cases / %u assertions; production FOV adapter; Unity consumer untested\n",cases,checks);
 return 0;
}
