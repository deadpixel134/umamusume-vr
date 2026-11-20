/* Actual reset and reclaim adapters, with scripted zero-wait keyed mutexes. */
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/base_producer_state.h"
#include "../src/base_source_capability.h"
#include "../src/base_transport.h"
typedef struct IDXGIKeyedMutex { UINT64 key;bool held;HRESULT forced,release_result;unsigned acquires,releases; } IDXGIKeyedMutex;
static HRESULT acquire(IDXGIKeyedMutex* m,UINT64 key,DWORD wait) {
 ++m->acquires;if(wait) return E_INVALIDARG;if(m->forced!=S_OK) return m->forced;
 if(m->held || m->key!=key) return WAIT_TIMEOUT;m->held=true;return S_OK;
}
static HRESULT release(IDXGIKeyedMutex* m,UINT64 key) {
 ++m->releases;if(!m->held || m->release_result!=S_OK) return E_FAIL;m->held=false;m->key=key;return S_OK;
}
#define IDXGIKeyedMutex_AcquireSync acquire
#define IDXGIKeyedMutex_ReleaseSync release
#include "../src/base_keyed_mutex.h"
static struct {
 BpControl c;bool clean_retired,failed,disable_done,destroy_called,witness_saved,backend_verified;
 unsigned root_count;void* native[2];void* readback[2];void* sentinel_view[2];void* fence;void* gpu_device;
 uint32_t roots[24],source_root,source_transform_root,tag_root;int32_t source_pose_frame;
 unsigned eye[2],background,optics;BpEmpty empty;const char* stage;const char* error;
 BpSourceKind source_kind;LONG width,height;float scale;bool post;ULONGLONG observed_tick;
 DWORD present_thread;uint64_t last_committed_tick;
 /* Persistent production fields represented by sentinels. */
 void* method;void* api;void* types;void* fov_field;DWORD owner_thread;bool bound;
} bp;
static BpTransport bp_transport;
static LONG bp_stop_requested,live_retired_through;
static struct {bool attempted;unsigned original_mask,write_mask;} bp_fov;
static unsigned events,failures,cases,checks;
static IDXGIKeyedMutex mutexes[2],*shared_eye_km_game[2];
static void bp_context_event(const char* a,const char* b) {(void)a;(void)b;++events;}
static void bp_fail(const char* reason) {(void)reason;++failures;bp.failed=true;bp_retire(&bp.c,true);}
#include "../src/base_generation_rearm.h"
#include "transport_reclaim.inc"
#define CHECK(x) do {++checks;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);return 1;}} while(0)
static void reset(void) {
 ++cases;ZeroMemory(&bp,sizeof(bp));ZeroMemory(&bp_transport,sizeof(bp_transport));
 ZeroMemory(mutexes,sizeof(mutexes));ZeroMemory(&bp_fov,sizeof(bp_fov));
 bp_stop_requested=live_retired_through=0;events=failures=0;
 shared_eye_km_game[0]=&mutexes[0];shared_eye_km_game[1]=&mutexes[1];
}
static void clean(void) {
 reset();bp.c.phase=BP_RETIRED;bp.c.attempted=bp.c.retiring=bp.c.disabled=true;bp.clean_retired=true;
 bp.c.identity=(BpIdentity){7,8,7,1,23,9,10};bp.c.pending=bp.c.identity;
 bp.source_root=bp.source_transform_root=bp.tag_root=77;bp.source_pose_frame=123;
 bp.eye[0]=bp.eye[1]=bp.background=bp.optics=44;bp.empty.known=3;bp.empty.finished=true;
 bp.disable_done=bp.destroy_called=bp.witness_saved=bp.backend_verified=true;
 bp.stage=bp.error="old";bp.source_kind=BP_SOURCE_STORY;bp.width=2496;bp.height=2688;
 bp.scale=2;bp.post=true;bp.observed_tick=99;bp.present_thread=66;bp.last_committed_tick=88;
 bp.method=(void*)1;bp.api=(void*)2;bp.types=(void*)3;bp.fov_field=(void*)4;bp.owner_thread=55;bp.bound=true;
 bp_fov.attempted=true;bp_fov.original_mask=bp_fov.write_mask=3;
}
static bool receipt(LONG owner,LONG gen,LONG64 serial) {
 mutexes[0].key=mutexes[1].key=1;return bp_transport_begin(&bp_transport,owner,gen,serial);
}
int main(void) {
 CHECK(bp_story_component_set(15,0,true,true,true,true));
 clean();CHECK(bp_rearm());CHECK(bp.c.phase==BP_NEW && !bp.c.attempted && !bp.clean_retired && !bp.failed);
 CHECK(!bp.source_root && !bp.source_transform_root && !bp.tag_root && bp.source_pose_frame==-1);
 CHECK(!bp.eye[0] && !bp.eye[1] && !bp.empty.known && !bp.background && !bp.optics && !bp_fov.attempted && !bp_fov.write_mask);
 CHECK(!bp.stage && !bp.error && bp.source_kind==BP_SOURCE_NONE && !bp.width && !bp.height && !bp.scale && !bp.post);
 CHECK(!bp.observed_tick && !bp.present_thread && !bp.last_committed_tick && !bp.disable_done && !bp.destroy_called && !bp.witness_saved && !bp.backend_verified);
 CHECK(bp.method==(void*)1 && bp.api==(void*)2 && bp.types==(void*)3 && bp.fov_field==(void*)4 && bp.owner_thread==55 && bp.bound);
 CHECK(events==1 && !bp_rearm());CHECK(bp_begin(&bp.c,(BpIdentity){8,18,8,1,0,11,0}));
 for(unsigned fail=0;fail<13;++fail) {
  clean();switch(fail) {
   case 0:bp.clean_retired=false;break;case 1:bp.c.quarantined=true;break;
   case 2:bp.c.query_failed=true;break;case 3:bp.c.query=BP_Q_PAIR;break;
   case 4:bp.c.mutex_owned=1;break;case 5:bp.failed=true;break;case 6:bp_stop_requested=1;break;
   case 7:bp.root_count=1;break;case 8:bp.native[0]=(void*)1;break;
   case 9:bp.readback[1]=(void*)1;break;case 10:bp.fence=(void*)1;break;
   case 11:CHECK(receipt(BP_TRANSPORT_OWN,7,20));break;case 12:bp_transport_quarantine(&bp_transport);break;
  }
  CHECK(!bp_rearm());CHECK(bp.c.identity.generation==7 && bp_fov.attempted && !events);
 }
 reset();CHECK(receipt(BP_TRANSPORT_OWN,7,20));bp.c.identity.generation=7;bp.c.retiring=true;
 CHECK(bp_reclaim_transport());CHECK(!bp_transport_pending(&bp_transport) && !mutexes[0].held && !mutexes[1].held && !mutexes[0].key && !mutexes[1].key && !failures);
 reset();CHECK(receipt(BP_TRANSPORT_OWN,7,20));bp.c.identity.generation=8;bp.c.retiring=true;
 CHECK(!bp_reclaim_transport() && !mutexes[0].acquires && bp_transport_pending(&bp_transport)==20);
 reset();CHECK(receipt(BP_TRANSPORT_OWN,7,20));bp.c.identity.generation=7;
 CHECK(!bp_reclaim_transport() && !mutexes[0].acquires);
 reset();CHECK(receipt(BP_TRANSPORT_LIVE,6,30));live_retired_through=6;
 CHECK(bp_reclaim_transport() && !bp_transport_pending(&bp_transport));
 reset();CHECK(receipt(BP_TRANSPORT_LIVE,8,31));live_retired_through=6;
 CHECK(!bp_reclaim_transport() && !mutexes[0].acquires);
 reset();CHECK(receipt(BP_TRANSPORT_LIVE,6,30));CHECK(bp_transport_ack(&bp_transport,BP_TRANSPORT_LIVE,6,30));
 CHECK(receipt(BP_TRANSPORT_OWN,7,31));CHECK(!bp_transport_ack(&bp_transport,BP_TRANSPORT_LIVE,6,30));
 CHECK(bp_transport_pending(&bp_transport)==31 && !bp_transport_begin(&bp_transport,BP_TRANSPORT_LIVE,8,32));
 for(unsigned eye=0;eye<2;++eye) {
  reset();CHECK(receipt(BP_TRANSPORT_OWN,7,20));bp.c.identity.generation=7;bp.c.retiring=true;mutexes[eye].forced=WAIT_TIMEOUT;
  CHECK(!bp_reclaim_transport() && !bp_transport_blocked(&bp_transport) && bp_transport_pending(&bp_transport)==20);
  CHECK(!mutexes[0].held && !mutexes[1].held && mutexes[0].key==1 && mutexes[1].key==1);
  mutexes[eye].forced=S_OK;CHECK(bp_reclaim_transport());
 }
 for(unsigned eye=0;eye<2;++eye) {
  reset();CHECK(receipt(BP_TRANSPORT_OWN,7,20));bp.c.identity.generation=7;bp.c.retiring=true;mutexes[eye].forced=WAIT_ABANDONED;
  CHECK(!bp_reclaim_transport() && bp_transport_blocked(&bp_transport) && failures==1);
 }
 reset();CHECK(receipt(BP_TRANSPORT_OWN,7,20));bp.c.identity.generation=7;bp.c.retiring=true;
 mutexes[1].forced=WAIT_TIMEOUT;mutexes[0].release_result=E_FAIL;
 CHECK(!bp_reclaim_transport() && bp_transport_blocked(&bp_transport) && mutexes[0].held);
 reset();CHECK(receipt(BP_TRANSPORT_OWN,7,20));bp.c.identity.generation=7;bp.c.retiring=true;
 mutexes[1].release_result=E_FAIL;CHECK(!bp_reclaim_transport() && bp_transport_blocked(&bp_transport));
 reset();CHECK(receipt(BP_TRANSPORT_OWN,7,20));bp.c.identity.generation=7;bp.c.retiring=true;shared_eye_km_game[1]=NULL;
 CHECK(!bp_reclaim_transport() && bp_transport_blocked(&bp_transport));
 /* Live->Story: allocate OWN generation before old LIVE invalidation. Old
    textures must never receive the fresh global token or an unreclaimable debt. */
 reset();LONG authored=6,global=6;CHECK(bp_transport_live_current(authored,global,true));
 ++global;CHECK(!bp_transport_live_current(authored,global,true));
 CHECK(!bp_transport_live_current(global,global,false));CHECK(!bp_transport_pending(&bp_transport));
 CHECK(receipt(BP_TRANSPORT_LIVE,authored,41));live_retired_through=authored;
 CHECK(bp_reclaim_transport());CHECK(!bp_transport_pending(&bp_transport));
 /* Post-disable drain cannot advance while pending; only explicit completion
    plus native-gone supplies the clean retirement proof. */
 reset();bp.c.attempted=bp.c.retiring=bp.c.disabled=true;bp.c.phase=BP_RETIRING;
 CHECK(bp_arm(&bp.c,BP_Q_DRAIN,0,5));CHECK(bp_complete(&bp.c,0)==BP_Q_NONE && bp.c.query==BP_Q_DRAIN);
 CHECK(!bp_clean_rearm_allowed(&bp.c,true,false,false,0,false,false,false));
 CHECK(bp_complete(&bp.c,1)==BP_Q_DRAIN && bp.c.phase==BP_DESTROY_READY);
 bp.c.phase=BP_DESTROY_PENDING;CHECK(!bp_destroy_observed(&bp.c,true));CHECK(bp_destroy_observed(&bp.c,false));
 CHECK(!bp_rearm());bp.clean_retired=true;CHECK(bp_rearm());
 printf("PASS actual generation reset/transport: %u cases %u assertions; no GPU/Unity executed\n",cases,checks);return 0;
}
