/* One optional original-composite baseline, after the existing first-pair
   fence. Independent GPU ownership and CPU-only disk job; never bp_fail. */
#define BP_BC_MAX_PIXELS UINT64_C(8388608)
#define BP_BC_MAX_POLLS 120u
enum { BP_BC_NONE,BP_BC_PENDING,BP_BC_EXPORTING,BP_BC_SAVED,BP_BC_SKIPPED,
 BP_BC_FAILED,BP_BC_RETAINED,BP_BC_CANCELLED };
typedef struct BpBcJob {
 HMODULE module;
 BpIdentity identity;
 uint64_t attach,enqueue_present,completion_present;
 DWORD pid;UINT width,height,format,bytes;
 WCHAR image_path[MAX_PATH],sidecar_path[MAX_PATH];
 char image_name[128],own_name[2][96];
 uint8_t pixels[];
} BpBcJob;
static struct {
 unsigned state,polls;bool attempted,cancelled,summary_allowed,summary_attempted;
 DWORD thread,pid;uint64_t attach,enqueue_present,completion_present;
 BpIdentity identity;D3D11_TEXTURE2D_DESC desc;
 ID3D11Texture2D* staging;ID3D11Query* query;ID3D11Device* device;
 const char* reason;
} bp_bc;
static volatile LONG64 bp_bc_serial;
static volatile LONG bp_bc_disk_result;
static bool bp_bc_descriptor(const D3D11_TEXTURE2D_DESC* d,UINT* bytes) {
 bool format=d->Format==DXGI_FORMAT_R8G8B8A8_UNORM || d->Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
  d->Format==DXGI_FORMAT_B8G8R8A8_UNORM || d->Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
 uint64_t pixels=(uint64_t)d->Width*d->Height;
 if(!format || !d->Width || !d->Height || d->Width>8192 || d->Height>8192 ||
  pixels>BP_BC_MAX_PIXELS || d->MipLevels!=1 || d->ArraySize!=1 ||
  d->SampleDesc.Count!=1 || d->SampleDesc.Quality || d->Usage!=D3D11_USAGE_DEFAULT ||
  !(d->BindFlags&D3D11_BIND_RENDER_TARGET) || pixels>(UINT32_MAX-54u)/4u ||
  pixels>(SIZE_MAX-sizeof(BpBcJob))/4u) return false;
 *bytes=(UINT)(pixels*4u);return true;
}
/* Call only before submission, or after independent completion is proved. */
static void bp_bc_release_completed(void) {
 if(bp_bc.staging) { ID3D11Texture2D_Release(bp_bc.staging);bp_bc.staging=NULL; }
 if(bp_bc.query) { ID3D11Query_Release(bp_bc.query);bp_bc.query=NULL; }
 if(bp_bc.device) { ID3D11Device_Release(bp_bc.device);bp_bc.device=NULL; }
}
static bool bp_bc_write(HANDLE file,const void* data,DWORD bytes) {
 DWORD written=0;return WriteFile(file,data,bytes,&written,NULL) && written==bytes;
}
/* Disk-only: no global bp/bp_cc/GPU objects or live capture state are read. */
static bool bp_bc_disk_write(const BpBcJob* job) {
 HANDLE file=CreateFileW(job->image_path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
 if(file==INVALID_HANDLE_VALUE) return false;
 BITMAPFILEHEADER fh={0};BITMAPINFOHEADER ih={0};
 _Static_assert(sizeof(fh)+sizeof(ih)==54,"Canonical BMP header size");
 fh.bfType=0x4d42;fh.bfOffBits=54;fh.bfSize=54+job->bytes;
 ih.biSize=sizeof(ih);ih.biWidth=(LONG)job->width;ih.biHeight=-(LONG)job->height;
 ih.biPlanes=1;ih.biBitCount=32;ih.biCompression=BI_RGB;ih.biSizeImage=job->bytes;
 bool image_ok=bp_bc_write(file,&fh,sizeof(fh)) && bp_bc_write(file,&ih,sizeof(ih)) && bp_bc_write(file,job->pixels,job->bytes);
 bool image_closed=CloseHandle(file)!=FALSE;
 if(!image_ok || !image_closed) return false; /* preserve partial file */
 char sidecar[4096];
 int n=snprintf(sidecar,sizeof(sidecar),
  "{\"schema\":1,\"kind\":\"original_final_composite\",\"build_id\":\"%s\",\"pid\":%lu,\"attach_id\":%llu,"
  "\"identity\":[%llu,%llu,%d,%llu,%llu],\"capture_tick\":%llu,\"main_observed_frame\":%d,"
  "\"enqueue_present\":%llu,\"completion_present\":%llu,\"descriptor\":[%u,%u,%u],"
  "\"image_file\":\"%s\",\"image_bytes\":%u,\"own_files_expected\":[\"%s\",\"%s\"],\"own_files_verified\":false,"
  "\"image_write_complete\":true,\"render_frame\":\"unverified\",\"row_order\":\"mapped_top_down_bmp\","
  "\"alpha\":\"source_preserved_not_coverage\",\"wedge_cause\":\"INCONCLUSIVE\"}\n",
  UMAVR_BUILD_ID,(unsigned long)job->pid,(unsigned long long)job->attach,
  (unsigned long long)job->identity.generation,(unsigned long long)job->identity.source,job->identity.scene,
  (unsigned long long)job->identity.pose,(unsigned long long)job->identity.config,(unsigned long long)job->identity.capture_tick,job->identity.frame,
  (unsigned long long)job->enqueue_present,(unsigned long long)job->completion_present,job->width,job->height,job->format,
  job->image_name,54u+job->bytes,job->own_name[0],job->own_name[1]);
 if(n<=0 || n>=(int)sizeof(sidecar)) return false;
 file=CreateFileW(job->sidecar_path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
 if(file==INVALID_HANDLE_VALUE) return false;
 bool sidecar_ok=bp_bc_write(file,sidecar,(DWORD)n);bool sidecar_closed=CloseHandle(file)!=FALSE;
 return sidecar_ok && sidecar_closed;
}
static DWORD WINAPI bp_bc_disk_worker(LPVOID value) {
 BpBcJob* job=value;HMODULE module=job->module;
 bool saved=bp_bc_disk_write(job);
 HeapFree(GetProcessHeap(),0,job);
 InterlockedExchange(&bp_bc_disk_result,saved?1:-1);
 FreeLibraryAndExitThread(module,0);return 0;
}
static bool bp_bc_schedule(BpBcJob* job) {
 HMODULE module=NULL;
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
  (LPCWSTR)(uintptr_t)&bp_bc_disk_worker,&module) || !module) return false;
 job->module=module;
 HANDLE thread=CreateThread(NULL,0,bp_bc_disk_worker,job,0,NULL);
 if(!thread) { FreeLibrary(module);job->module=NULL;return false; }
 CloseHandle(thread);return true; /* job ownership transferred */
}
static BpBcJob* bp_bc_job(UINT bytes) {
 BpBcJob* job=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*job)+(SIZE_T)bytes);
 if(!job) return NULL;
 job->identity=bp_bc.identity;job->attach=bp_bc.attach;job->pid=bp_bc.pid;
 job->enqueue_present=bp_bc.enqueue_present;job->completion_present=bp_bc.completion_present;
 job->width=bp_bc.desc.Width;job->height=bp_bc.desc.Height;job->format=(UINT)bp_bc.desc.Format;job->bytes=bytes;
 WCHAR dir[MAX_PATH];wcscpy_s(dir,MAX_PATH,log_path);WCHAR* slash=wcsrchr(dir,L'\\');
 if(!slash) goto fail;*slash=0;
 int n=snprintf(job->image_name,sizeof(job->image_name),"original-composite-%lu-%llu-g%llu-p%llu.bmp",
  (unsigned long)job->pid,(unsigned long long)job->attach,(unsigned long long)job->identity.generation,(unsigned long long)job->enqueue_present);
 if(n<=0 || n>=(int)sizeof(job->image_name)) goto fail;
 n=swprintf_s(job->image_path,MAX_PATH,L"%ls\\original-composite-%lu-%llu-g%llu-p%llu.bmp",dir,
  (unsigned long)job->pid,(unsigned long long)job->attach,(unsigned long long)job->identity.generation,(unsigned long long)job->enqueue_present);
 if(n<=0) goto fail;
 n=swprintf_s(job->sidecar_path,MAX_PATH,L"%ls\\original-composite-%lu-%llu-g%llu-p%llu.json",dir,
  (unsigned long)job->pid,(unsigned long long)job->attach,(unsigned long long)job->identity.generation,(unsigned long long)job->enqueue_present);
 if(n<=0) goto fail;
 for(unsigned eye=0;eye<2;++eye) {
  n=snprintf(job->own_name[eye],sizeof(job->own_name[eye]),"own-base-%lu-%llu-g%llu-%u.bmp",
   (unsigned long)job->pid,(unsigned long long)job->attach,(unsigned long long)job->identity.generation,eye);
  if(n<=0 || n>=(int)sizeof(job->own_name[eye])) goto fail;
 }
 return job;
fail:
 HeapFree(GetProcessHeap(),0,job);return NULL;
}
static void bp_bc_arm(ID3D11Device* dev,ID3D11DeviceContext* ctx,ID3D11Texture2D* bb,
 const BpIdentity* identity,uint64_t serial) {
 if(bp_bc.attempted) return;
 bp_bc.attempted=true;bp_bc.state=BP_BC_SKIPPED;bp_bc.reason="descriptor_or_device";
 bp_bc.identity=*identity;bp_bc.enqueue_present=serial;bp_bc.pid=GetCurrentProcessId();bp_bc.attach=telemetry_attach;
 bp_bc.thread=GetCurrentThreadId();
 UINT bytes=0;ID3D11Device *owner=NULL,*context_owner=NULL;
 ID3D11Texture2D_GetDesc(bb,&bp_bc.desc);ID3D11Texture2D_GetDevice(bb,&owner);ID3D11DeviceContext_GetDevice(ctx,&context_owner);
 bool valid=dev && owner==dev && context_owner==dev && ID3D11DeviceContext_GetType(ctx)==D3D11_DEVICE_CONTEXT_IMMEDIATE &&
  serial && identity->generation && bp_bc_descriptor(&bp_bc.desc,&bytes);
 if(owner) ID3D11Device_Release(owner);if(context_owner) ID3D11Device_Release(context_owner);
 if(!valid) return;
 D3D11_TEXTURE2D_DESC staging=bp_bc.desc;staging.Usage=D3D11_USAGE_STAGING;
 staging.BindFlags=0;staging.MiscFlags=0;staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 D3D11_QUERY_DESC query={D3D11_QUERY_EVENT,0};
 bp_bc.reason="optional_resource_create";
 if(FAILED(ID3D11Device_CreateTexture2D(dev,&staging,NULL,&bp_bc.staging)) || !bp_bc.staging ||
  FAILED(ID3D11Device_CreateQuery(dev,&query,&bp_bc.query)) || !bp_bc.query) { bp_bc_release_completed();return; }
 bp_bc.device=dev;ID3D11Device_AddRef(dev);
 /* Caller has already ended the product fence. This copy/query is separate. */
 ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)bp_bc.staging,(ID3D11Resource*)bb);
 ID3D11DeviceContext_End(ctx,(ID3D11Asynchronous*)bp_bc.query);
 bp_bc.state=BP_BC_PENDING;bp_bc.reason="independent_query_pending";
}
static void bp_bc_note_context(const BpIdentity* identity,bool retiring) {
 if(bp_bc.state==BP_BC_PENDING && (retiring || !bp_identity_equal(identity,&bp_bc.identity))) bp_bc.cancelled=true;
}
static void bp_bc_poll(ID3D11Texture2D* bb,uint64_t serial) {
 if(bp_bc.state!=BP_BC_PENDING) return;
 if(GetCurrentThreadId()!=bp_bc.thread) { bp_bc.state=BP_BC_RETAINED;bp_bc.reason="present_thread_changed_retained";return; }
 ID3D11Device* dev=NULL;ID3D11DeviceContext* ctx=NULL;ID3D11Texture2D_GetDevice(bb,&dev);
 if(!dev || dev!=bp_bc.device) { bp_bc.state=BP_BC_RETAINED;bp_bc.reason="device_changed_retained";goto done; }
 ID3D11Device_GetImmediateContext(dev,&ctx);
 if(!ctx || ID3D11DeviceContext_GetType(ctx)!=D3D11_DEVICE_CONTEXT_IMMEDIATE) {
  bp_bc.state=BP_BC_RETAINED;bp_bc.reason="context_unavailable_retained";goto done;
 }
 BOOL complete=FALSE;++bp_bc.polls;
 HRESULT hr=ID3D11DeviceContext_GetData(ctx,(ID3D11Asynchronous*)bp_bc.query,&complete,sizeof(complete),D3D11_ASYNC_GETDATA_DONOTFLUSH);
 if(hr==S_FALSE || (hr==S_OK && !complete)) {
  if(bp_bc.polls>=BP_BC_MAX_POLLS) { bp_bc.state=BP_BC_RETAINED;bp_bc.reason="poll_cap_pending_retained"; }
  goto done;
 }
 if(hr!=S_OK) { bp_bc.state=BP_BC_RETAINED;bp_bc.reason="query_failure_retained";goto done; }
 bp_bc.completion_present=serial;
 if(bp_bc.cancelled) { bp_bc_release_completed();bp_bc.state=BP_BC_CANCELLED;bp_bc.reason="source_retired_after_completion";goto done; }
 D3D11_MAPPED_SUBRESOURCE map={0};
 hr=ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)bp_bc.staging,0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&map);
 if(hr!=S_OK) { bp_bc_release_completed();bp_bc.state=BP_BC_FAILED;bp_bc.reason="nonblocking_map_failed";goto done; }
 UINT bytes=0;BpBcJob* job=NULL;
 if(map.pData && bp_bc_descriptor(&bp_bc.desc,&bytes) && map.RowPitch>=bp_bc.desc.Width*4u &&
  (uint64_t)map.RowPitch*bp_bc.desc.Height<=SIZE_MAX) job=bp_bc_job(bytes);
 if(job) {
  bool bgra=bp_bc.desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM || bp_bc.desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
  for(unsigned y=0;y<job->height;++y) {
   const uint8_t* source=(const uint8_t*)map.pData+(size_t)y*map.RowPitch;uint8_t* target=job->pixels+(size_t)y*job->width*4u;
   if(bgra) memcpy(target,source,(size_t)job->width*4u);
   else for(unsigned x=0;x<job->width;++x) { target[x*4]=source[x*4+2];target[x*4+1]=source[x*4+1];target[x*4+2]=source[x*4];target[x*4+3]=source[x*4+3]; }
  }
 }
 ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)bp_bc.staging,0);
 bp_bc_release_completed();
 bp_bc.state=BP_BC_FAILED;bp_bc.reason="cpu_job_or_schedule_failed";
 if(job) {
  if(bp_bc_schedule(job)) { bp_bc.state=BP_BC_EXPORTING;bp_bc.reason="disk_job_pending"; }
  else HeapFree(GetProcessHeap(),0,job);
 }
done:
 if(ctx) ID3D11DeviceContext_Release(ctx);if(dev) ID3D11Device_Release(dev);
}
static uint64_t bp_bc_next_present(void) { return (uint64_t)InterlockedIncrement64(&bp_bc_serial); }
static void bp_bc_pair_processed(const BpIdentity* identity,bool witnesses_saved,uint64_t committed_tick) {
 if(bp_bc.attempted && witnesses_saved && committed_tick==bp_bc.identity.capture_tick &&
  identity->capture_tick==bp_bc.identity.capture_tick && bp_identity_equal(identity,&bp_bc.identity)) bp_bc.summary_allowed=true;
}
static void bp_bc_summary(void) {
 if(bp_bc.state==BP_BC_EXPORTING) {
  LONG result=InterlockedCompareExchange(&bp_bc_disk_result,0,0);
  if(result) { bp_bc.state=result>0?BP_BC_SAVED:BP_BC_FAILED;bp_bc.reason=result>0?"disk_export_complete":"disk_export_failed"; }
 }
 if(!bp_bc.summary_allowed || bp_bc.summary_attempted || bp_bc.state==BP_BC_PENDING || bp_bc.state==BP_BC_EXPORTING) return;
 bp_bc.summary_attempted=true;char detail[512];
 int n=telemetry_snprintf(detail,sizeof(detail),
  "\"status\":\"%s\",\"state\":%u,\"generation\":%llu,\"enqueue_present\":%llu,\"completion_present\":%llu,\"sidecar_primary\":true,\"wedge_cause\":\"INCONCLUSIVE\"",
  bp_bc.reason?bp_bc.reason:"unavailable",bp_bc.state,(unsigned long long)bp_bc.identity.generation,
  (unsigned long long)bp_bc.enqueue_present,(unsigned long long)bp_bc.completion_present);
 if(n>0 && n<(int)sizeof(detail)) (void)bp_projection_normal_record("original_composite_baseline",detail);
}
