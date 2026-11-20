/* Source-relative eye placement. Only owned eye transforms are written. */
typedef struct BpSourceEyePose { Vec3 position;Quat rotation; } BpSourceEyePose;
static bool bp_source_eye_poses(Vec3 position,Quat rotation,float scale,float half_ipd,BpSourceEyePose out[2]) {
 float norm=rotation.x*rotation.x+rotation.y*rotation.y+rotation.z*rotation.z+rotation.w*rotation.w;
 if(!isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z) ||
  !isfinite(norm) || norm<0.000001f) return false;
 float yaw,pitch;quat_to_yaw_pitch_roll(quat_normalized(rotation),&yaw,&pitch,NULL);
 rotation=quat_from_yaw_pitch_roll(yaw,pitch,0);
 for(unsigned eye=0;eye<2;++eye) {
  float eye_x=0;if(!bp_fixed_eye_x(half_ipd,scale,WORLD_SCALE_MIN,eye,&eye_x)) return false;
  Vec3 offset=quat_rotate(rotation,(Vec3){eye_x,0,0});
  out[eye]=(BpSourceEyePose){.position={position.x+offset.x,position.y+offset.y,position.z+offset.z},.rotation=rotation};
  if(!isfinite(out[eye].position.x) || !isfinite(out[eye].position.y) || !isfinite(out[eye].position.z)) return false;
 }
 return true;
}
static CameraFollowAnchor owned_follow_anchor;
static bool bp_place_source_eyes(Vec3 position,Quat rotation,float scale,const EyeOptics* optics,BpSourceEyePose out[2]){
 LONG token=InterlockedCompareExchange(&camera_follow_token,0,0);
 apply_camera_anchor(&owned_follow_anchor,token,bp.c.identity.scene,&position,&rotation);
 if(bp.source_kind!=BP_SOURCE_HOME && bp.source_kind!=BP_SOURCE_RACE && bp.source_kind!=BP_SOURCE_DRESS)return bp_source_eye_poses(position,rotation,scale,optics->half_ipd_m,out);
 for(int eye=0;eye<2;eye++){
  if(!compose_unity_eye_pose(&position,&rotation,FALSE,optics,scale,eye,&out[eye].position,&out[eye].rotation))return false;
 }
 return true;
}
static bool bp_source_follow(int32_t frame) {
 if(!bp_mono_owner() || bp.c.phase!=BP_ACTIVE || frame<0 || bp.c.retiring || bp.c.enabled!=3) return false;
 if(bp.source_pose_frame==frame) return true;
 Il2CppObject* source=bp_target(bp.source_transform_root);Vec3 position;Quat rotation;
 if(bp.failed || !bp_get(BM_POSITION_GET,source,&position,sizeof(position)) ||
  !bp_get(BM_ROTATION_GET,source,&rotation,sizeof(rotation))) return false;
 BpSourceEyePose pose[2];
 EyeOptics current=bp.optics;
 if((bp.source_kind==BP_SOURCE_HOME || bp.source_kind==BP_SOURCE_RACE || bp.source_kind==BP_SOURCE_DRESS) && !snapshot_eye_optics(&current))return false;
 if(!bp_place_source_eyes(position,rotation,bp.scale,&current,pose)) { bp_fail("source_follow_pose_contract");return false; }

 Il2CppObject* left=bp_target(bp.eye[0].transform);Il2CppObject* right=bp_target(bp.eye[1].transform);
 if(bp.failed) return false;
 if(left==right || left==source || right==source) { bp_fail("source_follow_transform_alias");return false; }
 /* Derive both before the first write, then verify both before publishing any
    observation freshness. Existing failure invalidates the owned generation. */
 for(unsigned eye=0;eye<2;++eye) {
  Il2CppObject* transform=bp_target(bp.eye[eye].transform);
  if(!bp.failed && transform==source) bp_fail("source_follow_original_alias");
  if(bp.failed || !bp_set(BM_POSITION_SET,transform,&pose[eye].position) ||
   !bp_set(BM_ROTATION_SET,transform,&pose[eye].rotation)) return false;
 }
 for(unsigned eye=0;eye<2;++eye) {
  Il2CppObject* transform=bp_target(bp.eye[eye].transform);Vec3 p;Quat r;
  if(bp.failed || !bp_get(BM_POSITION_GET,transform,&p,sizeof(p)) || !bp_get(BM_ROTATION_GET,transform,&r,sizeof(r))) return false;
  float dot=r.x*pose[eye].rotation.x+r.y*pose[eye].rotation.y+r.z*pose[eye].rotation.z+r.w*pose[eye].rotation.w;
  float norm=r.x*r.x+r.y*r.y+r.z*r.z+r.w*r.w;
  if(!isfinite(p.x) || !isfinite(p.y) || !isfinite(p.z) || !isfinite(dot) || !isfinite(norm) || fabsf(norm-1)>0.0001f || fabsf(fabsf(dot)-1)>0.0001f ||
   fabsf(p.x-pose[eye].position.x)>0.001f || fabsf(p.y-pose[eye].position.y)>0.001f || fabsf(p.z-pose[eye].position.z)>0.001f) {
   bp_fail("source_follow_readback_mismatch");return false;
  }
 }
 for(unsigned eye=0;eye<2;++eye) { bp.eye[eye].position=pose[eye].position;bp.eye[eye].rotation=pose[eye].rotation; }
 /* Projection was fixed at construction. Only advance the render-view receipt. */
 bp.optics.render_views=current.render_views;
 bp.source_pose_frame=frame;return true;
}

