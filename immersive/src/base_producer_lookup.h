/* Exact producer metadata selection; extracted unchanged spec table for the shared production lookup fixture. */
enum BpType { BT_VOID, BT_BOOL, BT_INT, BT_FLOAT, BT_STRING, BT_TYPE, BT_INTPTR,
 BT_OBJECT, BT_GO, BT_COMPONENT, BT_BEHAVIOUR, BT_CAMERA, BT_TEXTURE, BT_RT,
 BT_TRANSFORM, BT_V3, BT_Q4, BT_M4, BT_ADDITIONAL, BT_DATA, BT_RT_FORMAT,
 BT_RENDER_TYPE, BT_STEREO_MASK, BT_CLEAR_FLAGS, BT_TIME, BT_SYSTEMINFO, BT_GRAPHICS_API,
 BT_LIST, BT_COLOR, BT_COUNT };
enum BpMethod { BM_GO_CTOR, BM_ACTIVE_SET, BM_ACTIVE_GET, BM_ADD, BM_COMPARE_TAG,
 BM_NAME, BM_ENABLED_SET, BM_ENABLED_GET, BM_TRANSFORM, BM_INSTANCE,
 BM_ALIVE, BM_DESTROY, BM_RT_CTOR, BM_RT_AA, BM_RT_MIPS, BM_RT_AUTOMIPS,
 BM_RT_CREATE, BM_RT_CREATED, BM_XR_SET, BM_XR_GET, BM_STEREO_ENABLED, BM_TARGET_SET, BM_TARGET_GET,
 BM_ASPECT, BM_DEPTH, BM_NEAR_GET, BM_FAR_GET, BM_NEAR_SET, BM_FAR_SET,
 BM_CULL_GET, BM_CULL_SET, BM_ORTHO, BM_MSAA, BM_CLEAR,
 BM_POSITION_GET, BM_ROTATION_GET, BM_POSITION_SET, BM_ROTATION_SET,
 BM_PROJECTION, BM_PROJECTION_GET, BM_RENDER_TYPE_SET, BM_RENDER_TYPE_GET, BM_STACK,
 BM_POST, BM_SHADOWS, BM_INITIALIZE, BM_DATA_CAMERA, BM_DATA_ADDITIONAL,
 BM_FRAME, BM_BACKEND, BM_CLEAR_GET, BM_BG_GET, BM_BG_SET, BM_COUNT };
typedef struct BpMethodSpec {
 int owner; const char* name; int ret; unsigned argc; int arg[4]; bool is_static;
} BpMethodSpec;
static const BpMethodSpec bp_specs[BM_COUNT]={
 {BT_GO,".ctor",BT_VOID,0,{0},false},
 {BT_GO,"SetActive",BT_VOID,1,{BT_BOOL},false},
 {BT_GO,"get_activeSelf",BT_BOOL,0,{0},false},
 {BT_GO,"AddComponent",BT_COMPONENT,1,{BT_TYPE},false},
 {BT_GO,"CompareTag",BT_BOOL,1,{BT_STRING},false},
 {BT_OBJECT,"set_name",BT_VOID,1,{BT_STRING},false},
 {BT_BEHAVIOUR,"set_enabled",BT_VOID,1,{BT_BOOL},false},
 {BT_BEHAVIOUR,"get_enabled",BT_BOOL,0,{0},false},
 {BT_COMPONENT,"get_transform",BT_TRANSFORM,0,{0},false},
 {BT_OBJECT,"GetInstanceID",BT_INT,0,{0},false},
 {BT_OBJECT,"op_Implicit",BT_BOOL,1,{BT_OBJECT},true},
 {BT_OBJECT,"Destroy",BT_VOID,1,{BT_OBJECT},true},
 {BT_RT,".ctor",BT_VOID,4,{BT_INT,BT_INT,BT_INT,BT_RT_FORMAT},false},
 {BT_RT,"set_antiAliasing",BT_VOID,1,{BT_INT},false},
 {BT_RT,"set_useMipMap",BT_VOID,1,{BT_BOOL},false},
 {BT_RT,"set_autoGenerateMips",BT_VOID,1,{BT_BOOL},false},
 {BT_RT,"Create",BT_BOOL,0,{0},false},
 {BT_RT,"IsCreated",BT_BOOL,0,{0},false},
 {BT_ADDITIONAL,"set_allowXRRendering",BT_VOID,1,{BT_BOOL},false},
 {BT_ADDITIONAL,"get_allowXRRendering",BT_BOOL,0,{0},false},
 {BT_CAMERA,"get_stereoEnabled",BT_BOOL,0,{0},false},
 {BT_CAMERA,"set_targetTexture",BT_VOID,1,{BT_RT},false},
 {BT_CAMERA,"get_targetTexture",BT_RT,0,{0},false},
 {BT_CAMERA,"set_aspect",BT_VOID,1,{BT_FLOAT},false},
 {BT_CAMERA,"set_depth",BT_VOID,1,{BT_FLOAT},false},
 {BT_CAMERA,"get_nearClipPlane",BT_FLOAT,0,{0},false},
 {BT_CAMERA,"get_farClipPlane",BT_FLOAT,0,{0},false},
 {BT_CAMERA,"set_nearClipPlane",BT_VOID,1,{BT_FLOAT},false},
 {BT_CAMERA,"set_farClipPlane",BT_VOID,1,{BT_FLOAT},false},
 {BT_CAMERA,"get_cullingMask",BT_INT,0,{0},false},
 {BT_CAMERA,"set_cullingMask",BT_VOID,1,{BT_INT},false},
 {BT_CAMERA,"get_orthographic",BT_BOOL,0,{0},false},
 {BT_CAMERA,"set_allowMSAA",BT_VOID,1,{BT_BOOL},false},
 {BT_CAMERA,"set_clearFlags",BT_VOID,1,{BT_CLEAR_FLAGS},false},
 {BT_TRANSFORM,"get_position",BT_V3,0,{0},false},
 {BT_TRANSFORM,"get_rotation",BT_Q4,0,{0},false},
 {BT_TRANSFORM,"set_position",BT_VOID,1,{BT_V3},false},
 {BT_TRANSFORM,"set_rotation",BT_VOID,1,{BT_Q4},false},
 {BT_CAMERA,"set_projectionMatrix",BT_VOID,1,{BT_M4},false},
 {BT_CAMERA,"get_projectionMatrix",BT_M4,0,{0},false},
 {BT_ADDITIONAL,"set_renderType",BT_VOID,1,{BT_RENDER_TYPE},false},
 {BT_ADDITIONAL,"get_renderType",BT_RENDER_TYPE,0,{0},false},
 {BT_ADDITIONAL,"get_cameraStack",BT_LIST,0,{0},false},
 {BT_ADDITIONAL,"set_renderPostProcessing",BT_VOID,1,{BT_BOOL},false},
 {BT_ADDITIONAL,"set_renderShadows",BT_VOID,1,{BT_BOOL},false},
 {BT_DATA,"Initialize",BT_VOID,1,{BT_INT},false},
 {BT_DATA,"get_Camera",BT_CAMERA,0,{0},false},
 {BT_DATA,"get_AdditionalCameraData",BT_ADDITIONAL,0,{0},false},
 {BT_TIME,"get_frameCount",BT_INT,0,{0},true},
 {BT_SYSTEMINFO,"get_graphicsDeviceType",BT_GRAPHICS_API,0,{0},true},
 {BT_CAMERA,"get_clearFlags",BT_CLEAR_FLAGS,0,{0},false},
 {BT_CAMERA,"get_backgroundColor",BT_COLOR,0,{0},false},
 {BT_CAMERA,"set_backgroundColor",BT_VOID,1,{BT_COLOR},false}
};
/* Fixed-size evidence for the one required tag query. No addresses/invocation. */
#define BP_LOOKUP_CAPTURE_LIMIT 3
typedef struct BpClassEvidence { bool present,exact,truncated; char ns[32],name[48]; } BpClassEvidence;
typedef struct BpTypeEvidence {
 bool present,byref,class_observed,class_required; int code; BpClassEvidence klass;
} BpTypeEvidence;
typedef struct BpCandidateEvidence {
 unsigned scan_index,arity,rejection; uint32_t flags,implementation_flags;
 bool generic,inflated,name_truncated; char name[24]; BpClassEvidence owner;
 BpTypeEvidence result,parameter;
} BpCandidateEvidence;
typedef struct BpLookupEvidence {
 unsigned scan_count,evaluated_count,named_count,valid_matches,invalid_names,captured,omitted;
 bool scan_completed,capped,ambiguous; BpCandidateEvidence candidate[BP_LOOKUP_CAPTURE_LIMIT];
} BpLookupEvidence;
static inline bool bp_requires_readable_names(unsigned id) {
 return id==BM_COMPARE_TAG || id==BM_XR_SET || id==BM_XR_GET || id==BM_STEREO_ENABLED ||
  id==BM_CLEAR_GET || id==BM_BG_GET || id==BM_BG_SET;
}
static inline int bp_type_code(int type) {
 if(type==BT_VOID) return 1; if(type==BT_BOOL) return 2;
 if(type==BT_INT) return 8; if(type==BT_FLOAT) return 12;
 if(type==BT_STRING) return 14; if(type==BT_INTPTR) return 24;
 if(type==BT_LIST) return 21;
 if(type==BT_V3 || type==BT_Q4 || type==BT_M4 || type==BT_RT_FORMAT ||
  type==BT_RENDER_TYPE || type==BT_STEREO_MASK || type==BT_CLEAR_FLAGS || type==BT_GRAPHICS_API || type==BT_COLOR) return 17;
 return 18;
}
/* One predicate implementation shared by production selection and its tests.
   1 null, 2 byref, 3 type code, 4 class identity. */
static inline unsigned bp_type_rejection(const RdApi* a,Il2CppClass* const* types,const Il2CppType* t,int type) {
 if(!t) return 1; if(a->type_is_byref(t)) return 2;
 if(a->type_get_type(t)!=bp_type_code(type)) return 3;
 if(type==BT_VOID) return 0;
 Il2CppClass* c=a->class_from_type(t);
 if(type==BT_LIST) return c && !strcmp(a->class_get_namespace(c),"System.Collections.Generic") &&
  !strcmp(a->class_get_name(c),"List`1")?0u:4u;
 return c==types[type]?0u:4u;
}
static inline bool bp_lookup_type_match(const RdApi* a,Il2CppClass* const* types,const Il2CppType* t,int type) {
 return bp_type_rejection(a,types,t,type)==0;
}
static inline bool bp_evidence_text(char* out,size_t capacity,const char* text) {
 size_t n=0; if(text) while(n+1<capacity && text[n]) { out[n]=text[n]; ++n; }
 out[n]=0; return text && text[n]!=0;
}
static inline BpClassEvidence bp_evidence_class(const RdApi* a,Il2CppClass* actual,Il2CppClass* expected) {
 BpClassEvidence e={0}; e.present=actual!=NULL; e.exact=actual && actual==expected;
 if(actual) {
  e.truncated=bp_evidence_text(e.ns,sizeof(e.ns),a->class_get_namespace(actual));
  e.truncated|=bp_evidence_text(e.name,sizeof(e.name),a->class_get_name(actual));
 }
 return e;
}
static inline BpTypeEvidence bp_evidence_type(const RdApi* a,Il2CppClass* const* types,const Il2CppType* t,int expected) {
 BpTypeEvidence e={0}; e.code=-1; e.present=t!=NULL; e.class_required=expected!=BT_VOID;
 if(!t) return e;
 e.code=a->type_get_type(t); e.byref=a->type_is_byref(t);
 /* Do not resolve open generic/invalid/pointer metadata for observation.
    Its actual code/byref remains evidence, class identity stays unknown. */
 bool concrete=(e.code>=2 && e.code<=14) || e.code==17 || e.code==18 ||
  e.code==24 || e.code==25 || e.code==28;
 if(concrete) { e.class_observed=true; e.klass=bp_evidence_class(a,a->class_from_type(t),types[expected]); }
 return e;
}
static inline const char* bp_rejection_name(unsigned r) {
 switch(r) {
 case 0:return "match"; case 1:return "declaring_owner"; case 2:return "access";
 case 3:return "static"; case 4:return "abstract"; case 5:return "generic";
 case 6:return "inflated"; case 7:return "arity";
 case 10:return "return_null"; case 11:return "return_byref"; case 12:return "return_code"; case 13:return "return_class";
 case 20:return "parameter_null"; case 21:return "parameter_byref"; case 22:return "parameter_code"; case 23:return "parameter_class";
 default:return "unknown";
 }
}
static inline const char* bp_lookup_outcome(const BpLookupEvidence* e) {
 if(e->ambiguous) return "ambiguous";
 if(!e->scan_completed || e->capped) return "incomplete";
 if(e->valid_matches) return "exact_match";
 if(!e->named_count) return e->invalid_names?"name_unreadable":"name_absent";
 return "filtered_out";
}
static const MethodInfo* bp_find(const RdApi* a,Il2CppClass* const* types,const BpMethodSpec* s,BpLookupEvidence* evidence) {
 if(evidence) memset(evidence,0,sizeof(*evidence));
 void* it=NULL; const MethodInfo *m,*found=NULL; unsigned count=0;
 while((m=a->class_get_methods(types[s->owner],&it))) {
  ++count; if(evidence) evidence->scan_count=count;
  if(count>512) { if(evidence) evidence->capped=true; return NULL; }
  if(evidence) ++evidence->evaluated_count;
  const char* name=a->method_get_name(m); uint32_t impl=0,flags=a->method_get_flags(m,&impl);
  if(!name || strcmp(name,s->name)) { if(evidence && !name) ++evidence->invalid_names; continue; }
  if(evidence) ++evidence->named_count;
  unsigned reject=0;
  if(a->method_get_class(m)!=types[s->owner]) reject=1;
  else if((flags&7u)!=6u) reject=2;
  else if(((flags&0x10u)!=0)!=s->is_static) reject=3;
  else if(flags&0x400u) reject=4;
  else if(a->method_is_generic(m)) reject=5;
  else if(a->method_is_inflated(m)) reject=6;
  else if(a->method_get_param_count(m)!=s->argc) reject=7;
  else {
   unsigned type_reject=bp_type_rejection(a,types,a->method_get_return_type(m),s->ret);
   if(type_reject) reject=9+type_reject;
   else for(unsigned i=0;i<s->argc;++i) {
    type_reject=bp_type_rejection(a,types,a->method_get_param(m,i),s->arg[i]);
    if(type_reject && !reject) reject=19+type_reject;
   }
  }
  /* Snapshot only exact-name candidates, after the unchanged selection
     predicate has run. Recording cannot decide the returned method. */
  if(evidence) {
   if(evidence->captured<BP_LOOKUP_CAPTURE_LIMIT) {
    BpCandidateEvidence* e=&evidence->candidate[evidence->captured++];
    e->scan_index=count; e->flags=flags; e->implementation_flags=impl; e->rejection=reject;
    e->name_truncated=bp_evidence_text(e->name,sizeof(e->name),name);
    e->owner=bp_evidence_class(a,a->method_get_class(m),types[s->owner]);
    e->generic=a->method_is_generic(m); e->inflated=a->method_is_inflated(m);
    e->arity=a->method_get_param_count(m);
    e->result=bp_evidence_type(a,types,a->method_get_return_type(m),s->ret);
    e->parameter=bp_evidence_type(a,types,e->arity?a->method_get_param(m,0):NULL,s->arg[0]);
   } else ++evidence->omitted;
  }
  if(reject) continue;
  if(evidence) ++evidence->valid_matches;
  if(found) { if(evidence) evidence->ambiguous=true; return NULL; }
  found=m;
 }
 if(evidence) evidence->scan_completed=true;
 return found;
}

