/* Diagnostic only: one public-field intervention, never a projection fix.
   Field APIs have no exception-out. A native fault is NOT caught here.
   All entry points run under bp_lock and bp_main_depth on the attached owner. */
static struct {
 bool attempted, checked[2];
 unsigned original_mask, write_mask, readback_mask, check_mask[2];
 uint8_t original[2], readback[2], check[2][2];
 int32_t frame[2];
} bp_fov;

static bool bp_fov_owner(void) {
 if(bp.failed || bp.c.retiring || bp.c.quarantined) return false;
 if(!bp_mono_owner() || InterlockedCompareExchange(&bp_main_depth,0,0)!=1 ||
  !bp.field_get || !bp.field_set || !bp.fov_field || bp.c.initialized!=3)
  return bp_component_stop("owned_fov_owner_contract");
 return true;
}
static Il2CppObject* bp_fov_data(unsigned eye,bool disabled) {
 if(!bp_fov_owner() || eye>1) return NULL;
 BpEye* e=&bp.eye[eye];
 uint32_t handles[4]={e->go,e->data,e->camera,e->additional};
 int types[4]={BT_GO,BT_DATA,BT_CAMERA,BT_ADDITIONAL};
 Il2CppObject* objects[4];
 Il2CppObject* source=bp_target(bp.source_root);
 if(bp.failed) return NULL;
 for(unsigned i=0;i<4;++i) {
  objects[i]=bp_target(handles[i]);
  if(bp.failed) return NULL;
  if(bp.api.object_get_class(objects[i])!=bp.types[types[i]] || objects[i]==source) {
   bp_component_stop("owned_fov_receiver_contract");return NULL;
  }
  uint32_t other[4]={bp.eye[1-eye].go,bp.eye[1-eye].data,bp.eye[1-eye].camera,bp.eye[1-eye].additional};
  for(unsigned j=0;j<4;++j) {
   Il2CppObject* peer=bp_target(other[j]);
   if(bp.failed) return NULL;
   if(objects[i]==peer) { bp_component_stop("owned_fov_peer_alias");return NULL; }
  }
  bool alive=false;
  if(!bp_alive(objects[i],&alive) || !alive) { bp_component_stop("owned_fov_native_liveness");return NULL; }
 }
 /* Existing component->GO provider, no new native ABI/offset. */
 Il2CppObject* source_go=component_get_game_object(source);
 if(!source_go || source_go==objects[0]) { bp_component_stop("owned_fov_source_alias");return NULL; }
 for(unsigned i=1;i<4;++i) {
  if(component_get_game_object(objects[i])!=objects[0]) { bp_component_stop("owned_fov_go_identity");return NULL; }
 }
 if(bp_call(BM_DATA_CAMERA,objects[1],NULL)!=objects[2] || bp.failed ||
  bp_call(BM_DATA_ADDITIONAL,objects[1],NULL)!=objects[3] || bp.failed) {
  bp_component_stop("owned_fov_data_identity");return NULL;
 }
 if(disabled && !bp_read_false(BM_ENABLED_GET,e->camera,"owned_fov_camera_not_disabled")) return NULL;
 /* Recheck rooted identity immediately before the field operation. */
 Il2CppObject* data=bp_target(e->data);
 if(bp.failed || data!=objects[1]) { bp_component_stop("owned_fov_root_changed");return NULL; }
 return data;
}
static bool bp_fov_read(unsigned eye,bool disabled,uint8_t* value) {
 Il2CppObject* data=bp_fov_data(eye,disabled);
 if(!data) return false;
 *value=255; bp.field_get(data,bp.fov_field,value);
 if(*value>1) return bp_component_stop("owned_fov_non_boolean");
 return true;
}
static bool bp_fov_apply(void) {
 if(!bp_fov_owner()) return false;
 if(bp_fov.attempted || bp.c.phase!=BP_BUILDING || bp.c.enabled)
  return bp_component_stop("owned_fov_intervention_state");
 bp_fov.attempted=true;
 if(!bp_fov_data(0,true) || !bp_fov_data(1,true)) return false;
 /* Read BOTH original values before deciding to perform ANY write. */
 for(unsigned eye=0;eye<2;++eye) {
  if(!bp_fov_read(eye,true,&bp_fov.original[eye])) return false;
  bp_fov.original_mask|=1u<<eye;
 }
 if(bp_fov.original[0]!=1 || bp_fov.original[1]!=1)
  return bp_component_stop("owned_fov_original_not_both_true");
 for(unsigned eye=0;eye<2;++eye) {
  Il2CppObject* data=bp_fov_data(eye,true);
  if(!data) return false;
  uint8_t no=0;
  bp_fov.write_mask|=1u<<eye; /* attempted, not a success return value */
  bp.field_set(data,bp.fov_field,&no);
  if(!bp_fov_read(eye,true,&bp_fov.readback[eye])) return false;
  bp_fov.readback_mask|=1u<<eye;
  if(bp_fov.readback[eye]!=0) return bp_component_stop("owned_fov_write_readback");
 }
 return true;
}
static bool bp_fov_check(unsigned stage,int32_t frame) {
 if(!bp_fov_owner()) return false;
 if(stage>1 || !bp_fov.attempted || bp_fov.write_mask!=3 || bp_fov.readback_mask!=3 ||
  (stage==0 && (bp.c.phase!=BP_ENABLE_READY || bp.c.enabled)) ||
  (stage==1 && (bp.c.phase!=BP_EXCLUDE || bp.c.enabled!=3)))
  return bp_component_stop("owned_fov_observation_state");
 if(bp_fov.checked[stage]) return true;
 bp_fov.checked[stage]=true;bp_fov.frame[stage]=frame;
 for(unsigned eye=0;eye<2;++eye) {
  if(!bp_fov_read(eye,stage==0,&bp_fov.check[stage][eye])) return false;
  bp_fov.check_mask[stage]|=1u<<eye;
  if(bp_fov.check[stage][eye]!=0) return bp_component_stop("owned_fov_reset_no_reapply");
 }
 return true;
}
static int bp_fov_json(char* detail,size_t capacity) {
 if(!bp_fov.attempted) return 0;
 /* o/r/p/e arrays begin with observed mask; w is write ATTEMPT mask.
    p/e additionally carry frame before values. Missing mask bits = UNKNOWN. */
 return telemetry_snprintf(detail,capacity,
  ",\"fov\":{\"id\":[%llu,%llu,%d,%llu,%llu],\"preinit\":\"NOT_SAMPLED\","
  "\"o\":[%u,%u,%u],\"w\":%u,\"r\":[%u,%u,%u],"
  "\"p\":[%u,%d,%u,%u],\"e\":[%u,%d,%u,%u]}",
  (unsigned long long)bp.c.identity.generation,(unsigned long long)bp.c.identity.source,bp.c.identity.scene,
  (unsigned long long)bp.c.identity.pose,(unsigned long long)bp.c.identity.config,
  bp_fov.original_mask,bp_fov.original[0],bp_fov.original[1],bp_fov.write_mask,
  bp_fov.readback_mask,bp_fov.readback[0],bp_fov.readback[1],
  bp_fov.check_mask[0],bp_fov.frame[0],bp_fov.check[0][0],bp_fov.check[0][1],
  bp_fov.check_mask[1],bp_fov.frame[1],bp_fov.check[1][0],bp_fov.check[1][1]);
}
