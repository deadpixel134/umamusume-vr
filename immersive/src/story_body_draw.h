/* Inspect IA/OM bindings for one interval between game Presents. Draws themselves
   always use the original context, arguments and ordering. */
typedef void (STDMETHODCALLTYPE *StoryDrawFn)(ID3D11DeviceContext*,UINT,UINT,INT);
typedef void (STDMETHODCALLTYPE *StoryInstanceFn)(ID3D11DeviceContext*,UINT,UINT,UINT,INT,UINT);
static StoryDrawFn story_original_draw;
static StoryInstanceFn story_original_instance;
static void* story_draw_entries[2];
static SRWLOCK story_draw_lock=SRWLOCK_INIT;
static volatile LONG story_tracing,story_busy_drops;
static struct {
 ID3D11DeviceContext* context;
 ID3D11Buffer* body;
 ID3D11Texture2D* eyes[2];
 unsigned calls,hits,targets[4],used;
 uintptr_t target_ids[8];
 unsigned samples[8],width[8],height[8];
 uint64_t generation,deadline;bool expired;
} story_draw;

static void story_inspect_draw(ID3D11DeviceContext* ctx) {
 if(!InterlockedCompareExchange(&story_tracing,0,0))return;
 if(!TryAcquireSRWLockExclusive(&story_draw_lock)){InterlockedIncrement(&story_busy_drops);return;}
 if(ctx!=story_draw.context || !story_tracing)goto done;
 if(++story_draw.calls>8192)goto done;
 if(GetTickCount64()>=story_draw.deadline){story_draw.expired=true;goto done;}
 ID3D11Buffer* bound=NULL;DXGI_FORMAT format;UINT offset=0;
 ID3D11DeviceContext_IAGetIndexBuffer(ctx,&bound,&format,&offset);
 bool hit=bound && bound==story_draw.body;
 if(bound)ID3D11Buffer_Release(bound);
 if(!hit)goto done;
 ++story_draw.hits;
 ID3D11RenderTargetView* view=NULL;ID3D11Resource* resource=NULL;
 ID3D11DeviceContext_OMGetRenderTargets(ctx,1,&view,NULL);
 if(view){ID3D11RenderTargetView_GetResource(view,&resource);ID3D11RenderTargetView_Release(view);}
 unsigned bucket=!resource?3:resource==(ID3D11Resource*)story_draw.eyes[0]?0:resource==(ID3D11Resource*)story_draw.eyes[1]?1:2;
 ++story_draw.targets[bucket];
 if(resource){
  unsigned slot=0;
  for(;slot<story_draw.used;slot++)if(story_draw.target_ids[slot]==(uintptr_t)resource)break;
  if(slot<8){
   if(slot==story_draw.used){
    ++story_draw.used;story_draw.target_ids[slot]=(uintptr_t)resource;
    D3D11_RESOURCE_DIMENSION dimension;
    ID3D11Resource_GetType(resource,&dimension);
    if(dimension==D3D11_RESOURCE_DIMENSION_TEXTURE2D){
     D3D11_TEXTURE2D_DESC desc;ID3D11Texture2D_GetDesc((ID3D11Texture2D*)resource,&desc);
     story_draw.width[slot]=desc.Width;story_draw.height[slot]=desc.Height;
    }
   }
   ++story_draw.samples[slot];
  }
  ID3D11Resource_Release(resource);
 }
done:
 ReleaseSRWLockExclusive(&story_draw_lock);
}
static void STDMETHODCALLTYPE story_draw_indexed(ID3D11DeviceContext* ctx,UINT n,UINT first,INT base){
 story_inspect_draw(ctx);story_original_draw(ctx,n,first,base);
}
static void STDMETHODCALLTYPE story_draw_instanced(ID3D11DeviceContext* ctx,UINT n,UINT instances,UINT first,INT base,UINT start){
 story_inspect_draw(ctx);story_original_instance(ctx,n,instances,first,base,start);
}
static void story_stop_draw(void){
 if(!InterlockedExchange(&story_tracing,0))return;
 AcquireSRWLockExclusive(&story_draw_lock);
 for(unsigned i=0;i<story_draw.used;i++){
  char detail[224];
  telemetry_snprintf(detail,sizeof detail,"\"stage\":\"body_draw_target\",\"generation\":%llu,\"rtv_resource\":\"%p\",\"width\":%u,\"height\":%u,\"hits\":%u",
   (unsigned long long)story_draw.generation,(void*)story_draw.target_ids[i],story_draw.width[i],story_draw.height[i],story_draw.samples[i]);
  write_record("STORY","story_display",detail);
 }
 char detail[380];
 telemetry_snprintf(detail,sizeof detail,"\"stage\":\"body_draw_summary\",\"generation\":%llu,\"calls\":%u,\"limit\":8192,\"expired\":%s,\"contended\":%ld,\"matches\":%u,\"left\":%u,\"right\":%u,\"other\":%u,\"no_rtv0\":%u",
  (unsigned long long)story_draw.generation,story_draw.calls,story_draw.expired?"true":"false",(long)story_busy_drops,story_draw.hits,story_draw.targets[0],story_draw.targets[1],story_draw.targets[2],story_draw.targets[3]);
 write_record("STORY","story_display",detail);
 ID3D11DeviceContext_Release(story_draw.context);ID3D11Buffer_Release(story_draw.body);
 for(unsigned i=0;i<2;i++)ID3D11Texture2D_Release(story_draw.eyes[i]);
 ReleaseSRWLockExclusive(&story_draw_lock);
 /* Disable only our two entries. Keep trampolines valid for calls in flight. */
 for(unsigned i=0;i<2;i++)(void)MH_DisableHook(story_draw_entries[i]);
 InterlockedExchange(&layer_trial_finished,1);
}
static bool story_start_draw(ID3D11Device* device,ID3D11DeviceContext* ctx){
 if(!story_body_buffer)return false;
 ID3D11Device* owner=NULL;ID3D11Buffer_GetDevice(story_body_buffer,&owner);
 bool same=owner==device;if(owner)ID3D11Device_Release(owner);
 if(!same){story_lookup_result="mesh_device_mismatch";return false;}
 story_draw_entries[0]=(void*)ctx->lpVtbl->DrawIndexed;
 story_draw_entries[1]=(void*)ctx->lpVtbl->DrawIndexedInstanced;
 if(MH_CreateHook(story_draw_entries[0],story_draw_indexed,(void**)&story_original_draw)!=MH_OK ||
    MH_CreateHook(story_draw_entries[1],story_draw_instanced,(void**)&story_original_instance)!=MH_OK){
  story_lookup_result="draw_observer_unavailable";return false;
 }
 if(MH_EnableHook(story_draw_entries[0])!=MH_OK){story_lookup_result="draw_observer_enable";return false;}
 if(MH_EnableHook(story_draw_entries[1])!=MH_OK){
  (void)MH_DisableHook(story_draw_entries[0]);story_lookup_result="instance_observer_enable";return false;
 }
 story_draw.context=ctx;ID3D11DeviceContext_AddRef(ctx);
 story_draw.body=story_body_buffer;ID3D11Buffer_AddRef(story_draw.body);
 for(unsigned i=0;i<2;i++){story_draw.eyes[i]=bp.native[i];ID3D11Texture2D_AddRef(story_draw.eyes[i]);}
 story_draw.deadline=GetTickCount64()+1000;story_draw.generation=bp.c.identity.generation;
 InterlockedExchange(&story_tracing,1);return true;
}
