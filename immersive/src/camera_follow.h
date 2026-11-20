/* XR owns mode changes. Unity owns each world-space anchor. A zero token
   preserves the original context default until the user chooses a mode. */
static volatile LONG camera_follow_token;
typedef struct CameraFollowAnchor {
 LONG token;
 int32_t scene;
 Vec3 position;
 Quat rotation;
} CameraFollowAnchor;
static LONG toggle_camera_follow(BOOL default_follow){
 LONG token=InterlockedCompareExchange(&camera_follow_token,0,0);
 LONG next=token==0?(default_follow?1:2):token==INT32_MAX?2:token+1;
 InterlockedExchange(&camera_follow_token,next);
 return next;
}
/* XR input thread. Advance the anchor revision without changing follow mode. */
static void refresh_frozen_camera_anchor(void){
 LONG token=InterlockedCompareExchange(&camera_follow_token,0,0);
 if(token&1)InterlockedExchange(&camera_follow_token,token==INT32_MAX?1:token+2);
}
static void apply_camera_anchor(CameraFollowAnchor* anchor,LONG token,int32_t scene,
                                Vec3* position,Quat* rotation){
 if(!(token&1))return;
 if(anchor->token!=token || anchor->scene!=scene){
  anchor->position=*position;anchor->rotation=*rotation;anchor->scene=scene;anchor->token=token;
 }
 *position=anchor->position;*rotation=anchor->rotation;
}
