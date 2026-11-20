/* Read-only Story eligibility. Optional source probes must never call bp_fail:
   incomplete/ambiguous owners or invocation exceptions simply remain PANEL. */
static Il2CppArray* bp_source_cameras(BpSourceKind kind) {
 /* Existing FindObjectsByType contract: FindObjectsInactive.Exclude=0.
    An inactive GO may retain Camera.enabled, so Story cannot include it.
    Preserve the accepted Home enumeration's Include=1 policy. */
 return unity_find_objects(camera_reflection_type,kind==BP_SOURCE_HOME?1:0,0);
}
static bool bp_effect_owner(Il2CppObject* camera,BpSourceKind kind) {
 if(!camera || !component_get_game_object || !gameobject_get_components ||
  !bp.method[BM_DATA_CAMERA] || !bp.method[BM_DATA_ADDITIONAL] || !bp.method[BM_ENABLED_GET]) return false;
 uint32_t roots[3]={0};bool ok=false;
 roots[0]=bp.api.gchandle_new(camera,false);if(!roots[0]) goto done;
 Il2CppObject* go=component_get_game_object(camera);if(!go) goto done;
 roots[1]=bp.api.gchandle_new(go,false);if(!roots[1]) goto done;
 Il2CppArray* list=gameobject_get_components(go,component_reflection_type,0,0,1,0,NULL);
 if(!list || list->max_length>32) goto done;
 roots[2]=bp.api.gchandle_new((Il2CppObject*)list,false);if(!roots[2]) goto done;
 unsigned mask=0,duplicate=0;bool same_go=true,camera_same=false;
 Il2CppObject *data=NULL,*additional=NULL;
 for(uintptr_t i=0;i<list->max_length;++i) {
  Il2CppObject* c=list->vector[i];unsigned bit=0;
  if(component_is(c,"UnityEngine","Camera")) {bit=BP_STORY_CAMERA;camera_same=c==camera;}
  else if(component_is(c,"Gallop",kind==BP_SOURCE_RACE?"RaceImageEffect":"StoryImageEffect")) bit=BP_STORY_EFFECT;
  else if(component_is(c,"Gallop.RenderPipeline","CameraData")) {bit=BP_STORY_DATA;data=c;}
  else if(component_is(c,"UnityEngine.Rendering.Universal","UniversalAdditionalCameraData")) {bit=BP_STORY_URP;additional=c;}
  if(bit) { duplicate|=mask&bit;mask|=bit;same_go&=component_get_game_object(c)==go; }
 }
 if(!bp_story_component_set(mask,duplicate,same_go,camera_same,true,true)) goto done;
 Il2CppException* exc=NULL;
 Il2CppObject* related=bp.api.runtime_invoke(bp.method[BM_DATA_CAMERA],data,NULL,&exc);
 if(exc || related!=camera) goto done;
 related=bp.api.runtime_invoke(bp.method[BM_DATA_ADDITIONAL],data,NULL,&exc);
 if(exc || related!=additional) goto done;
 Il2CppObject* enabled=bp.api.runtime_invoke(bp.method[BM_ENABLED_GET],camera,NULL,&exc);
 uint8_t* value=!exc && enabled && bp.api.object_get_class(enabled)==bp.types[BT_BOOL]?
  bp.api.object_unbox(enabled):NULL;
 ok=value && *value==1;
done:
 for(unsigned i=0;i<3;++i) if(roots[i]) bp.api.gchandle_free(roots[i]);
 return ok;
}
