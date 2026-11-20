#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
typedef int MethodInfo;typedef int Il2CppClass;typedef int Il2CppException;
typedef struct Il2CppObject {int klass,id;struct Il2CppObject* go;union{int32_t i;float f;}value;} Il2CppObject;
typedef struct Il2CppArray {uintptr_t max_length;Il2CppObject* vector[4];} Il2CppArray;
typedef struct Scene {int handle;} Scene;
enum{BT_INT,BT_FLOAT,BT_CAMERA,BT_DATA,BT_COUNT};
enum{BM_FRAME,BM_INSTANCE,BM_CULL_GET,BM_NEAR_GET,BM_FAR_GET,BM_DATA_CAMERA,BM_COUNT};
enum{BP_SOURCE_STORY=2,BP_ACTIVE=5};
typedef struct BpMethodSpec {int owner;const char*name;int ret;unsigned argc;int args[4];bool is_static;} BpMethodSpec;
typedef struct BpLookupEvidence {bool scan_completed,capped;unsigned invalid_names,valid_matches;} BpLookupEvidence;
typedef struct Api {Il2CppObject*(*runtime_invoke)(const MethodInfo*,Il2CppObject*,void**,Il2CppException**);Il2CppClass*(*object_get_class)(Il2CppObject*);void*(*object_unbox)(Il2CppObject*);Il2CppObject*(*gchandle_get_target)(uint32_t);uint32_t(*gchandle_new)(Il2CppObject*,bool);void(*gchandle_free)(uint32_t);} Api;
static struct {Api api;int source_kind;struct{int phase;struct{uint64_t generation,source;int scene;}identity;}c;bool bound,failed;uint32_t source_root,owner_thread;struct{uint32_t camera;}eye[2];Il2CppClass*types[BT_COUNT];const MethodInfo*method[BM_COUNT];}bp;
static int bp_main_depth=1;static void*component_reflection_type;
static Il2CppClass types[BT_COUNT];static MethodInfo methods[BM_COUNT+1];
static Il2CppObject cameras[3],gos[3],data[3],boxed, *roots[64];static Il2CppArray lists[3];
static int frame,live_roots,new_roots,freed,records,bytes,assertions,fail_new,throw_method,wrong_box;static bool missing_getter,ambiguous;
static char output[3][640];
#define CHECK(x) do{++assertions;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned GetCurrentThreadId(void){return 7;}
static int InterlockedCompareExchange(int*p,int exchange,int compare){int old=*p;if(old==compare)*p=exchange;return old;}
static Il2CppClass*klass(Il2CppObject*o){return &types[o->klass];}
static void*unbox(Il2CppObject*o){return &o->value;}
static Il2CppObject*target(uint32_t h){return h<64?roots[h]:NULL;}
static uint32_t root_new(Il2CppObject*o,bool pin){(void)pin;if(++new_roots==fail_new)return 0;for(unsigned i=4;i<64;++i)if(!roots[i]){roots[i]=o;++live_roots;return i;}return 0;}
static void root_free(uint32_t h){CHECK(h>=4 && roots[h]);roots[h]=NULL;--live_roots;++freed;}
static Il2CppObject*invoke(const MethodInfo*m,Il2CppObject*o,void**args,Il2CppException**exc){
 (void)args;if(*m==throw_method){static int error;*exc=&error;return NULL;}
 boxed.klass=BT_INT;boxed.value.i=0;
 if(*m==BM_FRAME)boxed.value.i=frame;
 else if(*m==BM_INSTANCE)boxed.value.i=o->id;
 else if(*m==BM_CULL_GET)boxed.value.i=o==&cameras[0]?14337:8192;
 else if(*m==BM_NEAR_GET){boxed.klass=BT_FLOAT;boxed.value.f=0.1f;}
 else if(*m==BM_FAR_GET){boxed.klass=BT_FLOAT;boxed.value.f=200;}
 else if(*m==BM_DATA_CAMERA)return &cameras[o->id];
 else boxed.value.i=o==&data[0]?2:0;
 if(*m==wrong_box)boxed.klass=BT_CAMERA;
 return &boxed;
}
static Il2CppObject*component_get_game_object(Il2CppObject*o){return o->go;}
static Il2CppArray*gameobject_get_components(Il2CppObject*o,void*t,int a,int b,int c,int d,void*e){(void)t;(void)a;(void)b;(void)c;(void)d;(void)e;return &lists[o->id];}
static const MethodInfo*bp_find(Api*a,Il2CppClass**t,const BpMethodSpec*s,BpLookupEvidence*e){(void)a;(void)t;CHECK(!strcmp(s->name,"get_RendererIndex") && s->ret==BT_INT);*e=(BpLookupEvidence){.scan_completed=true,.valid_matches=missing_getter?0:ambiguous?2:1};return missing_getter?NULL:&methods[BM_COUNT];}
#define telemetry_snprintf snprintf
static void write_record(const char*e,const char*event,const char*detail){CHECK(!strcmp(e,"STORY-SOURCE-EYE") && !strcmp(event,"story_source_eye_contract"));CHECK(records<3 && strlen(detail)<640);strcpy(output[records++],detail);bytes+=(int)strlen(detail)+384;}
#include "../fixed-story-compare-0BAF1FA4/src/base_story_observe.h"
static void reset(void){
 memset(&bp,0,sizeof(bp));memset(roots,0,sizeof(roots));memset(output,0,sizeof(output));memset(&bp_story_compare_budget,0,sizeof(bp_story_compare_budget));
 frame=0;live_roots=new_roots=freed=records=bytes=0;fail_new=0;throw_method=wrong_box=-1;missing_getter=ambiguous=false;bp_main_depth=1;
 bp.api=(Api){invoke,klass,unbox,target,root_new,root_free};bp.source_kind=BP_SOURCE_STORY;bp.c.phase=BP_ACTIVE;bp.c.identity.generation=2;bp.c.identity.source=100;bp.c.identity.scene=99;bp.bound=true;bp.owner_thread=7;bp.source_root=1;bp.eye[0].camera=2;bp.eye[1].camera=3;
 for(unsigned i=0;i<BT_COUNT;++i)bp.types[i]=&types[i];for(unsigned i=0;i<=BM_COUNT;++i){methods[i]=(int)i;if(i<BM_COUNT)bp.method[i]=&methods[i];}
 for(int i=0;i<3;++i){cameras[i]=(Il2CppObject){.klass=BT_CAMERA,.id=100+i,.go=&gos[i]};gos[i]=(Il2CppObject){.id=i};data[i]=(Il2CppObject){.klass=BT_DATA,.id=i,.go=&gos[i]};lists[i]=(Il2CppArray){.max_length=1,.vector={&data[i]}};roots[i+1]=&cameras[i];}
}
static void run(void){bp_story_topology(NULL,&cameras[0],(Scene){99});frame=120;bp_story_topology(NULL,&cameras[0],(Scene){99});CHECK(!live_roots && !bp.failed);}
int main(void){
 reset();run();CHECK(records==3 && bytes<=4096 && new_roots==9 && freed==9);CHECK(strstr(output[0],"\"valid\":31") && strstr(output[0],"\"renderer_index\":2") && strstr(output[1],"\"renderer_index\":0"));CHECK(strstr(output[0],"\"culling_mask\":14337") && strstr(output[2],"\"culling_mask\":8192"));
 frame=999;bp_story_topology(NULL,&cameras[0],(Scene){99});CHECK(records==3);bp.c.identity.generation=3;bp_story_topology(NULL,&cameras[0],(Scene){99});CHECK(records==3);
 for(int f=1;f<=9;++f){reset();fail_new=f;run();CHECK(records<=3 && freed==new_roots-1);}
 reset();missing_getter=true;run();CHECK(records==3 && strstr(output[0],"\"valid\":15") && strstr(output[0],"\"renderer_getter\":false"));
 reset();ambiguous=true;run();CHECK(strstr(output[0],"\"valid\":15"));
 reset();throw_method=BM_CULL_GET;run();CHECK(strstr(output[0],"\"valid\":29"));
 reset();wrong_box=BM_COUNT;run();CHECK(strstr(output[0],"\"valid\":15"));
 reset();lists[0].max_length=2;lists[0].vector[1]=&data[0];run();CHECK(strstr(output[0],"\"data_matches\":2") && strstr(output[0],"\"valid\":15"));
 reset();bp.owner_thread=8;run();CHECK(!records && !new_roots);
 reset();bp.c.identity.scene=98;run();CHECK(!records && !new_roots);
 reset();bp.source_kind=1;run();CHECK(!records && !new_roots);
 reset();bp_main_depth=2;run();CHECK(!records && !new_roots);
 reset();bp_story_topology(NULL,&cameras[0],(Scene){99});frame=119;bp_story_topology(NULL,&cameras[0],(Scene){99});CHECK(!records);frame=120;bp_story_topology(NULL,&cameras[0],(Scene){99});CHECK(records==3);
 printf("PASS actual source-eye observer: %d assertions; substituted Unity metadata/getters, no runtime acceptance\n",assertions);return 0;
}
