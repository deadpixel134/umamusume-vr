/* Only called inside the already-qualified game swapchain Present, before
   original_present. No worker/main-thread immediate-context use. */
#include "base_composite_capture.h"
/* One zero-wait pair attempt per qualified Present. No stale copy/draw occurs.
   Writers cannot pass this receipt until both keys are returned to zero. */
static bool bp_reclaim_transport(void) {
 if(bp_transport_blocked(&bp_transport)) return false;
 LONG64 serial=bp_transport_pending(&bp_transport);if(!serial) return true;
 if(!bp_transport_reclaimable(&bp_transport,(LONG)bp.c.identity.generation,bp.c.retiring,
  InterlockedCompareExchange(&live_retired_through,0,0))) return false;
 if(!shared_eye_km_game[0] || !shared_eye_km_game[1]) {
  bp_transport_quarantine(&bp_transport);bp_fail("retired_transport_missing_mutex");return false;
 }
 LONG owner=bp_transport.owner,generation=bp_transport.generation;
 HRESULT result[3];
 int acquired=bp_keyed_pair_acquire(shared_eye_km_game[0],shared_eye_km_game[1],1,result);
 if(acquired==BP_KM_PENDING) return false;
 if(acquired==BP_KM_BROKEN) {
  bp_transport_quarantine(&bp_transport);bp_fail("retired_transport_acquire_failed");return false;
 }
 HRESULT right=IDXGIKeyedMutex_ReleaseSync(shared_eye_km_game[1],0);
 HRESULT left=IDXGIKeyedMutex_ReleaseSync(shared_eye_km_game[0],0);
 if(right!=S_OK || left!=S_OK || !bp_transport_ack(&bp_transport,owner,generation,serial)) {
  bp_transport_quarantine(&bp_transport);bp_fail("retired_transport_release_or_receipt_failed");return false;
 }
 return true;
}
static void bp_gpu_release(void) {
 for(unsigned eye=0;eye<2;++eye) {
  if(bp.sentinel_view[eye]) { ID3D11RenderTargetView_Release(bp.sentinel_view[eye]); bp.sentinel_view[eye]=NULL; }
  if(bp.readback[eye]) { ID3D11Texture2D_Release(bp.readback[eye]); bp.readback[eye]=NULL; }
 }
 if(bp.fence) { ID3D11Query_Release(bp.fence); bp.fence=NULL; }
 if(bp.gpu_device) { ID3D11Device_Release(bp.gpu_device); bp.gpu_device=NULL; }
}
static bool bp_unlock_pair(UINT64 key) {
 bool ok=true;
 for(unsigned eye=0;eye<2;++eye) if(bp.c.mutex_owned&(1u<<eye)) {
  HRESULT hr=IDXGIKeyedMutex_ReleaseSync(shared_eye_km_game[eye],key);
  if(!bp_release_result(&bp.c,eye,hr==S_OK)) ok=false;
 }
 if(!ok) { bp_transport_quarantine(&bp_transport);bp_fail("keyed_pair_release_failed_ownership_retained"); }
 return ok;
}
static bool bp_prepare_gpu(ID3D11Device* dev,ID3D11DeviceContext* ctx) {
 bp.gpu_device=dev; ID3D11Device_AddRef(dev);
 if(bp.native[0]==bp.native[1]) { bp_fail("native_rt_alias"); return false; }
 for(unsigned eye=0;eye<2;++eye) {
  D3D11_TEXTURE2D_DESC desc; ID3D11Device* owner=NULL;
  ID3D11Texture2D_GetDesc(bp.native[eye],&desc); ID3D11Texture2D_GetDevice(bp.native[eye],&owner);
  DXGI_FORMAT format=typed_format(desc.Format);
  bool rgba=format==DXGI_FORMAT_R8G8B8A8_UNORM || format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
   format==DXGI_FORMAT_B8G8R8A8_UNORM || format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
  bool ok=bp_descriptor_ok(desc.Width,desc.Height,(unsigned)bp.width,(unsigned)bp.height,
   desc.MipLevels,desc.ArraySize,desc.SampleDesc.Count,rgba,owner==dev,bp.native[0]!=bp.native[1]) &&
   (desc.BindFlags&D3D11_BIND_RENDER_TARGET)!=0;
  if(owner) ID3D11Device_Release(owner);
  if(!ok) { bp_fail("pre_enable_rt_descriptor_or_device"); return false; }
  D3D11_RENDER_TARGET_VIEW_DESC view={0}; view.Format=format; view.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
  if(FAILED(ID3D11Device_CreateRenderTargetView(dev,(ID3D11Resource*)bp.native[eye],&view,&bp.sentinel_view[eye]))) {
   bp_fail("own_sentinel_view_create"); return false;
  }
  desc.Format=format; desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=0;
  desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ; desc.MiscFlags=0;
  if(FAILED(ID3D11Device_CreateTexture2D(dev,&desc,NULL,&bp.readback[eye]))) { bp_fail("own_readback_create"); return false; }
 }
 D3D11_QUERY_DESC query={D3D11_QUERY_EVENT,0};
 if(FAILED(ID3D11Device_CreateQuery(dev,&query,&bp.fence))) { bp_fail("event_query_create"); return false; }
 if(!bp_arm(&bp.c,BP_Q_PREPARE,0,GetTickCount64())) { bp_fail("prepare_state"); return false; }
 const float sentinel[2][4]={{1,0,1,1},{0,1,1,1}};
 for(unsigned eye=0;eye<2;++eye) ID3D11DeviceContext_ClearRenderTargetView(ctx,bp.sentinel_view[eye],sentinel[eye]);
 ID3D11DeviceContext_End(ctx,(ID3D11Asynchronous*)bp.fence);
 bp_log("base_rt_validation","descriptor_device_verified_sentinel_pending"); return true;
}
static bool bp_write_all(HANDLE file,const void* data,DWORD bytes) {
 DWORD written=0; return WriteFile(file,data,bytes,&written,NULL) && written==bytes;
}
/* File exports are capped to one process generation. Fresh generations still
   validate actual readback pixels before publishing; they do not overwrite or
   multiply the first-process capture artifacts. */
static uint64_t bp_witness_export_generation;

static bool story_image_requested(void) {
 static ULONGLONG next_check;
 if(bp_witness_export_generation || bp.source_kind!=BP_SOURCE_STORY || !bp.witness_saved) return false;
 ULONGLONG now=GetTickCount64();
 if(now<next_check)return false;
 next_check=now+500;
 WCHAR path[MAX_PATH]; wcscpy_s(path,MAX_PATH,log_path);
 WCHAR* name=wcsrchr(path,L'\\');
 if(!name || swprintf_s(name+1,MAX_PATH-(size_t)(name+1-path),L"story-capture-%lu-%llu.ready",
     (unsigned long)GetCurrentProcessId(),(unsigned long long)telemetry_attach)<=0)return false;
 DWORD attributes=GetFileAttributesW(path);
 return attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
}
static uint64_t story_image_tick;
static bool bp_witness_can_retry(void) {
 return bp.source_kind==BP_SOURCE_RACE || bp.source_kind==BP_SOURCE_DRESS;
}
static bool bp_save_witness(ID3D11DeviceContext* ctx,unsigned eye,bool export_image) {
 D3D11_TEXTURE2D_DESC desc; ID3D11Texture2D_GetDesc(bp.readback[eye],&desc);
 D3D11_MAPPED_SUBRESOURCE map={0};
 HRESULT hr=ID3D11DeviceContext_Map(ctx,(ID3D11Resource*)bp.readback[eye],0,D3D11_MAP_READ,
  D3D11_MAP_FLAG_DO_NOT_WAIT,&map);
 if(FAILED(hr) || !map.pData || map.RowPitch<desc.Width*4) { if(SUCCEEDED(hr))ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)bp.readback[eye],0); if(!export_image)bp_fail("first_readback_map_after_fence"); return false; }
 bool bgra=desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM || desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
 unsigned sentinel[3]={eye?0u:255u,eye?255u:0u,255u};
 unsigned min[3]={255,255,255},max[3]={0,0,0};
 uint64_t changed=0,hash=UINT64_C(14695981039346656037);


 WCHAR path[MAX_PATH],dir[MAX_PATH]; wcscpy_s(dir,MAX_PATH,log_path);
 WCHAR* slash=wcsrchr(dir,L'\\'); if(slash) *slash=0;
 int n=slash && dir[0]?swprintf_s(path,MAX_PATH,L"%ls\\own-base-%lu-%llu-g%llu-%u.bmp",dir,
  (unsigned long)GetCurrentProcessId(),(unsigned long long)telemetry_attach,
  (unsigned long long)bp.c.pending.generation,eye):0;
 HANDLE file=export_image && n>0?CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL):INVALID_HANDLE_VALUE;
 BITMAPFILEHEADER fh={0}; BITMAPINFOHEADER ih={0};
 fh.bfType=0x4d42; fh.bfOffBits=sizeof(fh)+sizeof(ih); fh.bfSize=fh.bfOffBits+desc.Width*desc.Height*4;
 ih.biSize=sizeof(ih); ih.biWidth=(LONG)desc.Width; ih.biHeight=-(LONG)desc.Height;
 ih.biPlanes=1; ih.biBitCount=32; ih.biCompression=BI_RGB; ih.biSizeImage=desc.Width*desc.Height*4;
 uint8_t* row=export_image?HeapAlloc(GetProcessHeap(),0,desc.Width*4):NULL;
 bool saved=file!=INVALID_HANDLE_VALUE && row && bp_write_all(file,&fh,sizeof(fh)) && bp_write_all(file,&ih,sizeof(ih));
 for(unsigned y=0;y<desc.Height;++y) {
  const uint8_t* p=(const uint8_t*)map.pData+(size_t)y*map.RowPitch;
  for(unsigned x=0;x<desc.Width;++x) {
   unsigned color[3]={p[x*4+(bgra?2:0)],p[x*4+1],p[x*4+(bgra?0:2)]}; bool different=false;
   for(unsigned c=0;c<3;++c) {
    if(color[c]<min[c]) min[c]=color[c]; if(color[c]>max[c]) max[c]=color[c];
    if(color[c]!=sentinel[c]) different=true;
    hash=(hash^color[c])*UINT64_C(1099511628211);
   }
   if(different) ++changed;
   if(row) { row[x*4]=(uint8_t)color[2]; row[x*4+1]=(uint8_t)color[1]; row[x*4+2]=(uint8_t)color[0]; row[x*4+3]=255; }
  }
  if(saved && !bp_write_all(file,row,desc.Width*4)) saved=false;
 }
 if(file!=INVALID_HANDLE_VALUE) CloseHandle(file); if(row) HeapFree(GetProcessHeap(),0,row);
 ID3D11DeviceContext_Unmap(ctx,(ID3D11Resource*)bp.readback[eye],0);
 unsigned range=0; for(unsigned c=0;c<3;++c) if(max[c]-min[c]>range) range=max[c]-min[c];
 bool witness=bp_write_witness((uint64_t)desc.Width*desc.Height,changed,range);
 char detail[1024];
 telemetry_snprintf(detail,sizeof(detail),
  "\"status\":\"%s\",\"generation\":%llu,\"eye\":%u,\"source\":%llu,"
  "\"pose_epoch\":%llu,\"config_epoch\":%llu,\"main_observed_frame\":%d,\"capture_tick\":%llu,"
  "\"native_rt\":\"%p\",\"width\":%u,\"height\":%u,\"changed_pixels\":%llu,"
  "\"channel_range\":%u,\"rgb_fnv64\":\"%016llx\",\"image_saved\":%s,\"image_export_budget_skipped\":%s,"
  "\"position\":[%.6f,%.6f,%.6f],\"projection\":[%.6f,%.6f,%.6f,%.6f],"
  "\"render_frame\":\"unverified\",\"scene_image_review\":\"required\",\"stereo_fidelity_accepted\":false",
  witness?"rt_changed_nonuniform":"sentinel_or_uniform_rejected",(unsigned long long)bp.c.pending.generation,
  eye,(unsigned long long)bp.c.pending.source,(unsigned long long)bp.c.pending.pose,
  (unsigned long long)bp.c.pending.config,bp.c.pending.frame,(unsigned long long)bp.c.pending.capture_tick,
  (void*)bp.native[eye],desc.Width,desc.Height,(unsigned long long)changed,range,(unsigned long long)hash,
  saved?"true":"false",export_image?"false":"true",bp.eye[eye].position.x,bp.eye[eye].position.y,bp.eye[eye].position.z,
  bp.eye[eye].projection.m[0],bp.eye[eye].projection.m[5],bp.eye[eye].projection.m[8],bp.eye[eye].projection.m[9]);
 bool retryable_sample=!export_image && bp_witness_can_retry();
 if(retryable_sample && (bp.witness_samples==1 || bp.witness_samples==10)) {
  char receipt[256];
  telemetry_snprintf(receipt,sizeof receipt,"\"stage\":\"sample_%u\",\"generation\":%llu,\"frame\":%d,\"changed\":%llu,\"range\":%u,\"rgb_min\":[%u,%u,%u],\"rgb_max\":[%u,%u,%u]",
   bp.witness_samples,(unsigned long long)bp.c.pending.generation,bp.c.pending.frame,(unsigned long long)changed,range,min[0],min[1],min[2],max[0],max[1],max[2]);
  const char* event=bp.source_kind==BP_SOURCE_DRESS?
   (eye?"preview_right_witness":"preview_left_witness"):(eye?"race_right_witness":"race_left_witness");
  write_record("OWN-BASE",event,receipt);
 }
 write_record("OWN-BASE",export_image?(eye?"story_image_right":"story_image_left"):"base_eye_write_witness",detail);
 if(export_image)return saved;
 if(!witness && !retryable_sample) bp_fail("own_rt_not_changed_nonuniform");
 return witness;
}
static HomeRenderViews home_pending_views;
static void bp_commit_pair(void) {
 /* Metadata is written while both mutexes remain owned. Release failures leave
    failed ownership retained and unpublish; no half pair becomes current. */
 LONG generation=(LONG)bp.c.pending.generation;
 LONG64 serial=InterlockedIncrement64(&eye_pair_serial);
 stereo_observe_cache(&game_pair_telemetry,generation,serial);
 home_pair_views=home_pending_views;
 game_pair_telemetry.owned=TRUE; game_pair_telemetry.source=bp.c.pending.source;
 game_pair_telemetry.pose=bp.c.pending.pose; game_pair_telemetry.config=bp.c.pending.config;
 game_pair_telemetry.capture_tick=bp.c.pending.capture_tick;
 game_pair_telemetry.scene=bp.c.pending.scene;
 if(!bp_transport_begin(&bp_transport,BP_TRANSPORT_OWN,generation,serial)) {
  (void)bp_unlock_pair(0);bp_fail("owned_transport_receipt_unavailable");return;
 }
 if(!bp_unlock_pair(1)) return;
 if(!bp_publication_ready(&bp.c,&bp.c.pending) ||
  InterlockedCompareExchange(&source_generation,0,0)!=generation ||
  InterlockedCompareExchange(&bp_stop_requested,0,0) ||
  !bp_lease_publish(&bp_consumer_lease,&bp.c.pending,InterlockedCompareExchange(&bp_stop_requested,0,0)!=0)) {
  bp_unpublish(); bp_retire(&bp.c,false); return;
 }
 InterlockedExchange64(&eye_pair_tick,(LONG64)bp.c.pending.capture_tick);
 InterlockedExchange(&bp_published_generation,generation);
 InterlockedExchange(&published_eye_generation,generation);
 if(!bp.last_committed_tick || (story_image_tick && story_image_tick==bp.c.pending.capture_tick && bp_witness_export_generation==bp.c.pending.generation)) {
  char detail[640];
  telemetry_snprintf(detail,sizeof(detail),
   "\"status\":\"committed\",\"generation\":%ld,\"pair_serial\":%lld,\"source\":%llu,"
   "\"scene\":%d,\"pose_epoch\":%llu,\"config_epoch\":%llu,\"capture_tick\":%llu,"
   "\"main_observed_frame\":%d,\"query_complete\":true,\"both_mutex_released\":true,"
   "\"write_witness_mask\":3,\"render_frame\":\"unverified\",\"source_pose_follow\":true",
   (long)generation,(long long)serial,(unsigned long long)bp.c.pending.source,bp.c.pending.scene,
   (unsigned long long)bp.c.pending.pose,(unsigned long long)bp.c.pending.config,
   (unsigned long long)bp.c.pending.capture_tick,bp.c.pending.frame);
  write_record("OWN-BASE",story_image_tick && story_image_tick==bp.c.pending.capture_tick?"story_image_pair":"base_pair_committed",detail);
  if(!bp.last_committed_tick)bp_context_event("first_fresh_pair","fresh_witness_lease_and_pair_committed");
 }
 bp.last_committed_tick=bp.c.pending.capture_tick;
}
static void bp_present(ID3D11Texture2D* backbuffer) {
 uint64_t baseline_serial=bp_bc_next_present();
 if(!TryAcquireSRWLockExclusive(&bp_lock)) return;
 bp_bc_note_context(&bp.c.identity,bp.c.retiring || bp.c.quarantined || bp.c.phase==BP_RETIRED ||
  (bp.c.identity.generation && InterlockedCompareExchange(&source_generation,0,0)!=(LONG)bp.c.identity.generation));
 bp_bc_poll(backbuffer,baseline_serial);
 (void)bp_reclaim_transport();
 ID3D11Device* dev=NULL; ID3D11DeviceContext* ctx=NULL;
 if(!bp.c.attempted || bp.c.phase==BP_RETIRED || bp_transport_blocked(&bp_transport) ||
  (bp.c.quarantined && bp.c.query==BP_Q_NONE)) goto done;
 DWORD thread=GetCurrentThreadId();
 if(bp.present_thread && bp.present_thread!=thread) { bp_fail("game_present_owner_thread_changed"); goto done; }
 bp.present_thread=thread;
 ID3D11Texture2D_GetDevice(backbuffer,&dev);
 if(dev) ID3D11Device_GetImmediateContext(dev,&ctx);
 if(!dev || !ctx || ID3D11DeviceContext_GetType(ctx)!=D3D11_DEVICE_CONTEXT_IMMEDIATE) { bp_fail("game_present_immediate_context"); goto done; }
 if(bp.gpu_device && bp.gpu_device!=dev) { bp_fail("game_device_changed_pending_resources_retained"); goto done; }
 if(!bp.c.retiring && InterlockedCompareExchange(&source_generation,0,0)!=(LONG)bp.c.identity.generation) {
  bp_unpublish(); bp_retire(&bp.c,false);
 }
 if(bp.c.query!=BP_Q_NONE && !bp.c.query_failed) {
  BOOL complete=FALSE;
  HRESULT hr=ID3D11DeviceContext_GetData(ctx,(ID3D11Asynchronous*)bp.fence,&complete,sizeof(complete),D3D11_ASYNC_GETDATA_DONOTFLUSH);
  if(hr==S_FALSE || (hr==S_OK && !complete)) goto done;
  if(hr!=S_OK) { (void)bp_complete(&bp.c,-1); bp_fail("event_query_failed_resources_quarantined"); goto done; }
  BpQuery q=bp_complete(&bp.c,1);
  if(q==BP_Q_PAIR && bp.source_kind==BP_SOURCE_STORY){
   bool saved[2]={false,false};
   if(!bp.c.retiring)for(unsigned i=0;i<2;i++)saved[i]=bp_save_witness(ctx,i,true);
   char detail[260];telemetry_snprintf(detail,sizeof detail,"\"stage\":\"baked_body_pair\",\"scene\":%d,\"generation\":%llu,\"frame\":%d,\"left_saved\":%s,\"right_saved\":%s",bp.c.pending.scene,(unsigned long long)bp.c.pending.generation,bp.c.pending.frame,saved[0]?"true":"false",saved[1]?"true":"false");
   write_record("STORY","story_display",detail);
   InterlockedExchange(&layer_trial_finished,1);bp_retire(&bp.c,false);goto done;
  }
  if(q==BP_Q_PAIR) {
   if(!bp.c.retiring) {
    if(!bp.witness_saved) {
     if(bp_witness_can_retry() && bp.witness_samples<11) ++bp.witness_samples;
     bool left=bp_save_witness(ctx,0,false); bool right=!bp.failed && bp_save_witness(ctx,1,false);
     bp.c.witness=(left?1u:0u)|(right?2u:0u);
     bp.witness_saved=bp.c.witness==3;
     if(!bp.witness_saved && !bp.failed && bp.source_kind==BP_SOURCE_DRESS &&
        bp.preview_near_adjusted && bp.witness_samples>=2 && !preview_use_authored_clip){
      preview_use_authored_clip=true;
      write_record("MS-IM-515","owned_context_transition","\"stage\":\"preview_restore_authored_clip\"");
     }
     if(!bp.witness_saved && !bp.failed && bp_witness_can_retry())
      bp.witness_retry_after=GetTickCount64()+500;

    }
    if(story_image_tick && story_image_tick==bp.c.pending.capture_tick && bp_witness_export_generation==bp.c.pending.generation){ (void)bp_save_witness(ctx,0,true); (void)bp_save_witness(ctx,1,true); }
    if(bp_pair_eligible(&bp.c,&bp.c.pending)) bp_commit_pair();
    else (void)bp_unlock_pair(0);
   } else (void)bp_unlock_pair(0);
   bp_bc_pair_processed(&bp.c.pending,bp.witness_saved,bp.last_committed_tick); story_image_tick=0;
  } else if(q==BP_Q_DRAIN) {
   bp_gpu_release(); bp_log("base_gpu_drained","normal_destroy_permitted"); goto done;
  } else if(q==BP_Q_PREPARE) bp_log("base_rt_validation","sentinel_fence_complete_enable_permitted");
  /* Never reuse the slot in the same Present that completed it. */
  goto done;
 }
 if(bp.c.query_failed) goto done;
 if(bp.c.retiring) {
  if(bp.c.disabled && !bp.c.quarantined && bp.c.phase==BP_RETIRING && bp.c.query==BP_Q_NONE) {
   if(!bp.fence) {
    D3D11_QUERY_DESC query={D3D11_QUERY_EVENT,0};
    if(FAILED(ID3D11Device_CreateQuery(dev,&query,&bp.fence))) { bp_fail("retire_query_create"); goto done; }
    if(!bp.gpu_device) { bp.gpu_device=dev; ID3D11Device_AddRef(dev); }
   }
   if(!bp_arm(&bp.c,BP_Q_DRAIN,0,GetTickCount64())) { bp_fail("retire_drain_state"); goto done; }
   ID3D11DeviceContext_End(ctx,(ID3D11Asynchronous*)bp.fence);
   bp_log("base_retire_fence","post_disable_pending");
  }
  goto done;
 }
 if(bp.c.phase==BP_PREPARE) { (void)bp_prepare_gpu(dev,ctx); goto done; }
 if(bp.c.phase!=BP_ACTIVE || GetTickCount64()-bp.observed_tick>200) goto done;
 if(bp.source_kind==BP_SOURCE_STORY){
  if(story_bake.draw_frames<3 || layer_trial_finished)goto done;
  if(bp_arm(&bp.c,BP_Q_PAIR,bp.c.observed_frame,GetTickCount64())){
   for(unsigned i=0;i<2;i++)ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)bp.readback[i],(ID3D11Resource*)bp.native[i]);
   ID3D11DeviceContext_End(ctx,(ID3D11Asynchronous*)bp.fence);
  }
  goto done;
 }
 if(bp.source_kind==BP_SOURCE_DRESS && bp.preview_near_adjusted && preview_use_authored_clip)goto done;
 if(bp_witness_can_retry() && !bp.witness_saved && bp.witness_retry_after &&
    (GetTickCount64()<bp.witness_retry_after || bp.c.observed_frame==bp.c.pending.frame)) goto done;
 if(bp_transport_pending(&bp_transport)) goto done;
 if(!ensure_shared_eye_pair(bp.native)) { bp_fail("owned_shared_pair_contract"); goto done; }
 HRESULT acquire_result[3];
 int acquired=bp_keyed_pair_acquire(shared_eye_km_game[0],shared_eye_km_game[1],0,acquire_result);
 if(acquired==BP_KM_BROKEN) {
  bp_transport_quarantine(&bp_transport);bp_fail("owned_pair_acquire_or_partial_release_failed");goto done;
 }
 if(acquired==BP_KM_PENDING) goto done;
 bp.c.mutex_owned=3;
 if(!bp_arm(&bp.c,BP_Q_PAIR,bp.c.observed_frame,GetTickCount64())) { (void)bp_unlock_pair(0); bp_fail("owned_pair_slot_contract"); goto done; }
 home_pending_views=(bp.source_kind==BP_SOURCE_HOME || bp.source_kind==BP_SOURCE_RACE || bp.source_kind==BP_SOURCE_DRESS)?bp.optics.render_views:(HomeRenderViews){0};
 memcpy(home_pending_views.fov,bp.optics.fov,sizeof(home_pending_views.fov));
 bool capture_story=story_image_requested(); if(capture_story){bp_witness_export_generation=bp.c.pending.generation;story_image_tick=bp.c.pending.capture_tick;}
 for(unsigned eye=0;eye<2;++eye) {
  ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)shared_eye_game[eye],(ID3D11Resource*)bp.native[eye]);
  if(!bp.witness_saved || capture_story) ID3D11DeviceContext_CopyResource(ctx,(ID3D11Resource*)bp.readback[eye],(ID3D11Resource*)shared_eye_game[eye]);
 }
 ID3D11DeviceContext_End(ctx,(ID3D11Asynchronous*)bp.fence);
 if(capture_story) bp_bc_arm(dev,ctx,backbuffer,&bp.c.pending,baseline_serial);
done:
 if(ctx) ID3D11DeviceContext_Release(ctx); if(dev) ID3D11Device_Release(dev);
 bp_bc_summary();
 ReleaseSRWLockExclusive(&bp_lock);
}




