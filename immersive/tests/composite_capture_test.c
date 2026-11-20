#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "../src/base_producer_state.h"
#define UMAVR_BUILD_ID "source-FIXTURE"
static WCHAR log_path[MAX_PATH];static uint64_t telemetry_attach;
static int telemetry_snprintf(char* out,size_t cap,const char* format,...) {
 va_list args;va_start(args,format);int n=vsnprintf(out,cap,format,args);va_end(args);return n;
}
static unsigned summaries;static bool log_room=true;
static bool bp_projection_normal_record(const char* event,const char* detail) {
 if(strcmp(event,"original_composite_baseline") || strlen(detail)>=512) abort();
 ++summaries;printf("SUMMARY {%s}\n",detail);return log_room;
}
typedef struct FxDevice { unsigned refs; } FxDevice;
typedef struct FxTexture { unsigned refs;FxDevice* owner;D3D11_TEXTURE2D_DESC desc;uint8_t data[24]; } FxTexture;
typedef struct FxQuery { unsigned refs; } FxQuery;
typedef struct FxContext { unsigned refs;FxDevice* owner; } FxContext;
static FxDevice device,other_device;static FxTexture backbuffer,staging;static FxQuery query;
static FxContext context;
static unsigned texture_creates,query_creates,copies,ends,get_data_calls,maps,unmaps,texture_releases,query_releases;
static unsigned allocations,frees,module_refs,module_frees,thread_calls,worker_exits,writes,file_closes,queue_jobs;
static unsigned short_write_at;static bool create_texture_fail,create_query_fail,module_fail,thread_fail,alloc_fail;
static bool bad_pointer,short_pitch,close_image_fail,violation,complete_value,context_deferred;
static HRESULT query_hr,map_hr;static unsigned order,product_end_order,copy_order,end_order;
static WCHAR exported_image[MAX_PATH],exported_sidecar[MAX_PATH];static uint8_t exported_pixels[16];
static uint64_t exported_enqueue,exported_completion;static BpIdentity exported_identity;
static void fx_texture_desc(ID3D11Texture2D*,D3D11_TEXTURE2D_DESC*);
static void fx_texture_device(ID3D11Texture2D*,ID3D11Device**);
static void fx_context_device(ID3D11DeviceContext*,ID3D11Device**);
static D3D11_DEVICE_CONTEXT_TYPE fx_context_type(ID3D11DeviceContext*);
static HRESULT fx_create_texture(ID3D11Device*,const D3D11_TEXTURE2D_DESC*,const D3D11_SUBRESOURCE_DATA*,ID3D11Texture2D**);
static HRESULT fx_create_query(ID3D11Device*,const D3D11_QUERY_DESC*,ID3D11Query**);
static ULONG fx_device_addref(ID3D11Device*);
static ULONG fx_device_release(ID3D11Device*);
static ULONG fx_texture_release(ID3D11Texture2D*);
static ULONG fx_query_release(ID3D11Query*);
static ULONG fx_context_release(ID3D11DeviceContext*);
static void fx_get_context(ID3D11Device*,ID3D11DeviceContext**);
static void fx_copy(ID3D11DeviceContext*,ID3D11Resource*,ID3D11Resource*);
static void fx_end(ID3D11DeviceContext*,ID3D11Asynchronous*);
static HRESULT fx_get_data(ID3D11DeviceContext*,ID3D11Asynchronous*,void*,UINT,UINT);
static HRESULT fx_map(ID3D11DeviceContext*,ID3D11Resource*,UINT,D3D11_MAP,UINT,D3D11_MAPPED_SUBRESOURCE*);
static void fx_unmap(ID3D11DeviceContext*,ID3D11Resource*,UINT);
static LPVOID WINAPI fx_alloc(HANDLE,DWORD,SIZE_T);
static BOOL WINAPI fx_free(HANDLE,DWORD,LPVOID);
static BOOL WINAPI fx_module(DWORD,LPCWSTR,HMODULE*);
static HANDLE WINAPI fx_thread(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD);
static BOOL WINAPI fx_free_module(HMODULE);
static void WINAPI fx_exit(HMODULE,DWORD);
static BOOL WINAPI fx_write(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
static BOOL WINAPI fx_close(HANDLE);
#undef ID3D11Texture2D_GetDesc
#undef ID3D11Texture2D_GetDevice
#undef ID3D11DeviceContext_GetDevice
#undef ID3D11DeviceContext_GetType
#undef ID3D11Device_CreateTexture2D
#undef ID3D11Device_CreateQuery
#undef ID3D11Device_AddRef
#undef ID3D11Device_Release
#undef ID3D11Texture2D_Release
#undef ID3D11Query_Release
#undef ID3D11DeviceContext_Release
#undef ID3D11Device_GetImmediateContext
#undef ID3D11DeviceContext_CopyResource
#undef ID3D11DeviceContext_End
#undef ID3D11DeviceContext_GetData
#undef ID3D11DeviceContext_Map
#undef ID3D11DeviceContext_Unmap
#define ID3D11Texture2D_GetDesc fx_texture_desc
#define ID3D11Texture2D_GetDevice fx_texture_device
#define ID3D11DeviceContext_GetDevice fx_context_device
#define ID3D11DeviceContext_GetType fx_context_type
#define ID3D11Device_CreateTexture2D fx_create_texture
#define ID3D11Device_CreateQuery fx_create_query
#define ID3D11Device_AddRef fx_device_addref
#define ID3D11Device_Release fx_device_release
#define ID3D11Texture2D_Release fx_texture_release
#define ID3D11Query_Release fx_query_release
#define ID3D11DeviceContext_Release fx_context_release
#define ID3D11Device_GetImmediateContext fx_get_context
#define ID3D11DeviceContext_CopyResource fx_copy
#define ID3D11DeviceContext_End fx_end
#define ID3D11DeviceContext_GetData fx_get_data
#define ID3D11DeviceContext_Map fx_map
#define ID3D11DeviceContext_Unmap fx_unmap
#define HeapAlloc fx_alloc
#define HeapFree fx_free
#define GetModuleHandleExW fx_module
#define CreateThread fx_thread
#define FreeLibrary fx_free_module
#define FreeLibraryAndExitThread fx_exit
#define WriteFile fx_write
#define CloseHandle fx_close
#include "../src/base_composite_capture.h"
#undef HeapAlloc
#undef HeapFree
#undef GetModuleHandleExW
#undef CreateThread
#undef FreeLibrary
#undef FreeLibraryAndExitThread
#undef WriteFile
#undef CloseHandle
static unsigned assertions,cases,reset_number;
#define CHECK(x) do { ++assertions;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;} } while(0)
static void fx_texture_desc(ID3D11Texture2D* t,D3D11_TEXTURE2D_DESC* d) { *d=((FxTexture*)t)->desc; }
static void fx_texture_device(ID3D11Texture2D* t,ID3D11Device** d) { FxDevice* o=((FxTexture*)t)->owner;++o->refs;*d=(ID3D11Device*)o; }
static void fx_context_device(ID3D11DeviceContext* c,ID3D11Device** d) { FxDevice* o=((FxContext*)c)->owner;++o->refs;*d=(ID3D11Device*)o; }
static D3D11_DEVICE_CONTEXT_TYPE fx_context_type(ID3D11DeviceContext* c) { (void)c;return context_deferred?D3D11_DEVICE_CONTEXT_DEFERRED:D3D11_DEVICE_CONTEXT_IMMEDIATE; }
static HRESULT fx_create_texture(ID3D11Device* d,const D3D11_TEXTURE2D_DESC* desc,const D3D11_SUBRESOURCE_DATA* init,ID3D11Texture2D** out) {
 ++texture_creates;*out=NULL;
 if(d!=(ID3D11Device*)&device || init || desc->Usage!=D3D11_USAGE_STAGING || desc->BindFlags || desc->MiscFlags || desc->CPUAccessFlags!=D3D11_CPU_ACCESS_READ) violation=true;
 if(create_texture_fail) return E_FAIL;
 staging=(FxTexture){.refs=1,.owner=&device,.desc=*desc};*out=(ID3D11Texture2D*)&staging;return S_OK;
}
static HRESULT fx_create_query(ID3D11Device* d,const D3D11_QUERY_DESC* desc,ID3D11Query** out) {
 ++query_creates;*out=NULL;if(d!=(ID3D11Device*)&device || desc->Query!=D3D11_QUERY_EVENT || desc->MiscFlags) violation=true;
 if(create_query_fail) return E_FAIL;query.refs=1;*out=(ID3D11Query*)&query;return S_OK;
}
static ULONG fx_device_addref(ID3D11Device* d) { return ++((FxDevice*)d)->refs; }
static ULONG fx_device_release(ID3D11Device* d) { if(!((FxDevice*)d)->refs) violation=true;return --((FxDevice*)d)->refs; }
static ULONG fx_texture_release(ID3D11Texture2D* t) { ++texture_releases;return --((FxTexture*)t)->refs; }
static ULONG fx_query_release(ID3D11Query* q) { ++query_releases;return --((FxQuery*)q)->refs; }
static ULONG fx_context_release(ID3D11DeviceContext* c) { return --((FxContext*)c)->refs; }
static void fx_get_context(ID3D11Device* d,ID3D11DeviceContext** out) { if(d!=(ID3D11Device*)&device) violation=true;++context.refs;*out=(ID3D11DeviceContext*)&context; }
static void fx_copy(ID3D11DeviceContext* c,ID3D11Resource* dst,ID3D11Resource* src) {
 ++copies;copy_order=++order;
 if(c!=(ID3D11DeviceContext*)&context || dst!=(ID3D11Resource*)&staging || src!=(ID3D11Resource*)&backbuffer || !product_end_order) violation=true;
 memcpy(staging.data,backbuffer.data,sizeof(staging.data));
}
static void fx_end(ID3D11DeviceContext* c,ID3D11Asynchronous* q) { ++ends;end_order=++order;if(c!=(ID3D11DeviceContext*)&context || q!=(ID3D11Asynchronous*)&query) violation=true; }
static HRESULT fx_get_data(ID3D11DeviceContext* c,ID3D11Asynchronous* q,void* result,UINT size,UINT flags) {
 ++get_data_calls;if(c!=(ID3D11DeviceContext*)&context || q!=(ID3D11Asynchronous*)&query || size!=sizeof(BOOL) || flags!=D3D11_ASYNC_GETDATA_DONOTFLUSH) violation=true;
 *(BOOL*)result=complete_value?TRUE:FALSE;return query_hr;
}
static HRESULT fx_map(ID3D11DeviceContext* c,ID3D11Resource* r,UINT index,D3D11_MAP map,UINT flags,D3D11_MAPPED_SUBRESOURCE* out) {
 ++maps;if(c!=(ID3D11DeviceContext*)&context || r!=(ID3D11Resource*)&staging || index || map!=D3D11_MAP_READ || flags!=D3D11_MAP_FLAG_DO_NOT_WAIT || query_hr!=S_OK || !complete_value) violation=true;
 out->pData=bad_pointer?NULL:staging.data;out->RowPitch=short_pitch?4:12;return map_hr;
}
static void fx_unmap(ID3D11DeviceContext* c,ID3D11Resource* r,UINT index) { ++unmaps;if(c!=(ID3D11DeviceContext*)&context || r!=(ID3D11Resource*)&staging || index) violation=true; }
static LPVOID WINAPI fx_alloc(HANDLE heap,DWORD flags,SIZE_T bytes) { if(alloc_fail) return NULL;++allocations;return HeapAlloc(heap,flags,bytes); }
static BOOL WINAPI fx_free(HANDLE heap,DWORD flags,LPVOID memory) { ++frees;return HeapFree(heap,flags,memory); }
static BOOL WINAPI fx_module(DWORD flags,LPCWSTR address,HMODULE* out) {
 if(flags!=GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS || !address) violation=true;
 *out=NULL;if(module_fail) return FALSE;++module_refs;*out=(HMODULE)(uintptr_t)1;return TRUE;
}
static BOOL WINAPI fx_free_module(HMODULE m) { if(m!=(HMODULE)(uintptr_t)1 || !module_refs) violation=true;--module_refs;++module_frees;return TRUE; }
static void WINAPI fx_exit(HMODULE m,DWORD code) { if(code) violation=true;++worker_exits;(void)fx_free_module(m); }
static HANDLE WINAPI fx_thread(LPSECURITY_ATTRIBUTES a,SIZE_T stack,LPTHREAD_START_ROUTINE start,LPVOID value,DWORD flags,LPDWORD id) {
 ++thread_calls;if(a || stack || flags || id || start!=bp_bc_disk_worker) violation=true;
 if(thread_fail) return NULL;
 BpBcJob* job=value;
 if(bp_bc.device || bp_bc.staging || bp_bc.query || !unmaps || !texture_releases || !query_releases) violation=true;
 wcscpy_s(exported_image,MAX_PATH,job->image_path);wcscpy_s(exported_sidecar,MAX_PATH,job->sidecar_path);
 memcpy(exported_pixels,job->pixels,sizeof(exported_pixels));exported_enqueue=job->enqueue_present;exported_completion=job->completion_present;exported_identity=job->identity;
 ++queue_jobs;(void)start(value);return (HANDLE)(uintptr_t)2; /* synchronous substitute only */
}
static BOOL WINAPI fx_write(HANDLE file,LPCVOID data,DWORD bytes,LPDWORD written,LPOVERLAPPED overlap) {
 ++writes;
 if(writes==short_write_at) return WriteFile(file,data,bytes/2,written,overlap);
 return WriteFile(file,data,bytes,written,overlap);
}
static BOOL WINAPI fx_close(HANDLE file) {
 if(file==(HANDLE)(uintptr_t)2) return TRUE;
 ++file_closes;BOOL ok=CloseHandle(file);return close_image_fail && file_closes==1?FALSE:ok;
}
static BpIdentity identity(void) { return (BpIdentity){.generation=1,.source=42,.scene=-7,.pose=3,.config=4,.capture_tick=55,.frame=70}; }
static void fx_reset(void) {
 memset(&bp_bc,0,sizeof(bp_bc));bp_bc_disk_result=0;
 device=(FxDevice){.refs=1};other_device=(FxDevice){.refs=1};context=(FxContext){.refs=1,.owner=&device};
 backbuffer=(FxTexture){.refs=1,.owner=&device,.desc={.Width=2,.Height=2,.MipLevels=1,.ArraySize=1,.Format=DXGI_FORMAT_R8G8B8A8_UNORM,.SampleDesc={1,0},.Usage=D3D11_USAGE_DEFAULT,.BindFlags=D3D11_BIND_RENDER_TARGET}};
 const uint8_t pixels[24]={1,2,3,4,5,6,7,8,99,99,99,99,9,10,11,12,13,14,15,16,99,99,99,99};memcpy(backbuffer.data,pixels,sizeof(pixels));
 memset(&staging,0,sizeof(staging));memset(&query,0,sizeof(query));
 texture_creates=query_creates=copies=ends=get_data_calls=maps=unmaps=texture_releases=query_releases=0;
 allocations=frees=module_refs=module_frees=thread_calls=worker_exits=writes=file_closes=queue_jobs=0;
 create_texture_fail=create_query_fail=module_fail=thread_fail=alloc_fail=bad_pointer=short_pitch=close_image_fail=violation=context_deferred=false;
 short_write_at=0;summaries=0;log_room=true;query_hr=S_FALSE;map_hr=S_OK;complete_value=false;order=product_end_order=copy_order=end_order=0;
 exported_image[0]=exported_sidecar[0]=0;telemetry_attach=UINT64_C(900000)+ ++reset_number;
}
static void fx_arm(void) {
 BpIdentity id=identity();product_end_order=++order;
 bp_bc_arm((ID3D11Device*)&device,(ID3D11DeviceContext*)&context,(ID3D11Texture2D*)&backbuffer,&id,bp_bc_next_present());
}
static void fx_complete(void) { query_hr=S_OK;complete_value=true;bp_bc_poll((ID3D11Texture2D*)&backbuffer,bp_bc_next_present()); }
static int fx_done(void) { CHECK(!violation && context.refs==1);++cases;return 0; }
int main(int argc,char** argv) {
 CHECK(argc==2);WCHAR dir[MAX_PATH];CHECK(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,dir,MAX_PATH)>0);
 CHECK(swprintf_s(log_path,MAX_PATH,L"%ls\\immersive-001.log",dir)>0);
 fx_reset();fx_arm();CHECK(bp_bc.state==BP_BC_PENDING && copies==1 && ends==1 && product_end_order<copy_order && copy_order<end_order && !maps);
 for(unsigned i=0;i<2;++i) bp_bc_poll((ID3D11Texture2D*)&backbuffer,bp_bc_next_present());
 CHECK(get_data_calls==2 && !maps && !texture_releases && device.refs==2);
 query_hr=S_OK;bp_bc_poll((ID3D11Texture2D*)&backbuffer,bp_bc_next_present());CHECK(!maps && !texture_releases);
 fx_complete();CHECK(bp_bc.state==BP_BC_EXPORTING && maps==1 && unmaps==1 && texture_releases==1 && query_releases==1 && device.refs==1);
 CHECK(allocations==1 && frees==1 && queue_jobs==1 && worker_exits==1 && module_refs==0 && module_frees==1);
 const uint8_t expected[16]={3,2,1,4,7,6,5,8,11,10,9,12,15,14,13,16};CHECK(!memcmp(expected,exported_pixels,16));
 CHECK(exported_enqueue<exported_completion && bp_identity_equal(&exported_identity,&bp_bc.identity) && exported_identity.capture_tick==55);
 bp_bc_summary();CHECK(bp_bc.state==BP_BC_SAVED && !summaries);
 BpIdentity id=identity();bp_bc_pair_processed(&id,false,55);bp_bc_summary();CHECK(!summaries);
 bp_bc_pair_processed(&id,true,54);bp_bc_summary();CHECK(!summaries);
 bp_bc_pair_processed(&id,true,55);log_room=false;bp_bc_summary();bp_bc_summary();CHECK(summaries==1);
 printf("ARTIFACT %ls\n",exported_sidecar);
 BpBcJob* collision=bp_bc_job(16);CHECK(collision && !bp_bc_disk_write(collision));HeapFree(GetProcessHeap(),0,collision);
 CHECK(!fx_done());
 fx_reset();backbuffer.desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;fx_arm();fx_complete();CHECK(!memcmp(exported_pixels,backbuffer.data,8));CHECK(!fx_done());
 for(unsigned kind=0;kind<2;++kind) {
  fx_reset();create_texture_fail=kind==0;create_query_fail=kind==1;fx_arm();
  CHECK(bp_bc.state==BP_BC_SKIPPED && !copies && !ends && !bp_bc.staging && !bp_bc.query && !bp_bc.device);
  CHECK(texture_releases==kind && !query_releases && device.refs==1);CHECK(!fx_done());
 }
 fx_reset();fx_arm();id=identity();bp_bc_note_context(&id,true);fx_complete();
 CHECK(bp_bc.state==BP_BC_CANCELLED && !maps && !allocations && texture_releases==1 && device.refs==1);CHECK(!fx_done());
 fx_reset();fx_arm();id=identity();++id.source;bp_bc_note_context(&id,false);fx_complete();CHECK(bp_bc.state==BP_BC_CANCELLED && !maps);CHECK(!fx_done());
 for(unsigned mode=0;mode<4;++mode) {
  fx_reset();fx_arm();
  if(mode==0) backbuffer.owner=&other_device;
  if(mode==1) ++bp_bc.thread;
  if(mode==2) query_hr=E_FAIL;
  if(mode==3) context_deferred=true;
  bp_bc_poll((ID3D11Texture2D*)&backbuffer,bp_bc_next_present());
  CHECK(bp_bc.state==BP_BC_RETAINED && !maps && !texture_releases && !query_releases && device.refs==2 && bp_bc.device);
  CHECK(get_data_calls==(mode==2?1u:0u));CHECK(!fx_done());
 }
 fx_reset();fx_arm();for(unsigned i=0;i<BP_BC_MAX_POLLS+2;++i) bp_bc_poll((ID3D11Texture2D*)&backbuffer,bp_bc_next_present());
 CHECK(bp_bc.state==BP_BC_RETAINED && get_data_calls==120 && !maps && !texture_releases && device.refs==2);CHECK(!fx_done());
 for(unsigned mode=0;mode<6;++mode) {
  fx_reset();fx_arm();
  if(mode==0) map_hr=DXGI_ERROR_WAS_STILL_DRAWING;
  if(mode==1) bad_pointer=true;
  if(mode==2) short_pitch=true;
  if(mode==3) alloc_fail=true;
  if(mode==4) module_fail=true;
  if(mode==5) thread_fail=true;
  fx_complete();CHECK(bp_bc.state==BP_BC_FAILED && texture_releases==1 && query_releases==1 && device.refs==1 && !queue_jobs);
  CHECK(unmaps==(mode==0?0u:1u));CHECK(allocations==frees && !module_refs);
  CHECK(module_frees==(mode==5?1u:0u));CHECK(!fx_done());
 }
 for(unsigned mode=0;mode<2;++mode) {
  fx_reset();fx_arm();short_write_at=mode==0?3:0;close_image_fail=mode==1;fx_complete();
  CHECK(bp_bc_disk_result==-1 && GetFileAttributesW(exported_image)!=INVALID_FILE_ATTRIBUTES && GetFileAttributesW(exported_sidecar)==INVALID_FILE_ATTRIBUTES);
  CHECK(frees==1 && module_frees==1 && file_closes==1);CHECK(!fx_done());
 }
 fx_reset();UINT bytes=0;D3D11_TEXTURE2D_DESC desc=backbuffer.desc;CHECK(bp_bc_descriptor(&desc,&bytes) && bytes==16);
 for(unsigned mode=0;mode<9;++mode) {
  desc=backbuffer.desc;
  switch(mode) {
  case 0:desc.Width=0;break;case 1:desc.Height=8193;break;case 2:desc.Width=8192;desc.Height=8192;break;
  case 3:desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;break;case 4:desc.ArraySize=2;break;
  case 5:desc.MipLevels=2;break;case 6:desc.SampleDesc.Count=4;break;case 7:desc.SampleDesc.Quality=1;break;
  default:desc.BindFlags=0;break;
  }
  CHECK(!bp_bc_descriptor(&desc,&bytes));
 }
 desc=backbuffer.desc;desc.Width=8192;desc.Height=1024;CHECK(bp_bc_descriptor(&desc,&bytes) && bytes==33554432);
 fx_arm();unsigned original_copies=copies,original_creates=texture_creates;fx_arm();CHECK(copies==original_copies && texture_creates==original_creates);CHECK(!fx_done());
 printf("PASS composite capture: %u cases / %u assertions; fake GPU/thread/module, real bounded disk IO\n",cases,assertions);return 0;
}
