#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../src/base_producer_state.h"
#include "../src/base_transport.h"
enum { BP_KM_BROKEN=-1,BP_KM_PENDING=0,BP_KM_READY=1 };
typedef struct Meta {BOOL owned;LONG generation;uint64_t source,pose,config,capture_tick;int32_t scene;LONG64 pair_serial;} Meta;
static Meta game_pair_telemetry,xr_cache_telemetry;
static struct {BpControl c;} bp;
static BpTransport bp_transport;
static volatile LONG published_eye_generation,bp_published_generation;
static volatile LONG64 eye_pair_tick;
static LONG xr_active_source_generation,xr_eye_cache_generation;
static UINT g_proj_image_count=2;
static ID3D11Texture2D *g_eye_cache_tex[2],*g_local_eye_tex[2];
static IDXGIKeyedMutex* g_local_eye_km[2];
static ID3D11DeviceContext* probe_context;
static uint64_t now;
static int acquire_state,release_failure,calls,releases,copies,draws,stops;
static BOOL open_eye_pair_on_probe_device(void){return TRUE;}
static void write_record(const char*a,const char*b,const char*c){(void)a;(void)b;(void)c;}
static int bp_keyed_pair_acquire(IDXGIKeyedMutex*a,IDXGIKeyedMutex*b,UINT64 key,HRESULT*r){(void)a;(void)b;(void)r;if(key!=1)abort();calls++;return acquire_state;}
static HRESULT release_pair(IDXGIKeyedMutex*p,UINT64 k){(void)p;if(k)abort();releases++;return release_failure?E_FAIL:S_OK;}
static void copy_resource(ID3D11DeviceContext*c,ID3D11Resource*a,ID3D11Resource*b){(void)c;(void)a;(void)b;copies++;}
static void bp_consumer_stop(LONG g){(void)g;stops++;}
static void refresh_flat_cache_for_generation(LONG g){(void)g;}
static BOOL draw_cached_eye_pair_to_projection(uint32_t i,LONG g,BOOL reuse){(void)i;(void)g;(void)reuse;draws++;return TRUE;}
static ULONGLONG clock_now(void){return now;}
#undef IDXGIKeyedMutex_ReleaseSync
#undef ID3D11DeviceContext_CopyResource
#define IDXGIKeyedMutex_ReleaseSync release_pair
#define ID3D11DeviceContext_CopyResource copy_resource
#define GetTickCount64 clock_now
#include "stale_transport_body.inc"
static int assertions,cases;
#define CHECK(x) do{assertions++;if(!(x)){printf("FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static void reset(BOOL owned){ZeroMemory(&bp_transport,sizeof(bp_transport));ZeroMemory(&bp,sizeof(bp));calls=releases=copies=draws=stops=0;release_failure=0;acquire_state=BP_KM_READY;now=1750;eye_pair_tick=1000;published_eye_generation=bp_published_generation=1;xr_active_source_generation=xr_eye_cache_generation=0;game_pair_telemetry=(Meta){owned,1,2,3,4,1000,5,1};xr_cache_telemetry=(Meta){0};bp.c.phase=BP_ACTIVE;bp.c.enabled=bp.c.witness=3;bp.c.identity=(BpIdentity){.generation=1,.source=2,.pose=3,.config=4,.scene=5};if(!bp_transport_begin(&bp_transport,owned?BP_TRANSPORT_OWN:BP_TRANSPORT_LIVE,1,1))abort();}
int main(void){
 for(int owned=0;owned<2;owned++){
  reset(owned);cases++;CHECK(!copy_fresh_eye_pair_to_projection_body(0));CHECK(releases==2&&copies==2&&draws==0);CHECK(bp_transport_pending(&bp_transport)==0);CHECK(!bp_transport_blocked(&bp_transport));
  cases++;eye_pair_tick=game_pair_telemetry.capture_tick=now;game_pair_telemetry.pair_serial=2;CHECK(bp_transport_begin(&bp_transport,owned?1:2,1,2));CHECK(copy_fresh_eye_pair_to_projection_body(0));CHECK(draws==1&&releases==4&&!bp_transport_pending(&bp_transport));
 }
 reset(FALSE);now=1500;cases++;CHECK(copy_fresh_eye_pair_to_projection_body(0));CHECK(draws==1);
 reset(FALSE);now=1501;cases++;CHECK(!copy_fresh_eye_pair_to_projection_body(0));CHECK(!draws&&releases==2);
 reset(TRUE);acquire_state=BP_KM_PENDING;cases++;CHECK(!copy_fresh_eye_pair_to_projection_body(0));CHECK(!releases&&bp_transport_pending(&bp_transport)==1);
 reset(TRUE);game_pair_telemetry.pair_serial=9;cases++;CHECK(!copy_fresh_eye_pair_to_projection_body(0));CHECK(!calls&&!releases);
 reset(TRUE);release_failure=1;cases++;CHECK(!copy_fresh_eye_pair_to_projection_body(0));CHECK(bp_transport_blocked(&bp_transport)&&!draws&&stops==1);
 reset(TRUE);acquire_state=BP_KM_BROKEN;cases++;CHECK(!copy_fresh_eye_pair_to_projection_body(0));CHECK(bp_transport_blocked(&bp_transport)&&!releases);
 reset(FALSE);now=900;cases++;CHECK(!copy_fresh_eye_pair_to_projection_body(0));CHECK(!draws&&!bp_transport_pending(&bp_transport));
 CHECK(!bp_transport_live_current(1,2,true));CHECK(!bp_transport_reclaimable(&bp_transport,1,true,1));
 printf("PASS stale_transport cases=%d assertions=%d\n",cases,assertions);return 0;
}

