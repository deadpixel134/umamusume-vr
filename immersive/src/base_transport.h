/* Process-lifetime receipt for the ONE shared eye pair. Writers hold bp_lock
   exclusively; the single XR consumer holds it shared. The outstanding serial
   is atomic so an old consumer acknowledgement cannot erase a later receipt.
   This state is never reset when an owned generation is rearmed. */
enum { BP_TRANSPORT_NONE, BP_TRANSPORT_OWN, BP_TRANSPORT_LIVE };
static bool bp_transport_live_current(LONG authored_generation,LONG global_generation,bool live_owner) {
 return live_owner && authored_generation>0 && authored_generation==global_generation;
}
typedef struct BpTransport {
 LONG owner,generation;
 LONG64 serial;
 volatile LONG64 outstanding;
 volatile LONG quarantined;
} BpTransport;
static bool bp_transport_blocked(BpTransport* t) {
 return InterlockedCompareExchange(&t->quarantined,0,0)!=0;
}
static LONG64 bp_transport_pending(BpTransport* t) {
 return InterlockedCompareExchange64(&t->outstanding,0,0);
}
static void bp_transport_quarantine(BpTransport* t) {
 InterlockedExchange(&t->quarantined,1);
}
static bool bp_transport_begin(BpTransport* t,LONG owner,LONG generation,LONG64 serial) {
 if(bp_transport_blocked(t) || bp_transport_pending(t) || generation<=0 || serial<=0 ||
  (owner!=BP_TRANSPORT_OWN && owner!=BP_TRANSPORT_LIVE)) return false;
 t->owner=owner;t->generation=generation;t->serial=serial;
 InterlockedExchange64(&t->outstanding,serial);return true;
}
static bool bp_transport_matches(BpTransport* t,LONG owner,LONG generation,LONG64 serial) {
 return !bp_transport_blocked(t) && serial>0 && t->owner==owner &&
  t->generation==generation && t->serial==serial && bp_transport_pending(t)==serial;
}
static bool bp_transport_ack(BpTransport* t,LONG owner,LONG generation,LONG64 serial) {
 if(!bp_transport_matches(t,owner,generation,serial)) return false;
 return InterlockedCompareExchange64(&t->outstanding,0,serial)==serial;
}
/* The caller supplies retirement proofs from the owners, never from publication
   zero alone. A LIVE receipt is known retired only after its source invalidates;
   monotonic generations and no owner revival make retired-through sufficient. */
static bool bp_transport_reclaimable(BpTransport* t,LONG own_generation,bool own_retiring,
 LONG live_retired_through) {
 if(bp_transport_blocked(t) || !bp_transport_pending(t)) return false;
 if(t->owner==BP_TRANSPORT_OWN) return own_retiring && own_generation>0 && t->generation==own_generation;
 return t->owner==BP_TRANSPORT_LIVE && t->generation>0 && t->generation<=live_retired_through;
}
