/* XR-thread-local observation. This state never controls render eligibility. */
#include <stdint.h>
typedef struct StereoTelemetry {
    unsigned long long frame_id;
    long long predicted_time,render_pose_time;
    unsigned image, eye_mask, layer_count;
    long generation;
    long long pair_serial;
    BOOL cache_reused, stereo_layer, release_called, end_called;
    long release_result;
} StereoTelemetry;
typedef struct StereoCacheTelemetry {
 long generation; long long pair_serial;
 LONG render_percent;
 BOOL owned; uint64_t source,pose,config,capture_tick; int32_t scene;
} StereoCacheTelemetry;
static void stereo_observe_cache(StereoCacheTelemetry* cache, long generation, long long serial) {
    cache->generation=generation; cache->pair_serial=serial;
    cache->owned=FALSE; cache->source=cache->pose=cache->config=cache->capture_tick=0;
    cache->scene=0;
}
static void stereo_observe_draw(StereoTelemetry* frame,const StereoCacheTelemetry* cache,
        unsigned image,BOOL reused) {
    frame->image=image; frame->eye_mask=3; frame->generation=cache->generation;
    frame->pair_serial=cache->pair_serial; frame->cache_reused=reused;
}
static BOOL stereo_end_accepted(const StereoTelemetry* f,long result) {
    /* Exact XR_SUCCESS: qualified success such as XR_FRAME_DISCARDED is not acceptance. */
    return f->end_called && result==0 && f->stereo_layer && f->eye_mask==3 && f->layer_count>0 &&
        f->release_called && f->release_result==0;
}
static void stereo_telemetry_detail(const StereoTelemetry* f,long end_result,char* out,size_t cap) {
    telemetry_snprintf(out,cap,
        "\"frame_id\":%llu,\"predicted_display_time\":%lld,\"swapchain_image\":%u,"
        "\"eye_mask\":%u,\"generation\":%ld,\"pair_serial\":%lld,\"cache_reused\":%s,"
        "\"render_pose_time\":%lld,"
        "\"physical_tracking_space\":\"LOCAL\",\"projection_space\":\"%s\",\"stereo_layer\":%s,\"layer_count\":%u,"
        "\"release_called\":%s,\"release_result\":%ld,\"end_called\":%s,\"end_result\":%ld",
        f->frame_id,f->predicted_time,f->image,f->eye_mask,f->generation,f->pair_serial,
        f->cache_reused?"true":"false",f->render_pose_time,f->render_pose_time?"LOCAL":"VIEW",f->stereo_layer?"true":"false",f->layer_count,
        f->release_called?"true":"false",f->release_result,f->end_called?"true":"false",end_result);
}
