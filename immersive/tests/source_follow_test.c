#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "../src/base_producer_state.h"
#include "../src/base_consumer_lease.h"
typedef struct Vec3 {float x,y,z;} Vec3;
typedef struct Quat {float x,y,z,w;} Quat;
#include "source_follow_quaternion.inc"
#define WORLD_SCALE_MIN 0.55f
enum { BM_POSITION_GET,BM_ROTATION_GET,BM_POSITION_SET,BM_ROTATION_SET };
typedef struct Il2CppObject {Vec3 position;Quat rotation;} Il2CppObject;
typedef struct Eye {uint32_t transform;Vec3 position;Quat rotation;} Eye;
typedef struct {float half_ipd_m;float delta;int render_views;} EyeOptics;
enum {BP_SOURCE_HOME=1,BP_SOURCE_RACE=3,BP_SOURCE_DRESS=4};
static EyeOptics latest;
static bool snapshot_eye_optics(EyeOptics* out){*out=latest;return true;}
static bool compose_unity_eye_pose(const Vec3* p,const Quat* q,BOOL follow,const EyeOptics* o,float scale,int eye,Vec3* result,Quat* orientation){(void)follow;*result=*p;result->x+=(o->delta+(eye?1:-1)*o->half_ipd_m)/scale;*orientation=*q;return true;}
static struct {int source_kind;BpControl c;bool failed;uint32_t source_transform_root;int32_t source_pose_frame;float scale;EyeOptics optics;Eye eye[2];} bp;
static BpConsumerLease lease;
static Il2CppObject objects[3];static Il2CppObject* roots[4];
static unsigned gets_count,sets_count,original_writes,fail_get,fail_set,ignore_set;static bool owner=true,bad_rotation;
static const char* error;
static void bp_fail(const char* why) {error=why;bp.failed=true;bp_lease_revoke(&lease,bp.c.identity.generation);bp_retire(&bp.c,true);}
static bool bp_mono_owner(void) {if(owner) return true;bp_fail("owner");return false;}
static Il2CppObject* bp_target(uint32_t handle) {if(handle<4 && roots[handle]) return roots[handle];bp_fail("root");return NULL;}
static bool bp_get(unsigned method,Il2CppObject* o,void* out,size_t size) {
 if(++gets_count==fail_get) {bp_fail("getter_exception");return false;}
 if(!o || bp.failed) return false;
 if(method==BM_POSITION_GET && size==sizeof(Vec3)) memcpy(out,&o->position,size);
 else if(method==BM_ROTATION_GET && size==sizeof(Quat)) {Quat r=o->rotation;if(bad_rotation && o==&objects[2]) r.z+=1;memcpy(out,&r,size);}
 else {bp_fail("typed_get");return false;}return true;
}
static bool bp_set(unsigned method,Il2CppObject* o,void* value) {
 if(o==&objects[0]) {++original_writes;bp_fail("original_write");return false;}
 if(++sets_count==fail_set) {bp_fail("setter_exception");return false;}
 if(bp.failed || !o) return false;if(sets_count==ignore_set) return true;
 if(method==BM_POSITION_SET) memcpy(&o->position,value,sizeof(Vec3));
 else if(method==BM_ROTATION_SET) memcpy(&o->rotation,value,sizeof(Quat));
 else {bp_fail("typed_set");return false;}return true;
}
#include "../src/camera_follow.h"
#include "../src/base_source_follow.h"
static unsigned cases,assertions;
#define CHECK(x) do{++assertions;if(!(x)){printf("FAIL %d %s (%s)\n",__LINE__,#x,error?error:"");return 1;}}while(0)
static void reset(void) {
 memset(&bp,0,sizeof(bp));memset(&lease,0,sizeof(lease));memset(objects,0,sizeof(objects));
 owner=true;bad_rotation=false;error=NULL;gets_count=sets_count=original_writes=fail_get=fail_set=ignore_set=0;
 for(unsigned i=0;i<3;++i){roots[i+1]=&objects[i];objects[i].rotation=(Quat){0,0,0,1};}
 bp.c.phase=BP_ACTIVE;bp.c.initialized=bp.c.enabled=bp.c.witness=3;
 bp.c.identity=(BpIdentity){.generation=1,.source=2,.pose=3,.config=4,.scene=5};
 bp.source_transform_root=1;bp.source_pose_frame=-1;bp.scale=2;bp.optics.half_ipd_m=0.032f;bp.eye[0].transform=2;bp.eye[1].transform=3;
 (void)bp_lease_publish(&lease,&bp.c.identity,false);
}
int main(void) {
 reset();objects[0].position=(Vec3){1,2,3};CHECK(bp_source_follow(10));CHECK(gets_count==6 && sets_count==4 && !original_writes && bp.source_pose_frame==10);
 CHECK(fabsf(bp.eye[0].position.x-0.984f)<0.00001f && fabsf(bp.eye[1].position.x-1.016f)<0.00001f);
 CHECK(bp_source_follow(10) && gets_count==6 && sets_count==4);++cases;
 objects[0].position=(Vec3){5,6,7};objects[0].rotation=quat_from_yaw_pitch_roll(1.57079632679f,0,0.4f);
 CHECK(bp_source_follow(11));CHECK(fabsf(bp.eye[0].position.x-5)<0.00001f && fabsf(bp.eye[0].position.z-7.016f)<0.00001f);
 CHECK(fabsf(bp.eye[1].position.z-6.984f)<0.00001f && !original_writes && !bp.failed);
 float yaw,pitch,roll;quat_to_yaw_pitch_roll(bp.eye[0].rotation,&yaw,&pitch,&roll);CHECK(fabsf(roll)<0.0001f && fabsf(yaw-1.57079632679f)<0.0001f);++cases;
 reset();objects[0].rotation=quat_from_yaw_pitch_roll(0.6f,0.2f,0.5f);CHECK(bp_source_follow(1));quat_to_yaw_pitch_roll(bp.eye[0].rotation,&yaw,&pitch,&roll);
 CHECK(fabsf(yaw-0.6f)<0.0001f && fabsf(pitch-0.2f)<0.0001f && fabsf(roll)<0.0001f);++cases;
 for(unsigned mode=0;mode<7;++mode) {
  reset();switch(mode) {case 0:objects[0].position.x=NAN;break;case 1:objects[0].rotation=(Quat){0};break;
   case 2:bp.scale=0;break;case 3:fail_get=2;break;case 4:fail_set=3;break;case 5:ignore_set=1;break;default:bad_rotation=true;break;}
  CHECK(!bp_source_follow(1));CHECK(bp.failed && bp.c.retiring && !bp_lease_load64(&lease.active) && bp.source_pose_frame==-1 && !original_writes);++cases;
 }
 for(unsigned mode=0;mode<3;++mode) {
  reset();if(mode==0) roots[2]=&objects[0];if(mode==1) roots[3]=roots[2];if(mode==2) roots[1]=NULL;
  CHECK(!bp_source_follow(1) && bp.failed && !sets_count && !original_writes && bp.source_pose_frame==-1);++cases;
 }
 reset();owner=false;CHECK(!bp_source_follow(1) && !sets_count && bp.failed);++cases;
 reset();bp.c.phase=BP_EXCLUDE;CHECK(!bp_source_follow(1) && !sets_count && bp.source_pose_frame==-1);++cases;
 reset();CHECK(!bp_source_follow(-1) && !sets_count && bp.source_pose_frame==-1);++cases;
 for(int k=0;k<3;k++){
 reset();bp.source_kind=k==2?BP_SOURCE_DRESS:k?BP_SOURCE_RACE:BP_SOURCE_HOME;latest=(EyeOptics){0.032f,0.2f,20};CHECK(bp_source_follow(20));CHECK(bp.optics.render_views==20);float first=bp.eye[0].position.x;latest.delta+=0.3f;latest.render_views=21;CHECK(bp_source_follow(20));CHECK(bp.optics.render_views==20);CHECK(bp_source_follow(21));CHECK(bp.optics.render_views==21);CHECK(fabsf(bp.eye[0].position.x-first-0.3f/bp.scale)<0.0001f && !original_writes);++cases;
 }
 for(int k=0;k<3;k++){
  reset();bp.source_kind=k==2?BP_SOURCE_DRESS:k?BP_SOURCE_RACE:BP_SOURCE_HOME;bp.c.identity.scene=19;
  camera_follow_token=0;owned_follow_anchor=(CameraFollowAnchor){0};
  CHECK(toggle_camera_follow(TRUE)==1);latest=(EyeOptics){0.032f,0,30};
  CHECK(bp_source_follow(30));float anchor=bp.eye[0].position.x;
  objects[0].position.x=10;objects[0].rotation=quat_from_yaw_pitch_roll(.6f,0,0);
  ++bp.c.identity.generation;latest.delta=.2f;CHECK(bp_source_follow(31));
  CHECK(fabsf(bp.eye[0].position.x-anchor-.2f/bp.scale)<.0001f && fabsf(bp.eye[0].rotation.y)<.0001f);
  refresh_frozen_camera_anchor();CHECK(camera_follow_token==3);
  CHECK(bp_source_follow(32) && bp.eye[0].position.x>9 && fabsf(bp.eye[0].rotation.y)>.1f);
  objects[0].position.x=20;objects[0].rotation=(Quat){0,0,0,1};
  CHECK(bp_source_follow(33) && bp.eye[0].position.x<11 && fabsf(bp.eye[0].rotation.y)>.1f);
  bp.c.identity.scene=20;CHECK(bp_source_follow(34) && bp.eye[0].position.x>19);
  CHECK(toggle_camera_follow(TRUE)==4);refresh_frozen_camera_anchor();CHECK(camera_follow_token==4);
  objects[0].position.x=30;CHECK(bp_source_follow(35) && bp.eye[0].position.x>29 && !original_writes);++cases;
 }
 printf("PASS source follow: %u cases / %u assertions; production quaternion+adapter, substituted managed boundary\n",cases,assertions);return 0;
}
