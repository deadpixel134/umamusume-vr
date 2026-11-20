/* Only the existing normal stream. Never consume important/terminal reserves.
   The held logger lock makes the room check and normal submission atomic. */
static bool bp_projection_normal_record_tail(const char* event,const char* detail,unsigned tail_bytes,unsigned tail_records) {
 if(!InterlockedCompareExchange(&telemetry_ready,0,0)) return false;
 AcquireSRWLockExclusive(&log_lock);
 ++telemetry_stats.attempted;
 size_t length=strlen(detail),charge=length+384u;
 char json[1503];bool valid=false;
 if(length<1500u && !strstr(detail,TELEMETRY_FORMAT_ERROR)) {
  json[0]='{';memcpy(json+1,detail,length);json[length+1]='}';json[length+2]=0;
  valid=telemetry_json(json)!=FALSE;
 }
 /* Reject malformed evidence here, before the shared writer's format-error
    fallback could classify it as important. Outer envelope is under 384B. */
 const unsigned byte_ceiling=40960u-BP_PR_NORMAL_BYTE_RESERVE,record_ceiling=160u-BP_PR_NORMAL_RECORD_RESERVE;
 /* Admission slack for a later normal summary, not a durable reservation.
    Check tails before subtraction; all callers share this exact lock. */
 bool tails_valid=tail_bytes<=byte_ceiling && tail_records<=record_ceiling;
 unsigned bytes=tails_valid?byte_ceiling-tail_bytes:0,records=tails_valid?record_ceiling-tail_records:0;
 bool room=valid && tails_valid && charge<=bytes &&
  telemetry_stats.bytes[0]<=bytes-charge && telemetry_stats.records[0]<records;
 bool wrote=false;
 if(room) wrote=telemetry_emit_locked("OWN-BASE",event,detail,0)!=FALSE;
 else ++telemetry_stats.dropped;
 ReleaseSRWLockExclusive(&log_lock);
 return wrote;
}
static bool bp_projection_normal_record(const char* event,const char* detail) {
 return bp_projection_normal_record_tail(event,detail,0,0);
}
