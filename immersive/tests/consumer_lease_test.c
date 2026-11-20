#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../src/base_producer_state.h"
#include "../src/base_consumer_lease.h"
static unsigned assertions,cases;
#define CHECK(x) do{++assertions;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
static BpConsumerLease lease;static volatile LONG published,owned;static volatile LONG inconsistent;
static BpIdentity id(uint64_t gen) {return(BpIdentity){.generation=gen,.source=gen*2,.scene=-(int32_t)gen,.pose=gen,.config=gen+1};}
static DWORD WINAPI writer(LPVOID p) {
 (void)p;for(uint64_t i=1;i<=500;++i){BpIdentity value=id(i);(void)bp_lease_publish(&lease,&value,false);}return 0;
}
int main(void) {
 BpLeaseSnapshot s;BpIdentity a=id(1);CHECK(!bp_lease_snapshot(&lease,&s));CHECK(bp_lease_publish(&lease,&a,false));
 CHECK(bp_lease_snapshot(&lease,&s));LONG64 sequence=s.sequence;CHECK(sequence==2 && bp_lease_publish(&lease,&a,false) && bp_lease_load64(&lease.sequence)==sequence);
 published=owned=1;CHECK(bp_lease_authorize(&lease,&s,&a,1000,1200,&published,&owned));++cases;
 CHECK(bp_lease_authorize(&lease,&s,&a,1000,1500,&published,&owned));CHECK(!bp_lease_authorize(&lease,&s,&a,1000,1501,&published,&owned));
 CHECK(!bp_lease_authorize(&lease,&s,&a,1000,999,&published,&owned));CHECK(!bp_lease_authorize(&lease,&s,&a,0,1000,&published,&owned));++cases;
 /* A newer producer tick is deliberately not an input: only this cached
    pair's capture tick authorizes the reused image. */
 uint64_t newer_producer_tick=2000;CHECK(!bp_lease_authorize(&lease,&s,&a,1000,newer_producer_tick,&published,&owned));++cases;
 for(unsigned field=0;field<5;++field){BpIdentity b=a;switch(field){case 0:++b.generation;break;case 1:++b.source;break;case 2:++b.scene;break;case 3:++b.pose;break;default:++b.config;break;}
  CHECK(!bp_lease_authorize(&lease,&s,&b,1000,1200,&published,&owned));++cases;}
 published=0;CHECK(!bp_lease_authorize(&lease,&s,&a,1000,1200,&published,&owned));published=1;owned=2;
 CHECK(!bp_lease_authorize(&lease,&s,&a,1000,1200,&published,&owned));owned=1;++cases;
 bp_lease_revoke(&lease,1);CHECK(!bp_lease_snapshot(&lease,&s));CHECK(!bp_lease_publish(&lease,&a,true) && !bp_lease_load64(&lease.active));++cases;
 CHECK(bp_lease_publish(&lease,&a,false) && bp_lease_snapshot(&lease,&s));bp_lease_revoke(&lease,1);
 CHECK(!bp_lease_authorize(&lease,&s,&a,1000,1200,&published,&owned));
 CHECK(bp_lease_publish(&lease,&a,false));CHECK(!bp_lease_authorize(&lease,&s,&a,1000,1200,&published,&owned));++cases;
 BpIdentity b=id(2);CHECK(bp_lease_publish(&lease,&b,false));bp_lease_revoke(&lease,1);CHECK(bp_lease_load64(&lease.active)==2);++cases;
 CHECK(bp_lease_snapshot(&lease,&s));InterlockedExchange64(&lease.sequence,s.sequence+1);CHECK(!bp_lease_snapshot(&lease,&s));++cases;
 InterlockedExchange64(&lease.sequence,INT64_MAX-1);CHECK(!bp_lease_publish(&lease,&a,false) && !bp_lease_load64(&lease.active));++cases;
 ZeroMemory(&lease,sizeof(lease));HANDLE t=CreateThread(NULL,0,writer,NULL,0,NULL);CHECK(t!=NULL);
 for(unsigned i=0;i<5000;++i) if(bp_lease_snapshot(&lease,&s)) {
  BpIdentity expected=id(s.identity.generation);if(!bp_identity_equal(&s.identity,&expected)) InterlockedExchange(&inconsistent,1);
 }
 CHECK(WaitForSingleObject(t,5000)==WAIT_OBJECT_0);CloseHandle(t);CHECK(!inconsistent && bp_lease_snapshot(&lease,&s) && s.identity.generation==500);++cases;
 printf("PASS consumer lease: %u cases / %u assertions; actual Interlocked concurrent snapshot and revocation\n",cases,assertions);return 0;
}
