/* Current owned-source authorization, independent of the mutable bp.c.
   Payload publishers run under exclusive bp_lock. Revocation is one matching
   generation CAS, also safe for the existing shared-lock consumer-stop path.
   All shared payload accesses are Interlocked; no seqlock C data races. */
typedef struct BpConsumerLease {
 volatile LONG64 sequence,active,generation,source,pose,config;
 volatile LONG scene;
} BpConsumerLease;
_Static_assert(_Alignof(BpConsumerLease)>=8,"Interlocked64 lease alignment");
typedef struct BpLeaseSnapshot { BpIdentity identity;LONG64 sequence; } BpLeaseSnapshot;
static inline LONG64 bp_lease_load64(volatile LONG64* value) { return InterlockedCompareExchange64(value,0,0); }
static inline bool bp_lease_snapshot(BpConsumerLease* lease,BpLeaseSnapshot* out) {
 LONG64 before=bp_lease_load64(&lease->sequence);
 if(before<0 || (before&1)) return false;
 BpLeaseSnapshot s={0};s.sequence=before;
 s.identity.generation=(uint64_t)bp_lease_load64(&lease->generation);
 s.identity.source=(uint64_t)bp_lease_load64(&lease->source);
 s.identity.pose=(uint64_t)bp_lease_load64(&lease->pose);
 s.identity.config=(uint64_t)bp_lease_load64(&lease->config);
 s.identity.scene=InterlockedCompareExchange(&lease->scene,0,0);
 LONG64 active=bp_lease_load64(&lease->active);
 LONG64 after=bp_lease_load64(&lease->sequence);
 if(before!=after || (after&1) || active<=0 || (uint64_t)active!=s.identity.generation) return false;
 *out=s;return true;
}
static inline void bp_lease_revoke(BpConsumerLease* lease,uint64_t generation) {
 if(generation && generation<=INT32_MAX) (void)InterlockedCompareExchange64(&lease->active,0,(LONG64)generation);
}
static inline bool bp_lease_publish(BpConsumerLease* lease,const BpIdentity* id,bool stopped) {
 if(stopped || !id->generation || id->generation>INT32_MAX || !id->source || !id->pose || !id->config) {
  bp_lease_revoke(lease,id->generation);return false;
 }
 BpLeaseSnapshot previous;
 if(bp_lease_snapshot(lease,&previous) && bp_identity_equal(&previous.identity,id)) return true;
 LONG64 sequence=bp_lease_load64(&lease->sequence);
 if(sequence<0 || (sequence&1) || sequence>INT64_MAX-2) { InterlockedExchange64(&lease->active,0);return false; }
 InterlockedExchange64(&lease->active,0);
 InterlockedExchange64(&lease->sequence,sequence+1);
 InterlockedExchange64(&lease->generation,(LONG64)id->generation);
 InterlockedExchange64(&lease->source,(LONG64)id->source);
 InterlockedExchange64(&lease->pose,(LONG64)id->pose);
 InterlockedExchange64(&lease->config,(LONG64)id->config);
 InterlockedExchange(&lease->scene,id->scene);
 InterlockedExchange64(&lease->sequence,sequence+2);
 InterlockedExchange64(&lease->active,(LONG64)id->generation);return true;
}
/* A successful final check authorizes this frame's XR-owned cache draw.
   Retirement may follow that boundary; next-frame acquisition rejects it.
   This is not an instant recall of already queued GPU/EndFrame work. */
static inline bool bp_lease_authorize(BpConsumerLease* lease,const BpLeaseSnapshot* snapshot,
 const BpIdentity* cached,uint64_t capture_tick,uint64_t now,volatile LONG* published,volatile LONG* owned_published) {
 if(!bp_identity_equal(&snapshot->identity,cached) || !capture_tick || now<capture_tick || now-capture_tick>500 ||
  !cached->generation || cached->generation>INT32_MAX) return false;
 LONG generation=(LONG)cached->generation;
 bool valid=bp_lease_load64(&lease->active)==(LONG64)generation &&
  InterlockedCompareExchange(published,0,0)==generation && InterlockedCompareExchange(owned_published,0,0)==generation;
 LONG64 final_sequence=bp_lease_load64(&lease->sequence);
 return valid && final_sequence==snapshot->sequence && !(final_sequence&1);
}
