// Optional read-only Story geometry observation on the bound Unity thread.
static const Il2CppImage* story_geometry_core;
static const MethodInfo* geometry_method(Il2CppClass* owner,const char* name,Il2CppClass* result,Il2CppClass* arg) {
 void* iter=NULL; const MethodInfo* match=NULL;
 for(unsigned i=0;i<512;i++) {
  const MethodInfo* m=bp.api.class_get_methods(owner,&iter);
  if(!m)return match;
  const char* n=bp.api.method_get_name(m);
  if(!n || strcmp(n,name) || bp.api.method_get_class(m)!=owner)continue;
  uint32_t impl=0, flags=bp.api.method_get_flags(m,&impl);
  if((flags&(7|16|1024))!=6 || bp.api.method_is_generic(m) || bp.api.method_is_inflated(m) ||
     bp.api.method_get_param_count(m)!=(arg?1u:0u))continue;
  const Il2CppType* t=bp.api.method_get_return_type(m);
  if(!t || bp.api.type_is_byref(t) || bp.api.class_from_type(t)!=result)continue;
  if(arg){t=bp.api.method_get_param(m,0);if(!t || bp.api.type_is_byref(t) || bp.api.class_from_type(t)!=arg)continue;}
  if(match)return NULL;
  match=m;
 }
 return NULL;
}
static bool geometry_read(const MethodInfo* method,Il2CppObject* receiver,void** args,Il2CppClass* expected,void* dst,size_t bytes) {
 if(!method || !receiver)return false;
 Il2CppException* exc=NULL;
 Il2CppObject* obj=bp.api.runtime_invoke(method,receiver,args,&exc);
 if(exc || !obj || bp.api.object_get_class(obj)!=expected)return false;
 void* value=bp.api.object_unbox(obj);
 if(!value)return false;
 memcpy(dst,value,bytes);return true;
}
/* Main-thread-only Unity calls. The private mesh and material array are rooted
   for the generation. Only the mesh belongs to us and receives Destroy. */
static volatile LONG layer_trial_finished;
static struct {
 bool attempted;
 uint32_t mesh, materials;
 Matrix4x4 matrix;
 int submeshes,draw_frames,last_frame;
 int pieces,sub_for_piece[64],slot_for_piece[64];
 uint32_t blocks[33]; /* slot 32 holds the renderer-wide fallback. */
 void (*submit)(Il2CppObject*,int,const Matrix4x4*,Il2CppObject*,int,Il2CppObject*,Il2CppObject*,int,bool,Il2CppObject*,int,Il2CppObject*);
} story_bake;

static void story_bake_stop(const char* why) {
 char msg[180];telemetry_snprintf(msg,sizeof msg,"\"stage\":\"body_mesh_unavailable\",\"reason\":\"%s\"",why);
 write_record("STORY","story_display",msg);InterlockedExchange(&layer_trial_finished,1);
}
static void story_release_blocks(void){
 for(unsigned i=0;i<33;i++)if(story_bake.blocks[i]){
  bp.api.gchandle_free(story_bake.blocks[i]);story_bake.blocks[i]=0;
 }
}
static bool story_snapshot_blocks(Il2CppObject* renderer,unsigned slots){
 bool (*has)(Il2CppObject*)=(void*)p_resolve_icall("UnityEngine.Renderer::HasPropertyBlock()");
 if(!has)return false;
 if(!has(renderer))return true;
 Il2CppClass* block=p_class_from_name(story_geometry_core,"UnityEngine","MaterialPropertyBlock");
 const MethodInfo* ctor=block?geometry_method(block,".ctor",bp.types[BT_VOID],NULL):NULL;
 bool (*empty)(Il2CppObject*)=(void*)p_resolve_icall("UnityEngine.MaterialPropertyBlock::get_isEmpty()");
 void (*read_global)(Il2CppObject*,Il2CppObject*)=(void*)p_resolve_icall("UnityEngine.Renderer::Internal_GetPropertyBlock(UnityEngine.MaterialPropertyBlock)");
 void (*read_slot)(Il2CppObject*,Il2CppObject*,int)=(void*)p_resolve_icall("UnityEngine.Renderer::Internal_GetPropertyBlockMaterialIndex(UnityEngine.MaterialPropertyBlock,System.Int32)");
 if(!ctor || !empty || !read_global || !read_slot)return false;
 for(unsigned i=0;i<=slots;i++){
  unsigned key=i==slots?32:i;
  Il2CppObject* value=bp.object_new(block);
  if(!value || !(story_bake.blocks[key]=bp.api.gchandle_new(value,false)))return false;
  Il2CppException* exc=NULL;bp.api.runtime_invoke(ctor,value,NULL,&exc);if(exc)return false;
  if(key==32)read_global(renderer,value);else read_slot(renderer,value,(int)i);
  if(empty(value)){bp.api.gchandle_free(story_bake.blocks[key]);story_bake.blocks[key]=0;}
 }
 return true;
}
static bool story_prepare_body(void){
 Il2CppString* (*version)(void)=(void*)p_resolve_icall("UnityEngine.Application::get_unityVersion()");
 Il2CppString* v=version?version():NULL;
 const char* series="2022.3.";
 if(!v || v->length<7){story_bake_stop("engine_version_missing");return false;}
 for(unsigned i=0;i<7;i++)if(v->chars[i]!=(unsigned char)series[i]){story_bake_stop("unsupported_engine_abi");return false;}
 Il2CppClass* skin=p_class_from_name(story_geometry_core,"UnityEngine","SkinnedMeshRenderer");
 Il2CppClass* mesh=p_class_from_name(story_geometry_core,"UnityEngine","Mesh");
 Il2CppClass* mat=p_class_from_name(story_geometry_core,"UnityEngine","Material");
 if(!skin || !mesh || !mat){story_bake_stop("mesh_types");return false;}
 const MethodInfo* object_name=geometry_method(bp.types[BT_OBJECT],"get_name",bp.types[BT_STRING],NULL);
 const MethodInfo* layer=geometry_method(bp.types[BT_GO],"get_layer",bp.types[BT_INT],NULL);
 void (*create)(Il2CppObject*)=(void*)p_resolve_icall("UnityEngine.Mesh::Internal_Create(UnityEngine.Mesh)");
 void (*bake)(Il2CppObject*,Il2CppObject*,bool)=(void*)p_resolve_icall("UnityEngine.SkinnedMeshRenderer::BakeMesh(UnityEngine.Mesh,System.Boolean)");
 int (*count)(Il2CppObject*)=(void*)p_resolve_icall("UnityEngine.Mesh::get_subMeshCount()");
 Il2CppArray* (*materials)(Il2CppObject*)=(void*)p_resolve_icall("UnityEngine.Renderer::GetSharedMaterialArray()");
 get_matrix4x4_injected_fn matrix=(void*)p_resolve_icall("UnityEngine.Transform::get_localToWorldMatrix_Injected(UnityEngine.Matrix4x4&)");
 story_bake.submit=(void*)p_resolve_icall("UnityEngine.Graphics::Internal_DrawMesh_Injected(UnityEngine.Mesh,System.Int32,UnityEngine.Matrix4x4&,UnityEngine.Material,System.Int32,UnityEngine.Camera,UnityEngine.MaterialPropertyBlock,UnityEngine.Rendering.ShadowCastingMode,System.Boolean,UnityEngine.Transform,UnityEngine.Rendering.LightProbeUsage,UnityEngine.LightProbeProxyVolume)");
 if(!object_name || !layer || !create || !bake || !count || !materials || !matrix || !story_bake.submit){story_bake_stop(!object_name?"name_binding":!layer?"layer_binding":!create?"mesh_create_binding":!bake?"bake_binding":!count?"submesh_binding":!materials?"materials_binding":!matrix?"transform_binding":"draw_binding");return false;}
 Il2CppReflectionType* type=p_type_get_object(p_class_get_type(skin));
 Il2CppArray* list=type?unity_find_objects(type,0,0):NULL;
 if(!list || list->max_length>256 || !bp_root((Il2CppObject*)list)){story_bake_stop("skin_list");return false;}
 Il2CppObject* body=NULL;
 for(uintptr_t i=0;i<list->max_length;i++){
  Il2CppObject* r=list->vector[i];
  if(!r || !bp.api.class_is_assignable_from(skin,bp.api.object_get_class(r)))continue;
  Il2CppObject* go=component_get_game_object(r);if(!go)continue;
  Il2CppException* exc=NULL;
  Il2CppString* label=(Il2CppString*)bp.api.runtime_invoke(object_name,go,NULL,&exc);
  int32_t l=-1;
  if(exc || !il2cpp_string_equals_ascii(label,"M_Body") || !geometry_read(layer,go,NULL,bp.types[BT_INT],&l,sizeof l) || l!=13)continue;
  if(body){story_bake_stop("ambiguous_active_body");return false;}
  body=r;
 }
 if(!body){story_bake_stop("body_missing");return false;}
 Il2CppArray* slots=materials(body);
 if(!slots || !slots->max_length || slots->max_length>32 || !(story_bake.materials=bp_root((Il2CppObject*)slots))){story_bake_stop("material_slots");return false;}
 for(uintptr_t i=0;i<slots->max_length;i++)if(!slots->vector[i] || !bp.api.class_is_assignable_from(mat,bp.api.object_get_class(slots->vector[i]))){story_bake_stop("material_type");return false;}
 Il2CppObject* transform=component_get_transform(body);if(!transform){story_bake_stop("body_transform");return false;}
 matrix(transform,&story_bake.matrix);
 for(unsigned i=0;i<16;i++)if(!isfinite(story_bake.matrix.m[i])){story_bake_stop("body_matrix");return false;}
 Il2CppObject* owned=bp.object_new(mesh);
 if(!owned || !(story_bake.mesh=bp_root(owned))){story_bake_stop("mesh_allocation");return false;}
 create(owned);bake(body,owned,false);
 story_bake.submeshes=count(owned);
 char layout[192];
 telemetry_snprintf(layout,sizeof layout,"\"stage\":\"baked_body_layout\",\"submeshes\":%d,\"material_slots\":%llu",story_bake.submeshes,(unsigned long long)slots->max_length);
 write_record("STORY","story_display",layout);
 if(story_bake.submeshes<=0){story_bake_stop("baked_mesh_no_submeshes");return false;}
 if(story_bake.submeshes>64){story_bake_stop("submesh_limit");return false;}
 uint32_t (*index_count)(Il2CppObject*,int)=(void*)p_resolve_icall("UnityEngine.Mesh::GetIndexCountImpl(System.Int32)");
 if(!index_count){story_bake_stop("index_count_binding");return false;}
 unsigned counts[64];int populated=0;
 for(int i=0;i<story_bake.submeshes;i++){
  counts[i]=index_count(owned,i);if(counts[i])++populated;
 }
 if(!populated){story_bake_stop("baked_mesh_no_indices");return false;}
 if(!story_snapshot_blocks(body,(unsigned)slots->max_length)){story_bake_stop("material_property_snapshot");return false;}
 /* With one material, the private reconstruction covers all populated mesh
    pieces. This does not alter the original renderer's material assignments. */
 int entries=slots->max_length==1?story_bake.submeshes:(int)slots->max_length;
 for(int i=0;i<entries;i++){
  int sub=i<story_bake.submeshes?i:story_bake.submeshes-1;
  if(!counts[sub])continue;
  int piece=story_bake.pieces++;
  story_bake.sub_for_piece[piece]=sub;
  story_bake.slot_for_piece[piece]=slots->max_length==1?0:i;
 }
 if(!story_bake.pieces){story_bake_stop("no_assigned_geometry");return false;}
 char assignment[256];
 telemetry_snprintf(assignment,sizeof assignment,"\"stage\":\"baked_body_assignment\",\"populated\":%d,\"sub0_indices\":%u,\"sub1_indices\":%u,\"draw_pieces\":%d,\"global_block\":%s,\"slot0_block\":%s",populated,counts[0],story_bake.submeshes>1?counts[1]:0,story_bake.pieces,story_bake.blocks[32]?"true":"false",story_bake.blocks[0]?"true":"false");
 write_record("STORY","story_display",assignment);
 return true;
}
static void bp_story_topology(Il2CppArray* cameras,Il2CppObject* source,Scene scene) {
 (void)cameras;
 if(layer_trial_finished || !source || bp.c.phase!=BP_ACTIVE || !bp.backend_verified ||
    scene.handle!=bp.c.identity.scene || source!=bp.api.gchandle_get_target(bp.source_root) ||
    GetCurrentThreadId()!=bp.owner_thread || bp.c.retiring || bp.failed || !p_resolve_icall)return;
 int frame=-1;if(!bp_get(BM_FRAME,NULL,&frame,sizeof(frame)) || frame<0 || (story_bake.draw_frames && story_bake.last_frame==frame))return;
 if(!story_bake.attempted){story_bake.attempted=true;if(!story_prepare_body())return;}
 if(story_bake.draw_frames>=16)return;
 Il2CppArray* slots=(Il2CppArray*)bp_target(story_bake.materials);
 Il2CppObject* mesh=bp_target(story_bake.mesh);
 Il2CppObject* eyes[]={bp_target(bp.eye[0].camera),bp_target(bp.eye[1].camera)};
 if(bp.failed || !slots || !mesh || !eyes[0] || !eyes[1] || eyes[0]==eyes[1] || eyes[0]==source || eyes[1]==source){story_bake_stop("private_eye_identity");return;}
 for(int eye=0;eye<2;eye++){
  for(int piece=0;piece<story_bake.pieces;piece++){
   int sub=story_bake.sub_for_piece[piece],slot=story_bake.slot_for_piece[piece];
   uint32_t root=story_bake.blocks[slot]?story_bake.blocks[slot]:story_bake.blocks[32];
   Il2CppObject* properties=root?bp.api.gchandle_get_target(root):NULL;
   if(root && !properties){story_bake_stop("material_property_lifetime");return;}
   story_bake.submit(mesh,sub,&story_bake.matrix,slots->vector[slot],13,eyes[eye],properties,0,false,NULL,0,NULL);
  }
 }
 story_bake.last_frame=frame;++story_bake.draw_frames;
 if(story_bake.draw_frames==1){char msg[160];telemetry_snprintf(msg,sizeof msg,"\"stage\":\"baked_body_submitted\",\"submeshes\":%d,\"generation\":%llu,\"frame\":%d",story_bake.submeshes,(unsigned long long)bp.c.identity.generation,frame);write_record("STORY","story_display",msg);}
}

static bool story_layer_trial(Scene scene) {
 static bool armed; static int32_t trial_scene;static ULONGLONG next_check,deadline;
 if(InterlockedCompareExchange(&layer_trial_finished,0,0))return false;
 ULONGLONG now=GetTickCount64();
 if(armed){
  if(trial_scene!=scene.handle || now>=deadline){InterlockedExchange(&layer_trial_finished,1);return false;}
  return true;
 }
 if(now<next_check)return false;
 next_check=now+500;
 WCHAR path[MAX_PATH];wcscpy_s(path,MAX_PATH,log_path);WCHAR* leaf=wcsrchr(path,92);
 if(!leaf || swprintf_s(leaf+1,MAX_PATH-(size_t)(leaf+1-path),L"story-baked-body-%lu-%llu.ready",(unsigned long)GetCurrentProcessId(),(unsigned long long)telemetry_attach)<=0)return false;
 DWORD attributes=GetFileAttributesW(path);
 if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))return false;
 armed=true;trial_scene=scene.handle;deadline=now+12000;return true;
}
