#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "../src/base_background_policy.h"
#define BP_BACKGROUND_FIXTURE
#define main fov_fixture_main
#include "fov_diagnostic_test.c"
#undef main
#define BM_CLEAR_GET 20
#define BM_BG_GET 21
#define BM_BG_SET 22
#define BM_CLEAR 23
static int32_t source_flags,own_flags[2];
static BpColor source_color,own_color[2];
static unsigned source_writes,own_writes;
static bool reject_get,reject_set,ignore_color;
static bool bp_get(unsigned id,Il2CppObject* o,void* out,size_t size) {
 if(bp.failed || reject_get) return bp_component_stop("get_fault");
 if(id==BM_CLEAR_GET && size==sizeof(int32_t)) {
  *(int32_t*)out=o==&source?source_flags:own_flags[o->eye];return true;
 }
 if(id==BM_BG_GET && size==sizeof(BpColor)) {
  memcpy(out,o==&source?&source_color:&own_color[o->eye],size);return true;
 }
 return bp_component_stop("typed_get_contract");
}
static bool bp_set(unsigned id,Il2CppObject* o,void* value) {
 if(o==&source) {++source_writes;return false;}
 if(bp.failed || reject_set) return bp_component_stop("set_fault");
 ++own_writes;
 if(id==BM_BG_SET) {if(!ignore_color) memcpy(&own_color[o->eye],value,sizeof(BpColor));return true;}
 if(id==BM_CLEAR) {own_flags[o->eye]=*(int32_t*)value;return true;}
 return bp_component_stop("typed_set_contract");
}
#include "../src/base_background.h"
static void bg_reset(void) {
 reset();source_flags=2;source_color=(BpColor){0.1f,0.2f,3.5f,0.0f};
 memset(own_color,0,sizeof(own_color));memset(own_flags,0,sizeof(own_flags));
 source_writes=own_writes=0;reject_get=reject_set=ignore_color=false;
}
int main(void) {
 for(int32_t flags=1;flags<=4;++flags) {
  bg_reset();source_flags=flags;CHECK(bp_background_snapshot(&source));
  CHECK(bp.background.source_flags==flags && bp.background.own_flags==(flags<=2?flags:2));
  CHECK(bp_background_apply(&bp.eye[0]) && bp_background_apply(&bp.eye[1]));
  CHECK(own_writes==4 && !source_writes && !memcmp(&source_color,&own_color[0],16));
  enabled[0]=enabled[1]=true;CHECK(bp_background_check(&bp.eye[0]) && bp_background_check(&bp.eye[1]));
  own_flags[0]=4;CHECK(!bp_background_check(&bp.eye[0]));
 }
 for(unsigned mode=0;mode<7;++mode) {
  bg_reset();
  switch(mode) {
   case 0:source_flags=0;break;case 1:source_flags=5;break;case 2:source_color.r=NAN;break;
   case 3:source_color.g=INFINITY;break;case 4:source_color.b=-INFINITY;break;
   case 5:source_color.a=NAN;break;case 6:reject_get=true;break;
  }
  CHECK(!bp_background_snapshot(&source) && !own_writes && !source_writes);
 }
 for(unsigned mode=0;mode<6;++mode) {
  bg_reset();CHECK(bp_background_snapshot(&source));
  switch(mode) {
   case 0:enabled[0]=true;break;case 1:bp.eye[0].camera=bp.source_root;break;
   case 2:reject_set=true;break;case 3:ignore_color=true;break;
   case 4:attached=false;break;case 5:roots[bp.eye[0].camera]=NULL;break;
  }
  CHECK(!bp_background_apply(&bp.eye[0]) && !source_writes);
 }
 bg_reset();CHECK(bp_background_snapshot(&source) && bp_background_apply(&bp.eye[0]));
 own_color[0].r+=0.1f;CHECK(!bp_background_check(&bp.eye[0]));
 bg_reset();CHECK(bp_background_snapshot(&source));
 char json[200];CHECK(bp_background_json(json,sizeof(json))>0);printf("JSON { %s }\n",json+1);
 printf("PASS background adapter: %u assertions; no Unity execution\n",checks);return 0;
}
