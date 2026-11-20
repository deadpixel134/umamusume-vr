#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include "../src/base_producer_state.h"
#include "../src/base_consumer_lease.h"
#include "../src/base_eye_output.h"
typedef struct CacheMeta {BOOL owned;LONG generation;uint64_t source,pose,config,capture_tick;int32_t scene;LONG64 pair_serial;} CacheMeta;
static ULONGLONG xr_live_cache_produced_tick;
static volatile LONG live_retired_through,source_generation;
static bool bp_transport;
static bool bp_transport_blocked(const bool* blocked){return *blocked;}
static CacheMeta xr_cache_telemetry;static struct {uint64_t frame_id;} xr_frame_telemetry;
static struct {BpControl c;} bp;
static BpConsumerLease bp_consumer_lease;
static SRWLOCK bp_lock;static volatile LONG published_eye_generation,bp_published_generation;
static LONG xr_eye_cache_generation,xr_active_source_generation;static uint64_t immersive_frames_total;
static unsigned g_proj_image_count=2,g_eye_width=16,g_eye_height=16;
static ID3D11PixelShader *g_ps,*g_panel_ps;static ID3D11VertexShader* g_eye_copy_vs;static ID3D11SamplerState* g_sampler;
static ID3D11Texture2D* g_eye_cache_tex[2];static ID3D11ShaderResourceView* g_eye_cache_srv[2];
static ID3D11RenderTargetView* g_proj_rtvs[2][2];static ID3D11DeviceContext* probe_context;
static bool lock_available,locked_result,reused;static unsigned draws,body_calls,releases,guard_mode;
static uint64_t now;static ID3D11PixelShader* chosen;
static int telemetry_snprintf(char* out,size_t cap,const char* fmt,...) {va_list args;va_start(args,fmt);int n=vsnprintf(out,cap,fmt,args);va_end(args);return n;}
static void write_record(const char* a,const char* b,const char* c){(void)a;(void)b;(void)c;}
static void stereo_telemetry_detail(const void* a,int b,char* out,size_t cap){(void)a;(void)b;snprintf(out,cap,"\"fixture\":true");}
static void stereo_observe_draw(void* f,const CacheMeta* c,unsigned index,BOOL reuse){(void)f;(void)c;(void)index;reused=reuse!=FALSE;}
static void noop(void* ctx,...){(void)ctx;}
static void fx_ps(ID3D11DeviceContext* ctx,ID3D11PixelShader* ps,ID3D11ClassInstance* const* instances,UINT n){(void)ctx;(void)instances;(void)n;chosen=ps;}
static void fx_srv(ID3D11DeviceContext* ctx,UINT start,UINT n,ID3D11ShaderResourceView* const* views) {
 (void)ctx;(void)start;(void)n;
 if(views && views[0] && guard_mode>0 && guard_mode<3) {
  bp_lease_revoke(&bp_consumer_lease,1);
  if(guard_mode==2){BpIdentity id={.generation=1,.source=2,.scene=3,.pose=4,.config=5};(void)bp_lease_publish(&bp_consumer_lease,&id,false);}
  if(guard_mode<3) guard_mode=0;
 }
}
static void fx_draw(ID3D11DeviceContext* ctx,UINT n,UINT first){(void)ctx;if(n!=3 || first) abort();++draws;if(guard_mode==3)bp_lease_revoke(&bp_consumer_lease,1);}
static BOOLEAN fx_try(PSRWLOCK lock){(void)lock;return lock_available?TRUE:FALSE;}
static void fx_release(PSRWLOCK lock){(void)lock;++releases;}
static ULONGLONG fx_tick(void){return now;}
static BOOL copy_fresh_eye_pair_to_projection_body(uint32_t index){(void)index;++body_calls;return locked_result?TRUE:FALSE;}
#undef ID3D11DeviceContext_RSSetViewports
#undef ID3D11DeviceContext_IASetInputLayout
#undef ID3D11DeviceContext_IASetPrimitiveTopology
#undef ID3D11DeviceContext_VSSetShader
#undef ID3D11DeviceContext_PSSetShader
#undef ID3D11DeviceContext_PSSetSamplers
#undef ID3D11DeviceContext_OMSetRenderTargets
#undef ID3D11DeviceContext_PSSetShaderResources
#undef ID3D11DeviceContext_Draw
#define ID3D11DeviceContext_RSSetViewports noop
#define ID3D11DeviceContext_IASetInputLayout noop
#define ID3D11DeviceContext_IASetPrimitiveTopology noop
#define ID3D11DeviceContext_VSSetShader noop
#define ID3D11DeviceContext_PSSetShader fx_ps
#define ID3D11DeviceContext_PSSetSamplers noop
#define ID3D11DeviceContext_OMSetRenderTargets noop
#define ID3D11DeviceContext_PSSetShaderResources fx_srv
#define ID3D11DeviceContext_Draw fx_draw
#define TryAcquireSRWLockShared fx_try
#define ReleaseSRWLockShared fx_release
#define GetTickCount64 fx_tick
#include "cached_eye_draw.inc"
static unsigned cases,assertions;
#define CHECK(x) do{++assertions;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
static void reset(void) {
 ZeroMemory(&bp_consumer_lease,sizeof(bp_consumer_lease));ZeroMemory(&bp,sizeof(bp));
 xr_cache_telemetry=(CacheMeta){.owned=TRUE,.generation=1,.source=2,.scene=3,.pose=4,.config=5,.capture_tick=1000,.pair_serial=1};
 BpIdentity id={.generation=1,.source=2,.scene=3,.pose=4,.config=5};(void)bp_lease_publish(&bp_consumer_lease,&id,false);
 published_eye_generation=bp_published_generation=xr_eye_cache_generation=xr_active_source_generation=1;
 bp.c.phase=BP_ACTIVE;bp.c.identity=id;lock_available=locked_result=reused=false;draws=body_calls=releases=guard_mode=0;now=1200;
 g_ps=(ID3D11PixelShader*)(uintptr_t)1;g_panel_ps=(ID3D11PixelShader*)(uintptr_t)2;g_eye_copy_vs=(ID3D11VertexShader*)(uintptr_t)3;
 g_sampler=(ID3D11SamplerState*)(uintptr_t)4;probe_context=(ID3D11DeviceContext*)(uintptr_t)5;chosen=NULL;
 for(unsigned eye=0;eye<2;++eye){g_eye_cache_tex[eye]=(ID3D11Texture2D*)(uintptr_t)6;g_eye_cache_srv[eye]=(ID3D11ShaderResourceView*)(uintptr_t)7;for(unsigned i=0;i<2;++i)g_proj_rtvs[i][eye]=(ID3D11RenderTargetView*)(uintptr_t)8;}
}
int main(void) {
 reset();CHECK(copy_fresh_eye_pair_to_projection(0));CHECK(draws==2 && chosen==g_panel_ps && reused && !body_calls && !releases);++cases;
 for(unsigned mode=0;mode<10;++mode) {
  reset();switch(mode){case 0:bp_lease_revoke(&bp_consumer_lease,1);break;case 1:published_eye_generation=0;break;
   case 2:now=1501;break;case 3:xr_cache_telemetry.scene=99;break;case 4:g_panel_ps=NULL;break;
   case 5:g_eye_cache_srv[1]=NULL;break;case 6:g_proj_rtvs[0][1]=NULL;break;case 7:xr_eye_cache_generation=2;break;
   case 8:guard_mode=1;break;default:guard_mode=2;break;}
  CHECK(!copy_fresh_eye_pair_to_projection(0));CHECK(!draws && !body_calls && !releases);++cases;
 }
 reset();CHECK(!copy_fresh_eye_pair_to_projection(2) && !draws);++cases;
 reset();guard_mode=3;CHECK(copy_fresh_eye_pair_to_projection(0) && draws==2);draws=0;
 CHECK(!copy_fresh_eye_pair_to_projection(0) && !draws);++cases; /* authorized pair may complete; next frame revoked */
 for(unsigned variant=0;variant<9;++variant){
  reset();xr_cache_telemetry.owned=FALSE;bp_published_generation=0;source_generation=1;
  live_retired_through=0;bp_transport=false;xr_live_cache_produced_tick=1000;now=1500;
  switch(variant){case 1:now=1501;break;case 2:now=999;break;case 3:live_retired_through=1;break;
   case 4:bp_transport=true;break;case 5:source_generation=2;break;case 6:published_eye_generation=0;break;
   case 7:bp_published_generation=1;break;case 8:xr_live_cache_produced_tick=0;break;}
  CHECK(copy_fresh_eye_pair_to_projection(0)==(variant==0));
  CHECK(draws==(variant==0?2u:0u) && !body_calls && !releases);++cases;
 }
 lock_available=locked_result=true;CHECK(copy_fresh_eye_pair_to_projection(0) && body_calls==1 && releases==1 && !draws);++cases;
 printf("PASS cached draw: %u cases / %u assertions; actual extracted cache helper+wrapper, substituted GPU/lock boundary\n",cases,assertions);return 0;
}
