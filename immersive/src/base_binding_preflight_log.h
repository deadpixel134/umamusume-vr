/* Existing normal writer only. No reserve/emergency/health calls. Successful
   formatting/submission is not proof of delivery through its normal quota. */
typedef struct BpPreflightBudget {
 unsigned records,bytes,format_skipped,budget_skipped,rows_submitted,rows_omitted;
} BpPreflightBudget;
static bool bp_preflight_record(BpPreflightBudget* b,const char* event,const char* detail,int n,unsigned rows) {
 if(n<=0 || n>=(int)BP_PREFLIGHT_DETAIL_LIMIT) { ++b->format_skipped; b->rows_omitted+=rows; return false; }
 unsigned charge=(unsigned)n+384u;
 /* Reserve one record/1024B for the omission footer; not telemetry reserve. */
 if(b->records>=BP_PREFLIGHT_RECORD_LIMIT-1 || b->bytes+charge>BP_PREFLIGHT_BYTE_LIMIT-1024) {
  ++b->budget_skipped; b->rows_omitted+=rows; return false;
 }
 ++b->records; b->bytes+=charge; b->rows_submitted+=rows;
 write_record("OWN-BASE",event,detail); return true;
}
static int bp_preflight_row(char* out,size_t capacity,const BpPreflight* p,unsigned id) {
 const BpBindingRow* r=&p->row[id]; char method[192],ns[192],name[288];
 json_escape(bp_specs[id].name,method,sizeof(method)); json_escape(r->owner.ns,ns,sizeof(ns));
 json_escape(r->owner.name,name,sizeof(name));
 return telemetry_snprintf(out,capacity,
  "[%u,\"%s\",%d,\"%s\",\"%s\",%u,\"%s\",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u]",
  id,method,bp_specs[id].owner,ns,name,r->owner.truncated,r->result?r->result:"not_observed",
  r->scan,r->evaluated,r->complete,r->capped,r->named,r->valid,r->invalid_names,r->selected,
  r->captured,r->omitted,r->detail_kept);
}
static void bp_preflight_type_record(BpPreflightBudget* b,unsigned id,unsigned slot,const char* part,const BpTypeEvidence* t) {
 char ns[192],name[288],detail[BP_PREFLIGHT_DETAIL_LIMIT];
 json_escape(t->klass.ns,ns,sizeof(ns)); json_escape(t->klass.name,name,sizeof(name));
 int n=telemetry_snprintf(detail,sizeof(detail),
  "\"spec\":%u,\"slot\":%u,\"part\":\"%s\",\"present\":%s,\"type_code\":%d,\"byref\":%s,"
  "\"class_observed\":%s,\"class_required_for_match\":%s,\"class_present\":%s,\"class_exact\":%s,"
  "\"namespace\":\"%s\",\"class\":\"%s\",\"text_truncated\":%s",
  id,slot,part,t->present?"true":"false",t->code,t->byref?"true":"false",
  t->class_observed?"true":"false",t->class_required?"true":"false",t->klass.present?"true":"false",
  t->klass.exact?"true":"false",ns,name,t->klass.truncated?"true":"false");
 (void)bp_preflight_record(b,"base_binding_type",detail,n,0);
}
static void bp_preflight_detail_records(BpPreflightBudget* b,const BpBindingDetail* d) {
 if(!d->used) return;
 for(unsigned i=0;i<d->lookup.captured;++i) {
  const BpCandidateEvidence* c=&d->lookup.candidate[i];
  char name[144],ns[192],owner[288],detail[BP_PREFLIGHT_DETAIL_LIMIT];
  json_escape(c->name,name,sizeof(name)); json_escape(c->owner.ns,ns,sizeof(ns));
  json_escape(c->owner.name,owner,sizeof(owner));
  int n=telemetry_snprintf(detail,sizeof(detail),
   "\"spec\":%u,\"slot\":%u,\"scan_index\":%u,\"name\":\"%s\",\"owner_present\":%s,\"owner_exact\":%s,"
   "\"declaring_namespace\":\"%s\",\"declaring_class\":\"%s\",\"flags\":%u,\"implementation_flags\":%u,"
   "\"access\":%u,\"static\":%s,\"abstract\":%s,\"generic\":%s,\"inflated\":%s,"
   "\"arity\":%u,\"first_rejection\":\"%s\",\"text_truncated\":%s",
   d->id,i,c->scan_index,name,c->owner.present?"true":"false",c->owner.exact?"true":"false",ns,owner,
   c->flags,c->implementation_flags,c->flags&7u,(c->flags&0x10u)?"true":"false",
   (c->flags&0x400u)?"true":"false",c->generic?"true":"false",c->inflated?"true":"false",
   c->arity,bp_rejection_name(c->rejection),(c->name_truncated || c->owner.truncated)?"true":"false");
  (void)bp_preflight_record(b,"base_binding_candidate",detail,n,0);
  bp_preflight_type_record(b,d->id,i,"return",&c->result);
  bp_preflight_type_record(b,d->id,i,"parameter0",&c->parameter);
 }
}
static void bp_emit_preflight(const BpPreflight* p) {
 BpPreflightBudget b={0}; char detail[BP_PREFLIGHT_DETAIL_LIMIT],row[640];
 int n=telemetry_snprintf(detail,sizeof(detail),
  "\"specs\":%u,\"first_failure\":%u,\"blocked_before_methods\":%s,\"metadata_only\":true,"
  "\"field_contract\":\"not_available_not_queried_by_observation\","
  "\"columns\":[\"id\",\"requested_name\",\"owner_type\",\"owner_ns\",\"owner_class\",\"owner_text_truncated\","
  "\"result\",\"scan\",\"evaluated\",\"complete\",\"cap\",\"named\",\"valid\",\"invalid_names\",\"selected\","
  "\"captured\",\"capture_omitted\",\"details_kept\"],"
  "\"candidate_limit\":3,\"detail_specs\":3,\"parameter_detail\":\"parameter0_only\","
  "\"record_limit\":%u,\"byte_limit_with_envelope\":%u,\"writer_delivery\":\"normal_quota_may_drop\"",
  (unsigned)BM_COUNT,p->first_failure,p->blocked_before_methods?"true":"false",
  BP_PREFLIGHT_RECORD_LIMIT,BP_PREFLIGHT_BYTE_LIMIT);
 (void)bp_preflight_record(&b,"base_binding_header",detail,n,0);
 /* The old missing native method is no longer a managed dependency. Give
    the three actual new specs first claim on the same fixed evidence budget. */
 const unsigned priority[]={BM_XR_SET,BM_XR_GET,BM_STEREO_ENABLED};
 for(unsigned i=0;i<3;++i) {
  n=bp_preflight_row(row,sizeof(row),p,priority[i]);
  if(n>0 && n<(int)sizeof(row)) {
   n=telemetry_snprintf(detail,sizeof(detail),"\"priority\":true,\"rows\":[%s]",row);
   (void)bp_preflight_record(&b,"base_binding_summary",detail,n,1);
  } else { ++b.format_skipped; ++b.rows_omitted; }
 }
 for(unsigned i=0;i<BP_PREFLIGHT_DETAILS;++i) bp_preflight_detail_records(&b,&p->detail[i]);
 unsigned rows=0; size_t used=0;
 for(unsigned id=0;id<BM_COUNT;++id) {
  if(id==BM_XR_SET || id==BM_XR_GET || id==BM_STEREO_ENABLED) continue;
  n=bp_preflight_row(row,sizeof(row),p,id);
  if(n<=0 || n>=(int)sizeof(row)) { ++b.format_skipped; ++b.rows_omitted; continue; }
  /* Up to four compact rows; unusually escaped labels can flush sooner.
     Test fit before writing, so no partial JSON can reach the writer. */
  if(rows && used+(size_t)n+3>=sizeof(detail)) {
   detail[used++]=']';detail[used]=0;
   (void)bp_preflight_record(&b,"base_binding_summary",detail,(int)used,rows);rows=0;
  }
  if(!rows) { memcpy(detail,"\"priority\":false,\"rows\":[",25); used=25; }
  if(rows) detail[used++]=',';
  memcpy(detail+used,row,(size_t)n);used+=(size_t)n;detail[used]=0;++rows;
  if(rows==4) {
   detail[used++]=']';detail[used]=0;
   (void)bp_preflight_record(&b,"base_binding_summary",detail,(int)used,rows);rows=0;
  }
 }
 if(rows) {
  detail[used++]=']';detail[used]=0;
  (void)bp_preflight_record(&b,"base_binding_summary",detail,(int)used,rows);
 }
 n=telemetry_snprintf(detail,sizeof(detail),
  "\"specs\":%u,\"observed\":%u,\"observation_complete\":%s,\"not_available\":%u,\"nonmatching\":%u,"
  "\"detail_specs_omitted\":%u,\"rows_submitted\":%u,\"rows_omitted\":%u,"
  "\"format_skipped\":%u,\"budget_skipped\":%u,\"records_before_footer\":%u,"
  "\"charged_bytes_before_footer\":%u,\"writer_delivery\":\"unconfirmed_normal_quota\","
  "\"omission_is_not_absence\":true",
  (unsigned)BM_COUNT,p->observed,p->complete?"true":"false",p->unavailable,p->failed,p->details_omitted,
  b.rows_submitted,b.rows_omitted,b.format_skipped,b.budget_skipped,b.records,b.bytes);
 if(n>0 && n<(int)sizeof(detail)) write_record("OWN-BASE","base_binding_footer",detail);
}
