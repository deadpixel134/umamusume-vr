#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
static ID3D11Device* probe_device;static ID3D11DeviceContext* probe_context;
static ID3D11Texture2D* g_eye_cache_tex[2],*g_panel_tex;
static LONG xr_eye_cache_generation=1,xr_desktop_mirror_generation=1,g_panel_frame_ready=1;
static struct {bool owned;} xr_cache_telemetry={true};
static WCHAR log_path[MAX_PATH];static ULONGLONG telemetry_attach;
#define telemetry_snprintf snprintf
static void write_record(const char* group,const char* event,const char* msg){(void)group;(void)event;puts(msg);}
#include "../src/home_panel_visibility.h"
static void upload(ID3D11Texture2D* tex,bool pattern){
 uint32_t pixels[32*32];
 for(unsigned i=0;i<32*32;i++)pixels[i]=(pattern && i%32>=16)?0xffffffff:0xff000000;
 ID3D11DeviceContext_UpdateSubresource(probe_context,(ID3D11Resource*)tex,0,NULL,pixels,32*4,0);
}
static bool sample(void){
 menu_check.next_copy=0;
 (void)menu_prefer_panel(TRUE,TRUE,TRUE);
 ID3D11DeviceContext_Flush(probe_context);
 for(int i=0;menu_check.pending && i<500;i++){Sleep(1);menu_check.next_copy=GetTickCount64()+1000;(void)menu_prefer_panel(TRUE,TRUE,TRUE);}
 assert(!menu_check.pending);return menu_check.flat;
}
int main(void){
 assert(SUCCEEDED(D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,0,NULL,0,D3D11_SDK_VERSION,&probe_device,NULL,&probe_context)));
 D3D11_TEXTURE2D_DESC d={.Width=32,.Height=32,.MipLevels=1,.ArraySize=1,.Format=DXGI_FORMAT_R8G8B8A8_UNORM,.SampleDesc={1,0},.Usage=D3D11_USAGE_DEFAULT,.BindFlags=D3D11_BIND_SHADER_RESOURCE};
 ID3D11Texture2D** images[]={&g_eye_cache_tex[0],&g_eye_cache_tex[1],&g_panel_tex};
 for(int i=0;i<3;i++){assert(SUCCEEDED(ID3D11Device_CreateTexture2D(probe_device,&d,NULL,images[i])));upload(*images[i],i==2);}
 /* One bright UI sample in each otherwise black eye. */
 uint32_t white=0xffffffff;
 D3D11_BOX dot={16,16,0,17,17,1};
 for(int i=0;i<2;i++)ID3D11DeviceContext_UpdateSubresource(probe_context,(ID3D11Resource*)g_eye_cache_tex[i],0,&dot,&white,4,0);
 assert(!sample());assert(!sample());assert(sample());
 upload(g_eye_cache_tex[1],true);assert(sample());
 menu_check.last_mirror=GetTickCount64()-601;assert(menu_prefer_panel(TRUE,TRUE,FALSE));
 assert(menu_prefer_panel(FALSE,TRUE,TRUE));
 upload(g_eye_cache_tex[0],true);assert(sample());assert(sample());assert(!sample());
 upload(g_eye_cache_tex[0],false);
 upload(g_eye_cache_tex[1],false);upload(g_panel_tex,false);
 for(int i=0;i<4;i++)assert(!sample());
 upload(g_panel_tex,true);assert(!sample());assert(!sample());assert(sample());
 xr_eye_cache_generation++;assert(!sample());
 xr_cache_telemetry.owned=false;assert(!menu_prefer_panel(TRUE,TRUE,TRUE));
 menu_reset();for(int i=0;i<3;i++)ID3D11Texture2D_Release(*images[i]);
 ID3D11DeviceContext_Release(probe_context);ID3D11Device_Release(probe_device);puts("PASS hardware readback, hysteresis, visible eye, black mirror, generation reset and source isolation");return 0;
}
