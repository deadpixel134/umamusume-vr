/* XR-thread only. Read a sparse grid from our own eye and desktop caches.
   Never read a runtime swapchain or wait on a shared game resource. */
#define PANEL_GRID 5
typedef struct MenuSample { unsigned peak,low,bright,dark; } MenuSample;
static struct {
 ID3D11Texture2D* pixels[3]; DXGI_FORMAT formats[3];
 bool pending,flat,forced,inspected; unsigned dark_pairs,visible_pairs;
 LONG generation,mirror; ULONGLONG next_copy,next_marker,last_mirror;
} menu_check;

static bool menu_black_pair(const MenuSample samples[3]){
 return samples[0].dark>=PANEL_GRID*PANEL_GRID-1 && samples[1].dark>=PANEL_GRID*PANEL_GRID-1 &&
        samples[2].bright>=3 && samples[2].peak>=samples[2].low+32;
}
static void menu_reset(void){
 for(unsigned i=0;i<3;i++)if(menu_check.pixels[i])ID3D11Texture2D_Release(menu_check.pixels[i]);
 ZeroMemory(&menu_check,sizeof menu_check);
}
static bool menu_copy_grid(unsigned i,ID3D11Texture2D* source){
 if(!source)return false;
 D3D11_TEXTURE2D_DESC d;ID3D11Texture2D_GetDesc(source,&d);
 switch(d.Format){
 case DXGI_FORMAT_R8G8B8A8_TYPELESS:case DXGI_FORMAT_R8G8B8A8_UNORM:case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
 case DXGI_FORMAT_B8G8R8A8_TYPELESS:case DXGI_FORMAT_B8G8R8A8_UNORM:case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:break;
 default:return false;
 }
 if(d.SampleDesc.Count!=1 || !d.Width || !d.Height)return false;
 if(menu_check.pixels[i] && menu_check.formats[i]!=d.Format){
  ID3D11Texture2D_Release(menu_check.pixels[i]);menu_check.pixels[i]=NULL;
 }
 if(!menu_check.pixels[i]){
  D3D11_TEXTURE2D_DESC staging={0};
  staging.Width=staging.Height=PANEL_GRID;staging.ArraySize=staging.MipLevels=staging.SampleDesc.Count=1;
  staging.Format=d.Format;staging.Usage=D3D11_USAGE_STAGING;staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  if(FAILED(ID3D11Device_CreateTexture2D(probe_device,&staging,NULL,&menu_check.pixels[i])))return false;
  menu_check.formats[i]=d.Format;
 }
 for(UINT y=0;y<PANEL_GRID;y++)for(UINT x=0;x<PANEL_GRID;x++){
  UINT sx=(2*x+1)*d.Width/(2*PANEL_GRID),sy=(2*y+1)*d.Height/(2*PANEL_GRID);
  D3D11_BOX box={sx,sy,0,sx+1,sy+1,1};
  ID3D11DeviceContext_CopySubresourceRegion(probe_context,(ID3D11Resource*)menu_check.pixels[i],0,x,y,0,(ID3D11Resource*)source,0,&box);
 }
 return true;
}
static HRESULT menu_read_grid(unsigned i,MenuSample* out){
 D3D11_MAPPED_SUBRESOURCE m={0};
 HRESULT hr=ID3D11DeviceContext_Map(probe_context,(ID3D11Resource*)menu_check.pixels[i],0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&m);
 if(hr!=S_OK)return hr;
 MenuSample s={.low=255};
 for(unsigned y=0;y<PANEL_GRID;y++)for(unsigned x=0;x<PANEL_GRID;x++){
  const uint8_t* rgb=(const uint8_t*)m.pData+y*m.RowPitch+4*x;
  unsigned v=rgb[0]>rgb[1]?rgb[0]:rgb[1];if(rgb[2]>v)v=rgb[2];
  if(v>s.peak)s.peak=v;if(v<s.low)s.low=v;if(v>=48)++s.bright;if(v<=12)++s.dark;
 }
 ID3D11DeviceContext_Unmap(probe_context,(ID3D11Resource*)menu_check.pixels[i],0);
 *out=s;return S_OK;
}
static void menu_marker(ULONGLONG now,BOOL stereo,BOOL opened){
 if(now<menu_check.next_marker)return;
 menu_check.next_marker=now+400;
 WCHAR path[MAX_PATH];wcscpy_s(path,MAX_PATH,log_path);WCHAR* leaf=wcsrchr(path,L'\\');
 if(!leaf || swprintf_s(leaf+1,MAX_PATH-(size_t)(leaf+1-path),L"menu-flat-%lu-%llu.ready",(unsigned long)GetCurrentProcessId(),(unsigned long long)telemetry_attach)<=0)return;
 DWORD flags=GetFileAttributesW(path);
 bool forced=flags!=INVALID_FILE_ATTRIBUTES && !(flags&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
 if(forced!=menu_check.forced){
  menu_check.forced=forced;menu_check.inspected=false;char msg[230];
  telemetry_snprintf(msg,sizeof msg,"\"stage\":\"menu_override_%s\",\"stereo_input\":%s,\"mirror_open\":%s,\"cache_ready\":%s",forced?"on":"off",stereo?"true":"false",opened?"true":"false",g_panel_frame_ready?"true":"false");
  write_record("XR-PANEL","presentation_fallback",msg);
 }
}
static bool menu_prefer_panel(BOOL stereo,BOOL opened,BOOL refreshed){
 ULONGLONG now=GetTickCount64();menu_marker(now,stereo,opened);
 LONG generation=xr_cache_telemetry.owned?xr_eye_cache_generation:0;
 if(menu_check.generation!=generation || menu_check.mirror!=xr_desktop_mirror_generation){
  menu_check.generation=generation;menu_check.mirror=xr_desktop_mirror_generation;
  menu_check.dark_pairs=menu_check.visible_pairs=0;menu_check.pending=false;menu_check.flat=false;menu_check.last_mirror=0;
 }
 if(refreshed)menu_check.last_mirror=now;
 if(!opened || !g_panel_frame_ready || !generation)return menu_check.forced && opened && g_panel_frame_ready;
 if(!stereo || now-menu_check.last_mirror>600){menu_check.dark_pairs=menu_check.visible_pairs=0;menu_check.pending=false;return menu_check.flat || menu_check.forced;}
 if(menu_check.pending){
  MenuSample samples[3];HRESULT hr=S_OK;
  for(unsigned i=0;i<3 && hr==S_OK;i++)hr=menu_read_grid(i,&samples[i]);
  if(hr!=DXGI_ERROR_WAS_STILL_DRAWING){
   menu_check.pending=false;
   if(hr==S_OK){
    if(menu_check.forced && !menu_check.inspected){
     char msg[250];telemetry_snprintf(msg,sizeof msg,"\"stage\":\"menu_inspect_pixels\",\"generation\":%ld,\"left_peak\":%u,\"right_peak\":%u,\"mirror_low\":%u,\"mirror_peak\":%u,\"mirror_bright\":%u",(long)generation,samples[0].peak,samples[1].peak,samples[2].low,samples[2].peak,samples[2].bright);
     write_record("XR-PANEL","presentation_fallback",msg);menu_check.inspected=true;
    }
    bool black=menu_black_pair(samples);
    menu_check.dark_pairs=black?(menu_check.dark_pairs<3?menu_check.dark_pairs+1:3):0;
    /* Sparse UI highlights and unavailable samples cannot establish world recovery. */
    bool visible=samples[0].bright>=PANEL_GRID && samples[1].bright>=PANEL_GRID;
    menu_check.visible_pairs=visible?(menu_check.visible_pairs<3?menu_check.visible_pairs+1:3):0;
    bool flat=menu_check.flat ? menu_check.visible_pairs<3 : menu_check.dark_pairs==3;
    if(flat!=menu_check.flat){
     char msg[220];telemetry_snprintf(msg,sizeof msg,"\"stage\":\"menu_pixels_%s\",\"generation\":%ld,\"left_peak\":%u,\"right_peak\":%u,\"mirror_peak\":%u",flat?"flat":"stereo",(long)generation,samples[0].peak,samples[1].peak,samples[2].peak);
     write_record("XR-PANEL","presentation_fallback",msg);
    }
    menu_check.flat=flat;
   }else{menu_check.visible_pairs=menu_check.dark_pairs=0;}
  }
 }
 if(!menu_check.pending && now>=menu_check.next_copy){
  menu_check.next_copy=now+200;
  menu_check.pending=menu_copy_grid(0,g_eye_cache_tex[0]) && menu_copy_grid(1,g_eye_cache_tex[1]) && menu_copy_grid(2,g_panel_tex);
  if(!menu_check.pending){menu_check.visible_pairs=menu_check.dark_pairs=0;}
 }
 return menu_check.flat || menu_check.forced;
}
