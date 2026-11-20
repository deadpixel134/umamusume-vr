/* Bounded observation of the existing required spec array, not an API survey.
   No invocation, class resolution, initialization, or production-table writes. */
#define BP_PREFLIGHT_DETAILS 3u /* the three replacement XR/mono contracts */
#define BP_PREFLIGHT_RECORD_LIMIT 56u
#define BP_PREFLIGHT_BYTE_LIMIT 16384u /* includes a conservative 384B envelope */
#define BP_PREFLIGHT_DETAIL_LIMIT 1500u
typedef struct BpBindingRow {
 BpClassEvidence owner;
 unsigned scan,evaluated,named,valid,invalid_names,captured,omitted;
 bool observed,available,complete,capped,selected,detail_kept;
 const char* result;
} BpBindingRow;
typedef struct BpBindingDetail { bool used; unsigned id; BpLookupEvidence lookup; } BpBindingDetail;
typedef struct BpPreflight {
 BpBindingRow row[BM_COUNT]; BpBindingDetail detail[BP_PREFLIGHT_DETAILS];
 unsigned first_failure,observed,unavailable,failed,details_omitted;
 bool used,complete,blocked_before_methods;
} BpPreflight;
_Static_assert(BM_COUNT<=64,"Required spec observation has a fixed bound");
_Static_assert(sizeof(BpPreflight)<=16384,"Bounded static metadata snapshot");
static inline bool bp_metadata_ready(const RdApi* a) {
 return a->class_get_methods && a->class_get_name && a->class_get_namespace &&
  a->method_get_name && a->method_get_flags && a->method_get_class &&
  a->method_is_generic && a->method_is_inflated && a->method_get_param_count &&
  a->method_get_return_type && a->method_get_param && a->type_is_byref &&
  a->type_get_type && a->class_from_type;
}
static inline bool bp_preflight_available(const RdApi* a,Il2CppClass* const* types,unsigned id) {
 const BpMethodSpec* s=&bp_specs[id];
 if(!bp_metadata_ready(a) || !types[s->owner]) return false;
 if(s->ret!=BT_VOID && s->ret!=BT_LIST && !types[s->ret]) return false;
 for(unsigned i=0;i<s->argc;++i) if(s->arg[i]!=BT_VOID && s->arg[i]!=BT_LIST && !types[s->arg[i]]) return false;
 return true;
}
static inline void bp_preflight_note(BpPreflight* p,const RdApi* a,Il2CppClass* const* types,
 unsigned id,const BpLookupEvidence* e,bool selected) {
 BpBindingRow* r=&p->row[id]; r->observed=true; ++p->observed;
 if(!e) { r->result="not_available"; ++p->unavailable; return; }
 r->available=true; r->owner=bp_evidence_class(a,types[bp_specs[id].owner],types[bp_specs[id].owner]);
 r->scan=e->scan_count; r->evaluated=e->evaluated_count; r->named=e->named_count;
 r->valid=e->valid_matches; r->invalid_names=e->invalid_names;
 r->captured=e->captured; r->omitted=e->omitted; r->complete=e->scan_completed; r->capped=e->capped;
 r->selected=selected; r->result=bp_requires_readable_names(id) && e->invalid_names?"name_unreadable":bp_lookup_outcome(e);
 if(selected) return;
 ++p->failed;
 /* Preserve detailed evidence for the three new required contracts. The
    other 47 retain summaries; omitted detail never means absent metadata. */
 if(!e->captured) return;
 unsigned slot=id==BM_XR_SET?0u:id==BM_XR_GET?1u:id==BM_STEREO_ENABLED?2u:BP_PREFLIGHT_DETAILS;
 if(slot==BP_PREFLIGHT_DETAILS) { ++p->details_omitted; return; }
 p->detail[slot]=(BpBindingDetail){.used=true,.id=id,.lookup=*e}; r->detail_kept=true;
}
static inline void bp_preflight_remaining(BpPreflight* p,const RdApi* a,Il2CppClass* const* types,unsigned begin) {
 for(unsigned id=begin;id<BM_COUNT;++id) {
  if(!bp_preflight_available(a,types,id)) { bp_preflight_note(p,a,types,id,NULL,false); continue; }
  BpLookupEvidence e; const MethodInfo* m=bp_find(a,types,&bp_specs[id],&e);
  bool selected=m && !(bp_requires_readable_names(id) && e.invalid_names);
  bp_preflight_note(p,a,types,id,&e,selected);
 }
 p->complete=true;
}
