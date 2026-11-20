#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "eye_color_shaders.inc"
#include "../src/base_eye_output.h"
typedef HRESULT(WINAPI* CompileFn)(LPCVOID,SIZE_T,LPCSTR,const D3D_SHADER_MACRO*,ID3DInclude*,LPCSTR,LPCSTR,UINT,UINT,ID3DBlob**,ID3DBlob**);
static unsigned assertions;
#define CHECK(x) do{++assertions;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
 HMODULE compiler=LoadLibraryExW(L"d3dcompiler_47.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);CHECK(compiler);
 CompileFn compile=(CompileFn)(void*)GetProcAddress(compiler,"D3DCompile");CHECK(compile);
 const char* text[3]={g_eye_copy_vs_src,g_ps_src,g_panel_ps_src};ID3DBlob* blob[3]={0};
 for(unsigned i=0;i<3;++i){ID3DBlob* errors=NULL;HRESULT hr=compile(text[i],strlen(text[i]),NULL,NULL,NULL,"main",i?"ps_5_0":"vs_5_0",0,0,&blob[i],&errors);
  if(errors){printf("shader %s\n",(char*)ID3D10Blob_GetBufferPointer(errors));ID3D10Blob_Release(errors);}CHECK(SUCCEEDED(hr) && blob[i]);}
 ID3D11Device* device=NULL;ID3D11DeviceContext* context=NULL;D3D_FEATURE_LEVEL level,levels[]={D3D_FEATURE_LEVEL_11_0};
 HRESULT hr=D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,0,levels,1,D3D11_SDK_VERSION,&device,&level,&context);
 bool hardware=SUCCEEDED(hr);if(FAILED(hr)) hr=D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_WARP,NULL,0,levels,1,D3D11_SDK_VERSION,&device,&level,&context);
 CHECK(SUCCEEDED(hr) && device && context);
 ID3D11VertexShader* vs=NULL;ID3D11PixelShader *legacy=NULL,*display=NULL;
 CHECK(SUCCEEDED(ID3D11Device_CreateVertexShader(device,ID3D10Blob_GetBufferPointer(blob[0]),ID3D10Blob_GetBufferSize(blob[0]),NULL,&vs)));
 CHECK(SUCCEEDED(ID3D11Device_CreatePixelShader(device,ID3D10Blob_GetBufferPointer(blob[1]),ID3D10Blob_GetBufferSize(blob[1]),NULL,&legacy)));
 CHECK(SUCCEEDED(ID3D11Device_CreatePixelShader(device,ID3D10Blob_GetBufferPointer(blob[2]),ID3D10Blob_GetBufferSize(blob[2]),NULL,&display)));
 CHECK(bp_eye_output_shader(true,legacy,NULL)==NULL && bp_eye_output_shader(false,legacy,NULL)==legacy);
 const uint8_t value[9][3]={{0,0,0},{10,10,10},{11,11,11},{32,32,32},{128,128,128},{255,255,255},{255,0,0},{0,255,0},{0,0,255}};
 const uint8_t linear[9][3]={{0,0,0},{1,1,1},{1,1,1},{4,4,4},{55,55,55},{255,255,255},{255,0,0},{0,255,0},{0,0,255}};
 uint8_t pixels[2][9][4];for(unsigned y=0;y<2;++y)for(unsigned x=0;x<9;++x){unsigned i=y?8-x:x;memcpy(pixels[y][x],value[i],3);pixels[y][x][3]=(uint8_t)(x*23);}
 D3D11_TEXTURE2D_DESC d={0};d.Width=9;d.Height=2;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 D3D11_SUBRESOURCE_DATA initial={.pSysMem=pixels,.SysMemPitch=36};ID3D11Texture2D *source=NULL,*target=NULL,*staging=NULL;
 CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(device,&d,&initial,&source)));
 d.BindFlags=D3D11_BIND_RENDER_TARGET;CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(device,&d,NULL,&target)));
 d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(device,&d,NULL,&staging)));
 ID3D11ShaderResourceView* srv=NULL;ID3D11RenderTargetView* rtv=NULL;ID3D11SamplerState* sampler=NULL;
 CHECK(SUCCEEDED(ID3D11Device_CreateShaderResourceView(device,(ID3D11Resource*)source,NULL,&srv)));
 CHECK(SUCCEEDED(ID3D11Device_CreateRenderTargetView(device,(ID3D11Resource*)target,NULL,&rtv)));
 D3D11_SAMPLER_DESC sd={0};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
 CHECK(SUCCEEDED(ID3D11Device_CreateSamplerState(device,&sd,&sampler)));
 ID3D11DeviceContext_IASetInputLayout(context,NULL);ID3D11DeviceContext_IASetPrimitiveTopology(context,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 ID3D11DeviceContext_VSSetShader(context,vs,NULL,0);ID3D11DeviceContext_PSSetSamplers(context,0,1,&sampler);
 D3D11_VIEWPORT vp={0,0,9,2,0,1};ID3D11DeviceContext_RSSetViewports(context,1,&vp);
 unsigned owned_gray=0,legacy_gray=0;
 for(unsigned owned=0;owned<2;++owned) {
  ID3D11DeviceContext_PSSetShader(context,bp_eye_output_shader(owned!=0,legacy,display),NULL,0);
  ID3D11DeviceContext_PSSetShaderResources(context,0,1,&srv);ID3D11DeviceContext_OMSetRenderTargets(context,1,&rtv,NULL);ID3D11DeviceContext_Draw(context,3,0);
  ID3D11DeviceContext_OMSetRenderTargets(context,0,NULL,NULL);ID3D11DeviceContext_CopyResource(context,(ID3D11Resource*)staging,(ID3D11Resource*)target);
  D3D11_MAPPED_SUBRESOURCE map;CHECK(SUCCEEDED(ID3D11DeviceContext_Map(context,(ID3D11Resource*)staging,0,D3D11_MAP_READ,0,&map)));
  for(unsigned y=0;y<2;++y) for(unsigned x=0;x<9;++x){const uint8_t* p=(const uint8_t*)map.pData+(size_t)y*map.RowPitch+x*4;
   unsigned i=y?x:8-x;for(unsigned c=0;c<3;++c){int expected=owned?linear[i][c]:value[i][c];CHECK(abs((int)p[c]-expected)<=1);}CHECK(p[3]==255);
   if(i==4){if(owned) owned_gray=p[0];else legacy_gray=p[0];}
  }
  ID3D11DeviceContext_Unmap(context,(ID3D11Resource*)staging,0);
 }
 printf("PASS eye color: %u assertions / own128=%u legacy128=%u / hardware=%s / exact production shaders+selector, unchanged Yflip\n",assertions,owned_gray,legacy_gray,hardware?"true":"false");
 ID3D11DeviceContext_ClearState(context);ID3D11SamplerState_Release(sampler);ID3D11RenderTargetView_Release(rtv);ID3D11ShaderResourceView_Release(srv);
 ID3D11Texture2D_Release(staging);ID3D11Texture2D_Release(target);ID3D11Texture2D_Release(source);ID3D11PixelShader_Release(display);ID3D11PixelShader_Release(legacy);ID3D11VertexShader_Release(vs);
 for(unsigned i=0;i<3;++i)ID3D10Blob_Release(blob[i]);ID3D11DeviceContext_Release(context);ID3D11Device_Release(device);FreeLibrary(compiler);return 0;
}
