static bool bp_background_snapshot(Il2CppObject* source) {
 BpBackground* b=&bp.background;
 if(!bp_get(BM_CLEAR_GET,source,&b->source_flags,sizeof(b->source_flags)) ||
  !bp_get(BM_BG_GET,source,&b->color,sizeof(b->color))) return false;
 if(!bp_background_policy(b->source_flags,b->color,&b->own_flags))
  return bp_component_stop("source_background_contract");
 b->captured=true;return true;
}
static bool bp_background_check(BpEye* e) {
 if(!bp_mono_owner() || !bp.background.captured) return false;
 Il2CppObject* camera=bp_target(e->camera);
 if(bp.failed || camera==bp_target(bp.source_root) ||
  bp.api.object_get_class(camera)!=bp.types[BT_CAMERA])
  return bp_component_stop("owned_background_receiver");
 int32_t flags=0;BpColor color={0};
 if(!bp_get(BM_CLEAR_GET,camera,&flags,sizeof(flags)) ||
  !bp_get(BM_BG_GET,camera,&color,sizeof(color))) return false;
 if(flags!=bp.background.own_flags || !bp_background_equal(bp.background.color,color))
  return bp_component_stop("owned_background_changed");
 return true;
}
static bool bp_background_apply(BpEye* e) {
 if(!bp_mono_owner() || !bp.background.captured ||
  !bp_read_false(BM_ENABLED_GET,e->camera,"owned_background_not_disabled")) return false;
 Il2CppObject* camera=bp_target(e->camera);
 if(bp.failed || camera==bp_target(bp.source_root) ||
  bp.api.object_get_class(camera)!=bp.types[BT_CAMERA])
  return bp_component_stop("owned_background_receiver");
 if(!bp_set(BM_BG_SET,camera,&bp.background.color) ||
  !bp_set(BM_CLEAR,camera,&bp.background.own_flags)) return false;
 return bp_background_check(e);
}
static int bp_background_json(char* out,size_t cap) {
 if(!bp.background.captured) return 0;
 uint32_t bits[4];memcpy(bits,&bp.background.color,sizeof(bits));
 return telemetry_snprintf(out,cap,",\"background\":[%d,%d,\"%08X%08X%08X%08X\",%u,%u]",
  bp.background.source_flags,bp.background.own_flags,bits[0],bits[1],bits[2],bits[3],
  bp.background.configured,bp.background.observed);
}
