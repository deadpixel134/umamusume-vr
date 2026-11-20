/* Execute the read-only production selector, including actual root cleanup. */
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/base_source_capability.h"
typedef struct Il2CppClass { int id; } Il2CppClass;
typedef struct Il2CppObject { unsigned kind; struct Il2CppObject* go; } Il2CppObject;
typedef struct Il2CppArray { uintptr_t max_length; Il2CppObject* vector[34]; } Il2CppArray;
typedef struct Il2CppException { int unused; } Il2CppException;
enum { BM_DATA_CAMERA,BM_DATA_ADDITIONAL,BM_ENABLED_GET,BM_COUNT,BT_BOOL=0 };
static Il2CppObject objects[8],enabled_box;
static Il2CppArray components;
static Il2CppClass bool_class,wrong_class;
static unsigned roots_new,roots_free,roots_live,fail_root,invokes,exception_at;
static unsigned wrong_relationship,checks,cases;
static uint8_t enabled;
static bool list_missing,wrong_box,null_unbox;
static uint32_t root_new(Il2CppObject* o,bool pinned) {
 (void)o;(void)pinned;++roots_new;
 if(roots_new==fail_root) return 0;
 ++roots_live;return roots_new;
}
static void root_free(uint32_t h) { if(!h || !roots_live) abort();++roots_free;--roots_live; }
static Il2CppObject* get_go(Il2CppObject* o) {return o->go;}
static Il2CppArray* get_components(Il2CppObject* go,void* type,int a,int b,int c,int d,void* e) {
 (void)go;(void)type;(void)a;(void)b;(void)c;(void)d;(void)e;return list_missing?NULL:&components;
}
static Il2CppObject* (*component_get_game_object)(Il2CppObject*)=get_go;
static Il2CppArray* (*gameobject_get_components)(Il2CppObject*,void*,int,int,int,int,void*)=get_components;
static void* component_reflection_type;
static void* camera_reflection_type;
static int find_inactive,find_sort;
static Il2CppArray active_cameras,all_cameras;
static Il2CppArray* unity_find_objects(void* type,int inactive,int sort) {
 (void)type;find_inactive=inactive;find_sort=sort;return inactive?&all_cameras:&active_cameras;
}
static bool component_is(Il2CppObject* o,const char* ns,const char* name) {
 if(!o) return false;
 unsigned k=!strcmp(ns,"UnityEngine") && !strcmp(name,"Camera")?BP_STORY_CAMERA:
  !strcmp(ns,"Gallop") && !strcmp(name,"StoryImageEffect")?BP_STORY_EFFECT:
  !strcmp(ns,"Gallop") && !strcmp(name,"RaceImageEffect")?16:
  !strcmp(ns,"Gallop.RenderPipeline") && !strcmp(name,"CameraData")?BP_STORY_DATA:
  !strcmp(ns,"UnityEngine.Rendering.Universal") && !strcmp(name,"UniversalAdditionalCameraData")?BP_STORY_URP:0;
 return k && k==o->kind;
}
static Il2CppObject* invoke(void* method,Il2CppObject* receiver,void* args,Il2CppException** exc) {
 (void)receiver;(void)args;++invokes;
 if(invokes==exception_at) {*exc=(Il2CppException*)1;return NULL;}
 if(method==(void*)1) return wrong_relationship==1?&objects[7]:&objects[0];
 if(method==(void*)2) return wrong_relationship==2?&objects[7]:&objects[3];
 return &enabled_box;
}
static Il2CppClass* object_class(Il2CppObject* o) {(void)o;return wrong_box?&wrong_class:&bool_class;}
static void* unbox(Il2CppObject* o) {(void)o;return null_unbox?NULL:&enabled;}
static struct {
 void* method[BM_COUNT];Il2CppClass* types[1];
 struct {uint32_t (*gchandle_new)(Il2CppObject*,bool);void (*gchandle_free)(uint32_t);
 Il2CppObject* (*runtime_invoke)(void*,Il2CppObject*,void*,Il2CppException**);
 Il2CppClass* (*object_get_class)(Il2CppObject*);void* (*object_unbox)(Il2CppObject*);} api;
} bp;
#include "../src/base_story_source.h"
#define CHECK(x) do {++checks;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);return 1;}} while(0)
static void reset(void) {
 ++cases;memset(&bp,0,sizeof(bp));memset(objects,0,sizeof(objects));memset(&components,0,sizeof(components));
 roots_new=roots_free=roots_live=fail_root=invokes=exception_at=wrong_relationship=0;
 list_missing=wrong_box=null_unbox=false;enabled=1;
 component_get_game_object=get_go;gameobject_get_components=get_components;
 for(unsigned i=0;i<4;++i) {objects[i].kind=1u<<i;objects[i].go=&objects[4];components.vector[i]=&objects[i];}
 components.max_length=4;
 bp.method[0]=(void*)1;bp.method[1]=(void*)2;bp.method[2]=(void*)3;bp.types[0]=&bool_class;
 bp.api.gchandle_new=root_new;bp.api.gchandle_free=root_free;bp.api.runtime_invoke=invoke;
 bp.api.object_get_class=object_class;bp.api.object_unbox=unbox;
}
int main(void) {
 /* Dormant exact owners can have enabled=true and higher depth. Assert the
    production enumeration requests Exclude; the mock honors that API contract. */
 active_cameras.max_length=1;active_cameras.vector[0]=&objects[0];
 all_cameras.max_length=2;all_cameras.vector[0]=&objects[0];all_cameras.vector[1]=&objects[7];
 CHECK(bp_source_cameras(BP_SOURCE_STORY)==&active_cameras && find_inactive==0 && find_sort==0);
 CHECK(bp_source_cameras(BP_SOURCE_HOME)==&all_cameras && find_inactive==1);
 reset();CHECK(bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(roots_new==3 && roots_free==3 && !roots_live && invokes==3);
 CHECK(bp_source_cameras(BP_SOURCE_RACE)==&active_cameras);
 CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_RACE));
 objects[1].kind=16;
 CHECK(bp_effect_owner(&objects[0],BP_SOURCE_RACE));
 CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));
 enabled=0;
 CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_RACE));
 for(unsigned missing=0;missing<4;++missing) {
  reset();components.vector[missing]=NULL;CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(!roots_live && roots_free==roots_new);
 }
 for(unsigned dupe=0;dupe<4;++dupe) {
  reset();components.vector[4]=components.vector[dupe];components.max_length=5;
  CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(!roots_live && !invokes);
 }
 reset();objects[2].go=&objects[7];CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(!roots_live && !invokes);
 reset();objects[7].kind=BP_STORY_CAMERA;objects[7].go=&objects[4];components.vector[0]=&objects[7];
 CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(!roots_live);
 for(unsigned rel=1;rel<=2;++rel) {reset();wrong_relationship=rel;CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(!roots_live);}
 for(unsigned fault=1;fault<=3;++fault) {reset();exception_at=fault;CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(!roots_live);}
 for(unsigned fault=1;fault<=3;++fault) {
  reset();fail_root=fault;CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(!roots_live && roots_free==fault-1);
 }
 reset();list_missing=true;CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(!roots_live);
 reset();components.max_length=33;CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(!roots_live && !invokes);
 reset();enabled=0;CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(!roots_live);
 reset();enabled=2;CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(!roots_live);
 reset();wrong_box=true;CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(!roots_live);
 reset();null_unbox=true;CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(!roots_live);
 reset();component_get_game_object=NULL;CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(!roots_new);
 reset();gameobject_get_components=NULL;CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(!roots_new);
 reset();bp.method[BM_DATA_CAMERA]=NULL;CHECK(!bp_effect_owner(&objects[0],BP_SOURCE_STORY));CHECK(!roots_new);
 reset();CHECK(!bp_effect_owner(NULL,BP_SOURCE_STORY));CHECK(!roots_new);
 printf("PASS actual Story read-only selector: %u cases %u assertions; no Unity executed\n",cases,checks);return 0;
}
