#define WIN32_LEAN_AND_MEAN
#define CINTERFACE
#define COBJMACROS
#define XR_USE_GRAPHICS_API_D3D11
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "MinHook.h"

#define PROBE_ID "IMMERSIVE-001"
#define RECORD_LIMIT 256
#define LOG_LIMIT 65536
#define LINE_LIMIT 2048
#define MAX_EYES 2
#define MAX_IMAGES 8
#define PANEL_DISTANCE_M 4.0f
#define PANEL_SAFE_FRACTION 0.375f
#define AUX_PANEL_HEIGHT_M 0.55f
#define AUX_PANEL_UP_OFFSET_M 0.18f
#define AUX_PANEL_FORWARD_OFFSET_M 0.32f
#define CURSOR_HALF_SIZE_FRACTION 0.018f
#define CURSOR_SURFACE_OFFSET_M 0.003f
#define NAV_LOCOMOTION_SPEED_DEFAULT_MPS 5.0f
#define NAV_LOCOMOTION_SCALE_REFERENCE 2.0f
#define NAV_STICK_DEADZONE 0.25f
#define NAV_SNAP_THRESHOLD 0.75f
#define NAV_SNAP_RELEASE 0.45f
#define NAV_SNAP_ANGLE_DEFAULT_RAD 0.52359878f
#define NAV_ARTIFICIAL_PITCH_LIMIT_RAD 1.39626340f
#define EYE_RENDER_SCALE_MIN 0.50f
#define EYE_RENDER_SCALE_MAX 1.50f
#define EYE_RENDER_SCALE_EXPENSIVE 1.25f
#define WORLD_SCALE_DEFAULT 1.0f
#define WORLD_SCALE_MIN 0.55f
#ifndef D3D11_RESOURCE_MISC_SHARED_KEYEDHANDLE
#define D3D11_RESOURCE_MISC_SHARED_KEYEDHANDLE 0x100
#endif

typedef HRESULT (STDMETHODCALLTYPE *present_fn)(IDXGISwapChain*, UINT, UINT);

static const GUID iid_d3d11_texture2d = {0x6f15aaf2,0xd208,0x4e89,{0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};
static const GUID iid_dxgi_resource = {0x035f3ab4,0x482e,0x4e50,{0xb4,0x1f,0x8a,0x7f,0x8b,0xd8,0x96,0x0b}};
static const GUID iid_dxgi_keyed_mutex = {0x9d8e1289,0xd7b3,0x465f,{0x81,0x26,0x25,0x0e,0x34,0x9a,0xf8,0x5d}};
static const GUID iid_dxgi_factory1 = {0x770aae78,0xf26f,0x4dba,{0xa8,0x29,0x25,0x3c,0x83,0xd1,0xb3,0x87}};

static SRWLOCK log_lock = SRWLOCK_INIT;
static WCHAR log_path[MAX_PATH];
static volatile LONG records_written;
static volatile LONG fault_logged;
static volatile ULONGLONG produced_frames;
static BOOL fast_mode;
static HMODULE g_module;
static ULONGLONG attach_tick;

/* ---------------- telemetry ---------------- */

#include "telemetry.h"
#include "telemetry_frame.h"
/* Published with the texture pair while both game-side keyed mutexes are held. */
static StereoCacheTelemetry game_pair_telemetry;

static void write_disabled(const char* stage, const char* extra) {
    char detail[512], escaped[256];
    json_escape(stage, escaped, sizeof(escaped));
    telemetry_snprintf(detail, sizeof(detail), "\"status\":\"disabled\",\"stage\":\"%s\"%s%s",
        escaped, extra ? "," : "", extra ? extra : "");
    write_record(PROBE_ID, "disabled", detail);
}

/* ================= Unity/IL2CPP authored source adapter =================
   Whole-owner runtime cloning is forbidden by FAIL-014. The authoritative Live
   camera keeps its accepted synchronous eye route. Home uses a distinct route:
   its normal engine frame advances CENTER/LEFT/RIGHT through the proved
   HomeCameraController LateUpdate and GallopImageEffectOnRenderImage completion
   callbacks. LEFT/RIGHT post-effect results are copied into persistent eye
   textures, then the latest CENTER result is restored to the original output.
   No Home Camera.Render re-entry or Present-thread Unity API is permitted.
   RenderTextures stay game-device-owned; Present copies only a complete pair.
   A generation token prevents retired output from surviving an owner change. */

typedef struct Il2CppDomain Il2CppDomain;
typedef struct Il2CppAssembly Il2CppAssembly;
typedef struct Il2CppImage Il2CppImage;
typedef struct Il2CppClass Il2CppClass;
typedef struct Il2CppType Il2CppType;
typedef struct FieldInfo FieldInfo;
typedef struct Il2CppThread Il2CppThread;
typedef struct Il2CppReflectionType Il2CppReflectionType;
typedef void (*Il2CppMethodPointer)(void);
typedef struct MethodInfo { Il2CppMethodPointer methodPointer; } MethodInfo;
typedef struct Il2CppObject { Il2CppClass* klass; void* monitor; } Il2CppObject;
typedef struct Il2CppString { Il2CppObject object; int32_t length; uint16_t chars[1]; } Il2CppString;
typedef struct Il2CppArray { Il2CppObject object; void* bounds; uintptr_t max_length; Il2CppObject* vector[1]; } Il2CppArray;
#include "request_dispatch.h"
#include "end_frame_bridge.h"
static RdApi request_dispatch_api;
static RdBinding request_dispatch_binding;
static EfBridge end_frame_bridge;
static void report_end_frame_callback(const EfSnapshot* s) {
    char detail[1024];
    telemetry_snprintf(detail, sizeof(detail),
        "\"status\":\"%s\",\"event_result\":\"%s\",\"owner_thread\":%lu,\"callback_thread\":%lu,"
        "\"registration_attempted\":%ld,\"registration_returned\":%ld,\"disarmed\":%ld,"
        "\"frame_available\":%ld,\"frame\":%ld,\"callbacks\":%lld,\"wrong_thread\":%lld,"
        "\"nested\":%lld,\"repeated\":%lld,\"late\":%lld,\"inline\":%lld,\"own_render_depth\":%ld,"
        "\"phase_verified\":false,\"entry_verified\":false,\"tokens_issued\":0,\"producer_called\":false",
        ef_status_name(s->state), ef_status_name(s->event), (unsigned long)s->owner_thread,
        (unsigned long)s->last_thread, s->registration_attempted, s->registration_returned, s->disarmed,
        s->frame_available, s->frame, (long long)s->callbacks, (long long)s->wrong_thread,
        (long long)s->nested, (long long)s->repeated, (long long)s->late, (long long)s->inline_callbacks, s->own_render_depth);
    write_record("IMMERSIVE-SOURCE", "scheduler_callback_bridge", detail);
}
/* This boundary is deliberately unused until an owned-eye adapter supplies
   evidenced request support, non-reentrant entry and completion ownership. */
typedef struct Scene { int32_t handle; } Scene;
typedef struct Vec3 { float x, y, z; } Vec3;
typedef struct Quat { float x, y, z, w; } Quat;
typedef struct Matrix4x4 { float m[16]; } Matrix4x4;

typedef Il2CppDomain* (*domain_get_fn)(void);
typedef const Il2CppAssembly** (*domain_get_assemblies_fn)(const Il2CppDomain*, size_t*);
typedef const Il2CppImage* (*assembly_get_image_fn)(const Il2CppAssembly*);
typedef const char* (*image_get_name_fn)(const Il2CppImage*);
typedef Il2CppClass* (*class_from_name_fn)(const Il2CppImage*, const char*, const char*);
typedef const MethodInfo* (*class_get_method_fn)(Il2CppClass*, const char*, int);
typedef const char* (*class_get_name_fn)(Il2CppClass*);
typedef const char* (*class_get_namespace_fn)(Il2CppClass*);
typedef const Il2CppType* (*class_get_type_fn)(Il2CppClass*);
typedef FieldInfo* (*class_get_fields_fn)(Il2CppClass*, void**);
typedef const MethodInfo* (*class_get_methods_fn)(Il2CppClass*, void**);
typedef Il2CppClass* (*class_get_parent_fn)(Il2CppClass*);
typedef const char* (*field_get_name_fn)(FieldInfo*);
typedef const Il2CppType* (*field_get_type_fn)(FieldInfo*);
typedef int32_t (*type_get_type_fn)(const Il2CppType*);
typedef void (*field_get_value_fn)(Il2CppObject*, FieldInfo*, void*);
typedef void (*field_set_value_fn)(Il2CppObject*, FieldInfo*, void*);
typedef Il2CppObject* (*field_get_value_object_fn)(FieldInfo*, Il2CppObject*);
typedef void* (*object_unbox_fn)(Il2CppObject*);
typedef const char* (*method_get_name_fn)(const MethodInfo*);
typedef const Il2CppType* (*method_get_return_type_fn)(const MethodInfo*);
typedef Il2CppReflectionType* (*type_get_object_fn)(const Il2CppType*);
typedef Il2CppObject* (*object_new_fn)(Il2CppClass*);
typedef Il2CppMethodPointer (*resolve_icall_fn)(const char*);
typedef Il2CppThread* (*thread_current_fn)(void);
typedef Scene (*get_active_scene_fn)(const MethodInfo*);
typedef Il2CppString* (*get_scene_name_fn)(int32_t);
typedef Il2CppArray* (*find_objects_fn)(Il2CppReflectionType*, int32_t, int32_t);
typedef Il2CppObject* (*get_object_fn)(Il2CppObject*);
typedef Il2CppArray* (*get_components_internal_fn)(Il2CppObject*, Il2CppReflectionType*, int32_t, int32_t, int32_t, int32_t, Il2CppObject*);
typedef float (*get_float_fn)(Il2CppObject*);
typedef int32_t (*get_int_fn)(Il2CppObject*);
typedef void (*set_object_fn)(Il2CppObject*, Il2CppObject*, const MethodInfo*);
typedef void (*set_float_fn)(Il2CppObject*, float, const MethodInfo*);
typedef void (*rt_ctor_fn)(Il2CppObject*, int32_t, int32_t, int32_t, int32_t, const MethodInfo*);
typedef int32_t (*rt_create_fn)(Il2CppObject*, const MethodInfo*);
typedef void* (*native_texture_icall_fn)(Il2CppObject*);
typedef void (*camera_render_fn)(Il2CppObject*, const MethodInfo*);
typedef void (*home_late_update_fn)(Il2CppObject*, const MethodInfo*);
typedef void (*home_on_render_fn)(Il2CppObject*, Il2CppObject*, Il2CppObject*, const MethodInfo*);
typedef void (*graphics_blit_fn)(Il2CppObject*, Il2CppObject*, const MethodInfo*);
typedef void (*get_vec3_injected_fn)(Il2CppObject*, Vec3*);
typedef void (*get_quat_injected_fn)(Il2CppObject*, Quat*);
typedef void (*get_matrix4x4_injected_fn)(Il2CppObject*, Matrix4x4*);
typedef void (*set_vec3_injected_fn)(Il2CppObject*, const Vec3*);
typedef void (*set_quat_injected_fn)(Il2CppObject*, const Quat*);
typedef void (*set_matrix4x4_injected_fn)(Il2CppObject*, const Matrix4x4*);
typedef void (*destroy_object_fn)(Il2CppObject*, const MethodInfo*);

typedef struct HomeRenderViews {
    XrPosef eye[2];
    XrFovf fov[2];
    XrSpace space;
    XrTime time;
} HomeRenderViews;
static HomeRenderViews home_pair_views, home_cached_views;
typedef struct EyeOptics {
    HomeRenderViews render_views;
    XrFovf fov[2];
    float half_ipd_m;
    XrPosef origin_center;
    XrPosef current_center;
    XrQuaternionf return_orientation;
    BOOL return_orientation_valid;
    Vec3 artificial_position;
    float artificial_yaw;
    float artificial_pitch;
    float navigation_base_yaw;
    float navigation_base_pitch;
    BOOL origin_valid;
} EyeOptics;

static SRWLOCK eye_source_lock = SRWLOCK_INIT;
/* Unity thread only. Live temporarily moves the authored camera for each eye. */
static struct {Vec3 center; Quat rotation; EyeOptics optics;} live_contact_view;
static volatile LONG requested_eye_width;
static volatile LONG requested_eye_height;
static volatile LONG source_generation;
/* LIVE source retirement proof is independent of shared-pair publication.
   Only authored-source invalidation advances this monotonic watermark. */
static volatile LONG live_retired_through;
static LONG current_authored_generation;
static volatile LONG published_eye_generation;
static volatile LONG64 eye_pair_serial;
static volatile LONG64 eye_pair_tick;
static volatile LONG64 source_ready_after_present;
static volatile LONG64 eye_source_pair_serial;
static LONG64 game_published_source_serial;
static volatile LONG eye_pair_capture_ready;
static ID3D11Texture2D* eye_native_game[2];
static Il2CppObject* eye_render_texture[2];
static volatile LONG published_flat_generation;
static volatile LONG requested_flat_width;
static volatile LONG requested_flat_height;
static ID3D11Texture2D* flat_native_game;
static Il2CppObject* flat_render_texture;
static LONG flat_render_width;
static LONG flat_render_height;
static Il2CppObject* current_authored_owner;
enum AuthoredOwnerKind { AUTHORED_OWNER_NONE = 0, AUTHORED_OWNER_LIVE = 1, AUTHORED_OWNER_HOME = 2 };
enum HomeTemporalPhase { HOME_PHASE_CENTER = 0, HOME_PHASE_LEFT = 1, HOME_PHASE_RIGHT = 2, HOME_PHASE_NONE = -1 };
static enum AuthoredOwnerKind current_owner_kind;
static int32_t current_scene_handle = INT32_MIN;
static volatile LONG eye_render_in_progress;
static Vec3 authored_pose_anchor_position;
static Quat authored_pose_anchor_rotation;
static BOOL authored_pose_anchor_valid;
static BOOL live_camera_follow;
#include "camera_follow.h"
static CameraFollowAnchor live_follow_anchor;
static volatile LONG return_camera_generation, follow_display_state, camera_follow_default_on=1;
static BOOL live_camera_follow_setting_loaded;
static float eye_render_scale = 1.0f;
static BOOL eye_render_scale_setting_loaded;
static float world_scale = WORLD_SCALE_DEFAULT;
static BOOL world_scale_setting_loaded;
static BOOL nav_locomotion_enabled = TRUE;
static float nav_locomotion_speed_mps = NAV_LOCOMOTION_SPEED_DEFAULT_MPS;
static BOOL nav_locomotion_scale_compensation_enabled = TRUE;
static BOOL nav_snap_turn_enabled = TRUE;
static float nav_snap_angle_rad = NAV_SNAP_ANGLE_DEFAULT_RAD;
/* Persisted as navigationHandsSwapped for schema-v3 compatibility, but the
   product contract swaps the complete primary/secondary controller roles. */
static BOOL controller_hands_swapped;
static BOOL swap_move_sticks=TRUE, grab_panel_hand, grab_with_trigger;
static float move_deadzone=NAV_STICK_DEADZONE;
static BOOL post_processing_enabled = TRUE;
static BOOL blur_effect_enabled = TRUE;
static BOOL depth_of_field_enabled = FALSE;
static BOOL diffusion_effect_enabled = TRUE;
static BOOL bloom_effect_enabled = TRUE;
static BOOL global_fog_effect_enabled = TRUE;
static BOOL lens_distortion_effect_enabled = TRUE;
static BOOL radial_blur_effect_enabled = TRUE;
static BOOL sun_shafts_effect_enabled = TRUE;
static BOOL indirect_light_shafts_effect_enabled = TRUE;
static BOOL transmitted_light_effect_enabled = TRUE;
static BOOL dof_diffusion_bloom_overlay_enabled = TRUE;
static BOOL tilt_shift_effect_enabled = TRUE;
static BOOL fluctuation_effect_enabled = TRUE;
static BOOL chromatic_aberration_effect_enabled = TRUE;
static BOOL tone_curve_effect_enabled = TRUE;
static BOOL exposure_effect_enabled = TRUE;
static BOOL color_correction_effect_enabled = TRUE;
static BOOL color_grading_effect_enabled = TRUE;
static BOOL bg_blur_effect_enabled = TRUE;
static BOOL vortex_effect_enabled = TRUE;
static BOOL film_roll_effect_enabled = TRUE;
static BOOL hatching_effect_enabled = TRUE;
static BOOL letter_box_effect_enabled = TRUE;
static BOOL rain_splash_effect_enabled = TRUE;
typedef struct { int id,group; const char* key; BOOL* game_value; const WCHAR *ko,*en,*ja; } VfxOption;
static const VfxOption live_vfx_options[]={
    {123,0,"postProcessingEnabled",&post_processing_enabled,L"전체 후처리",L"All post-processing",L"全体の後処理"},
    {122,0,NULL,NULL,L"오라",L"Aura",L"オーラ"},
    {101,1,"blurEnabled",&blur_effect_enabled,L"블러",L"Blur",L"ブラー"},
    {105,1,"dofDiffusionBloomOverlayEnabled",&dof_diffusion_bloom_overlay_enabled,L"DoF·확산·블룸 묶음",L"DoF / diffusion / bloom",L"被写界深度・拡散・ブルーム"},
    {105,1,"depthOfFieldEnabled",&depth_of_field_enabled,L"피사계 심도 (부분)",L"Depth of field (partial)",L"被写界深度（一部）"},
    {105,1,"diffusionEnabled",&diffusion_effect_enabled,L"확산",L"Diffusion",L"拡散"},
    {105,1,"bloomEnabled",&bloom_effect_enabled,L"블룸",L"Bloom",L"ブルーム"},
    {106,1,"tiltShiftEnabled",&tilt_shift_effect_enabled,L"틸트 시프트",L"Tilt shift",L"チルトシフト"},
    {107,1,"radialBlurEnabled",&radial_blur_effect_enabled,L"방사형 블러",L"Radial blur",L"放射状ブラー"},
    {115,1,"bgBlurEnabled",&bg_blur_effect_enabled,L"배경 블러",L"Background blur",L"背景ブラー"},
    {102,2,"sunShaftsEnabled",&sun_shafts_effect_enabled,L"햇살 광선",L"Sun shafts",L"太陽光線"},
    {103,2,"indirectLightShaftsEnabled",&indirect_light_shafts_effect_enabled,L"간접 광선",L"Indirect shafts",L"間接光線"},
    {104,2,"transmittedLightEnabled",&transmitted_light_effect_enabled,L"투과광",L"Transmitted light",L"透過光"},
    {121,2,"globalFogEnabled",&global_fog_effect_enabled,L"안개",L"Global fog",L"フォグ"},
    {110,3,"chromaticAberrationEnabled",&chromatic_aberration_effect_enabled,L"색수차",L"Chromatic aberration",L"色収差"},
    {111,3,"toneCurveEnabled",&tone_curve_effect_enabled,L"톤 커브",L"Tone curve",L"トーンカーブ"},
    {112,3,"exposureEnabled",&exposure_effect_enabled,L"노출",L"Exposure",L"露出"},
    {113,3,"colorCorrectionEnabled",&color_correction_effect_enabled,L"색 보정",L"Color correction",L"色補正"},
    {114,3,"colorGradingEnabled",&color_grading_effect_enabled,L"색 그레이딩",L"Color grading",L"カラーグレーディング"},
    {108,4,"fluctuationEnabled",&fluctuation_effect_enabled,L"화면 흔들림 효과",L"Fluctuation",L"揺らぎ"},
    {109,4,"lensDistortionEnabled",&lens_distortion_effect_enabled,L"렌즈 왜곡",L"Lens distortion",L"レンズ歪み"},
    {116,4,"vortexEnabled",&vortex_effect_enabled,L"소용돌이",L"Vortex",L"渦"},
    {117,4,"filmRollEnabled",&film_roll_effect_enabled,L"필름 롤",L"Film roll",L"フィルムロール"},
    {118,4,"hatchingEnabled",&hatching_effect_enabled,L"해칭",L"Hatching",L"ハッチング"},
    {119,4,"letterBoxEnabled",&letter_box_effect_enabled,L"레터박스",L"Letterbox",L"レターボックス"},
    {120,4,"rainSplashEnabled",&rain_splash_effect_enabled,L"빗방울",L"Rain splash",L"雨しぶき"},
};
enum { VFX_COUNT=sizeof(live_vfx_options)/sizeof(live_vfx_options[0]) };
_Static_assert(VFX_COUNT<31,"VFX options must fit a single atomic snapshot");
static volatile LONG live_vfx_mask,home_vfx_allowed=TRUE,home_vfx_disabled;
static BOOL read_home_vfx_mask(const char* text,LONG* mask){
    const char* key="\"homeVfxDisabled\"";
    const char* p=strstr(text,key);
    if(!p){*mask=0;return TRUE;}
    p+=strlen(key);
    if(strstr(p,key))return FALSE;
    p+=strspn(p," \t\r\n");
    if(*p++!=':')return FALSE;
    p+=strspn(p," \t\r\n");
    if(*p<'0'||*p>'9')return FALSE;
    char* end;
    long number=strtol(p,&end,10);
    end+=strspn(end," \t\r\n");
    if((*end!=',' && *end!='}') || number<0 || number>=1L<<VFX_COUNT)return FALSE;
    *mask=number;
    return TRUE;
}
static BOOL vfx_log_change;
static uint32_t live_vfx_camera_root;
static BOOL parse_json_bool_setting(const char* text, const char* key, BOOL* value);
static BOOL parse_json_float_setting(const char* text, const char* key, float* value);
/* The first LOCAL pose is available while the user is still looking at the
   viewer-locked panel.  It is only provisional: the physical origin becomes
   authoritative at the first immersive entry and then survives source
   replacement for the rest of the XR session. */
static BOOL immersive_entry_origin_committed;

typedef struct HomeCameraSnapshot {
    Vec3 position;
    Quat rotation;
    Matrix4x4 projection;
    float aspect;
    LONG generation;
    enum HomeTemporalPhase phase;
    BOOL valid;
} HomeCameraSnapshot;

static Il2CppObject* current_home_controller;
static Il2CppObject* home_center_render_texture;
static LONG home_center_width;
static LONG home_center_height;
static BOOL home_center_ready;
static enum HomeTemporalPhase home_next_phase = HOME_PHASE_CENTER;
static enum HomeTemporalPhase home_active_phase = HOME_PHASE_NONE;
static LONG home_active_generation;
static unsigned home_eye_completion_mask;
static HomeCameraSnapshot home_camera_snapshot;
static volatile LONG home_adapter_ready;
static volatile LONG home_pair_events;

static class_get_name_fn il2cpp_class_get_name;
static class_get_method_fn p_class_get_method;
static class_get_namespace_fn il2cpp_class_get_namespace;
static class_get_fields_fn il2cpp_class_get_fields;
static class_get_methods_fn il2cpp_class_get_methods;
static class_get_parent_fn il2cpp_class_get_parent;
static field_get_name_fn il2cpp_field_get_name;
static field_get_type_fn il2cpp_field_get_type;
static type_get_type_fn il2cpp_type_get_type;
static field_get_value_fn il2cpp_field_get_value;
static field_set_value_fn il2cpp_field_set_value;
static field_get_value_object_fn il2cpp_field_get_value_object;
static object_unbox_fn il2cpp_object_unbox;
static method_get_name_fn il2cpp_method_get_name;
static method_get_return_type_fn il2cpp_method_get_return_type;
typedef struct VfxFieldOverride {
    Il2CppObject* parameter;
    FieldInfo* effect_field;
    FieldInfo* value_field;
    uint8_t saved_value[8];
    uint8_t value_size;
    BOOL applied;
} VfxFieldOverride;
typedef struct VfxMasterOverride {
    Il2CppObject* parameter;
    FieldInfo* enabled_field;
    uint8_t saved_value;
    BOOL applied;
} VfxMasterOverride;
static VfxMasterOverride vfx_master_override;
static VfxFieldOverride vfx_overrides[32];
static find_objects_fn unity_find_objects;
static get_object_fn component_get_game_object;
static get_object_fn component_get_transform;
static get_components_internal_fn gameobject_get_components;
static get_active_scene_fn frame_get_active_scene;
static const MethodInfo *frame_scene_method,*frame_tasks_method;
static void (*original_execute_tasks)(const MethodInfo*);
static int (*scene_frame_count)(void);
static get_scene_name_fn scene_get_name;
static get_float_fn camera_get_depth;
static get_float_fn camera_get_aspect;
static get_float_fn camera_get_near_clip;
static get_float_fn camera_get_far_clip;
static get_int_fn texture_get_width;
static get_int_fn texture_get_height;
static get_object_fn camera_get_target;
static Il2CppReflectionType* camera_reflection_type;
static Il2CppReflectionType* component_reflection_type;
static Il2CppClass* rendertexture_class;
static const MethodInfo* mi_camera_set_target;
static const MethodInfo* mi_camera_set_aspect;
static const MethodInfo* mi_camera_set_near_clip;
static const MethodInfo* mi_camera_render;
static const MethodInfo* mi_rt_ctor;
static const MethodInfo* mi_rt_create;
static native_texture_icall_fn get_native_texture;
static const MethodInfo* mi_object_destroy;
static const MethodInfo* mi_graphics_blit;
static home_late_update_fn original_home_late_update;
static home_on_render_fn original_home_on_render;
static object_new_fn il2cpp_object_new;
static get_vec3_injected_fn transform_get_position;
static get_quat_injected_fn transform_get_rotation;
static get_matrix4x4_injected_fn camera_get_projection_matrix;
static set_vec3_injected_fn transform_set_position;
static set_quat_injected_fn transform_set_rotation;
static set_matrix4x4_injected_fn camera_set_projection_matrix;
static SRWLOCK eye_optics_lock = SRWLOCK_INIT;
static EyeOptics eye_optics;
static volatile LONG eye_optics_ready;
static volatile LONG unity_adapter_ready;
static ULONGLONG last_source_update_tick;

static BOOL snapshot_eye_optics(EyeOptics* out) {
    if (!out || !InterlockedCompareExchange(&eye_optics_ready, 0, 0)) return FALSE;
    AcquireSRWLockShared(&eye_optics_lock);
    *out = eye_optics;
    ReleaseSRWLockShared(&eye_optics_lock);
    return TRUE;
}

/* Unity Camera.projectionMatrix uses the conventional right-handed camera
   projection. Unity performs the backend/render-target conversion; the raw
   RenderTexture orientation is corrected only at the XR-device copy. */
static void build_unity_eye_projection(
        const XrFovf* fov, float znear, float zfar, Matrix4x4* out) {
    float l = tanf(fov->angleLeft), r = tanf(fov->angleRight);
    float b = tanf(fov->angleDown), t = tanf(fov->angleUp);
    ZeroMemory(out, sizeof(*out));
    out->m[0] = 2.0f / (r - l);
    out->m[5] = 2.0f / (t - b);
    out->m[8] = (r + l) / (r - l);
    out->m[9] = (t + b) / (t - b);
    out->m[10] = -(zfar + znear) / (zfar - znear);
    out->m[11] = -1.0f;
    out->m[14] = -(2.0f * zfar * znear) / (zfar - znear);
}

static Quat quat_normalized(Quat q) {
    float length = sqrtf(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    if (!isfinite(length) || length < 0.000001f) return (Quat){0, 0, 0, 1};
    float inverse = 1.0f / length;
    return (Quat){q.x*inverse, q.y*inverse, q.z*inverse, q.w*inverse};
}

static Quat quat_conjugate(Quat q) { return (Quat){-q.x, -q.y, -q.z, q.w}; }

static Quat quat_multiply(Quat a, Quat b) {
    return quat_normalized((Quat){
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z});
}

static Vec3 quat_rotate(Quat q, Vec3 v) {
    q = quat_normalized(q);
    Vec3 t = {2.0f*(q.y*v.z - q.z*v.y), 2.0f*(q.z*v.x - q.x*v.z),
              2.0f*(q.x*v.y - q.y*v.x)};
    return (Vec3){v.x + q.w*t.x + (q.y*t.z - q.z*t.y),
                  v.y + q.w*t.y + (q.z*t.x - q.x*t.z),
                  v.z + q.w*t.z + (q.x*t.y - q.y*t.x)};
}

static void quat_to_yaw_pitch_roll(Quat q, float* yaw, float* pitch, float* roll) {
    q = quat_normalized(q);
    float m02 = 2.0f*(q.x*q.z + q.w*q.y);
    float m12 = 2.0f*(q.y*q.z - q.w*q.x);
    float m22 = 1.0f - 2.0f*(q.x*q.x + q.y*q.y);
    float m10 = 2.0f*(q.x*q.y + q.w*q.z);
    float m11 = 1.0f - 2.0f*(q.x*q.x + q.z*q.z);
    if (yaw) *yaw = atan2f(m02, m22);
    if (pitch) *pitch = asinf(fmaxf(-1.0f, fminf(1.0f, -m12)));
    if (roll) *roll = atan2f(m10, m11);
}

static Quat quat_from_yaw_pitch_roll(float yaw, float pitch, float roll) {
    float hy = yaw*0.5f, hp = pitch*0.5f, hr = roll*0.5f;
    Quat qy = {0, sinf(hy), 0, cosf(hy)};
    Quat qp = {sinf(hp), 0, 0, cosf(hp)};
    Quat qr = {0, 0, sinf(hr), cosf(hr)};
    return quat_multiply(quat_multiply(qy, qp), qr);
}

static Quat xr_to_unity_quat(XrQuaternionf q) {
    return quat_normalized((Quat){-q.x, -q.y, q.z, q.w});
}

static float shortest_angle_delta(float origin, float current) {
    float difference = current - origin;
    return atan2f(sinf(difference), cosf(difference));
}

static void physical_orientation_deltas(const EyeOptics* optics,
        float* yaw, float* pitch, float* roll) {
    Quat origin = xr_to_unity_quat(optics->origin_center.orientation);
    Quat current = xr_to_unity_quat(optics->current_center.orientation);
    if(optics->return_orientation_valid) {
        float neutral_yaw, neutral_pitch, current_yaw, current_pitch;
        quat_to_yaw_pitch_roll(xr_to_unity_quat(optics->return_orientation), &neutral_yaw, &neutral_pitch, NULL);
        quat_to_yaw_pitch_roll(current, &current_yaw, &current_pitch, roll);
        if(yaw) *yaw=shortest_angle_delta(neutral_yaw,current_yaw);
        if(pitch) *pitch=shortest_angle_delta(neutral_pitch,current_pitch);
        return;
    }
    float origin_yaw, origin_pitch, origin_roll;
    float current_yaw, current_pitch, current_roll;
    quat_to_yaw_pitch_roll(origin, &origin_yaw, &origin_pitch, &origin_roll);
    quat_to_yaw_pitch_roll(current, &current_yaw, &current_pitch, &current_roll);
    if (yaw) *yaw = shortest_angle_delta(origin_yaw, current_yaw);
    if (pitch) *pitch = shortest_angle_delta(origin_pitch, current_pitch);
    if (roll) *roll = shortest_angle_delta(origin_roll, current_roll);
}

/* Map physical displacements into the same reference used by snap-turn and return.
   Roll and head lean must never rotate a hand translation across the floor. */
static Quat tracking_to_navigation(const EyeOptics* optics) {
    float reference_yaw;
    XrQuaternionf reference=optics->return_orientation_valid?optics->return_orientation:optics->origin_center.orientation;
    quat_to_yaw_pitch_roll(xr_to_unity_quat(reference),&reference_yaw,NULL,NULL);
    return quat_multiply(quat_from_yaw_pitch_roll(optics->artificial_yaw,optics->artificial_pitch,0),
        quat_from_yaw_pitch_roll(-reference_yaw,0,0));
}
static Vec3 physical_displacement(const EyeOptics* optics) {
    XrVector3f p=optics->current_center.position,o=optics->origin_center.position;
    return quat_rotate(tracking_to_navigation(optics),(Vec3){p.x-o.x,p.y-o.y,o.z-p.z});
}

static BOOL commit_immersive_environment_origin(EyeOptics* optics) {
    if (!optics || !optics->origin_valid) return FALSE;
    optics->origin_center.position = optics->current_center.position;
    optics->origin_center.orientation = (XrQuaternionf){0.0f, 0.0f, 0.0f, 1.0f};
    return TRUE;
}

enum { NAV_STEP_MOVED = 1, NAV_STEP_SNAPPED = 2 };

static float effective_navigation_speed_mps(float configured_speed,
        float perceived_world_scale, BOOL compensate_for_world_scale) {
    if (!isfinite(configured_speed) || configured_speed < 0.0f) return 0.0f;
    if (!compensate_for_world_scale) return configured_speed;
    if (!isfinite(perceived_world_scale) || perceived_world_scale < WORLD_SCALE_MIN)
        return configured_speed;
    float compensated = configured_speed *
        (perceived_world_scale / NAV_LOCOMOTION_SCALE_REFERENCE);
    return isfinite(compensated) && compensated >= 0.0f ? compensated : configured_speed;
}

static XrVector2f navigation_apply_deadzone(XrVector2f input) {
    float magnitude = sqrtf(input.x * input.x + input.y * input.y);
    if (!isfinite(magnitude) || magnitude <= move_deadzone)
        return (XrVector2f){0.0f, 0.0f};
    if (magnitude > 1.0f) magnitude = 1.0f;
    float scaled = (magnitude - move_deadzone) / (1.0f - move_deadzone);
    float original = sqrtf(input.x * input.x + input.y * input.y);
    if (!(original > 0.0f)) return (XrVector2f){0.0f, 0.0f};
    return (XrVector2f){input.x * scaled / original, input.y * scaled / original};
}

static uint32_t apply_navigation_input(EyeOptics* optics, XrVector2f move,
        XrVector2f turn, float dt_seconds, BOOL* turn_x_latched,
        BOOL* turn_y_latched) {
    if (!optics || !optics->origin_valid || !turn_x_latched || !turn_y_latched)
        return 0;
    Vec3 before_snap=physical_displacement(optics);
    uint32_t changed = 0;
    if (fabsf(turn.x) < NAV_SNAP_RELEASE) *turn_x_latched = FALSE;
    if (fabsf(turn.y) < NAV_SNAP_RELEASE) *turn_y_latched = FALSE;
    if (nav_snap_turn_enabled && fabsf(turn.x) >= NAV_SNAP_THRESHOLD && !*turn_x_latched) {
        optics->artificial_yaw += turn.x > 0.0f ? nav_snap_angle_rad : -nav_snap_angle_rad;
        *turn_x_latched = TRUE;
        changed |= NAV_STEP_SNAPPED;
    }
    if (nav_snap_turn_enabled && fabsf(turn.y) >= NAV_SNAP_THRESHOLD && !*turn_y_latched) {
        optics->artificial_pitch += turn.y > 0.0f ? -nav_snap_angle_rad : nav_snap_angle_rad;
        optics->artificial_pitch = fmaxf(-NAV_ARTIFICIAL_PITCH_LIMIT_RAD,
            fminf(NAV_ARTIFICIAL_PITCH_LIMIT_RAD, optics->artificial_pitch));
        *turn_y_latched = TRUE;
        changed |= NAV_STEP_SNAPPED;
    }

    if(changed & NAV_STEP_SNAPPED){
        Vec3 after=physical_displacement(optics);
        optics->artificial_position.x+=before_snap.x-after.x;
        optics->artificial_position.y+=before_snap.y-after.y;
        optics->artificial_position.z+=before_snap.z-after.z;
    }
    move = navigation_apply_deadzone(move);
    if (nav_locomotion_enabled && dt_seconds > 0.0f && isfinite(dt_seconds) &&
            (fabsf(move.x) > 0.0001f || fabsf(move.y) > 0.0001f)) {
        if (dt_seconds > 0.1f) dt_seconds = 0.1f;
        float physical_yaw, physical_pitch, ignored_roll;
        physical_orientation_deltas(optics,
            &physical_yaw, &physical_pitch, &ignored_roll);
        (void)ignored_roll;
        float relative_pitch = fmaxf(-1.55334306f, fminf(1.55334306f,
            optics->artificial_pitch + physical_pitch));
        relative_pitch = fmaxf(-1.55334306f, fminf(1.55334306f,
            optics->navigation_base_pitch + relative_pitch));
        Quat roll_free_view = quat_from_yaw_pitch_roll(
            optics->navigation_base_yaw + optics->artificial_yaw + physical_yaw,
            relative_pitch, 0.0f);
        Vec3 direction = quat_rotate(roll_free_view, (Vec3){move.x, 0.0f, move.y});
        float effective_speed = effective_navigation_speed_mps(nav_locomotion_speed_mps,
            world_scale, nav_locomotion_scale_compensation_enabled);
        float distance = effective_speed * dt_seconds;
        Vec3 previous = optics->artificial_position;
        optics->artificial_position.x += direction.x * distance;
        optics->artificial_position.y += direction.y * distance;
        optics->artificial_position.z += direction.z * distance;
        if (!isfinite(optics->artificial_position.x) ||
            !isfinite(optics->artificial_position.y) ||
            !isfinite(optics->artificial_position.z)) {
            optics->artificial_position = previous;
        } else {
            changed |= NAV_STEP_MOVED;
        }
    }
    return changed;
}

static BOOL compose_unity_eye_pose(const Vec3* base_position, const Quat* base_rotation,
        BOOL authored_rotation_follow, const EyeOptics* optics, float perceived_world_scale,
        int eye,
        Vec3* position, Quat* rotation) {
    if (!base_position || !base_rotation || !optics || !optics->origin_valid ||
        !position || !rotation || eye < 0 || eye > 1 ||
        !isfinite(perceived_world_scale) || perceived_world_scale < WORLD_SCALE_MIN) return FALSE;
    float inverse_world_scale = 1.0f / perceived_world_scale;
    float base_yaw, base_pitch, base_roll;
    float physical_yaw, physical_pitch, physical_roll;
    quat_to_yaw_pitch_roll(*base_rotation, &base_yaw, &base_pitch, &base_roll);
    physical_orientation_deltas(optics, &physical_yaw, &physical_pitch, &physical_roll);
    const float pitch_limit = 1.55334306f;
    float final_pitch = fmaxf(-pitch_limit, fminf(pitch_limit,
        base_pitch + optics->artificial_pitch + physical_pitch));
    *rotation = quat_from_yaw_pitch_roll(
        base_yaw + optics->artificial_yaw + physical_yaw,
        final_pitch, (authored_rotation_follow ? base_roll : 0.0f) + physical_roll);

    Vec3 unity_delta=physical_displacement(optics);
    unity_delta.x += optics->artificial_position.x;
    unity_delta.y += optics->artificial_position.y;
    unity_delta.z += optics->artificial_position.z;
    unity_delta.x *= inverse_world_scale;
    unity_delta.y *= inverse_world_scale;
    unity_delta.z *= inverse_world_scale;
    Quat base_no_roll = quat_from_yaw_pitch_roll(base_yaw, base_pitch, 0.0f);
    Vec3 translated = quat_rotate(base_no_roll, unity_delta);
    Vec3 eye_offset = quat_rotate(*rotation,
        (Vec3){(eye ? optics->half_ipd_m : -optics->half_ipd_m) * inverse_world_scale,
            0, 0});
    *position = (Vec3){base_position->x + translated.x + eye_offset.x,
                       base_position->y + translated.y + eye_offset.y,
                       base_position->z + translated.z + eye_offset.z};
    return isfinite(position->x) && isfinite(position->y) && isfinite(position->z);
}

static BOOL il2cpp_string_equals_ascii(const Il2CppString* value, const char* ascii) {
    if (!value || !ascii) return FALSE;
    size_t length = strlen(ascii);
    if ((size_t)value->length != length) return FALSE;
    for (size_t i = 0; i < length; ++i)
        if (value->chars[i] != (uint16_t)(unsigned char)ascii[i]) return FALSE;
    return TRUE;
}

static BOOL component_is(Il2CppObject* component, const char* ns, const char* name) {
    if (!component || !component->klass || !il2cpp_class_get_name) return FALSE;
    const char* actual_name = il2cpp_class_get_name(component->klass);
    const char* actual_ns = il2cpp_class_get_namespace ? il2cpp_class_get_namespace(component->klass) : "";
    return actual_name && actual_ns && strcmp(actual_name, name) == 0 && strcmp(actual_ns, ns) == 0;
}

static BOOL is_live_owner(Il2CppObject* camera) {
    if (!camera || !component_get_game_object || !gameobject_get_components) return FALSE;
    Il2CppObject* go = component_get_game_object(camera);
    Il2CppArray* components = go ? gameobject_get_components(go, component_reflection_type, 0, 0, 1, 0, NULL) : NULL;
    BOOL timeline = FALSE, effect = FALSE, final_composite = FALSE;
    uintptr_t count = components ? components->max_length : 0;
    if (count > 32) count = 32;
    for (uintptr_t i = 0; i < count; ++i) {
        Il2CppObject* c = components->vector[i];
        timeline |= component_is(c, "Gallop.Live", "LiveTimelineCamera");
        effect |= component_is(c, "Gallop", "LiveImageEffect");
        final_composite |= component_is(c, "Gallop.Live", "MultiCameraFinalComposite");
    }
    return timeline && effect && final_composite;
}

enum AuthoredEyeComponentMask {
    AUTHORED_EYE_CAMERA = 1u << 0,
    AUTHORED_EYE_LIVE_IMAGE_EFFECT = 1u << 1,
    AUTHORED_EYE_LIVE_FINAL_COMPOSITE = 1u << 2,
    AUTHORED_EYE_CAMERA_DATA = 1u << 3,
    AUTHORED_EYE_URP_DATA = 1u << 4,
    AUTHORED_EYE_HOME_LOW_RESOLUTION = 1u << 5,
    AUTHORED_EYE_HOME_CONTROLLER = 1u << 6,
    AUTHORED_EYE_HOME_IMAGE_EFFECT = 1u << 7,
    AUTHORED_EYE_COMMON_REQUIRED = AUTHORED_EYE_CAMERA | AUTHORED_EYE_CAMERA_DATA | AUTHORED_EYE_URP_DATA,
    AUTHORED_EYE_LIVE_REQUIRED = AUTHORED_EYE_COMMON_REQUIRED | AUTHORED_EYE_LIVE_IMAGE_EFFECT |
        AUTHORED_EYE_LIVE_FINAL_COMPOSITE,
    AUTHORED_EYE_HOME_REQUIRED = AUTHORED_EYE_COMMON_REQUIRED | AUTHORED_EYE_HOME_LOW_RESOLUTION |
        AUTHORED_EYE_HOME_CONTROLLER | AUTHORED_EYE_HOME_IMAGE_EFFECT
};

#include "base_source_capability.h"

static uint32_t classify_authored_eye_component(Il2CppObject* component) {
    if (component_is(component, "UnityEngine", "Camera")) return AUTHORED_EYE_CAMERA;
    if (component_is(component, "Gallop", "LiveImageEffect")) return AUTHORED_EYE_LIVE_IMAGE_EFFECT;
    if (component_is(component, "Gallop.Live", "MultiCameraFinalComposite")) return AUTHORED_EYE_LIVE_FINAL_COMPOSITE;
    if (component_is(component, "Gallop.RenderPipeline", "CameraData")) return AUTHORED_EYE_CAMERA_DATA;
    if (component_is(component, "UnityEngine.Rendering.Universal", "UniversalAdditionalCameraData"))
        return AUTHORED_EYE_URP_DATA;
    if (component_is(component, "Gallop", "LowResolutionCamera"))
        return AUTHORED_EYE_HOME_LOW_RESOLUTION;
    if (component_is(component, "Gallop", "HomeCameraController"))
        return AUTHORED_EYE_HOME_CONTROLLER;
    if (component_is(component, "Gallop", "GallopImageEffectOnRenderImage"))
        return AUTHORED_EYE_HOME_IMAGE_EFFECT;
    return 0;
}

typedef struct HomeOwnerComponents {
    Il2CppObject* controller;
    Il2CppObject* image_effect;
    Il2CppObject* camera_data;
    uint32_t mask;
} HomeOwnerComponents;

static BOOL collect_home_owner_components(Il2CppObject* camera, HomeOwnerComponents* out) {
    if (!out) return FALSE;
    ZeroMemory(out, sizeof(*out));
    if (!camera || !component_get_game_object || !gameobject_get_components) return FALSE;
    Il2CppObject* go = component_get_game_object(camera);
    Il2CppArray* components = go ? gameobject_get_components(
        go, component_reflection_type, 0, 0, 1, 0, NULL) : NULL;
    uintptr_t count = components ? components->max_length : 0;
    if (count > 32) count = 32;
    for (uintptr_t i = 0; i < count; ++i) {
        Il2CppObject* component = components->vector[i];
        uint32_t bit = classify_authored_eye_component(component);
        out->mask |= bit;
        if (bit == AUTHORED_EYE_HOME_CONTROLLER) out->controller = component;
        else if (bit == AUTHORED_EYE_HOME_IMAGE_EFFECT) out->image_effect = component;
        else if (bit == AUTHORED_EYE_CAMERA_DATA) out->camera_data = component;
    }
    return (out->mask & AUTHORED_EYE_HOME_REQUIRED) == AUTHORED_EYE_HOME_REQUIRED &&
        out->controller && out->image_effect && out->camera_data;
}

static FieldInfo* lookup_vfx_field(Il2CppClass* owner, const char* expected_name) {
    if (!owner || !expected_name || !il2cpp_class_get_fields || !il2cpp_field_get_name) return NULL;
    unsigned depth = 0;
    while (owner && depth++ < 16) {
        void* iterator = NULL;
        FieldInfo* field;
        unsigned count = 0;
        while (count++ < 128 && (field = il2cpp_class_get_fields(owner, &iterator)) != NULL) {
            const char* name = il2cpp_field_get_name(field);
            if (name && strcmp(name, expected_name) == 0) return field;
        }
        owner = il2cpp_class_get_parent ? il2cpp_class_get_parent(owner) : NULL;
    }
    return NULL;
}

/* Metadata lives for the game domain. Cache field descriptors, never effect
   values or scene objects, across the hot eye-render path. */
static FieldInfo* find_vfx_field(Il2CppClass* owner,const char* name) {
    static struct {Il2CppClass* owner;const char* name;FieldInfo* field;} cache[96];
    static unsigned used;
    for(unsigned i=0;i<used;i++)if(cache[i].owner==owner && !strcmp(cache[i].name,name))return cache[i].field;
    FieldInfo* field=lookup_vfx_field(owner,name);
    if(used<96){cache[used].owner=owner;cache[used].name=name;cache[used++].field=field;}
    return field;
}
static BOOL write_vfx_value(VfxFieldOverride* resolved, const void* value) {
    if (!resolved || !resolved->parameter || !resolved->effect_field ||
        !resolved->value_field || !value || !il2cpp_field_get_value_object ||
        !il2cpp_field_set_value || !il2cpp_object_unbox) return FALSE;
    Il2CppObject* boxed = il2cpp_field_get_value_object(
        resolved->effect_field, resolved->parameter);
    if (!boxed) return FALSE;
    il2cpp_field_set_value(boxed, resolved->value_field, (void*)value);
    void* unboxed = il2cpp_object_unbox(boxed);
    if (!unboxed) return FALSE;
    il2cpp_field_set_value(resolved->parameter, resolved->effect_field, unboxed);
    return TRUE;
}

static BOOL resolve_vfx_override(Il2CppObject* current_parameter,
        const char* effect_name, const char* value_field_name, uint8_t value_size,
        int32_t expected_type,
        VfxFieldOverride* resolved) {
    if (!resolved || !current_parameter || !current_parameter->klass ||
        !il2cpp_field_get_value_object || !il2cpp_field_get_value ||
        !il2cpp_field_get_type || !il2cpp_type_get_type ||
        !il2cpp_field_set_value || !il2cpp_object_unbox) return FALSE;
    /* Effect Parameter fields are inline value types. Box the current value to
       inspect its exact field, then write the modified unboxed struct back to
       the owning current CameraData.Parameter. */
    FieldInfo* effect_field = find_vfx_field(current_parameter->klass, effect_name);
    Il2CppObject* boxed = effect_field ?
        il2cpp_field_get_value_object(effect_field, current_parameter) : NULL;
    if (!boxed || !boxed->klass) return FALSE;
    FieldInfo* value_field = find_vfx_field(boxed->klass, value_field_name);
    if (!value_field || value_size == 0 || value_size > sizeof(resolved->saved_value)) return FALSE;
    const Il2CppType* value_type = il2cpp_field_get_type(value_field);
    if (!value_type || il2cpp_type_get_type(value_type) != expected_type) return FALSE;
    memset(resolved->saved_value, 0, sizeof(resolved->saved_value));
    il2cpp_field_get_value(boxed, value_field, resolved->saved_value);
    resolved->parameter = current_parameter;
    resolved->effect_field = effect_field;
    resolved->value_field = value_field;
    resolved->value_size = value_size;
    resolved->applied = FALSE;
    return TRUE;
}

static void restore_vfx_overrides(void) {
    unsigned restored = 0;
    if (il2cpp_field_set_value) {
        for (unsigned i = 0; i < 32; ++i) {
            if (!vfx_overrides[i].applied)
                continue;
            if (write_vfx_value(&vfx_overrides[i], vfx_overrides[i].saved_value)) ++restored;
        }
    }
    if (vfx_master_override.applied && vfx_master_override.parameter &&
        vfx_master_override.enabled_field && il2cpp_field_set_value) {
        uint8_t value = vfx_master_override.saved_value;
        il2cpp_field_set_value(vfx_master_override.parameter,
            vfx_master_override.enabled_field, &value);
        ++restored;
    }
    memset(&vfx_master_override, 0, sizeof(vfx_master_override));
    memset(vfx_overrides, 0, sizeof(vfx_overrides));
    if (restored && vfx_log_change) {
        char detail[160];
        telemetry_snprintf(detail, sizeof(detail),
            "\"status\":\"restored\",\"field_count\":%u,\"original_objects_mutated\":false",
            restored);
        write_record("SETTINGS-008", "vfx_override_restore", detail);
    }
}

static void apply_vfx_overrides(Il2CppObject* camera_data) {
    restore_vfx_overrides();
    if (post_processing_enabled && blur_effect_enabled && depth_of_field_enabled &&
        diffusion_effect_enabled && bloom_effect_enabled && global_fog_effect_enabled &&
        lens_distortion_effect_enabled && radial_blur_effect_enabled &&
        sun_shafts_effect_enabled && indirect_light_shafts_effect_enabled &&
        transmitted_light_effect_enabled && dof_diffusion_bloom_overlay_enabled &&
        tilt_shift_effect_enabled && fluctuation_effect_enabled &&
        chromatic_aberration_effect_enabled && tone_curve_effect_enabled &&
        exposure_effect_enabled && color_correction_effect_enabled &&
        color_grading_effect_enabled && bg_blur_effect_enabled &&
        vortex_effect_enabled && film_roll_effect_enabled &&
        hatching_effect_enabled && letter_box_effect_enabled && rain_splash_effect_enabled) {
        if(vfx_log_change)write_record("SETTINGS-008", "vfx_override_apply",
            "\"status\":\"authored_preserved\",\"post_processing_enabled\":true,"
            "\"all_22_effect_slots_enabled\":true");
        return;
    }
    if (!camera_data || !camera_data->klass || !il2cpp_class_get_fields ||
        !il2cpp_field_get_name || !il2cpp_class_get_name || !il2cpp_class_get_parent ||
        !il2cpp_field_get_value_object || !il2cpp_field_get_value ||
        !il2cpp_field_get_type || !il2cpp_type_get_type ||
        !il2cpp_field_set_value || !il2cpp_object_unbox) {
        if(vfx_log_change)write_record("SETTINGS-008", "vfx_override_apply",
            "\"status\":\"fail_open\",\"reason\":\"metadata_or_write_api_unavailable\"");
        return;
    }
    FieldInfo* current_field = find_vfx_field(camera_data->klass, "ImageEffectParameter");
    Il2CppObject* current = current_field ?
        il2cpp_field_get_value_object(current_field, camera_data) : NULL;
    BOOL master_contract = TRUE;
    unsigned requested = 0;
    unsigned applied = 0;
    unsigned next_override = 0;
    if (!post_processing_enabled) {
        FieldInfo* enabled_field = current && current->klass ?
            find_vfx_field(current->klass, "IsEnable") : NULL;
        const Il2CppType* master_type = enabled_field ? il2cpp_field_get_type(enabled_field) : NULL;
        if (!master_type || il2cpp_type_get_type(master_type) != 0x02 /* IL2CPP_TYPE_BOOLEAN */) {
            master_contract = FALSE;
        } else {
            uint8_t saved = 0;
            uint8_t disabled = 0;
            il2cpp_field_get_value(current, enabled_field, &saved);
            il2cpp_field_set_value(current, enabled_field, &disabled);
            vfx_master_override.parameter = current;
            vfx_master_override.enabled_field = enabled_field;
            vfx_master_override.saved_value = saved ? 1u : 0u;
            vfx_master_override.applied = TRUE;
        }
    }
#define APPLY_ZERO(enabled, effect, field, size, type) do { \
        if (!(enabled)) { \
            ++requested; \
            uint8_t zero[8] = {0}; \
            VfxFieldOverride* item = next_override < 32 ? &vfx_overrides[next_override++] : NULL; \
            if (item && resolve_vfx_override(current, effect, field, size, type, item)) { \
                if (!memcmp(item->saved_value,zero,size)) ++applied; \
                else if (write_vfx_value(item,zero)) {item->applied=TRUE;++applied;} \
            } \
            else if (item) memset(item, 0, sizeof(*item)); \
        } \
    } while (0)
    if(post_processing_enabled){
    APPLY_ZERO(blur_effect_enabled, "BlurOptimized", "IsEnable", 1, 2);
    APPLY_ZERO(sun_shafts_effect_enabled, "SunShafts", "IsEnable", 1, 2);
    APPLY_ZERO(indirect_light_shafts_effect_enabled, "IndirectLightShafts", "IsEnable", 1, 2);
    APPLY_ZERO(transmitted_light_effect_enabled, "TransmittedLight", "IsEnable", 1, 2);
    APPLY_ZERO(dof_diffusion_bloom_overlay_enabled && depth_of_field_enabled,
        "DofDiffuionBloomOverlay", "IsEnableDof", 1, 2);
    if (!(dof_diffusion_bloom_overlay_enabled && depth_of_field_enabled)) {
        ++requested;
        uint8_t disabled = 1;
        VfxFieldOverride* item = next_override < 32 ? &vfx_overrides[next_override++] : NULL;
        if (item && resolve_vfx_override(current, "DofDiffuionBloomOverlay",
                "IsDisableDofTemporary", 1, 2, item) &&
                write_vfx_value(item, &disabled)) {
            item->applied = TRUE;
            ++applied;
        } else if (item) {
            memset(item, 0, sizeof(*item));
        }
    }
    APPLY_ZERO(dof_diffusion_bloom_overlay_enabled,
        "DofDiffuionBloomOverlay", "IsEnableOldDof", 1, 2);
    APPLY_ZERO(dof_diffusion_bloom_overlay_enabled && diffusion_effect_enabled,
        "DofDiffuionBloomOverlay", "IsEnableDiffusion", 1, 2);
    APPLY_ZERO(dof_diffusion_bloom_overlay_enabled && bloom_effect_enabled,
        "DofDiffuionBloomOverlay", "IsEnableBloom", 1, 2);
    APPLY_ZERO(tilt_shift_effect_enabled, "TiltShift", "MaxBlurSize", 4, 12);
    APPLY_ZERO(radial_blur_effect_enabled, "RadialBlur", "RadialBlurPower", 4, 12);
    APPLY_ZERO(fluctuation_effect_enabled, "Fluctuation", "IsEnable", 1, 2);
    APPLY_ZERO(lens_distortion_effect_enabled, "LensDistortion", "Intensity", 4, 12);
    APPLY_ZERO(chromatic_aberration_effect_enabled, "ChromaticAberration", "IsEnable", 1, 2);
    APPLY_ZERO(tone_curve_effect_enabled, "ToneCurve", "IsEnable", 1, 2);
    APPLY_ZERO(exposure_effect_enabled, "Exposure", "IsEnable", 1, 2);
    APPLY_ZERO(color_correction_effect_enabled, "ColorCorrection", "IsEnable", 1, 2);
    APPLY_ZERO(color_grading_effect_enabled, "ColorGrading", "IsEnable", 1, 2);
    APPLY_ZERO(bg_blur_effect_enabled, "BgBlur", "IsEnable", 1, 2);
    APPLY_ZERO(vortex_effect_enabled, "Vortex", "IsEnable", 1, 2);
    APPLY_ZERO(film_roll_effect_enabled, "FilmRoll", "IsEnable", 1, 2);
    APPLY_ZERO(hatching_effect_enabled, "Hatching", "BlendAlpha", 4, 12);
    APPLY_ZERO(letter_box_effect_enabled, "LetterBox", "IsEnable", 1, 2);
    APPLY_ZERO(rain_splash_effect_enabled, "RainSplash", "IsEnable", 1, 2);
    if (!global_fog_effect_enabled) {
        APPLY_ZERO(FALSE, "GlobalFog", "IsDistanceFog", 1, 2);
        APPLY_ZERO(FALSE, "GlobalFog", "IsHeightFog", 1, 2);
    }
#undef APPLY_ZERO
    }
    char detail[384];
    telemetry_snprintf(detail, sizeof(detail),
        "\"status\":\"%s\",\"post_processing_enabled\":%s,"
        "\"master_contract\":%s,\"requested_field_count\":%u,"
        "\"applied_field_count\":%u,\"failed_field_count\":%u,"
        "\"original_objects_mutated\":false",
        (master_contract && requested == applied) ? "applied" : "partial_fail_open",
        post_processing_enabled ? "true" : "false", master_contract ? "true" : "false",
        requested, applied, requested - applied);
    if(vfx_log_change)write_record("SETTINGS-008", "vfx_override_apply", detail);
}

static BOOL blit_render_texture(Il2CppObject* source, Il2CppObject* destination) {
    graphics_blit_fn blit = mi_graphics_blit ? (graphics_blit_fn)mi_graphics_blit->methodPointer : NULL;
    if (!blit || !source || !destination) return FALSE;
    blit(source, destination, mi_graphics_blit);
    return TRUE;
}

static void restore_home_camera_snapshot(void) {
    if (!home_camera_snapshot.valid || !current_authored_owner) return;
    Il2CppObject* transform = component_get_transform ? component_get_transform(current_authored_owner) : NULL;
    if (transform) {
        set_float_fn set_aspect = (set_float_fn)mi_camera_set_aspect->methodPointer;
        set_aspect(current_authored_owner, home_camera_snapshot.aspect, mi_camera_set_aspect);
        transform_set_position(transform, &home_camera_snapshot.position);
        transform_set_rotation(transform, &home_camera_snapshot.rotation);
        camera_set_projection_matrix(current_authored_owner, &home_camera_snapshot.projection);
    }
    home_camera_snapshot.valid = FALSE;
}

static void reject_home_cycle_locked(const char* reason) {
    restore_home_camera_snapshot();
    InterlockedExchange(&eye_pair_capture_ready, 0);
    InterlockedExchange(&published_eye_generation, 0);
    home_center_ready = FALSE;
    home_next_phase = HOME_PHASE_CENTER;
    home_active_phase = HOME_PHASE_NONE;
    home_active_generation = 0;
    home_eye_completion_mask = 0;
    LONG event = InterlockedIncrement(&home_pair_events);
    if (event <= 8) {
        char detail[192], escaped_reason[128];
        json_escape(reason ? reason : "unknown", escaped_reason, sizeof(escaped_reason));
        telemetry_snprintf(detail, sizeof(detail),
            "\"status\":\"panel\",\"reason\":\"%s\","
            "\"recovery\":\"new_center_pair\"", escaped_reason);
        write_record("STEREO-011", "home_temporal_cycle_rejected", detail);
    }
}

static BOOL ensure_home_center_texture(Il2CppObject* output) {
    if (!output || !texture_get_width || !texture_get_height || !mi_rt_ctor || !mi_rt_create) return FALSE;
    LONG width = texture_get_width(output);
    LONG height = texture_get_height(output);
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192) return FALSE;
    if (home_center_render_texture && home_center_width == width && home_center_height == height) return TRUE;
    destroy_object_fn destroy = mi_object_destroy ? (destroy_object_fn)mi_object_destroy->methodPointer : NULL;
    if (destroy && home_center_render_texture)
        destroy(home_center_render_texture, mi_object_destroy);
    home_center_render_texture = il2cpp_object_new(rendertexture_class);
    home_center_width = 0;
    home_center_height = 0;
    home_center_ready = FALSE;
    if (!home_center_render_texture) return FALSE;
    ((rt_ctor_fn)mi_rt_ctor->methodPointer)(home_center_render_texture,
        width, height, 0, 0, mi_rt_ctor);
    if (!((rt_create_fn)mi_rt_create->methodPointer)(home_center_render_texture, mi_rt_create)) {
        if (destroy) destroy(home_center_render_texture, mi_object_destroy);
        home_center_render_texture = NULL;
        return FALSE;
    }
    home_center_width = width;
    home_center_height = height;
    return TRUE;
}

static BOOL prepare_home_temporal_phase_locked(void) {
    LONG generation = InterlockedCompareExchange(&source_generation, 0, 0);
    if (current_owner_kind != AUTHORED_OWNER_HOME || generation <= 0 ||
        !current_authored_owner ||
        (!home_center_ready && home_next_phase != HOME_PHASE_CENTER)) return FALSE;
    home_active_generation = generation;
    home_active_phase = home_next_phase;
    if (home_active_phase == HOME_PHASE_CENTER) return TRUE;

    EyeOptics optics;
    Il2CppObject* transform = component_get_transform(current_authored_owner);
    LONG width = InterlockedCompareExchange(&requested_eye_width, 0, 0);
    LONG height = InterlockedCompareExchange(&requested_eye_height, 0, 0);
    if (!transform || !snapshot_eye_optics(&optics) || width <= 0 || height <= 0) return FALSE;
    float near_clip = camera_get_near_clip(current_authored_owner);
    float far_clip = camera_get_far_clip(current_authored_owner);
    if (!isfinite(near_clip) || !isfinite(far_clip) || near_clip <= 0.0f || far_clip <= near_clip)
        return FALSE;

    transform_get_position(transform, &home_camera_snapshot.position);
    transform_get_rotation(transform, &home_camera_snapshot.rotation);
    camera_get_projection_matrix(current_authored_owner, &home_camera_snapshot.projection);
    home_camera_snapshot.aspect = camera_get_aspect(current_authored_owner);
    home_camera_snapshot.generation = generation;
    home_camera_snapshot.phase = home_active_phase;
    home_camera_snapshot.valid = TRUE;

    float base_yaw = 0.0f, base_pitch = 0.0f;
    quat_to_yaw_pitch_roll(home_camera_snapshot.rotation, &base_yaw, &base_pitch, NULL);
    AcquireSRWLockExclusive(&eye_optics_lock);
    eye_optics.navigation_base_yaw = base_yaw;
    eye_optics.navigation_base_pitch = base_pitch;
    ReleaseSRWLockExclusive(&eye_optics_lock);

    int eye = home_active_phase == HOME_PHASE_RIGHT ? 1 : 0;
    Vec3 eye_position;
    Quat eye_rotation;
    Matrix4x4 eye_projection;
    if (!compose_unity_eye_pose(&home_camera_snapshot.position, &home_camera_snapshot.rotation,
            FALSE, &optics, world_scale, eye, &eye_position, &eye_rotation)) return FALSE;
    build_unity_eye_projection(&optics.fov[eye], near_clip, far_clip, &eye_projection);
    ((set_float_fn)mi_camera_set_aspect->methodPointer)(current_authored_owner,
        (float)width / (float)height, mi_camera_set_aspect);
    transform_set_position(transform, &eye_position);
    transform_set_rotation(transform, &eye_rotation);
    camera_set_projection_matrix(current_authored_owner, &eye_projection);
    return TRUE;
}

static void hooked_home_late_update(Il2CppObject* self, const MethodInfo* method) {
    if (home_active_phase != HOME_PHASE_NONE && self == current_home_controller) {
        AcquireSRWLockExclusive(&eye_source_lock);
        reject_home_cycle_locked("completion_callback_missing");
        ReleaseSRWLockExclusive(&eye_source_lock);
    }
    original_home_late_update(self, method);
    if (!InterlockedCompareExchange(&home_adapter_ready, 0, 0) ||
        self != current_home_controller || current_owner_kind != AUTHORED_OWNER_HOME) return;
    AcquireSRWLockExclusive(&eye_source_lock);
    if (!prepare_home_temporal_phase_locked())
        reject_home_cycle_locked("phase_prepare_failed");
    ReleaseSRWLockExclusive(&eye_source_lock);
}

static void hooked_home_on_render(Il2CppObject* self, Il2CppObject* source,
        Il2CppObject* destination, const MethodInfo* method) {
    original_home_on_render(self, source, destination, method);
    if (!InterlockedCompareExchange(&home_adapter_ready, 0, 0) ||
        current_owner_kind != AUTHORED_OWNER_HOME)
        return;
    AcquireSRWLockExclusive(&eye_source_lock);
    LONG generation = InterlockedCompareExchange(&source_generation, 0, 0);
    Il2CppObject* completed = destination;
    if (home_active_phase == HOME_PHASE_NONE) {
        ReleaseSRWLockExclusive(&eye_source_lock);
        return;
    }
    if (generation <= 0 || generation != home_active_generation || !completed) {
        reject_home_cycle_locked("callback_generation_or_output_invalid");
        ReleaseSRWLockExclusive(&eye_source_lock);
        return;
    }
    enum HomeTemporalPhase phase = home_active_phase;
    home_active_phase = HOME_PHASE_NONE;
    home_active_generation = 0;
    if (phase == HOME_PHASE_CENTER) {
        if (ensure_home_center_texture(completed) &&
            blit_render_texture(completed, home_center_render_texture)) {
            home_center_ready = TRUE;
            home_eye_completion_mask = 0;
            home_next_phase = HOME_PHASE_LEFT;
        } else {
            reject_home_cycle_locked("center_capture_failed");
        }
        ReleaseSRWLockExclusive(&eye_source_lock);
        return;
    }
    BOOL eye_copied = phase >= HOME_PHASE_LEFT && phase <= HOME_PHASE_RIGHT &&
        eye_render_texture[phase == HOME_PHASE_RIGHT ? 1 : 0] &&
        blit_render_texture(completed,
            eye_render_texture[phase == HOME_PHASE_RIGHT ? 1 : 0]);
    restore_home_camera_snapshot();
    BOOL pc_restored = home_center_ready && home_center_render_texture &&
        blit_render_texture(home_center_render_texture, completed);
    if (!eye_copied || !pc_restored) {
        reject_home_cycle_locked("eye_capture_or_pc_restore_failed");
        ReleaseSRWLockExclusive(&eye_source_lock);
        return;
    }
    home_eye_completion_mask |= phase == HOME_PHASE_RIGHT ? 2u : 1u;
    if (phase == HOME_PHASE_LEFT) {
        home_next_phase = HOME_PHASE_RIGHT;
    } else if (home_eye_completion_mask == 3u) {
        InterlockedIncrement64(&eye_source_pair_serial);
        InterlockedExchange64(&source_ready_after_present,
            InterlockedCompareExchange64((volatile LONG64*)&produced_frames, 0, 0) + 1);
        InterlockedExchange(&eye_pair_capture_ready, 1);
        home_next_phase = HOME_PHASE_CENTER;
        LONG event = InterlockedIncrement(&home_pair_events);
        if (event <= 8) {
            char detail[192];
            telemetry_snprintf(detail, sizeof(detail),
                "\"status\":\"complete\",\"generation\":%ld,\"phase_order\":\"CENTER_LEFT_RIGHT\","
                "\"pc_center_restored\":true", (long)generation);
            write_record("STEREO-011", "home_temporal_pair_complete", detail);
        }
    } else {
        reject_home_cycle_locked("right_without_left");
    }
    ReleaseSRWLockExclusive(&eye_source_lock);
}

static void invalidate_eye_source_locked(const char* reason) {
    /* A scene/owner transition can land between the Home LateUpdate mutation
       and its post-effect completion callback. Restore while the owner is
       still current so fail-open never strands an eye pose on the PC camera. */
    restore_home_camera_snapshot();
    restore_vfx_overrides();
    if(current_owner_kind==AUTHORED_OWNER_LIVE && current_authored_generation>0)
        InterlockedExchange(&live_retired_through,current_authored_generation);
    current_authored_generation=0;
    LONG old_generation = InterlockedExchange(&published_eye_generation, 0);
    InterlockedExchange(&published_flat_generation, 0);
    InterlockedExchange(&eye_pair_capture_ready, 0);
    InterlockedExchangePointer((volatile PVOID*)&eye_native_game[0], NULL);
    InterlockedExchangePointer((volatile PVOID*)&eye_native_game[1], NULL);
    InterlockedExchangePointer((volatile PVOID*)&flat_native_game, NULL);
    current_authored_owner = NULL;
    current_owner_kind = AUTHORED_OWNER_NONE;
    current_home_controller = NULL;
    current_scene_handle = INT32_MIN;
    authored_pose_anchor_valid = FALSE;
    home_camera_snapshot.valid = FALSE;
    home_center_ready = FALSE;
    home_next_phase = HOME_PHASE_CENTER;
    home_active_phase = HOME_PHASE_NONE;
    home_active_generation = 0;
    home_eye_completion_mask = 0;
    if (old_generation) {
        char detail[256], escaped_reason[192];
        json_escape(reason ? reason : "unknown", escaped_reason, sizeof(escaped_reason));
        telemetry_snprintf(detail, sizeof(detail), "\"status\":\"retired\",\"generation\":%ld,\"reason\":\"%s\"",
            (long)old_generation, escaped_reason);
        write_record("IMMERSIVE-SOURCE", "source_generation_retired", detail);
    }
}

#include "live_render_scale.h"

static void contact_retire_live(void);
static void destroy_eye_unity_objects(void) {
    if(current_owner_kind==AUTHORED_OWNER_LIVE)contact_retire_live();
    if(live_vfx_camera_root){request_dispatch_api.gchandle_free(live_vfx_camera_root);live_vfx_camera_root=0;}
    AcquireSRWLockExclusive(&eye_source_lock);
    invalidate_eye_source_locked("owner_or_scene_changed");
    ReleaseSRWLockExclusive(&eye_source_lock);
    destroy_object_fn destroy = mi_object_destroy ? (destroy_object_fn)mi_object_destroy->methodPointer : NULL;
    for (int eye = 0; eye < 2; ++eye) {
        if (destroy && eye_render_texture[eye]) destroy(eye_render_texture[eye], mi_object_destroy);
        eye_render_texture[eye] = NULL;
    }
    for (unsigned i=0;i<2;++i) scale_free(&scale_targets[i]);
    if (destroy && flat_render_texture) destroy(flat_render_texture, mi_object_destroy);
    flat_render_texture = NULL;
    flat_render_width = 0;
    flat_render_height = 0;
    if (destroy && home_center_render_texture)
        destroy(home_center_render_texture, mi_object_destroy);
    home_center_render_texture = NULL;
    home_center_width = 0;
    home_center_height = 0;
}

static BOOL render_authored_eye_pair(Il2CppObject* owner) {
    if (!owner || InterlockedCompareExchange(&eye_render_in_progress, 1, 0) != 0) return FALSE;
    BOOL rendered = FALSE;
    EyeOptics optics;
    Il2CppObject* transform = component_get_transform(owner);
    if (!transform || !snapshot_eye_optics(&optics)) goto done;

    Il2CppObject* saved_target = camera_get_target(owner);
    float saved_aspect = camera_get_aspect(owner);
    float saved_near_clip = camera_get_near_clip(owner);
    float eye_near_clip = saved_near_clip;
    BOOL restore_near_clip = FALSE;
    float saved_far_clip = camera_get_far_clip(owner);
    Vec3 saved_position;
    Quat saved_rotation;
    Matrix4x4 saved_projection;
    /* The final user pose is a world-space ownership contract.  Applying it as
       a local pose lets an animated parent inject authored roll after RI-02 has
       already reconstructed a roll-clean orientation. */
    transform_get_position(transform, &saved_position);
    transform_get_rotation(transform, &saved_rotation);
    camera_get_projection_matrix(owner, &saved_projection);

    set_object_fn set_target = (set_object_fn)mi_camera_set_target->methodPointer;
    set_float_fn set_aspect = (set_float_fn)mi_camera_set_aspect->methodPointer;
    camera_render_fn render = (camera_render_fn)mi_camera_render->methodPointer;
    LONG width = InterlockedCompareExchange(&requested_eye_width, 0, 0);
    LONG height = InterlockedCompareExchange(&requested_eye_height, 0, 0);
    if (width <= 0 || height <= 0 || !isfinite(saved_near_clip) ||
        !isfinite(saved_far_clip) || saved_near_clip <= 0.0f ||
        saved_far_clip <= saved_near_clip || !authored_pose_anchor_valid) goto restore;

    LONG token=InterlockedCompareExchange(&camera_follow_token,0,0);
    BOOL follow_current=token ? !(token&1) : live_camera_follow;
    BOOL reset=current_authored_generation>0 && InterlockedCompareExchange(&return_camera_generation,0,current_authored_generation)==current_authored_generation;
    if(reset){
        authored_pose_anchor_position=saved_position;
        authored_pose_anchor_rotation=saved_rotation;
        live_follow_anchor.token=0;
    }
    InterlockedExchange(&follow_display_state,follow_current);
    Vec3 effective_position=(token || follow_current)?saved_position:authored_pose_anchor_position;
    Quat effective_rotation=(token || follow_current)?saved_rotation:authored_pose_anchor_rotation;
    apply_camera_anchor(&live_follow_anchor,token,current_scene_handle,&effective_position,&effective_rotation);
    const Vec3* base_position=&effective_position;
    const Quat* base_rotation=&effective_rotation;
    float navigation_base_yaw = 0.0f, navigation_base_pitch = 0.0f;
    if (follow_current)
        quat_to_yaw_pitch_roll(*base_rotation, &navigation_base_yaw,
            &navigation_base_pitch, NULL);
    AcquireSRWLockExclusive(&eye_optics_lock);
    eye_optics.navigation_base_yaw = navigation_base_yaw;
    eye_optics.navigation_base_pitch = navigation_base_pitch;
    ReleaseSRWLockExclusive(&eye_optics_lock);

    if(live_vfx_camera_root){
        static LONG last_mask=-1,last_generation=-1;
        LONG mask=InterlockedCompareExchange(&live_vfx_mask,0,0);
        vfx_log_change=mask!=last_mask || last_generation!=current_authored_generation;
        if(vfx_log_change){
            for(unsigned i=0;i<VFX_COUNT;i++)if(live_vfx_options[i].game_value)
                *live_vfx_options[i].game_value=(mask&(1L<<i))!=0;
            last_mask=mask;last_generation=current_authored_generation;
        }
        Il2CppObject* camera_data=request_dispatch_api.gchandle_get_target(live_vfx_camera_root);
        apply_vfx_overrides(camera_data);
    }
    LONG percent = InterlockedCompareExchange(&scale_requested,0,0);
    Il2CppObject* scaled = scale_target(percent);
    rendered_percent = scaled ? percent : scale_native;
    Vec3 contact_eyes[2];
    Quat contact_rotation;
    if (mi_camera_set_near_clip && mi_camera_set_near_clip->methodPointer &&
        isfinite(world_scale) && world_scale > 0.0f) {
        float limit = 0.001f / world_scale;
        if (limit > 0.0f && limit < saved_near_clip) {
            restore_near_clip = TRUE;
            ((set_float_fn)mi_camera_set_near_clip->methodPointer)(owner, limit, mi_camera_set_near_clip);
            eye_near_clip = camera_get_near_clip(owner);
            if (!isfinite(eye_near_clip) || eye_near_clip <= 0.0f || eye_near_clip >= saved_far_clip)
                goto restore;
        }
    }
    for (int eye = 0; eye < 2; ++eye) {
        if (!eye_render_texture[eye] || !eye_native_game[eye]) goto restore;
        Vec3 eye_position;
        Quat eye_rotation;
        Matrix4x4 eye_projection;
        if (!compose_unity_eye_pose(base_position, base_rotation, follow_current,
                &optics, world_scale, eye, &eye_position, &eye_rotation)) goto restore;
        contact_eyes[eye]=eye_position;
        contact_rotation=eye_rotation;
        /* Keep projection depth exactly aligned with Camera clip properties.
           Authored DoF and other depth reconstruction read those properties;
           a hard-coded projection range makes otherwise valid effects blur. */
        build_unity_eye_projection(&optics.fov[eye], eye_near_clip,
            saved_far_clip, &eye_projection);
        set_target(owner, scaled ? scaled : eye_render_texture[eye], mi_camera_set_target);
        set_aspect(owner, (float)width / (float)height, mi_camera_set_aspect);
        transform_set_position(transform, &eye_position);
        transform_set_rotation(transform, &eye_rotation);
        camera_set_projection_matrix(owner, &eye_projection);
        render(owner, mi_camera_render);
        if (scaled && !scale_copy(scaled,eye_render_texture[eye])) goto restore;
    }
    live_contact_view.center=(Vec3){(contact_eyes[0].x+contact_eyes[1].x)*.5f,
        (contact_eyes[0].y+contact_eyes[1].y)*.5f,(contact_eyes[0].z+contact_eyes[1].z)*.5f};
    live_contact_view.rotation=contact_rotation;
    live_contact_view.optics=optics;
    rendered = TRUE;

restore:
    if (restore_near_clip)
        ((set_float_fn)mi_camera_set_near_clip->methodPointer)(owner, saved_near_clip, mi_camera_set_near_clip);
    if(live_vfx_camera_root)restore_vfx_overrides();
    set_target(owner, saved_target, mi_camera_set_target);
    set_aspect(owner, saved_aspect, mi_camera_set_aspect);
    transform_set_position(transform, &saved_position);
    transform_set_rotation(transform, &saved_rotation);
    camera_set_projection_matrix(owner, &saved_projection);
done:
    InterlockedExchange(&eye_render_in_progress, 0);
    return rendered;
}

static BOOL create_eye_unity_objects(Il2CppObject* owner, int32_t scene_handle,
        enum AuthoredOwnerKind owner_kind) {
    BOOL committed_entry_origin = FALSE;
    LONG width = InterlockedCompareExchange(&requested_eye_width, 0, 0);
    LONG height = InterlockedCompareExchange(&requested_eye_height, 0, 0);
    if (!owner || !InterlockedCompareExchange(&eye_optics_ready, 0, 0) ||
        width <= 0 || height <= 0 || width > 8192 || height > 8192) return FALSE;
    rt_ctor_fn rt_ctor = (rt_ctor_fn)mi_rt_ctor->methodPointer;
    rt_create_fn rt_create = (rt_create_fn)mi_rt_create->methodPointer;
    Il2CppObject* owner_game_object = component_get_game_object(owner);
    if (!owner_game_object) return FALSE;
    Il2CppObject* owner_transform = component_get_transform(owner);
    if (!owner_transform) return FALSE;

    transform_get_position(owner_transform, &authored_pose_anchor_position);
    transform_get_rotation(owner_transform, &authored_pose_anchor_rotation);
    if (!isfinite(authored_pose_anchor_position.x) || !isfinite(authored_pose_anchor_position.y) ||
        !isfinite(authored_pose_anchor_position.z) || !isfinite(authored_pose_anchor_rotation.x) ||
        !isfinite(authored_pose_anchor_rotation.y) || !isfinite(authored_pose_anchor_rotation.z) ||
        !isfinite(authored_pose_anchor_rotation.w)) goto fail;
    authored_pose_anchor_rotation = quat_normalized(authored_pose_anchor_rotation);
    authored_pose_anchor_valid = TRUE;

    Il2CppArray* owner_components = gameobject_get_components(
        owner_game_object, component_reflection_type, 0, 0, 1, 0, NULL);
    uintptr_t component_count = owner_components ? owner_components->max_length : 0;
    if (component_count > 32) component_count = 32;
    uint32_t authored_mask = 0;
    Il2CppObject* camera_data = NULL;
    Il2CppObject* home_controller = NULL;
    Il2CppObject* home_image_effect = NULL;
    for (uintptr_t i = 0; i < component_count; ++i) {
        Il2CppObject* component = owner_components->vector[i];
        uint32_t component_mask = classify_authored_eye_component(component);
        authored_mask |= component_mask;
        if (component_mask == AUTHORED_EYE_CAMERA_DATA) camera_data = component;
        else if (component_mask == AUTHORED_EYE_HOME_CONTROLLER) home_controller = component;
        else if (component_mask == AUTHORED_EYE_HOME_IMAGE_EFFECT) home_image_effect = component;
    }
    uint32_t required_mask = owner_kind == AUTHORED_OWNER_HOME ?
        AUTHORED_EYE_HOME_REQUIRED : AUTHORED_EYE_LIVE_REQUIRED;
    const char* context = owner_kind == AUTHORED_OWNER_HOME ? "Home" : "Live";
    if ((authored_mask & required_mask) != required_mask) {
        char extra[160];
        telemetry_snprintf(extra, sizeof(extra),
            "\"context\":\"%s\",\"component_mask\":\"0x%03x\",\"required_mask\":\"0x%03x\"",
            context, authored_mask, required_mask);
        write_disabled("authored_owner_component_contract", extra);
        return FALSE;
    }
    if (owner_kind == AUTHORED_OWNER_HOME &&
        (!InterlockedCompareExchange(&home_adapter_ready, 0, 0) ||
         !home_controller || !home_image_effect || !mi_graphics_blit ||
         !texture_get_width || !texture_get_height)) {
        write_disabled("home_engine_callback_contract", NULL);
        return FALSE;
    }
    if(owner_kind==AUTHORED_OWNER_LIVE && request_dispatch_api.gchandle_new &&
       request_dispatch_api.gchandle_get_target && request_dispatch_api.gchandle_free)
        live_vfx_camera_root=request_dispatch_api.gchandle_new(camera_data,false);

    for (int eye = 0; eye < 2; ++eye) {
        eye_render_texture[eye] = il2cpp_object_new(rendertexture_class);
        if (!eye_render_texture[eye]) goto fail;
        rt_ctor(eye_render_texture[eye], width, height, 24, 0, mi_rt_ctor);
        if (!rt_create(eye_render_texture[eye], mi_rt_create)) goto fail;
        ID3D11Texture2D* native = (ID3D11Texture2D*)get_native_texture(eye_render_texture[eye]);
        if (!native) goto fail;
        InterlockedExchangePointer((volatile PVOID*)&eye_native_game[eye],native);
    }
    AcquireSRWLockExclusive(&eye_optics_lock);
    if (!immersive_entry_origin_committed &&
            commit_immersive_environment_origin(&eye_optics)) {
        immersive_entry_origin_committed = TRUE;
        committed_entry_origin = TRUE;
    }
    ReleaseSRWLockExclusive(&eye_optics_lock);
    if (owner_kind == AUTHORED_OWNER_LIVE) {
        if (!render_authored_eye_pair(owner)) goto fail;
        InterlockedIncrement64(&eye_source_pair_serial);
        InterlockedExchange(&eye_pair_capture_ready, 1);
    } else {
        InterlockedExchange(&eye_pair_capture_ready, 0);
    }
    /* Publish the completed source/token together. Unity calls above remain
       outside this lock; Present sees NONE until this exclusive commit. */
    AcquireSRWLockExclusive(&eye_source_lock);
    current_authored_owner = owner;
    current_owner_kind = owner_kind;
    current_home_controller = owner_kind == AUTHORED_OWNER_HOME ? home_controller : NULL;
    current_scene_handle = scene_handle;
    LONG generation = InterlockedIncrement(&source_generation);
    current_authored_generation=generation;
    if (owner_kind == AUTHORED_OWNER_LIVE)
        InterlockedExchange64(&source_ready_after_present,
            InterlockedCompareExchange64((volatile LONG64*)&produced_frames, 0, 0) + 2);
    else
        InterlockedExchange64(&source_ready_after_present, INT64_MAX);
    ReleaseSRWLockExclusive(&eye_source_lock);
    char detail[1024];
    telemetry_snprintf(detail, sizeof(detail),
        "\"status\":\"created\",\"generation\":%ld,\"scene_handle\":%d,"
        "\"context\":\"%s\",\"owner_contract\":\"%s\","
        "\"width\":%ld,\"height\":%ld,\"flat_width\":%ld,\"flat_height\":%ld,"
        "\"optics\":\"openxr_fov_ipd\"," 
        "\"visual_pipeline\":\"%s\"," 
        "\"pose_policy\":\"%s\"," 
        "\"tracking_origin\":\"first_immersive_entry\"," 
        "\"original_camera_restored\":true",
        (long)generation, scene_handle, context,
        owner_kind == AUTHORED_OWNER_HOME ?
            "LowResolutionCamera+HomeCameraController+GallopImageEffectOnRenderImage._cachedCamera" :
            "LiveTimelineCamera+LiveImageEffect+MultiCameraFinalComposite", (long)width, (long)height,
        (long)flat_render_width, (long)flat_render_height,
        owner_kind == AUTHORED_OWNER_HOME ?
            "engine_frame_center_left_right_post_effect" : "authored_owner_sequential_render",
        owner_kind == AUTHORED_OWNER_HOME ? "current_authored_yaw_pitch_plus_physical_hmd" :
            (live_camera_follow ? "current_authored_world_pose_plus_physical_hmd" :
                "entry_anchor_plus_physical_hmd"));
    write_record("IMMERSIVE-SOURCE", "source_generation_created", detail);
    return TRUE;
fail:
    if (committed_entry_origin) {
        AcquireSRWLockExclusive(&eye_optics_lock);
        immersive_entry_origin_committed = FALSE;
        ReleaseSRWLockExclusive(&eye_optics_lock);
    }
    destroy_eye_unity_objects();
    write_disabled("eye_rendertexture_or_authored_render", NULL);
    return FALSE;
}

static void bp_main_step(Scene scene,BpSourceKind kind);
static bool story_layer_trial(Scene scene);
static BOOL contact_enabled=TRUE;
static BOOL contact_motion=TRUE;
static float contact_strength=100.0f;
static float contact_tilt=100.0f;
static float contact_duration=1.0f;
static float contact_reach=100.0f;
static float hand_radius_mm=13.f, contact_hold_ms=100.f;
static BOOL contact_haptics=TRUE;
static float haptic_strength=22.0f;
static float haptic_duration=35.0f;
static BOOL contact_consent; /* Set once during startup, before hooks start. */
static void contact_scene_step(Scene scene,bool live);
static volatile LONG scene_hook_calls, scene_updates, scene_home_updates;
static volatile LONG scene_owner_updates, scene_owner_home_updates, scene_owner_tid;
static void update_authored_source(Scene scene) {
    Il2CppString* scene_name = scene_get_name ? scene_get_name(scene.handle) : NULL;
    BOOL live_scene = il2cpp_string_equals_ascii(scene_name, "Live");
    BOOL home_scene = il2cpp_string_equals_ascii(scene_name, "Home");
    BOOL story_scene = il2cpp_string_equals_ascii(scene_name, "Story");
    BOOL owner_thread = GetCurrentThreadId() == (DWORD)InterlockedCompareExchange(&scene_owner_tid,0,0);
    InterlockedIncrement(&scene_updates);
    if (owner_thread) {
        static int32_t logged_scene=INT32_MIN;
        if(scene.handle!=logged_scene && scene_name){
            logged_scene=scene.handle;
            char name[64]={0},detail[150];
            for(int i=0;i<scene_name->length && i<(int)sizeof(name)-1;i++){
                unsigned c=scene_name->chars[i];
                name[i]=(c>=32 && c<127 && c!=34 && c!=92)?(char)c:'_';
            }
            telemetry_snprintf(detail,sizeof detail,"\"stage\":\"scene_%s\",\"scene_handle\":%d",name,scene.handle);
            write_record("MS-IM-508","owned_context_transition",detail);
        }
        LONG default_on=!live_scene || live_camera_follow;
        if(camera_follow_default_on!=default_on)InterlockedExchange(&camera_follow_default_on,default_on);
        InterlockedIncrement(&scene_owner_updates);
    }
    if (home_scene) {
        InterlockedIncrement(&scene_home_updates);
        if (owner_thread) InterlockedIncrement(&scene_owner_home_updates);
    }
    /* The Story component signature also matches a background-only eye path.
       Keep the composite visible during the private draw observation. */
    BOOL race_scene = il2cpp_string_equals_ascii(scene_name, "Race");
    BpSourceKind route=BP_SOURCE_NONE;
    if (race_scene) route=BP_SOURCE_RACE;
    else if (home_scene) route=BP_SOURCE_HOME;
    else if (il2cpp_string_equals_ascii(scene_name,"OutGame")) route=BP_SOURCE_DRESS;
    else if (story_scene && story_layer_trial(scene)) route=BP_SOURCE_STORY;
    bp_main_step(scene,route);

    if (!live_scene) contact_scene_step(scene,false);
    else if (!current_authored_owner) contact_scene_step(scene,true);
    if (home_scene || story_scene) {
        if (current_authored_owner) destroy_eye_unity_objects();
        return;
    }
    if (!live_scene && !home_scene) {
        if (current_authored_owner) destroy_eye_unity_objects();
        return;
    }
    Il2CppArray* cameras = unity_find_objects ? unity_find_objects(camera_reflection_type, 1, 0) : NULL;
    Il2CppObject* owner = NULL;
    float best_depth = -100000.0f;
    uintptr_t count = cameras ? cameras->max_length : 0;
    if (count > 64) count = 64;
    enum AuthoredOwnerKind owner_kind = live_scene ? AUTHORED_OWNER_LIVE : AUTHORED_OWNER_HOME;
    for (uintptr_t i = 0; i < count; ++i) {
        Il2CppObject* candidate = cameras->vector[i];
        HomeOwnerComponents home_components;
        BOOL matches = owner_kind == AUTHORED_OWNER_LIVE ? is_live_owner(candidate) :
            collect_home_owner_components(candidate, &home_components);
        if (matches) {
            float depth = camera_get_depth ? camera_get_depth(candidate) : 0.0f;
            if (!owner || depth > best_depth) {
                owner = candidate;
                best_depth = depth;
            }
        }
    }
    if (!owner) {
        if (current_authored_owner) destroy_eye_unity_objects();
        return;
    }
    if (owner != current_authored_owner || scene.handle != current_scene_handle ||
        owner_kind != current_owner_kind) {
        if (current_authored_owner) destroy_eye_unity_objects();
        create_eye_unity_objects(owner, scene.handle, owner_kind);
    } else if (owner_kind == AUTHORED_OWNER_LIVE) {
        if (!render_authored_eye_pair(owner)) {
            destroy_eye_unity_objects();
        } else {
            contact_scene_step(scene,true);
            LONG64 serial = InterlockedIncrement64(&eye_source_pair_serial);
            InterlockedExchange64(&scale_frame_stamp,(serial<<8)|rendered_percent);
            InterlockedExchange(&eye_pair_capture_ready, 1);
        }
    }
}

static const Il2CppImage* find_image(const Il2CppAssembly** assemblies, size_t count,
        assembly_get_image_fn assembly_get_image, image_get_name_fn image_get_name, const char* wanted) {
    for (size_t i = 0; i < count; ++i) {
        const Il2CppImage* image = assembly_get_image(assemblies[i]);
        const char* name = image ? image_get_name(image) : NULL;
        if (name && _stricmp(name, wanted) == 0) return image;
    }
    return NULL;
}

static domain_get_fn p_domain_get;
static domain_get_assemblies_fn p_domain_get_assemblies;
static assembly_get_image_fn p_assembly_get_image;
static image_get_name_fn p_image_get_name;
static class_from_name_fn p_class_from_name;
static class_get_type_fn p_class_get_type;
static type_get_object_fn p_type_get_object;
static resolve_icall_fn p_resolve_icall;
static thread_current_fn p_thread_current;
static HHOOK unity_readiness_hook;
static HANDLE unity_readiness_event;
static volatile LONG unity_readiness_state;
static volatile LONG unity_readiness_worker_abandoned;

#include "base_producer.h"
#include "home_hand_contact.h"

/* An intermediate camera may live in the character's additive scene and its
   texture may be composited again. Verify the visible body in camera space.
   Cache only numeric renderer IDs, and resolve/validate them every sample. */
static Il2CppObject* preview_model_camera(Il2CppArray* cameras,Scene scene,int32_t* source_mask,const char** result){
    static Il2CppClass* skin;
    static const MethodInfo *viewport,*layer_get,*mesh_get;
    static Il2CppObject* (*resolve)(int32_t);
    static bool bound;
    if(!bound){
        bound=true;contact_bind_native();
        skin=p_class_from_name(story_geometry_core,"UnityEngine","SkinnedMeshRenderer");
        Il2CppClass* mesh=p_class_from_name(story_geometry_core,"UnityEngine","Mesh");
        if(skin && mesh)mesh_get=geometry_method(skin,"get_sharedMesh",mesh,NULL);
        viewport=geometry_method(bp.types[BT_CAMERA],"WorldToViewportPoint",bp.types[BT_V3],bp.types[BT_V3]);
        layer_get=geometry_method(bp.types[BT_GO],"get_layer",bp.types[BT_INT],NULL);
        resolve=(void*)p_resolve_icall("UnityEngine.Object::FindObjectFromInstanceID(System.Int32)");
    }
    *result="preview_model_contract_unavailable";
    if(!skin || !resolve || !viewport || !layer_get || !contact_native.bounds ||
       !contact_native.visible || !contact_native.name || !cameras)return NULL;
    static int32_t body_ids[16],cached_scene,cached_host;
    static size_t count;
    static ULONGLONG refresh;
    ULONGLONG now=GetTickCount64();
    if(now>=refresh || cached_scene!=scene.handle || cached_host!=dress_host_id){
        cached_scene=scene.handle;cached_host=dress_host_id;refresh=now+300;count=0;
        Il2CppReflectionType* type=p_type_get_object(p_class_get_type(skin));
        Il2CppArray* bodies=type?unity_find_objects(type,0,0):NULL;
        if(!bodies || bodies->max_length>512){*result="preview_model_list_unavailable";return NULL;}
        for(uintptr_t i=0;i<bodies->max_length;i++){
            Il2CppObject* body=bodies->vector[i];
            if(!body)continue;
            Il2CppObject* go=component_get_game_object(body);
            if(contact_part_name(go,NULL)!=1){
                Il2CppException* exc=NULL;
                Il2CppObject* mesh=mesh_get?bp.api.runtime_invoke(mesh_get,body,NULL,&exc):NULL;
                if(exc || contact_part_name(mesh,NULL)!=1)continue;
            }
            if(count==16){count=0;*result="preview_model_limit";return NULL;}
            int32_t id=dress_instance(body);if(id)body_ids[count++]=id;
        }
    }
    Il2CppObject* body=NULL;
    *result="preview_visible_body_missing";
    for(size_t i=0;i<count;i++){
        Il2CppObject* value=resolve(body_ids[i]);
        if(!value || !bp.api.class_is_assignable_from(skin,bp.api.object_get_class(value)) ||
           !contact_native.visible(value))continue;
        if(body){*result="preview_visible_body_ambiguous";return NULL;}
        body=value;
    }
    if(!body)return NULL;
    ContactBounds bounds={0};contact_native.bounds(body,&bounds);
    Il2CppObject* go=component_get_game_object(body);int32_t layer=-1;
    *result="preview_body_bounds_unavailable";
    if(!go || !geometry_read(layer_get,go,NULL,bp.types[BT_INT],&layer,sizeof layer) ||
       layer<0 || layer>=32 || !isfinite(bounds.center.x) || !isfinite(bounds.center.y) ||
       !isfinite(bounds.center.z) || !(bounds.extent.y>0))return NULL;
    Il2CppObject* eyes[2]={NULL,NULL};
    for(unsigned i=0;i<2;i++)if(bp.eye[i].camera)eyes[i]=bp.api.gchandle_get_target(bp.eye[i].camera);
    Il2CppObject* selected=NULL;
    *result="preview_body_camera_missing";
    for(uintptr_t i=0;i<cameras->max_length;i++){
        Il2CppObject* camera=cameras->vector[i];uint8_t enabled=0,ortho=1;int32_t mask=0;
        if(!camera || camera==eyes[0] || camera==eyes[1])continue;
        if(!geometry_read(gacha_enabled,camera,NULL,bp.types[BT_BOOL],&enabled,1) || !enabled ||
           !geometry_read(bp.method[BM_ORTHO],camera,NULL,bp.types[BT_BOOL],&ortho,1) || ortho ||
           !geometry_read(bp.method[BM_CULL_GET],camera,NULL,bp.types[BT_INT],&mask,sizeof mask) ||
           !((uint32_t)mask&(1u<<layer)))continue;
        Vec3 point={0};float near_clip=0,far_clip=0;
        if(!geometry_read(viewport,camera,(void*[]){&bounds.center},bp.types[BT_V3],&point,sizeof point) ||
           !geometry_read(bp.method[BM_NEAR_GET],camera,NULL,bp.types[BT_FLOAT],&near_clip,sizeof near_clip) ||
           !geometry_read(bp.method[BM_FAR_GET],camera,NULL,bp.types[BT_FLOAT],&far_clip,sizeof far_clip) ||
           !(point.x>=0 && point.x<=1 && point.y>=0 && point.y<=1 && point.z>near_clip && point.z<far_clip))continue;
        if(selected){*result="preview_body_camera_ambiguous";return NULL;}
        selected=camera;*source_mask=mask;
    }
    if(selected)*result="preview_body_camera_matched";
    return selected;
}


static void initialize_end_frame_bridge(const Il2CppImage* core,const Il2CppImage* game,
        const Il2CppImage* corlib) {
    EfTypes t = {0};
#define EF_TYPE(target,image,ns,name) t.target = image ? p_class_from_name(image,ns,name) : NULL
    EF_TYPE(marshal,corlib,"System.Runtime.InteropServices","Marshal");
    EF_TYPE(action,corlib,"System","Action"); EF_TYPE(intptr,corlib,"System","IntPtr");
    EF_TYPE(type,corlib,"System","Type"); EF_TYPE(delegate,corlib,"System","Delegate");
    EF_TYPE(int32,corlib,"System","Int32"); EF_TYPE(boolean,corlib,"System","Boolean");
    EF_TYPE(mono_behaviour,core,"UnityEngine","MonoBehaviour");
    EF_TYPE(behaviour,core,"UnityEngine","Behaviour"); EF_TYPE(unity_object,core,"UnityEngine","Object");
    EF_TYPE(time,core,"UnityEngine","Time");
    EF_TYPE(scheduler,game,"Gallop","MonoBehaviourExtension"); EF_TYPE(host,game,"Gallop","GameSystem");
#undef EF_TYPE
    /* Existing instance census only, no singleton/static-field read or host
       construction. An absent/ambiguous active GameSystem disables registration. */
    Il2CppReflectionType* host_type = t.host ? p_type_get_object(p_class_get_type(t.host)) : NULL;
    Il2CppArray* hosts = host_type ? unity_find_objects(host_type, 0, 0) : NULL;
    Il2CppObject* host = NULL; unsigned matches = 0;
    uintptr_t count = hosts ? hosts->max_length : 0;
    if (count <= 64) for (uintptr_t i = 0; i < count; ++i) {
        Il2CppObject* candidate = hosts->vector[i];
        if (candidate && request_dispatch_api.object_get_class &&
            request_dispatch_api.object_get_class(candidate) == t.host) { host = candidate; ++matches; }
    }
    if (matches != 1) host = NULL;
    EfStatus status = ef_start(&end_frame_bridge, &request_dispatch_api, &t, host, report_end_frame_callback);
    char detail[640], exception[128], ns[128];
    json_escape(end_frame_bridge.exception_name ? end_frame_bridge.exception_name : "none", exception, sizeof(exception));
    json_escape(end_frame_bridge.exception_namespace ? end_frame_bridge.exception_namespace : "none", ns, sizeof(ns));
    telemetry_snprintf(detail, sizeof(detail),
        "\"status\":\"%s\",\"stage\":\"%s\",\"exception_type\":\"%s\",\"exception_namespace\":\"%s\","
        "\"phase_verified\":false,\"entry_verified\":false,\"tokens_issued\":0,\"producer_called\":false,"
        "\"process_lifetime_roots\":true,\"unload_supported\":false,\"module_pinned\":%s,\"module_pin_error\":%lu",
        ef_status_name(status), end_frame_bridge.binding_stage ? end_frame_bridge.binding_stage : "unbound", exception, ns,
        end_frame_bridge.module_pinned ? "true" : "false", (unsigned long)end_frame_bridge.module_pin_error);
    write_record("IMMERSIVE-SOURCE", "scheduler_bridge_registration", detail);
}

/* Unity's ScriptRunDelayedTasks entry drains its own work before the mod runs.
   The scene getter stays unmodified and no Unity work is queued on a window timer. */
static void hooked_execute_tasks(const MethodInfo* method){
    original_execute_tasks(method);
    if(method!=frame_tasks_method ||
       !InterlockedCompareExchange(&unity_adapter_ready,0,0) ||
       GetCurrentThreadId()!=(DWORD)InterlockedCompareExchange(&scene_owner_tid,0,0) ||
       InterlockedCompareExchange(&eye_render_in_progress,0,0))return;
    static BOOL busy;
    static int last_frame=-1;
    if(busy)return;
    busy=TRUE;
    int frame=scene_frame_count();
    ULONGLONG now=GetTickCount64();
    if(frame>=0 && frame!=last_frame && now-last_source_update_tick>=16){
        last_frame=frame;last_source_update_tick=now;
        if(InterlockedIncrement(&scene_hook_calls)==1)
            write_record("OWN-BASE","owned_context_transition","\"stage\":\"player_loop_scene_owner\"");
        Scene scene=frame_get_active_scene(frame_scene_method);
        update_authored_source(scene);
    }
    busy=FALSE;
}

static bool frame_tasks_signature(Il2CppClass* owner,const MethodInfo* m){
    RdApi* a=&request_dispatch_api;
    if(!m || !m->methodPointer || !a->method_get_flags || !a->method_get_class ||
       !a->method_get_param_count || !a->method_is_generic || !a->method_is_inflated ||
       !a->method_get_return_type || !a->type_get_type || !a->type_is_byref)return false;
    uint32_t impl=0;
    const Il2CppType* ret=a->method_get_return_type(m);
    return a->method_get_class(m)==owner && a->method_get_param_count(m)==0 &&
        (a->method_get_flags(m,&impl)&(0x10|0x400))==0x10 &&
        !a->method_is_generic(m) && !a->method_is_inflated(m) && ret &&
        !a->type_is_byref(ret) && a->type_get_type(ret)==0x01; /* IL2CPP_TYPE_VOID */
}
static void initialize_unity_adapter_on_attached_thread(void) {
    Il2CppDomain* domain = p_domain_get();
    size_t assembly_count = 0;
    const Il2CppAssembly** assemblies = domain ? p_domain_get_assemblies(domain, &assembly_count) : NULL;
    const Il2CppImage* core = assemblies ? find_image(assemblies, assembly_count,
        p_assembly_get_image, p_image_get_name, "UnityEngine.CoreModule.dll") : NULL;
    const Il2CppImage* game_image = assemblies ? find_image(assemblies, assembly_count,
        p_assembly_get_image, p_image_get_name, "umamusume.dll") : NULL;
    Il2CppClass* scene_manager = core ? p_class_from_name(core, "UnityEngine.SceneManagement", "SceneManager") : NULL;
    Il2CppClass* scene_class = core ? p_class_from_name(core, "UnityEngine.SceneManagement", "Scene") : NULL;
    Il2CppClass* camera_class = core ? p_class_from_name(core, "UnityEngine", "Camera") : NULL;
    Il2CppClass* component_class = core ? p_class_from_name(core, "UnityEngine", "Component") : NULL;
    Il2CppClass* texture_class = core ? p_class_from_name(core, "UnityEngine", "Texture") : NULL;
    Il2CppClass* graphics_class = core ? p_class_from_name(core, "UnityEngine", "Graphics") : NULL;
    Il2CppClass* object_class = core ? p_class_from_name(core, "UnityEngine", "Object") : NULL;
    {
        const Il2CppImage* corlib = assemblies ? find_image(assemblies, assembly_count,
            p_assembly_get_image, p_image_get_name, "mscorlib.dll") : NULL;
        RdTypes types = {0};
        types.camera = camera_class;
        types.object = corlib ? p_class_from_name(corlib, "System", "Object") : NULL;
        types.type = corlib ? p_class_from_name(corlib, "System", "Type") : NULL;
        types.boolean = corlib ? p_class_from_name(corlib, "System", "Boolean") : NULL;
        types.int32 = corlib ? p_class_from_name(corlib, "System", "Int32") : NULL;
        types.method_info = corlib ? p_class_from_name(corlib, "System.Reflection", "MethodInfo") : NULL;
        RdStatus status = rd_bind(&request_dispatch_binding, &request_dispatch_api, &types);
        char detail[640], exception_name[128], exception_namespace[128];
        json_escape(request_dispatch_binding.exception_name ? request_dispatch_binding.exception_name : "none",
            exception_name, sizeof(exception_name));
        json_escape(request_dispatch_binding.exception_namespace ? request_dispatch_binding.exception_namespace : "none",
            exception_namespace, sizeof(exception_namespace));
        telemetry_snprintf(detail, sizeof(detail),
            "\"status\":\"%s\",\"route\":%u,\"stage\":\"%s\","
            "\"request_executed\":false,\"renderer_support\":\"unverified\","
            "\"gpu_complete\":false,\"eye_allocation_enabled\":false,"
            "\"exception_namespace\":\"%s\",\"exception_type\":\"%s\"",
            rd_status_name(status), (unsigned)request_dispatch_binding.route,
            request_dispatch_binding.stage ? request_dispatch_binding.stage : "unbound",
            exception_namespace, exception_name);
        write_record("IMMERSIVE-SOURCE", "request_dispatch_binding", detail);
    }
    Il2CppClass* home_controller_class = game_image ?
        p_class_from_name(game_image, "Gallop", "HomeCameraController") : NULL;
    Il2CppClass* home_image_effect_class = game_image ?
        p_class_from_name(game_image, "Gallop", "GallopImageEffectOnRenderImage") : NULL;
    rendertexture_class = core ? p_class_from_name(core, "UnityEngine", "RenderTexture") : NULL;
    const MethodInfo* active = scene_manager ? p_class_get_method(scene_manager, "GetActiveScene", 0) : NULL;
    const MethodInfo* scene_name = scene_class ? p_class_get_method(scene_class, "GetNameInternal", 1) : NULL;
    const MethodInfo* camera_target = camera_class ? p_class_get_method(camera_class, "get_targetTexture", 0) : NULL;
    const MethodInfo* camera_aspect = camera_class ? p_class_get_method(camera_class, "get_aspect", 0) : NULL;
    const MethodInfo* camera_depth = camera_class ? p_class_get_method(camera_class, "get_depth", 0) : NULL;
    const MethodInfo* camera_near_clip = camera_class ? p_class_get_method(camera_class, "get_nearClipPlane", 0) : NULL;
    const MethodInfo* camera_far_clip = camera_class ? p_class_get_method(camera_class, "get_farClipPlane", 0) : NULL;
    const MethodInfo* texture_width = texture_class ? p_class_get_method(texture_class, "get_width", 0) : NULL;
    const MethodInfo* texture_height = texture_class ? p_class_get_method(texture_class, "get_height", 0) : NULL;
    const MethodInfo* home_late_update = home_controller_class ?
        p_class_get_method(home_controller_class, "LateUpdate", 0) : NULL;
    const MethodInfo* home_on_render = home_image_effect_class ?
        p_class_get_method(home_image_effect_class, "OnRender", 2) : NULL;
    mi_camera_set_target = camera_class ? p_class_get_method(camera_class, "set_targetTexture", 1) : NULL;
    mi_camera_set_aspect = camera_class ? p_class_get_method(camera_class, "set_aspect", 1) : NULL;
    mi_camera_set_near_clip = camera_class ? p_class_get_method(camera_class, "set_nearClipPlane", 1) : NULL;
    mi_camera_render = camera_class ? p_class_get_method(camera_class, "Render", 0) : NULL;
    mi_rt_ctor = rendertexture_class ? p_class_get_method(rendertexture_class, ".ctor", 4) : NULL;
    mi_rt_create = rendertexture_class ? p_class_get_method(rendertexture_class, "Create", 0) : NULL;
    contact_physics=assemblies?find_image(assemblies,assembly_count,p_assembly_get_image,p_image_get_name,"UnityEngine.PhysicsModule.dll"):NULL;
    scale_bind(graphics_class,texture_class,rendertexture_class);
    mi_graphics_blit = graphics_class ? p_class_get_method(graphics_class, "Blit", 2) : NULL;
    get_native_texture = texture_class ? (native_texture_icall_fn)p_resolve_icall(
        "UnityEngine.Texture::GetNativeTexturePtr()") : NULL;
    mi_object_destroy = object_class ? p_class_get_method(object_class, "Destroy", 1) : NULL;
    camera_reflection_type = camera_class ? p_type_get_object(p_class_get_type(camera_class)) : NULL;
    component_reflection_type = component_class ? p_type_get_object(p_class_get_type(component_class)) : NULL;
    unity_find_objects = (find_objects_fn)p_resolve_icall("UnityEngine.Object::FindObjectsByType()");
    component_get_game_object = (get_object_fn)p_resolve_icall("UnityEngine.Component::get_gameObject()");
    component_get_transform = (get_object_fn)p_resolve_icall("UnityEngine.Component::get_transform()");
    gameobject_get_components = (get_components_internal_fn)p_resolve_icall("UnityEngine.GameObject::GetComponentsInternal()");
    transform_get_position = (get_vec3_injected_fn)p_resolve_icall("UnityEngine.Transform::get_position_Injected(UnityEngine.Vector3&)");
    transform_get_rotation = (get_quat_injected_fn)p_resolve_icall("UnityEngine.Transform::get_rotation_Injected(UnityEngine.Quaternion&)");
    transform_set_position = (set_vec3_injected_fn)p_resolve_icall("UnityEngine.Transform::set_position_Injected(UnityEngine.Vector3&)");
    transform_set_rotation = (set_quat_injected_fn)p_resolve_icall("UnityEngine.Transform::set_rotation_Injected(UnityEngine.Quaternion&)");
    camera_get_projection_matrix = (get_matrix4x4_injected_fn)p_resolve_icall(
        "UnityEngine.Camera::get_projectionMatrix_Injected(UnityEngine.Matrix4x4&)");
    camera_set_projection_matrix = (set_matrix4x4_injected_fn)p_resolve_icall(
        "UnityEngine.Camera::set_projectionMatrix_Injected(UnityEngine.Matrix4x4&)");
    camera_get_target = camera_target ? (get_object_fn)camera_target->methodPointer : NULL;
    camera_get_aspect = camera_aspect ? (get_float_fn)camera_aspect->methodPointer : NULL;
    camera_get_depth = camera_depth ? (get_float_fn)camera_depth->methodPointer : NULL;
    camera_get_near_clip = camera_near_clip ? (get_float_fn)camera_near_clip->methodPointer : NULL;
    camera_get_far_clip = camera_far_clip ? (get_float_fn)camera_far_clip->methodPointer : NULL;
    texture_get_width = texture_width ? (get_int_fn)texture_width->methodPointer : NULL;
    texture_get_height = texture_height ? (get_int_fn)texture_height->methodPointer : NULL;
    scene_get_name = scene_name ? (get_scene_name_fn)scene_name->methodPointer : NULL;
    uint32_t missing_mask = 0;
    char missing_names[320] = {0};
#define CHECK_CONTRACT(value, bit, label) do { \
        if (!(value)) { \
            missing_mask |= (1u << (bit)); \
            size_t used = strlen(missing_names); \
            if (used < sizeof(missing_names) - 1) \
                telemetry_snprintf(missing_names + used, sizeof(missing_names) - used, \
                    "%s%s", used ? "," : "", label); \
        } \
    } while (0)
    CHECK_CONTRACT(active, 0, "active");
    CHECK_CONTRACT(active && active->methodPointer, 1, "active_ptr");
    CHECK_CONTRACT(scene_get_name, 2, "scene_name");
    CHECK_CONTRACT(camera_get_target, 3, "camera_target_get");
    CHECK_CONTRACT(camera_get_aspect, 4, "camera_aspect_get");
    CHECK_CONTRACT(mi_camera_set_target, 5, "camera_target_set");
    CHECK_CONTRACT(mi_camera_set_aspect, 6, "camera_aspect_set");
    CHECK_CONTRACT(mi_camera_render, 7, "camera_render");
    CHECK_CONTRACT(transform_get_position, 8, "world_position_get");
    CHECK_CONTRACT(mi_rt_ctor, 9, "rt_ctor");
    CHECK_CONTRACT(mi_rt_create, 10, "rt_create");
    CHECK_CONTRACT(get_native_texture, 11, "native_texture");
    CHECK_CONTRACT(transform_get_rotation, 12, "world_rotation_get");
    CHECK_CONTRACT(mi_object_destroy, 13, "object_destroy");
    CHECK_CONTRACT(camera_reflection_type, 14, "camera_type");
    CHECK_CONTRACT(component_reflection_type, 15, "component_type");
    CHECK_CONTRACT(unity_find_objects, 16, "find_objects");
    CHECK_CONTRACT(component_get_game_object, 17, "component_go");
    CHECK_CONTRACT(component_get_transform, 18, "component_transform");
    CHECK_CONTRACT(gameobject_get_components, 19, "go_components");
    CHECK_CONTRACT(transform_set_position, 20, "world_position");
    CHECK_CONTRACT(transform_set_rotation, 21, "world_rotation");
    CHECK_CONTRACT(camera_get_depth, 22, "camera_depth_get");
    CHECK_CONTRACT(camera_set_projection_matrix, 23, "camera_projection");
    CHECK_CONTRACT(camera_get_projection_matrix, 24, "camera_projection_get");
    CHECK_CONTRACT(camera_get_near_clip, 25, "camera_near_clip_get");
    CHECK_CONTRACT(camera_get_far_clip, 26, "camera_far_clip_get");
#undef CHECK_CONTRACT
    if (missing_mask) {
        char extra[416];
        telemetry_snprintf(extra, sizeof(extra), "\"missing_mask\":\"0x%08x\",\"missing\":\"%s\"",
            missing_mask, missing_names);
        write_disabled("unity_adapter_contract", extra);
        return;
    }
    MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) { write_disabled("unity_minhook_initialize", NULL); return; }
    Il2CppClass* context=core?p_class_from_name(core,"UnityEngine","UnitySynchronizationContext"):NULL;
    frame_tasks_method=context?p_class_get_method(context,"ExecuteTasks",0):NULL;
    scene_frame_count=(void*)p_resolve_icall("UnityEngine.Time::get_frameCount()");
    if(!frame_tasks_signature(context,frame_tasks_method) || !scene_frame_count){
        write_disabled("unity_frame_tasks_contract",NULL);return;
    }
    frame_get_active_scene=(get_active_scene_fn)active->methodPointer;
    frame_scene_method=active;
    HMODULE pinned=NULL;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
           (LPCWSTR)(void*)hooked_execute_tasks,&pinned) ||
       MH_CreateHook((LPVOID)frame_tasks_method->methodPointer,(LPVOID)hooked_execute_tasks,
           (LPVOID*)&original_execute_tasks)!=MH_OK ||
       MH_EnableHook((LPVOID)frame_tasks_method->methodPointer)!=MH_OK){
        write_disabled("unity_frame_tasks_hook",NULL);return;
    }
    /* FAIL-036: the metadata-selected OnRender native pointer is shared with
       unrelated IL2CPP call sites, so it cannot be hooked as an object method.
       Keep Home on the complete PANEL until a distinct completed-output owner
       supplies an instance-safe callback/lifetime contract. */
    const Il2CppImage* callback_corlib = assemblies ? find_image(assemblies, assembly_count,
        p_assembly_get_image, p_image_get_name, "mscorlib.dll") : NULL;
    const Il2CppImage* owned_urp = assemblies ? find_image(assemblies, assembly_count,
        p_assembly_get_image, p_image_get_name, "Unity.RenderPipelines.Universal.Runtime.dll") : NULL;
    InterlockedExchange(&scene_owner_tid,(LONG)GetCurrentThreadId());
    const Il2CppImage* ui = assemblies ? find_image(assemblies, assembly_count,
        p_assembly_get_image, p_image_get_name, "UnityEngine.UI.dll") : NULL;
    bp_bind(core,game_image,callback_corlib,owned_urp);
    if (bp.bound && ui) bind_dress_ui(ui);
    initialize_end_frame_bridge(core, game_image, callback_corlib);
    BOOL home_contract = FALSE;
    if (home_contract &&
        MH_CreateHook((LPVOID)home_late_update->methodPointer,
            (LPVOID)hooked_home_late_update, (LPVOID*)&original_home_late_update) == MH_OK &&
        MH_CreateHook((LPVOID)home_on_render->methodPointer,
            (LPVOID)hooked_home_on_render,
            (LPVOID*)&original_home_on_render) == MH_OK &&
        MH_EnableHook((LPVOID)home_late_update->methodPointer) == MH_OK &&
        MH_EnableHook((LPVOID)home_on_render->methodPointer) == MH_OK) {
        InterlockedExchange(&home_adapter_ready, 1);
        write_record("STEREO-011", "home_adapter_ready",
            "\"status\":\"hook_enabled\",\"route\":\"engine_temporal_cached_camera_owner\"," 
            "\"phase_order\":\"CENTER_LEFT_RIGHT\"");
    } else {
        InterlockedExchange(&home_adapter_ready, 0);
        write_record("STEREO-011", "home_adapter_ready",
            "\"status\":\"fail_open\",\"route\":\"panel\"," 
            "\"reason\":\"callback_or_blit_contract_unavailable\"");
    }
    InterlockedExchange(&unity_adapter_ready, 1);
    write_record("IMMERSIVE-SOURCE", "adapter_ready",
        InterlockedCompareExchange(&home_adapter_ready, 0, 0) ?
            "\"status\":\"hook_enabled\",\"owner_profiles\":\"Live,Home\"" :
            "\"status\":\"hook_enabled\",\"owner_profiles\":\"Live\"");
}

static LRESULT CALLBACK unity_readiness_hook_proc(int code, WPARAM wparam, LPARAM lparam) {
    LRESULT next = CallNextHookEx(unity_readiness_hook, code, wparam, lparam);
    if (code >= 0 && p_thread_current && p_thread_current() &&
        InterlockedCompareExchange(&unity_readiness_state, 1, 0) == 0) {
        initialize_unity_adapter_on_attached_thread();
        InterlockedExchange(&unity_readiness_state, 2);
        HANDLE event = unity_readiness_event;
        if (event) SetEvent(event);
        if (event && InterlockedCompareExchange(&unity_readiness_worker_abandoned, 0, 0) != 0 &&
            InterlockedCompareExchangePointer((PVOID volatile*)&unity_readiness_event, NULL, event) == event)
            CloseHandle(event);
    }
    return next;
}

typedef struct UnityWindowSearch { DWORD process_id; DWORD thread_id; } UnityWindowSearch;
static BOOL CALLBACK find_unity_window_thread(HWND window, LPARAM value) {
    UnityWindowSearch* search = (UnityWindowSearch*)value;
    DWORD process_id = 0;
    DWORD thread_id = GetWindowThreadProcessId(window, &process_id);
    if (process_id == search->process_id && thread_id && IsWindowVisible(window)) {
        search->thread_id = thread_id;
        return FALSE;
    }
    return TRUE;
}

static DWORD WINAPI unity_adapter_worker(LPVOID unused) {
    (void)unused;
    if (fast_mode) return 0;
    HMODULE game = NULL;
    for (int i = 0; i < 600 && !(game = GetModuleHandleW(L"GameAssembly.dll")); ++i) Sleep(100);
    if (!game) { write_disabled("unity_gameassembly_timeout", NULL); return 0; }
#define LOAD_EXPORT(target, type, name) target = (type)(void*)GetProcAddress(game, name)
    LOAD_EXPORT(p_domain_get, domain_get_fn, "il2cpp_domain_get");
    LOAD_EXPORT(p_domain_get_assemblies, domain_get_assemblies_fn, "il2cpp_domain_get_assemblies");
    LOAD_EXPORT(p_assembly_get_image, assembly_get_image_fn, "il2cpp_assembly_get_image");
    LOAD_EXPORT(p_image_get_name, image_get_name_fn, "il2cpp_image_get_name");
    LOAD_EXPORT(p_class_from_name, class_from_name_fn, "il2cpp_class_from_name");
    LOAD_EXPORT(p_class_get_method, class_get_method_fn, "il2cpp_class_get_method_from_name");
    LOAD_EXPORT(il2cpp_class_get_name, class_get_name_fn, "il2cpp_class_get_name");
    LOAD_EXPORT(il2cpp_class_get_namespace, class_get_namespace_fn, "il2cpp_class_get_namespace");
    LOAD_EXPORT(il2cpp_class_get_fields, class_get_fields_fn, "il2cpp_class_get_fields");
    LOAD_EXPORT(il2cpp_class_get_methods, class_get_methods_fn, "il2cpp_class_get_methods");
    LOAD_EXPORT(il2cpp_class_get_parent, class_get_parent_fn, "il2cpp_class_get_parent");
    LOAD_EXPORT(il2cpp_field_get_name, field_get_name_fn, "il2cpp_field_get_name");
    LOAD_EXPORT(il2cpp_field_get_type, field_get_type_fn, "il2cpp_field_get_type");
    LOAD_EXPORT(il2cpp_type_get_type, type_get_type_fn, "il2cpp_type_get_type");
    LOAD_EXPORT(il2cpp_field_get_value, field_get_value_fn, "il2cpp_field_get_value");
    LOAD_EXPORT(il2cpp_field_set_value, field_set_value_fn, "il2cpp_field_set_value");
    LOAD_EXPORT(il2cpp_field_get_value_object, field_get_value_object_fn, "il2cpp_field_get_value_object");
    LOAD_EXPORT(il2cpp_object_unbox, object_unbox_fn, "il2cpp_object_unbox");
    LOAD_EXPORT(il2cpp_method_get_name, method_get_name_fn, "il2cpp_method_get_name");
    LOAD_EXPORT(il2cpp_method_get_return_type, method_get_return_type_fn, "il2cpp_method_get_return_type");
    LOAD_EXPORT(p_class_get_type, class_get_type_fn, "il2cpp_class_get_type");
    LOAD_EXPORT(p_type_get_object, type_get_object_fn, "il2cpp_type_get_object");
    LOAD_EXPORT(il2cpp_object_new, object_new_fn, "il2cpp_object_new");
    LOAD_EXPORT(p_resolve_icall, resolve_icall_fn, "il2cpp_resolve_icall");
    LOAD_EXPORT(p_thread_current, thread_current_fn, "il2cpp_thread_current");
#define RD_LOAD(ret,name,args) request_dispatch_api.name = (ret (*) args)(void*)GetProcAddress(game,"il2cpp_" #name);
    RD_API_LIST(RD_LOAD)
#undef RD_LOAD
#undef LOAD_EXPORT
    if (!p_domain_get || !p_domain_get_assemblies || !p_assembly_get_image || !p_image_get_name ||
        !p_class_from_name || !p_class_get_method || !il2cpp_class_get_name || !p_class_get_type ||
        !p_type_get_object || !il2cpp_object_new || !p_resolve_icall || !p_thread_current) {
        write_disabled("unity_il2cpp_exports", NULL); return 0;
    }
    UnityWindowSearch search = {GetCurrentProcessId(), 0};
    for (int i = 0; i < 600 && !search.thread_id; ++i) {
        EnumWindows(find_unity_window_thread, (LPARAM)&search);
        if (!search.thread_id) Sleep(100);
    }
    if (!search.thread_id) { write_disabled("unity_window_thread", NULL); return 0; }
    unity_readiness_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    unity_readiness_hook = SetWindowsHookExW(WH_GETMESSAGE, unity_readiness_hook_proc, g_module, search.thread_id);
    if (!unity_readiness_event || !unity_readiness_hook) {
        write_disabled("unity_attached_callback_hook", NULL); return 0;
    }
    DWORD wait = WaitForSingleObject(unity_readiness_event, 60000);
    if (wait == WAIT_TIMEOUT && InterlockedCompareExchange(&unity_readiness_state, 0, 0) == 1) {
        wait = WaitForSingleObject(unity_readiness_event, 5000);
        if (wait == WAIT_TIMEOUT) InterlockedExchange(&unity_readiness_worker_abandoned, 1);
    }
    HHOOK hook = (HHOOK)InterlockedExchangePointer((PVOID volatile*)&unity_readiness_hook, NULL);
    if (hook) UnhookWindowsHookEx(hook);
    if (InterlockedCompareExchange(&unity_readiness_worker_abandoned, 0, 0) == 0) {
        HANDLE event = (HANDLE)InterlockedExchangePointer((PVOID volatile*)&unity_readiness_event, NULL);
        if (event) CloseHandle(event);
    }
    return 0;
}

/* ================= game-side capture =================
   One CopyResource into a shared keyed-mutex mirror per game Present,
   on the game's own Present thread. The game device is never bound to
   the OpenXR session (FAIL-004). Failures disable capture permanently
   and leave the original Present untouched (fail-open). */

static present_fn original_present;
static void* present_target;
static volatile LONG capture_active;
static volatile LONG km_enabled = 1;
static volatile LONG desktop_mirror_generation;

static ID3D11Texture2D* shared_tex_game;
static IDXGIKeyedMutex* shared_km_game;
static UINT bb_width;
static UINT bb_height;
static HWND g_game_window;
static HANDLE g_shared_handle;
static ID3D11Texture2D* shared_eye_game[2];
static IDXGIKeyedMutex* shared_eye_km_game[2];
static HANDLE shared_eye_handle[2];
static ID3D11Texture2D* shared_flat_game;
static IDXGIKeyedMutex* shared_flat_km_game;
static HANDLE shared_flat_handle;
static DXGI_FORMAT typed_format(DXGI_FORMAT f);

static BOOL ensure_shared_eye_pair(ID3D11Texture2D* source[2]) {
    if (!source[0] || !source[1] || !km_enabled) return FALSE;
    D3D11_TEXTURE2D_DESC desc[2];
    ID3D11Device* devices[2] = {NULL, NULL};
    for (int eye = 0; eye < 2; ++eye) {
        ID3D11Texture2D_GetDesc(source[eye], &desc[eye]);
        ID3D11Texture2D_GetDevice(source[eye], &devices[eye]);
    }
    BOOL compatible = devices[0] && devices[1] && devices[0] == devices[1] &&
        desc[0].Width == desc[1].Width && desc[0].Height == desc[1].Height &&
        typed_format(desc[0].Format) == typed_format(desc[1].Format) &&
        desc[0].SampleDesc.Count == 1 && desc[1].SampleDesc.Count == 1;
    if (!compatible) {
        write_disabled("eye_source_texture_contract", NULL);
        if (devices[0]) ID3D11Device_Release(devices[0]);
        if (devices[1]) ID3D11Device_Release(devices[1]);
        return FALSE;
    }
    if (shared_eye_game[0] && shared_eye_game[1] && shared_eye_handle[0] && shared_eye_handle[1]) {
        D3D11_TEXTURE2D_DESC shared_desc[2];
        ID3D11Device* shared_devices[2] = {NULL, NULL};
        for (int eye = 0; eye < 2; ++eye) {
            ID3D11Texture2D_GetDesc(shared_eye_game[eye], &shared_desc[eye]);
            ID3D11Texture2D_GetDevice(shared_eye_game[eye], &shared_devices[eye]);
        }
        BOOL reusable = shared_devices[0] && shared_devices[1] &&
            devices[0] == shared_devices[0] && devices[1] == shared_devices[1] &&
            desc[0].Width == shared_desc[0].Width && desc[0].Height == shared_desc[0].Height &&
            desc[1].Width == shared_desc[1].Width && desc[1].Height == shared_desc[1].Height &&
            typed_format(desc[0].Format) == typed_format(shared_desc[0].Format) &&
            typed_format(desc[1].Format) == typed_format(shared_desc[1].Format) &&
            desc[0].SampleDesc.Count == shared_desc[0].SampleDesc.Count &&
            desc[1].SampleDesc.Count == shared_desc[1].SampleDesc.Count;
        for (int eye = 0; eye < 2; ++eye) {
            if (shared_devices[eye]) ID3D11Device_Release(shared_devices[eye]);
            if (devices[eye]) ID3D11Device_Release(devices[eye]);
        }
        if (!reusable) {
            write_disabled("shared_eye_pair_reuse_contract", NULL);
        }
        return reusable;
    }
    D3D11_TEXTURE2D_DESC shared_desc = desc[0];
    shared_desc.Format = typed_format(shared_desc.Format);
    shared_desc.MipLevels = 1;
    shared_desc.ArraySize = 1;
    shared_desc.SampleDesc.Count = 1;
    shared_desc.SampleDesc.Quality = 0;
    shared_desc.Usage = D3D11_USAGE_DEFAULT;
    shared_desc.CPUAccessFlags = 0;
    shared_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    shared_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDHANDLE;
    BOOL ok = TRUE;
    for (int eye = 0; eye < 2 && ok; ++eye) {
        HRESULT hr = ID3D11Device_CreateTexture2D(devices[0], &shared_desc, NULL, &shared_eye_game[eye]);
        if (FAILED(hr) || !shared_eye_game[eye]) { ok = FALSE; break; }
        hr = ID3D11Texture2D_QueryInterface(shared_eye_game[eye], &iid_dxgi_keyed_mutex,
            (void**)&shared_eye_km_game[eye]);
        IDXGIResource* resource = NULL;
        if (FAILED(hr) || !shared_eye_km_game[eye] ||
            FAILED(ID3D11Texture2D_QueryInterface(shared_eye_game[eye], &iid_dxgi_resource, (void**)&resource)) ||
            !resource) { if (resource) IDXGIResource_Release(resource); ok = FALSE; break; }
        hr = IDXGIResource_GetSharedHandle(resource, &shared_eye_handle[eye]);
        IDXGIResource_Release(resource);
        if (FAILED(hr) || !shared_eye_handle[eye]) { ok = FALSE; break; }
    }
    if (devices[0]) ID3D11Device_Release(devices[0]);
    if (devices[1]) ID3D11Device_Release(devices[1]);
    if (!ok) {
        for (int eye = 0; eye < 2; ++eye) {
            shared_eye_handle[eye] = NULL;
            if (shared_eye_km_game[eye]) { IDXGIKeyedMutex_Release(shared_eye_km_game[eye]); shared_eye_km_game[eye] = NULL; }
            if (shared_eye_game[eye]) { ID3D11Texture2D_Release(shared_eye_game[eye]); shared_eye_game[eye] = NULL; }
        }
        write_disabled("shared_eye_pair_create", NULL);
        return FALSE;
    }
    char detail[320];
    telemetry_snprintf(detail, sizeof(detail),
        "\"status\":\"observed\",\"width\":%u,\"height\":%u,\"format\":%u,"
        "\"pair_atomic\":true,\"sync\":\"keyed_mutex\"",
        shared_desc.Width, shared_desc.Height, (unsigned)shared_desc.Format);
    write_record("IMMERSIVE-SOURCE", "shared_eye_pair_created", detail);
    return TRUE;
}

static BOOL ensure_shared_flat_source(ID3D11Texture2D* source) {
    if (shared_flat_game && shared_flat_km_game && shared_flat_handle) return TRUE;
    if (!source || !km_enabled) return FALSE;
    D3D11_TEXTURE2D_DESC desc;
    ID3D11Texture2D_GetDesc(source, &desc);
    if (!desc.Width || !desc.Height || desc.SampleDesc.Count != 1) {
        write_disabled("flat_source_texture_contract", NULL);
        return FALSE;
    }
    ID3D11Device* device = NULL;
    ID3D11Texture2D_GetDevice(source, &device);
    if (!device) return FALSE;
    desc.Format = typed_format(desc.Format);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDHANDLE;
    HRESULT hr = ID3D11Device_CreateTexture2D(device, &desc, NULL, &shared_flat_game);
    if (SUCCEEDED(hr) && shared_flat_game)
        hr = ID3D11Texture2D_QueryInterface(shared_flat_game, &iid_dxgi_keyed_mutex,
            (void**)&shared_flat_km_game);
    IDXGIResource* resource = NULL;
    if (SUCCEEDED(hr) && shared_flat_game)
        hr = ID3D11Texture2D_QueryInterface(shared_flat_game, &iid_dxgi_resource,
            (void**)&resource);
    if (SUCCEEDED(hr) && resource) hr = IDXGIResource_GetSharedHandle(resource, &shared_flat_handle);
    if (resource) IDXGIResource_Release(resource);
    ID3D11Device_Release(device);
    if (FAILED(hr) || !shared_flat_game || !shared_flat_km_game || !shared_flat_handle) {
        shared_flat_handle = NULL;
        if (shared_flat_km_game) {
            IDXGIKeyedMutex_Release(shared_flat_km_game);
            shared_flat_km_game = NULL;
        }
        if (shared_flat_game) {
            ID3D11Texture2D_Release(shared_flat_game);
            shared_flat_game = NULL;
        }
        write_disabled("shared_flat_source_create", NULL);
        return FALSE;
    }
    char detail[256];
    telemetry_snprintf(detail, sizeof(detail),
        "\"status\":\"observed\",\"width\":%u,\"height\":%u,\"format\":%u,"
        "\"generation_tokened\":true,\"sync\":\"keyed_mutex\"",
        desc.Width, desc.Height, (unsigned)desc.Format);
    write_record("CTRL-002", "shared_flat_source_created", detail);
    return TRUE;
}

static BOOL output_window_is_eligible(HWND window) {
    if (!window || !IsWindow(window) || !IsWindowVisible(window)) return FALSE;
    DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    return process_id == GetCurrentProcessId();
}

static BOOL mirror_matches_backbuffer(ID3D11Texture2D* bb) {
    if (!bb || !shared_tex_game || !bb_width || !bb_height) return FALSE;
    D3D11_TEXTURE2D_DESC source_desc, mirror_desc;
    ID3D11Texture2D_GetDesc(bb, &source_desc);
    ID3D11Texture2D_GetDesc(shared_tex_game, &mirror_desc);
    if (source_desc.Width != mirror_desc.Width || source_desc.Height != mirror_desc.Height ||
        typed_format(source_desc.Format) != typed_format(mirror_desc.Format) ||
        source_desc.SampleDesc.Count != mirror_desc.SampleDesc.Count ||
        source_desc.SampleDesc.Quality != mirror_desc.SampleDesc.Quality) return FALSE;
    ID3D11Device* source_device = NULL;
    ID3D11Device* mirror_device = NULL;
    ID3D11Texture2D_GetDevice(bb, &source_device);
    ID3D11Texture2D_GetDevice(shared_tex_game, &mirror_device);
    BOOL matches = source_device && mirror_device && source_device == mirror_device;
    if (source_device) ID3D11Device_Release(source_device);
    if (mirror_device) ID3D11Device_Release(mirror_device);
    return matches;
}

static DXGI_FORMAT typed_format(DXGI_FORMAT f) {
    switch (f) {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
        default: return f;
    }
}

static BOOL rebuild_shared_texture(ID3D11Texture2D* bb) {
    char detail[512];
    ID3D11Device* device = NULL;
    ID3D11Texture2D_GetDevice(bb, &device);
    if (!device) {
        write_disabled("capture_device_lookup", NULL);
        return FALSE;
    }
    D3D11_TEXTURE2D_DESC bd;
    ID3D11Texture2D_GetDesc(bb, &bd);
    if (bd.SampleDesc.Count != 1) {
        ID3D11Device_Release(device);
        write_disabled("capture_msaa_unsupported", NULL);
        return FALSE;
    }
    D3D11_TEXTURE2D_DESC sd = bd;
    sd.Format = typed_format(bd.Format);
    sd.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    sd.MiscFlags = km_enabled ? D3D11_RESOURCE_MISC_SHARED_KEYEDHANDLE
                              : D3D11_RESOURCE_MISC_SHARED;
    ID3D11Texture2D* replacement_tex = NULL;
    IDXGIKeyedMutex* replacement_km = NULL;
    HANDLE replacement_handle = NULL;
    HRESULT hr = ID3D11Device_CreateTexture2D(device, &sd, NULL, &replacement_tex);
    if (FAILED(hr) || !replacement_tex) {
        telemetry_snprintf(detail, sizeof(detail), "\"create_hr\":%ld,\"km_enabled\":%ld",
            (long)hr, (long)km_enabled);
        write_disabled("capture_shared_texture_create", detail);
        ID3D11Device_Release(device);
        return FALSE;
    }
    if (km_enabled) {
        hr = ID3D11Texture2D_QueryInterface(replacement_tex, &iid_dxgi_keyed_mutex, (void**)&replacement_km);
        if (FAILED(hr) || !replacement_km) {
            ID3D11Texture2D_Release(replacement_tex); replacement_tex = NULL;
            InterlockedExchange(&km_enabled, 0);
            sd.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
            hr = ID3D11Device_CreateTexture2D(device, &sd, NULL, &replacement_tex);
            if (FAILED(hr) || !replacement_tex) {
                telemetry_snprintf(detail, sizeof(detail), "\"create_hr_fallback\":%ld", (long)hr);
                write_disabled("capture_shared_texture_create", detail);
                ID3D11Device_Release(device);
                return FALSE;
            }
            telemetry_snprintf(detail, sizeof(detail),
                "\"status\":\"observed\",\"sync_mode\":\"none\",\"note\":\"keyed_mutex_unavailable\"");
            write_record("CAP-009", "shared_sync_fallback", detail);
        }
    }
    IDXGIResource* res = NULL;
    hr = ID3D11Texture2D_QueryInterface(replacement_tex, &iid_dxgi_resource, (void**)&res);
    if (SUCCEEDED(hr) && res) {
        hr = IDXGIResource_GetSharedHandle(res, &replacement_handle);
        IDXGIResource_Release(res);
        if (FAILED(hr) || !replacement_handle) {
            telemetry_snprintf(detail, sizeof(detail), "\"get_shared_handle_hr\":%ld", (long)hr);
            write_disabled("capture_shared_handle", detail);
            if (replacement_km) IDXGIKeyedMutex_Release(replacement_km);
            ID3D11Texture2D_Release(replacement_tex);
            ID3D11Device_Release(device);
            return FALSE;
        }
    } else {
        telemetry_snprintf(detail, sizeof(detail), "\"resource_qi_hr\":%ld", (long)hr);
        write_disabled("capture_resource_query", detail);
        if (replacement_km) IDXGIKeyedMutex_Release(replacement_km);
        ID3D11Texture2D_Release(replacement_tex);
        ID3D11Device_Release(device);
        return FALSE;
    }
    ID3D11Device_Release(device);
    ID3D11Texture2D* retired_tex = shared_tex_game;
    IDXGIKeyedMutex* retired_km = shared_km_game;
    shared_tex_game = replacement_tex;
    shared_km_game = replacement_km;
    bb_width = bd.Width;
    bb_height = bd.Height;
    InterlockedExchangePointer((volatile PVOID*)&g_shared_handle, replacement_handle);
    LONG generation = InterlockedIncrement(&desktop_mirror_generation);
    InterlockedExchange(&requested_flat_width, (LONG)bb_width);
    InterlockedExchange(&requested_flat_height, (LONG)bb_height);
    if (retired_km) IDXGIKeyedMutex_Release(retired_km);
    if (retired_tex) ID3D11Texture2D_Release(retired_tex);
    telemetry_snprintf(detail, sizeof(detail),
        "\"status\":\"observed\",\"generation\":%ld,\"width\":%u,\"height\":%u,"
        "\"format\":%u,\"shared_handle_published\":true",
        (long)generation, bb_width, bb_height, (unsigned)sd.Format);
    write_record("CAP-009", "desktop_mirror_generation_created", detail);
    return TRUE;
}


#include "base_producer_gpu.h"

static HRESULT STDMETHODCALLTYPE hooked_present(IDXGISwapChain* chain, UINT sync_interval, UINT flags) {
    if (!original_present) return E_FAIL;
    if (InterlockedCompareExchange(&capture_active, 0, 0) == 0)
        return original_present(chain, sync_interval, flags);

    DXGI_SWAP_CHAIN_DESC quick;
    if (FAILED(IDXGISwapChain_GetDesc(chain, &quick)) ||
        quick.OutputWindow != g_game_window ||
        !output_window_is_eligible(quick.OutputWindow))
        return original_present(chain, sync_interval, flags);

    if(flags & DXGI_PRESENT_TEST)return original_present(chain,sync_interval,flags);

    ID3D11Texture2D* bb = NULL;
    if (FAILED(IDXGISwapChain_GetBuffer(chain, 0, &iid_d3d11_texture2d, (void**)&bb)) || !bb)
        return original_present(chain, sync_interval, flags);
    /* flip-model chains may hand out a different backbuffer texture every
       frame: rebuild the mirror only when the surface description changes,
       never on pointer identity (FAIL-008) */
    if (!mirror_matches_backbuffer(bb)) {
        if (!rebuild_shared_texture(bb)) {
            InterlockedExchangePointer((volatile PVOID*)&g_shared_handle, NULL);
            InterlockedIncrement(&desktop_mirror_generation);
            ID3D11Texture2D_Release(bb);
            write_disabled("desktop_mirror_rebuild_failed", NULL);
            return original_present(chain, sync_interval, flags);
        }
    }

    if (mirror_matches_backbuffer(bb)) {
        BOOL copied = FALSE;
        /* Never wait on the game's Present thread. If the XR consumer owns
           the shared image, keep the previous complete frame (FAIL-005). */
        if (!km_enabled || (shared_km_game &&
            IDXGIKeyedMutex_AcquireSync(shared_km_game, 0, 0)==S_OK)) {
            ID3D11DeviceContext* ctx = NULL;
            ID3D11Device* dev = NULL;
            ID3D11Texture2D_GetDevice(bb, &dev);
            if (dev) {
                ID3D11Device_GetImmediateContext(dev, &ctx);
                if (ctx) {
                    ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource*)shared_tex_game,
                        (ID3D11Resource*)bb);
                    ID3D11DeviceContext_Release(ctx);
                    copied = TRUE;
                }
                ID3D11Device_Release(dev);
            }
            if (km_enabled && IDXGIKeyedMutex_ReleaseSync(shared_km_game, copied?1:0)!=S_OK)copied=FALSE;
        }
        if (copied) InterlockedIncrement64((volatile LONG64*)&produced_frames);
    }

    bp_present(bb);

    /* Eye publication is all-or-nothing. Never wait on Present, and never
       expose a generation until both game-device copies completed under the
       same keyed-mutex ownership interval. */
    if (TryAcquireSRWLockShared(&eye_source_lock)) {
        /* Old authored textures keep their own token even if an owned build
           allocated a new global generation before authored invalidation. */
        LONG generation = current_authored_generation;
        LONG64 ready_after = InterlockedCompareExchange64(&source_ready_after_present, 0, 0);
        LONG64 source_serial = InterlockedCompareExchange64(&eye_source_pair_serial, 0, 0);
        ID3D11Texture2D* source[2] = {eye_native_game[0], eye_native_game[1]};
        ID3D11Texture2D* flat_source = flat_native_game;
        /* The shared pair has ONE owner across OWN/LIVE. The short transport
           interval never waits behind managed configuration or XR consumption. */
        BOOL eye_transport_lock=TryAcquireSRWLockExclusive(&bp_lock);
        if(eye_transport_lock) {
        if (bp_transport_live_current(generation,InterlockedCompareExchange(&source_generation,0,0),
            current_owner_kind==AUTHORED_OWNER_LIVE) &&
            current_owner_kind==AUTHORED_OWNER_LIVE && source[0] && source[1] && source_serial > 0 &&
            source_serial != game_published_source_serial &&
            !bp_transport_blocked(&bp_transport) && !bp_transport_pending(&bp_transport) &&
            bp.c.query==BP_Q_NONE && !bp.c.mutex_owned &&
            InterlockedCompareExchange(&eye_pair_capture_ready, 0, 0) &&
            InterlockedCompareExchange64((volatile LONG64*)&produced_frames, 0, 0) >= ready_after &&
            ensure_shared_eye_pair(source)) {
            HRESULT acquire_result[3];
            int acquired=bp_keyed_pair_acquire(shared_eye_km_game[0],shared_eye_km_game[1],0,acquire_result);
            if(acquired==BP_KM_BROKEN) bp_transport_quarantine(&bp_transport);
            if (acquired==BP_KM_READY) {
                ID3D11Device* eye_device = NULL;
                ID3D11DeviceContext* eye_context = NULL;
                LONG64 serial=0;
                ID3D11Texture2D_GetDevice(source[0], &eye_device);
                if (eye_device) ID3D11Device_GetImmediateContext(eye_device, &eye_context);
                if (eye_context) {
                    ID3D11DeviceContext_CopyResource(eye_context,
                        (ID3D11Resource*)shared_eye_game[0], (ID3D11Resource*)source[0]);
                    ID3D11DeviceContext_CopyResource(eye_context,
                        (ID3D11Resource*)shared_eye_game[1], (ID3D11Resource*)source[1]);
                    ID3D11DeviceContext_Release(eye_context);
                    serial = InterlockedIncrement64(&eye_pair_serial);
                    stereo_observe_cache(&game_pair_telemetry, generation, serial);
                    LONG64 stamp=InterlockedCompareExchange64(&scale_frame_stamp,0,0);
                    game_pair_telemetry.render_percent=(stamp>>8)==source_serial?(LONG)(stamp&255):0;
                    if(!bp_transport_begin(&bp_transport,BP_TRANSPORT_LIVE,generation,serial)) {
                        bp_transport_quarantine(&bp_transport);serial=0;
                    }
                }
                if (eye_device) ID3D11Device_Release(eye_device);
                HRESULT right=IDXGIKeyedMutex_ReleaseSync(shared_eye_km_game[1],serial?1:0);
                HRESULT left=IDXGIKeyedMutex_ReleaseSync(shared_eye_km_game[0],serial?1:0);
                if(right!=S_OK || left!=S_OK) bp_transport_quarantine(&bp_transport);
                if(serial && !bp_transport_blocked(&bp_transport)) {
                    InterlockedExchange64(&eye_pair_tick, (LONG64)GetTickCount64());
                    InterlockedExchange(&published_eye_generation, generation);
                    game_published_source_serial = source_serial;
                }
            }
        }
        ReleaseSRWLockExclusive(&bp_lock);
        }
        /* The auxiliary monoscopic surface is optional for stereo and carries
           the same generation token.  A flat sharing failure must not regress
           the accepted eye-pair route. */
        if (generation > 0 && flat_source && ensure_shared_flat_source(flat_source) &&
            SUCCEEDED(IDXGIKeyedMutex_AcquireSync(shared_flat_km_game, 0, 0))) {
            ID3D11Device* flat_device = NULL;
            ID3D11DeviceContext* flat_context = NULL;
            ID3D11Texture2D_GetDevice(flat_source, &flat_device);
            if (flat_device) ID3D11Device_GetImmediateContext(flat_device, &flat_context);
            if (flat_context) {
                ID3D11DeviceContext_CopyResource(flat_context,
                    (ID3D11Resource*)shared_flat_game, (ID3D11Resource*)flat_source);
                ID3D11DeviceContext_Release(flat_context);
                InterlockedExchange(&published_flat_generation, generation);
            }
            if (flat_device) ID3D11Device_Release(flat_device);
            IDXGIKeyedMutex_ReleaseSync(shared_flat_km_game, 1);
        }
        ReleaseSRWLockShared(&eye_source_lock);
    }
    ID3D11Texture2D_Release(bb);
    return original_present(chain, sync_interval, flags);
}

static HRESULT discover_present_target(D3D_DRIVER_TYPE driver, void** target) {
    *target = NULL;
    HWND window = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"", WS_POPUP,
        0, 0, 1, 1, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!window) return HRESULT_FROM_WIN32(GetLastError());
    DXGI_SWAP_CHAIN_DESC desc; ZeroMemory(&desc, sizeof(desc));
    desc.BufferDesc.Width = 1; desc.BufferDesc.Height = 1;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1; desc.OutputWindow = window; desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL selected = 0;
    IDXGISwapChain* chain = NULL; ID3D11Device* device = NULL; ID3D11DeviceContext* context = NULL;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, driver, NULL, 0, levels,
        (UINT)(sizeof(levels) / sizeof(levels[0])), D3D11_SDK_VERSION, &desc,
        &chain, &device, &selected, &context);
    if (SUCCEEDED(hr) && chain) *target = (*(void***)chain)[8];
    if (context) ID3D11DeviceContext_Release(context);
    if (device) ID3D11Device_Release(device);
    if (chain) IDXGISwapChain_Release(chain);
    DestroyWindow(window);
    return hr;
}

typedef struct WindowSearch { DWORD process_id; BOOL found; HWND window; } WindowSearch;
static BOOL CALLBACK find_visible_process_window(HWND window, LPARAM value) {
    WindowSearch* search = (WindowSearch*)value; DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id == search->process_id && IsWindowVisible(window) && GetWindow(window, GW_OWNER) == NULL) {
        search->found = TRUE; search->window = window; return FALSE;
    }
    return TRUE;
}

static DWORD WINAPI capture_worker(LPVOID unused) {
    (void)unused;
    {
        char sk[8] = {0};
        if (GetEnvironmentVariableA("UMAVR_PANEL_SKIP_CAPTURE", sk, sizeof(sk)) > 0 && sk[0] == '1') {
            write_record(PROBE_ID, "lifecycle_step", "\"status\":\"observed\",\"stage\":\"capture_skipped\"");
            return 0;
        }
    }
    WindowSearch search = {GetCurrentProcessId(), FALSE, NULL};
    int window_waits = fast_mode ? 10 : 1200;
    for (int i = 0; i < window_waits && !search.found; ++i) {
        EnumWindows(find_visible_process_window, (LPARAM)&search);
        if (!search.found) Sleep(fast_mode ? 20 : 100);
    }
    if (!search.found) { write_disabled("visible_process_window", NULL); return 0; }
    g_game_window = search.window;

    HRESULT hardware_hr = discover_present_target(D3D_DRIVER_TYPE_HARDWARE, &present_target);
    if (!present_target) {
        char extra[128];
        telemetry_snprintf(extra, sizeof(extra), "\"hardware_hr\":%ld", (long)hardware_hr);
        write_disabled("present_target_discovery", extra);
        return 0;
    }
    MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        write_disabled("minhook_initialize", NULL); return 0;
    }
    if (MH_CreateHook(present_target, hooked_present, (void**)&original_present) != MH_OK ||
        MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        write_disabled("present_hook_enable", NULL);
        return 0;
    }
    InterlockedExchange(&capture_active, 1);
    char detail[256];
    telemetry_snprintf(detail, sizeof(detail),
        "\"status\":\"hook_enabled\",\"attach_to_capture_ms\":%lu",
        (unsigned long)(GetTickCount64() - attach_tick));
    write_record(PROBE_ID, "ready", detail);
    return 0;
}

/* ================= XR side (probe-owned isolated device) ================= */

static XrSession g_session;
static XrSpace g_space;
static XrSpace g_tracking_space;
static XrActionSet g_controller_action_set;
static XrAction g_aim_action;
static XrAction g_trigger_action;
static XrAction g_primary_face_action;
static XrAction g_secondary_face_action;
static XrAction g_panel_grip_pose_action;
static XrAction g_panel_grip_value_action;
static XrAction g_world_grip_pose, g_world_squeeze;
static XrAction g_return_click, g_follow_click, g_contact_haptic, panel_haptic;
typedef struct { XrTime pressed; BOOL armed; } ReturnButton;
static ReturnButton g_return_button;
static BOOL g_follow_armed;

typedef enum { CAMERA_NONE, CAMERA_RETURN, CAMERA_RECENTER } CameraGesture;

static CameraGesture camera_return_gesture(ReturnButton* button, BOOL active, BOOL down, XrTime now) {
    if (!active) {
        *button = (ReturnButton){0};
        return CAMERA_NONE;
    }
    if (!down) {
        BOOL short_press = button->pressed != 0;
        *button = (ReturnButton){.armed=TRUE};
        return short_press ? CAMERA_RETURN : CAMERA_NONE;
    }
    if (!button->armed) return CAMERA_NONE;
    if (!button->pressed) button->pressed = now;
    if (now - button->pressed < 800000000) return CAMERA_NONE;
    *button = (ReturnButton){0};
    return CAMERA_RECENTER;
}
static LONG g_click_generation;
static XrSpace g_world_grip_space;
typedef struct WorldGrab {
    BOOL armed, holding;
    LONG generation;
    XrTime time;
    XrVector3f position;
} WorldGrab;
static WorldGrab g_world_grab;
static XrAction g_right_stick_action;
static XrAction g_left_stick_action;
static XrSpace g_aim_space;
static XrSpace g_panel_grip_space;
static XrPath g_right_hand_path;
static XrPath g_left_hand_path;
static XrPath g_pointer_hand_path;
static XrPath g_panel_hand_path;
static BOOL g_controller_actions_configured;
static BOOL g_controller_actions_ready;
static BOOL g_aux_panel_visible;
static BOOL g_panel_grip_was_down, panel_short_armed;
static BOOL g_primary_down;
static BOOL g_back_was_down;
static BOOL g_pointer_logged;
static BOOL g_cursor_logged;
static BOOL g_last_pointer_valid;
static float g_last_pointer_u;
static float g_last_pointer_v;
static BOOL g_presented_pointer_valid;
static float g_presented_pointer_u;
static float g_presented_pointer_v;
static XrTime g_navigation_last_time;
static BOOL g_navigation_turn_x_latched;
static BOOL g_navigation_turn_y_latched;
static BOOL g_navigation_move_logged;
static BOOL g_navigation_turn_logged;
static XrSwapchain g_swapchain_proj;
static uint32_t g_view_count;
static uint32_t g_proj_image_count;
static ID3D11Texture2D* g_proj_images[MAX_IMAGES];
static ID3D11RenderTargetView* g_proj_rtvs[MAX_IMAGES][MAX_EYES];
static ID3D11Texture2D* g_local_shared_tex;
static IDXGIKeyedMutex* g_shared_km_probe;
static ID3D11Texture2D* g_local_eye_tex[2];
static IDXGIKeyedMutex* g_local_eye_km[2];
static ID3D11Texture2D* g_eye_cache_tex[2];
static ID3D11ShaderResourceView* g_eye_cache_srv[2];
static ID3D11Texture2D* g_local_flat_tex;
static IDXGIKeyedMutex* g_local_flat_km;
static ID3D11Texture2D* g_flat_cache_tex;
static ID3D11ShaderResourceView* g_flat_cache_srv;
static LONG xr_flat_cache_generation;
static UINT g_flat_width;
static UINT g_flat_height;
static LONG xr_eye_cache_generation;
static StereoCacheTelemetry xr_cache_telemetry;
static StereoTelemetry xr_frame_telemetry;
static unsigned long long xr_observed_frame_id, stereo_end_accepted_total;
typedef struct StereoCadence {
    LONG generation, percent;
    BOOL owned, sampled;
    LONG64 serial, start, fresh_time, sum, longest;
    unsigned long long fresh, reused, intervals;
} StereoCadence;
static StereoCadence cadence;
static void cadence_report(const char* stage) {
    if (!cadence.fresh) return;
    char detail[384];
    telemetry_snprintf(detail,sizeof detail,
        "\"stage\":\"%s\",\"generation\":%ld,\"render_percent\":%ld,\"owned\":%s,"
        "\"fresh_pairs\":%llu,\"reused_frames\":%llu,\"intervals\":%llu,"
        "\"mean_fresh_ms\":%.3f,\"max_fresh_ms\":%.3f",
        stage,cadence.generation,cadence.percent,cadence.owned?"true":"false",
        cadence.fresh,cadence.reused,cadence.intervals,
        cadence.intervals?(double)cadence.sum/(double)cadence.intervals/1e6:0.0,(double)cadence.longest/1e6);
    write_record("PERF","stereo_cadence",detail);
}
static void cadence_step(BOOL accepted,LONG gen,LONG percent,BOOL owned,LONG64 serial,LONG64 time) {
    if (!accepted || gen!=cadence.generation || percent!=cadence.percent || owned!=cadence.owned ||
        time<=cadence.fresh_time || serial<cadence.serial) {
        cadence_report("segment_end");cadence=(StereoCadence){0};
    }
    if(!accepted || gen<=0 || serial<=0 || time<=0) return;
    if(!cadence.fresh){cadence.generation=gen;cadence.percent=percent;cadence.owned=owned;cadence.start=time;}
    if(serial==cadence.serial)++cadence.reused;
    else{
        if(cadence.fresh){LONG64 gap=time-cadence.fresh_time;cadence.sum+=gap;++cadence.intervals;
            if(gap>cadence.longest)cadence.longest=gap;}
        ++cadence.fresh;cadence.serial=serial;cadence.fresh_time=time;
    }
    if(!cadence.sampled && time-cadence.start>=5000000000LL){cadence_report("five_seconds");cadence.sampled=TRUE;}
}
static long stereo_end_logged_generation;
static LONG xr_active_source_generation;
static ULONGLONG xr_live_cache_produced_tick; /* XR-owned timestamp of the copied Live pair */
static unsigned long long immersive_frames_total;
static uint32_t g_eye_width;
static uint32_t g_eye_height;
static XrEnvironmentBlendMode g_blend_mode;
static BOOL g_begun;
static int g_session_state;
static BOOL g_endframe_logged;
static unsigned long long g_submitted_frames;
static unsigned long long copied_frames_total;
static ID3D11Device* probe_device;
static ID3D11DeviceContext* probe_context;
static ULONGLONG attach_tick;

typedef struct XrRuntimeState {
    XrInstance instance;
    XrSystemId system;
    BOOL instance_alive;
} XrRuntimeState;

/* precompiled shader blobs (compiled once on the init thread, before any
   concurrency: D3DCompile crashed when run beside the live capture hook) */
static ID3DBlob* g_vs_blob;
static ID3DBlob* g_ps_blob;
static ID3DBlob* g_panel_ps_blob;
static ID3DBlob* g_vs_dbg_blob;
static ID3DBlob* g_ps_solid_blob;
static ID3DBlob *sphere_vs_blob,*sphere_ps_blob;
static ID3D11VertexShader* sphere_vs;
static ID3D11PixelShader* sphere_ps;
static ID3DBlob* marker_blob;
static ID3D11PixelShader* marker_ps;
static BOOL show_hands=TRUE, show_contact=TRUE;
static ID3DBlob* g_eye_copy_vs_blob;

static ID3D11VertexShader* g_vs;
static ID3D11VertexShader* g_vs_dbg;
static ID3D11PixelShader* g_ps;
static ID3D11PixelShader* g_panel_ps;
static ID3D11PixelShader* g_ps_solid;
static ID3D11VertexShader* g_eye_copy_vs;
static ID3D11InputLayout* g_il;
static ID3D11Buffer* g_vb;
static ID3D11Buffer* g_cb;
static ID3D11SamplerState* g_sampler;
static ID3D11RasterizerState* g_raster;
static ID3D11Texture2D* g_panel_tex;
static ID3D11ShaderResourceView* g_panel_srv;
static volatile LONG g_panel_frame_ready;
static volatile LONG g_aux_panel_source_logged;
static LONG xr_desktop_mirror_generation;
static HANDLE xr_desktop_mirror_handle;
static UINT g_panel_width;
static UINT g_panel_height;

#include "home_panel_visibility.h"
typedef struct PanelVertex {
    float x, y, z, u, v;
} PanelVertex;

static const char* g_vs_src =
    "cbuffer VPCB : register(b0) { float4x4 vp; };"
    "struct VSIn { float3 pos : POSITION; float2 uv : TEXCOORD0; };"
    "struct PSIn { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };"
    "PSIn main(VSIn i) { PSIn o; o.pos = mul(vp, float4(i.pos, 1.0)); o.uv = i.uv; return o; }";

static const char* g_ps_src =
    "Texture2D tex : register(t0); SamplerState samp : register(s0);"
    "struct PSIn { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };"
    "float4 main(PSIn i) : SV_Target { float4 c = tex.Sample(samp, i.uv); c.a = 1.0; return c; }";

/* The PC composite and current own-Base RT bytes are display encoded. Their
   UNORM XR paths use this exact EOTF; accepted authored Live retains g_ps. */
static const char* g_panel_ps_src =
    "Texture2D tex : register(t0); SamplerState samp : register(s0);"
    "struct PSIn { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };"
    "float srgb_to_linear(float c) { return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }"
    "float4 main(PSIn i) : SV_Target { float4 c = tex.Sample(samp, i.uv);"
    "c.rgb = float3(srgb_to_linear(c.r), srgb_to_linear(c.g), srgb_to_linear(c.b));"
    "c.a = 1.0; return c; }";

static const char* g_vs_dbg_src =
    "struct VSIn { float3 pos : POSITION; float2 uv : TEXCOORD0; };"
    "struct PSIn { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };"
    "PSIn main(VSIn i) { PSIn o; o.pos = float4(i.pos.x * 0.25, i.pos.y * 0.25, 0.5, 1); o.uv = i.uv; return o; }";

/* Generate 16 longitude by 12 latitude cells in object space. Project each
   eye separately. The normal and light are in view space. */
static const char* sphere_vertex_shader=
    "cbuffer Sphere : register(b0){float4x4 mvp;};"
    "struct V{float4 p:SV_Position;float3 n:TEXCOORD0;float facing:TEXCOORD1;};"
    "V main(uint id:SV_VertexID){uint cell=id/6,k=id%6;"
    "float u=(cell%16+(k==2||k==4||k==5))/16.;"
    "float v=(cell/16+(k==1||k==3||k==4))/12.;"
    "float a=u*6.2831853,b=v*3.14159265-1.5707963;"
    "float3 n=float3(cos(b)*cos(a),sin(b),cos(b)*sin(a));"
    "V o;o.n=n;o.p=mul(mvp,float4(n,1));"
    "o.facing=n.z;return o;}";
static const char* sphere_pixel_shader=
    "float4 main(float4 p:SV_Position,float3 n:TEXCOORD0,float facing:TEXCOORD1):SV_Target{"
    "clip(facing);float l=saturate(dot(normalize(n),normalize(float3(-.4,.65,.7))));"
    "return float4(.16+.79*l,.23+.73*l,.31+.68*l,1);}";
static const char* marker_shader =
    "float4 main(float4 p:SV_Position,float2 uv:TEXCOORD0):SV_Target {"
    "float r=length(uv-.5); if(r>.5) discard;"
    "if(p.z>.5 && r<.30) discard;"
    "return r>.44?float4(.02,.02,.02,1):p.z>.5?float4(.05,1,.20,1):float4(1,1,1,1);}";
static const char* g_ps_solid_src =
    "struct PSIn { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };"
    "float4 main(PSIn i) : SV_Target {"
    "float2 centered = i.uv - float2(0.5, 0.5);"
    "float radius_sq = dot(centered, centered);"
    "if (radius_sq > 0.25) discard;"
    "return radius_sq > 0.1024 ? float4(0.0, 0.0, 0.0, 1.0) : float4(1.0, 1.0, 1.0, 1.0); }";

/* Unity's native RenderTexture surface is vertically opposite the OpenXR
   D3D11 projection image convention in this path. A fullscreen triangle
   performs the orientation correction without touching either game camera. */
static const char* g_eye_copy_vs_src =
    "struct PSIn { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };"
    "PSIn main(uint id : SV_VertexID) { PSIn o;"
    "float2 uv = float2((id << 1) & 2, id & 2);"
    "o.pos = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);"
    "o.uv = float2(uv.x, 1.0 - uv.y); return o; }";

#define DECLARE(name) static PFN_##name p##name
#define RESOLVE(name) p##name = (PFN_##name)(void*)GetProcAddress(loader, #name)

DECLARE(xrCreateInstance); DECLARE(xrDestroyInstance); DECLARE(xrGetInstanceProperties);
DECLARE(xrGetInstanceProcAddr); DECLARE(xrGetSystem); DECLARE(xrGetSystemProperties);
DECLARE(xrEnumerateViewConfigurationViews); DECLARE(xrEnumerateEnvironmentBlendModes);
DECLARE(xrCreateSession); DECLARE(xrDestroySession); DECLARE(xrPollEvent);
DECLARE(xrCreateReferenceSpace); DECLARE(xrDestroySpace); DECLARE(xrLocateViews);
DECLARE(xrCreateSwapchain); DECLARE(xrDestroySwapchain); DECLARE(xrEnumerateSwapchainFormats);
DECLARE(xrEnumerateSwapchainImages); DECLARE(xrAcquireSwapchainImage);
DECLARE(xrWaitSwapchainImage); DECLARE(xrReleaseSwapchainImage);
DECLARE(xrBeginSession); DECLARE(xrEndSession); DECLARE(xrRequestExitSession);
DECLARE(xrWaitFrame); DECLARE(xrBeginFrame); DECLARE(xrEndFrame); DECLARE(xrResultToString);
DECLARE(xrStringToPath); DECLARE(xrCreateActionSet); DECLARE(xrDestroyActionSet);
DECLARE(xrCreateAction); DECLARE(xrSuggestInteractionProfileBindings);
DECLARE(xrAttachSessionActionSets); DECLARE(xrCreateActionSpace);
DECLARE(xrApplyHapticFeedback); DECLARE(xrSyncActions); DECLARE(xrGetActionStateBoolean); DECLARE(xrGetActionStateFloat);
DECLARE(xrGetActionStateVector2f);
DECLARE(xrLocateSpace);

static BOOL resolve_all(HMODULE loader) {
    RESOLVE(xrCreateInstance); RESOLVE(xrDestroyInstance); RESOLVE(xrGetInstanceProperties);
    RESOLVE(xrGetInstanceProcAddr); RESOLVE(xrGetSystem); RESOLVE(xrGetSystemProperties);
    RESOLVE(xrEnumerateViewConfigurationViews); RESOLVE(xrEnumerateEnvironmentBlendModes);
    RESOLVE(xrCreateSession); RESOLVE(xrDestroySession); RESOLVE(xrPollEvent);
    RESOLVE(xrCreateReferenceSpace); RESOLVE(xrDestroySpace); RESOLVE(xrLocateViews);
    RESOLVE(xrCreateSwapchain); RESOLVE(xrDestroySwapchain); RESOLVE(xrEnumerateSwapchainFormats);
    RESOLVE(xrEnumerateSwapchainImages); RESOLVE(xrAcquireSwapchainImage);
    RESOLVE(xrWaitSwapchainImage); RESOLVE(xrReleaseSwapchainImage);
    RESOLVE(xrBeginSession); RESOLVE(xrEndSession); RESOLVE(xrRequestExitSession);
    RESOLVE(xrWaitFrame); RESOLVE(xrBeginFrame); RESOLVE(xrEndFrame); RESOLVE(xrResultToString);
    RESOLVE(xrStringToPath); RESOLVE(xrCreateActionSet); RESOLVE(xrDestroyActionSet);
    RESOLVE(xrCreateAction); RESOLVE(xrSuggestInteractionProfileBindings);
    RESOLVE(xrAttachSessionActionSets); RESOLVE(xrCreateActionSpace);
    RESOLVE(xrApplyHapticFeedback); RESOLVE(xrSyncActions); RESOLVE(xrGetActionStateBoolean); RESOLVE(xrGetActionStateFloat);
    RESOLVE(xrGetActionStateVector2f);
    RESOLVE(xrLocateSpace);
    return pxrCreateInstance && pxrGetInstanceProperties &&
        pxrGetSystem && pxrGetSystemProperties && pxrEnumerateViewConfigurationViews &&
        pxrEnumerateEnvironmentBlendModes && pxrCreateSession && pxrDestroySession &&
        pxrPollEvent && pxrCreateReferenceSpace && pxrDestroySpace && pxrLocateViews &&
        pxrCreateSwapchain && pxrDestroySwapchain && pxrEnumerateSwapchainFormats &&
        pxrEnumerateSwapchainImages && pxrAcquireSwapchainImage && pxrWaitSwapchainImage &&
        pxrReleaseSwapchainImage && pxrBeginSession && pxrEndSession && pxrRequestExitSession &&
        pxrWaitFrame && pxrBeginFrame && pxrEndFrame && pxrResultToString && pxrGetInstanceProcAddr;
}

static BOOL find_loader_dll(WCHAR* out, DWORD out_chars) {
    WCHAR candidate[MAX_PATH];
    CHAR override_path[MAX_PATH];
    DWORD override_size = GetEnvironmentVariableA("UMAVR_OPENXR_LOADER", override_path, MAX_PATH);
    if (override_size > 0 && override_size < MAX_PATH) {
        MultiByteToWideChar(CP_UTF8, 0, override_path, -1, out, (int)out_chars);
        return TRUE;
    }
    DWORD n = GetModuleFileNameW(g_module, candidate, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        WCHAR* slash = wcsrchr(candidate, L'\\');
        if (slash) { *(slash + 1) = 0; wcscat_s(candidate, MAX_PATH, L"openxr_loader.dll");
            if (GetFileAttributesW(candidate) != INVALID_FILE_ATTRIBUTES) { wcscpy_s(out, out_chars, candidate); return TRUE; } }
    }
    CHAR steam_path[MAX_PATH];
    DWORD size = sizeof(steam_path);
    if (RegGetValueA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath",
            RRF_RT_REG_SZ, NULL, steam_path, &size) == ERROR_SUCCESS) {
        WCHAR wide[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, steam_path, -1, wide, MAX_PATH);
        swprintf_s(out, out_chars, L"%ls\\steamapps\\common\\SteamVR\\bin\\win64\\openxr_loader.dll", wide);
        if (GetFileAttributesW(out) != INVALID_FILE_ATTRIBUTES) return TRUE;
    }
    ExpandEnvironmentStringsW(L"%ProgramFiles(x86)%\\Steam\\steamapps\\common\\SteamVR\\bin\\win64\\openxr_loader.dll",
        candidate, MAX_PATH);
    if (GetFileAttributesW(candidate) != INVALID_FILE_ATTRIBUTES) { wcscpy_s(out, out_chars, candidate); return TRUE; }
    return FALSE;
}

static void log_xr_result(XrInstance instance, const char* label, XrResult result) {
    char text[XR_MAX_RESULT_STRING_SIZE];
    text[0] = 0;
    if (instance && pxrResultToString) pxrResultToString(instance, result, text);
    if (!text[0]) telemetry_snprintf(text, sizeof(text), "xr_result_%ld", (long)result);
    char detail[512], escaped_text[XR_MAX_RESULT_STRING_SIZE * 2], escaped_label[128];
    text[sizeof(text)-1] = 0;
    json_escape(text, escaped_text, sizeof(escaped_text));
    json_escape(label, escaped_label, sizeof(escaped_label));
    telemetry_snprintf(detail, sizeof(detail), "\"status\":\"failed\",\"stage\":\"%s\",\"xr_result\":%ld,\"xr_result_name\":\"%hs\"",
        escaped_label, (long)result, escaped_text);
    write_record(PROBE_ID, "disabled", detail);
}

static void release_synthetic_input(void) {
    if (g_primary_down) {
        INPUT input; ZeroMemory(&input, sizeof(input));
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(input));
        g_primary_down = FALSE;
    }
    g_back_was_down = FALSE;
    g_last_pointer_valid = FALSE;
    g_presented_pointer_valid = FALSE;
}

static void release_primary_input(void) {
    if (!g_primary_down) return;
    INPUT input; ZeroMemory(&input, sizeof(input));
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(input));
    g_primary_down = FALSE;
}

static BOOL create_controller_action(XrActionSet set, XrActionType type,
        const char* name, const char* localized, XrPath subaction_path,
        XrAction* action) {
    XrActionCreateInfo info; ZeroMemory(&info, sizeof(info));
    info.type = XR_TYPE_ACTION_CREATE_INFO;
    strcpy_s(info.actionName, sizeof(info.actionName), name);
    strcpy_s(info.localizedActionName, sizeof(info.localizedActionName), localized);
    info.actionType = type;
    info.countSubactionPaths = 1;
    info.subactionPaths = &subaction_path;
    return XR_SUCCEEDED(pxrCreateAction(set, &info, action));
}

static BOOL create_controller_actions(XrInstance instance) {
    if (!pxrStringToPath || !pxrCreateActionSet || !pxrDestroyActionSet ||
        !pxrCreateAction || !pxrSuggestInteractionProfileBindings ||
        !pxrAttachSessionActionSets || !pxrCreateActionSpace || !pxrSyncActions ||
        !pxrGetActionStateBoolean || !pxrGetActionStateFloat ||
        !pxrGetActionStateVector2f || !pxrLocateSpace) {
        write_disabled("controller_action_api_missing", NULL);
        return FALSE;
    }
    if (XR_FAILED(pxrStringToPath(instance, "/user/hand/right", &g_right_hand_path)) ||
        XR_FAILED(pxrStringToPath(instance, "/user/hand/left", &g_left_hand_path)))
        return FALSE;
    g_pointer_hand_path = controller_hands_swapped ? g_left_hand_path : g_right_hand_path;
    g_panel_hand_path = controller_hands_swapped ? g_right_hand_path : g_left_hand_path;
    XrActionSetCreateInfo set_info; ZeroMemory(&set_info, sizeof(set_info));
    set_info.type = XR_TYPE_ACTION_SET_CREATE_INFO;
    strcpy_s(set_info.actionSetName, sizeof(set_info.actionSetName), "umavr_ui");
    strcpy_s(set_info.localizedActionSetName, sizeof(set_info.localizedActionSetName), "UmaVR UI");
    set_info.priority = 0;
    if (XR_FAILED(pxrCreateActionSet(instance, &set_info, &g_controller_action_set)))
        return FALSE;
    if (!create_controller_action(g_controller_action_set, XR_ACTION_TYPE_POSE_INPUT,
            "pointer_aim", "Pointer Aim", g_pointer_hand_path, &g_aim_action) ||

        !create_controller_action(g_controller_action_set, XR_ACTION_TYPE_FLOAT_INPUT,
            "pointer_trigger", "Pointer Trigger", g_pointer_hand_path, &g_trigger_action) ||
        !create_controller_action(g_controller_action_set, XR_ACTION_TYPE_BOOLEAN_INPUT,
            "pointer_primary", "Pointer Primary Face", g_pointer_hand_path, &g_primary_face_action) ||
        !create_controller_action(g_controller_action_set, XR_ACTION_TYPE_BOOLEAN_INPUT,
            "pointer_secondary", "Pointer Secondary Face", g_pointer_hand_path, &g_secondary_face_action) ||
        !create_controller_action(g_controller_action_set, XR_ACTION_TYPE_POSE_INPUT,
            "panel_grip_pose", "Panel Grip Pose", g_panel_hand_path, &g_panel_grip_pose_action) ||
        !create_controller_action(g_controller_action_set, XR_ACTION_TYPE_FLOAT_INPUT,
            "panel_grip", "Panel Grip", g_panel_hand_path, &g_panel_grip_value_action)) {
        write_disabled("controller_action_create", NULL);
        return FALSE;
    }
    if (!create_controller_action(g_controller_action_set, XR_ACTION_TYPE_VECTOR2F_INPUT,
            "right_stick", "Right Thumbstick", g_right_hand_path,
            &g_right_stick_action) ||
        !create_controller_action(g_controller_action_set, XR_ACTION_TYPE_VECTOR2F_INPUT,
            "left_stick", "Left Thumbstick", g_left_hand_path,
            &g_left_stick_action)) {
        write_disabled("controller_navigation_action_create", NULL);
        return FALSE;
    }

    XrPath touch_profile = XR_NULL_PATH;
    XrPath aim_path = XR_NULL_PATH, trigger_path = XR_NULL_PATH;
    XrPath primary_path = XR_NULL_PATH, secondary_path = XR_NULL_PATH;
    XrPath panel_grip_pose_path = XR_NULL_PATH, panel_grip_value_path = XR_NULL_PATH;
    XrPath right_stick_path = XR_NULL_PATH, left_stick_path = XR_NULL_PATH;
    BOOL aim_ready = XR_SUCCEEDED(pxrStringToPath(instance,
        controller_hands_swapped ? "/user/hand/left/input/aim/pose" :
                                   "/user/hand/right/input/aim/pose", &aim_path));
    XrPath grab_pose_path = XR_NULL_PATH, grab_value_path = XR_NULL_PATH;
    BOOL grab_bindings = create_controller_action(g_controller_action_set, XR_ACTION_TYPE_POSE_INPUT,
            "world_grip_pose", "World Grip Pose", g_pointer_hand_path, &g_world_grip_pose) &&
        create_controller_action(g_controller_action_set, XR_ACTION_TYPE_FLOAT_INPUT,
            "world_squeeze", "World Grab", g_pointer_hand_path, &g_world_squeeze) &&
        XR_SUCCEEDED(pxrStringToPath(instance, controller_hands_swapped ? "/user/hand/left/input/grip/pose" : "/user/hand/right/input/grip/pose", &grab_pose_path)) &&
        XR_SUCCEEDED(pxrStringToPath(instance, controller_hands_swapped ? "/user/hand/left/input/squeeze/value" : "/user/hand/right/input/squeeze/value", &grab_value_path));
    XrPath return_path = XR_NULL_PATH, follow_path = XR_NULL_PATH;
    BOOL camera_bindings = create_controller_action(g_controller_action_set, XR_ACTION_TYPE_BOOLEAN_INPUT,
            "camera_return", "Return to Game Camera", g_left_hand_path, &g_return_click) &&
        create_controller_action(g_controller_action_set, XR_ACTION_TYPE_BOOLEAN_INPUT,
            "camera_follow", "Toggle Live Camera Follow", g_right_hand_path, &g_follow_click) &&
        XR_SUCCEEDED(pxrStringToPath(instance,"/user/hand/left/input/thumbstick/click",&return_path)) &&
        XR_SUCCEEDED(pxrStringToPath(instance,"/user/hand/right/input/thumbstick/click",&follow_path));
    XrPath haptic_path=XR_NULL_PATH;
    BOOL haptic_binding=pxrApplyHapticFeedback &&
        create_controller_action(g_controller_action_set,XR_ACTION_TYPE_VIBRATION_OUTPUT,
            "character_contact","Character Contact",g_pointer_hand_path,&g_contact_haptic) &&
        XR_SUCCEEDED(pxrStringToPath(instance,controller_hands_swapped?"/user/hand/left/output/haptic":"/user/hand/right/output/haptic",&haptic_path));
    XrPath panel_haptic_path=XR_NULL_PATH;
    BOOL panel_haptic_binding=pxrApplyHapticFeedback &&
        create_controller_action(g_controller_action_set,XR_ACTION_TYPE_VIBRATION_OUTPUT,
            "panel_hand_contact","Panel Hand Contact",g_panel_hand_path,&panel_haptic) &&
        XR_SUCCEEDED(pxrStringToPath(instance,controller_hands_swapped?"/user/hand/right/output/haptic":"/user/hand/left/output/haptic",&panel_haptic_path));
    BOOL touch_ready = aim_ready &&
        XR_SUCCEEDED(pxrStringToPath(instance, "/interaction_profiles/oculus/touch_controller", &touch_profile)) &&
        XR_SUCCEEDED(pxrStringToPath(instance,
            controller_hands_swapped ? "/user/hand/left/input/trigger/value" :
                                       "/user/hand/right/input/trigger/value", &trigger_path)) &&
        XR_SUCCEEDED(pxrStringToPath(instance,
            controller_hands_swapped ? "/user/hand/left/input/x/click" :
                                       "/user/hand/right/input/a/click", &primary_path)) &&
        XR_SUCCEEDED(pxrStringToPath(instance,
            controller_hands_swapped ? "/user/hand/left/input/y/click" :
                                       "/user/hand/right/input/b/click", &secondary_path)) &&
        XR_SUCCEEDED(pxrStringToPath(instance,
            controller_hands_swapped ? "/user/hand/right/input/grip/pose" :
                                       "/user/hand/left/input/grip/pose", &panel_grip_pose_path)) &&
        XR_SUCCEEDED(pxrStringToPath(instance,
            controller_hands_swapped ? "/user/hand/right/input/squeeze/value" :
                                       "/user/hand/left/input/squeeze/value", &panel_grip_value_path)) &&
        XR_SUCCEEDED(pxrStringToPath(instance, "/user/hand/right/input/thumbstick", &right_stick_path)) &&
        XR_SUCCEEDED(pxrStringToPath(instance, "/user/hand/left/input/thumbstick", &left_stick_path));
    if (touch_ready) {
        XrActionSuggestedBinding bindings[] = {
            {g_aim_action, aim_path}, {g_trigger_action, trigger_path},
            {g_primary_face_action, primary_path}, {g_secondary_face_action, secondary_path},
            {g_panel_grip_pose_action, panel_grip_pose_path},
            {g_panel_grip_value_action, panel_grip_value_path},
            {g_right_stick_action, right_stick_path},
            {g_left_stick_action, left_stick_path},
            {0}, {0}, {0}, {0}, {0}, {0}
        };
        XrInteractionProfileSuggestedBinding suggested; ZeroMemory(&suggested, sizeof(suggested));
        suggested.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
        suggested.interactionProfile = touch_profile;
        suggested.countSuggestedBindings = (uint32_t)(sizeof(bindings) / sizeof(bindings[0]));
        suggested.suggestedBindings = bindings;
        suggested.countSuggestedBindings -= 6;
        if (grab_bindings) {
            bindings[suggested.countSuggestedBindings++] = (XrActionSuggestedBinding){g_world_grip_pose,grab_pose_path};
            bindings[suggested.countSuggestedBindings++] = (XrActionSuggestedBinding){g_world_squeeze,grab_value_path};
        }
        if (camera_bindings) {
            bindings[suggested.countSuggestedBindings++] = (XrActionSuggestedBinding){g_return_click,return_path};
            bindings[suggested.countSuggestedBindings++] = (XrActionSuggestedBinding){g_follow_click,follow_path};
        }
        if(haptic_binding)
            bindings[suggested.countSuggestedBindings++]=(XrActionSuggestedBinding){g_contact_haptic,haptic_path};
        if(panel_haptic_binding)
            bindings[suggested.countSuggestedBindings++]=(XrActionSuggestedBinding){panel_haptic,panel_haptic_path};
        touch_ready = XR_SUCCEEDED(pxrSuggestInteractionProfileBindings(instance, &suggested));
        if(!touch_ready && panel_haptic_binding){
            --suggested.countSuggestedBindings;
            touch_ready=XR_SUCCEEDED(pxrSuggestInteractionProfileBindings(instance,&suggested));
        }
        if(!touch_ready && haptic_binding){
            --suggested.countSuggestedBindings;
            touch_ready=XR_SUCCEEDED(pxrSuggestInteractionProfileBindings(instance,&suggested));
        }
        if (!touch_ready && camera_bindings) {
            suggested.countSuggestedBindings -= 2;
            touch_ready = XR_SUCCEEDED(pxrSuggestInteractionProfileBindings(instance, &suggested));
        }
        if (!touch_ready && grab_bindings) {
            suggested.countSuggestedBindings -= 2;
            touch_ready = XR_SUCCEEDED(pxrSuggestInteractionProfileBindings(instance, &suggested));
        }
    }

    XrPath simple_profile = XR_NULL_PATH, simple_select = XR_NULL_PATH;
    BOOL simple_ready = aim_ready &&
        XR_SUCCEEDED(pxrStringToPath(instance, "/interaction_profiles/khr/simple_controller", &simple_profile)) &&
        XR_SUCCEEDED(pxrStringToPath(instance,
            controller_hands_swapped ? "/user/hand/left/input/select/click" :
                                       "/user/hand/right/input/select/click", &simple_select));
    if (simple_ready) {
        XrActionSuggestedBinding bindings[] = {
            {g_aim_action, aim_path}, {g_primary_face_action, simple_select}
        };
        XrInteractionProfileSuggestedBinding suggested; ZeroMemory(&suggested, sizeof(suggested));
        suggested.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
        suggested.interactionProfile = simple_profile;
        suggested.countSuggestedBindings = (uint32_t)(sizeof(bindings) / sizeof(bindings[0]));
        suggested.suggestedBindings = bindings;
        simple_ready = XR_SUCCEEDED(pxrSuggestInteractionProfileBindings(instance, &suggested));
    }
    if (!touch_ready && !simple_ready) {
        write_disabled("controller_profile_bindings", NULL);
        return FALSE;
    }
    write_record("CTRL-002", "controller_actions_created",
        touch_ready ? "\"status\":\"observed\",\"profile\":\"oculus_touch\"" :
                      "\"status\":\"observed\",\"profile\":\"khr_simple\"");
    if (touch_ready) {
        char detail[256];
        telemetry_snprintf(detail, sizeof(detail),
            "\"status\":\"observed\",\"locomotion_hand\":\"%s\"," 
            "\"view_turn_hand\":\"%s\",\"hands_swapped\":%s",
            controller_hands_swapped ? "left" : "right",
            controller_hands_swapped ? "right" : "left",
            controller_hands_swapped ? "true" : "false");
        write_record("CTRL-006", "controller_navigation_actions_created", detail);
    }
    {
        char detail[320];
        telemetry_snprintf(detail, sizeof(detail),
            "\"status\":\"observed\",\"pointer_hand\":\"%s\"," 
            "\"panel_hand\":\"%s\",\"primary_face\":\"%s\"," 
            "\"secondary_face\":\"%s\",\"hands_swapped\":%s",
            controller_hands_swapped ? "left" : "right",
            controller_hands_swapped ? "right" : "left",
            controller_hands_swapped ? "X" : "A",
            controller_hands_swapped ? "Y" : "B",
            controller_hands_swapped ? "true" : "false");
        write_record("CTRL-007", "controller_role_actions_created", detail);
    }
    g_controller_actions_configured = TRUE;
    return TRUE;
}

static BOOL attach_controller_actions(void) {
    if (!g_controller_action_set || !g_session) return FALSE;
    XrSessionActionSetsAttachInfo attach; ZeroMemory(&attach, sizeof(attach));
    attach.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
    attach.countActionSets = 1;
    attach.actionSets = &g_controller_action_set;
    if (XR_FAILED(pxrAttachSessionActionSets(g_session, &attach))) {
        write_disabled("controller_action_attach", NULL);
        return FALSE;
    }
    XrActionSpaceCreateInfo space; ZeroMemory(&space, sizeof(space));
    space.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
    space.action = g_aim_action;
    space.subactionPath = g_pointer_hand_path;
    space.poseInActionSpace.orientation.w = 1.0f;
    if (XR_FAILED(pxrCreateActionSpace(g_session, &space, &g_aim_space))) {
        write_disabled("controller_aim_space", NULL);
        return FALSE;
    }
    space.action = g_world_grip_pose;
    if (!g_world_grip_pose || XR_FAILED(pxrCreateActionSpace(g_session, &space, &g_world_grip_space)))
        g_world_grip_space = XR_NULL_HANDLE;
    space.action = g_panel_grip_pose_action;
    space.subactionPath = g_panel_hand_path;
    if (XR_FAILED(pxrCreateActionSpace(g_session, &space, &g_panel_grip_space))) {
        write_disabled("controller_panel_grip_space", NULL);
        return FALSE;
    }
    g_controller_actions_ready = TRUE;
    {
        char detail[320];
        telemetry_snprintf(detail, sizeof(detail),
            "\"status\":\"observed\",\"pointer_hand\":\"%s\",\"panel_hand\":\"%s\"",
            controller_hands_swapped ? "left" : "right",
            controller_hands_swapped ? "right" : "left");
        write_record("CTRL-002", "controller_actions_attached", detail);
    }
    return TRUE;
}

static void destroy_xr_state(XrRuntimeState* st) {
    release_synthetic_input();
    g_controller_actions_configured = FALSE;
    g_controller_actions_ready = FALSE;
    g_aux_panel_visible = FALSE;
    g_panel_grip_was_down = FALSE;
    AcquireSRWLockExclusive(&eye_optics_lock);
    eye_optics.origin_valid = FALSE;
    eye_optics.return_orientation_valid=FALSE;
    eye_optics.artificial_position = (Vec3){0.0f, 0.0f, 0.0f};
    eye_optics.artificial_yaw = 0.0f;
    eye_optics.artificial_pitch = 0.0f;
    immersive_entry_origin_committed = FALSE;
    ReleaseSRWLockExclusive(&eye_optics_lock);
    g_navigation_last_time = 0;
    g_navigation_turn_x_latched = FALSE;
    g_navigation_turn_y_latched = FALSE;
    g_navigation_move_logged = FALSE;
    g_navigation_turn_logged = FALSE;
    InterlockedExchange(&eye_optics_ready, 0);
    if (probe_context) ID3D11DeviceContext_ClearState(probe_context);
    for (uint32_t image=0; image<g_proj_image_count && image<MAX_IMAGES; ++image) {
        for (uint32_t eye=0;eye<MAX_EYES;++eye) {
            if(g_proj_rtvs[image][eye]) ID3D11RenderTargetView_Release(g_proj_rtvs[image][eye]);
            g_proj_rtvs[image][eye]=NULL;
        }
        /* xrEnumerateSwapchainImages lends its textures. Only the views are ours. */
        g_proj_images[image]=NULL;
    }
    g_proj_image_count=0;
    if (g_swapchain_proj) { pxrDestroySwapchain(g_swapchain_proj); g_swapchain_proj = 0; }
    g_world_grab = (WorldGrab){0};
    if (g_world_grip_space) { pxrDestroySpace(g_world_grip_space); g_world_grip_space = 0; }
    g_world_grip_pose = g_world_squeeze = g_return_click = g_follow_click = XR_NULL_HANDLE;
    g_click_generation = 0;g_contact_haptic=panel_haptic=XR_NULL_HANDLE;
    AcquireSRWLockExclusive(&contact_lock);ZeroMemory(&contact,sizeof contact);ReleaseSRWLockExclusive(&contact_lock);
    InterlockedExchange(&return_camera_generation,0);
    g_follow_armed=FALSE;
    if (g_panel_grip_space) { pxrDestroySpace(g_panel_grip_space); g_panel_grip_space = 0; }
    if (g_aim_space) { pxrDestroySpace(g_aim_space); g_aim_space = 0; }
    if (g_tracking_space) { pxrDestroySpace(g_tracking_space); g_tracking_space = 0; }
    if (g_space) { pxrDestroySpace(g_space); g_space = 0; }
    if (g_session) { pxrDestroySession(g_session); g_session = 0; }
    if (g_controller_action_set) {
        pxrDestroyActionSet(g_controller_action_set);
        g_controller_action_set = 0;
        g_aim_action = g_trigger_action = g_primary_face_action = g_secondary_face_action = 0;
        g_panel_grip_pose_action = g_panel_grip_value_action = 0;
        g_right_stick_action = g_left_stick_action = 0;
    }
    xr_eye_cache_generation=0;
    xr_cache_telemetry=(StereoCacheTelemetry){0};
    g_begun=FALSE;
    g_session_state=XR_SESSION_STATE_UNKNOWN;
    menu_reset();
    if (g_panel_srv) { ID3D11ShaderResourceView_Release(g_panel_srv); g_panel_srv = NULL; }
    if (g_panel_tex) { ID3D11Texture2D_Release(g_panel_tex); g_panel_tex = NULL; }
    InterlockedExchange(&g_panel_frame_ready, 0);
    if (g_shared_km_probe) { IDXGIKeyedMutex_Release(g_shared_km_probe); g_shared_km_probe = NULL; }
    if (g_local_shared_tex) { ID3D11Texture2D_Release(g_local_shared_tex); g_local_shared_tex = NULL; }
    xr_desktop_mirror_generation = 0;
    xr_desktop_mirror_handle = NULL;
    g_panel_width = 0;
    g_panel_height = 0;
    for (int eye = 0; eye < 2; ++eye) {
        if (g_eye_cache_srv[eye]) { ID3D11ShaderResourceView_Release(g_eye_cache_srv[eye]); g_eye_cache_srv[eye] = NULL; }
        if (g_local_eye_km[eye]) { IDXGIKeyedMutex_Release(g_local_eye_km[eye]); g_local_eye_km[eye] = NULL; }
        if (g_local_eye_tex[eye]) { ID3D11Texture2D_Release(g_local_eye_tex[eye]); g_local_eye_tex[eye] = NULL; }
        if (g_eye_cache_tex[eye]) { ID3D11Texture2D_Release(g_eye_cache_tex[eye]); g_eye_cache_tex[eye] = NULL; }
    }
    if (g_flat_cache_srv) { ID3D11ShaderResourceView_Release(g_flat_cache_srv); g_flat_cache_srv = NULL; }
    if (g_flat_cache_tex) { ID3D11Texture2D_Release(g_flat_cache_tex); g_flat_cache_tex = NULL; }
    if (g_local_flat_km) { IDXGIKeyedMutex_Release(g_local_flat_km); g_local_flat_km = NULL; }
    if (g_local_flat_tex) { ID3D11Texture2D_Release(g_local_flat_tex); g_local_flat_tex = NULL; }
    xr_flat_cache_generation = 0;
    g_flat_width = 0;
    g_flat_height = 0;
    if (st->instance_alive) { pxrDestroyInstance(st->instance); st->instance = 0; st->instance_alive = FALSE; }
}

static BOOL create_probe_device(const LUID* preferred_luid, char* detail, size_t detail_chars) {
    IDXGIFactory1* factory = NULL;
    HRESULT hr = CreateDXGIFactory1(&iid_dxgi_factory1, (void**)&factory);
    if (FAILED(hr) || !factory) {
        telemetry_snprintf(detail, detail_chars, "\"factory_hr\":%ld", (long)hr);
        return FALSE;
    }
    IDXGIAdapter1* chosen = NULL;
    IDXGIAdapter1* fallback = NULL;
    for (UINT i = 0;; ++i) {
        IDXGIAdapter1* adapter = NULL;
        if (factory->lpVtbl->EnumAdapters1(factory, i, &adapter) == DXGI_ERROR_NOT_FOUND) break;
        if (!adapter) break;
        DXGI_ADAPTER_DESC1 desc;
        if (SUCCEEDED(adapter->lpVtbl->GetDesc1(adapter, &desc))) {
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) { adapter->lpVtbl->Release(adapter); continue; }
            if (preferred_luid &&
                desc.AdapterLuid.LowPart == preferred_luid->LowPart &&
                desc.AdapterLuid.HighPart == preferred_luid->HighPart) {
                if (chosen) chosen->lpVtbl->Release(chosen);
                chosen = adapter;
                break;
            }
            if (!fallback) fallback = adapter;
            else adapter->lpVtbl->Release(adapter);
        } else {
            adapter->lpVtbl->Release(adapter);
        }
    }
    if (!chosen && fallback) chosen = fallback;
    else if (fallback) fallback->lpVtbl->Release(fallback);
    factory->lpVtbl->Release(factory);
    if (!chosen) return FALSE;

    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL selected = 0;
    hr = D3D11CreateDevice((IDXGIAdapter*)chosen, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0,
        levels, 2, D3D11_SDK_VERSION, &probe_device, &selected, &probe_context);
    DXGI_ADAPTER_DESC1 desc;
    ZeroMemory(&desc, sizeof(desc));
    BOOL have_desc = SUCCEEDED(chosen->lpVtbl->GetDesc1(chosen, &desc));
    chosen->lpVtbl->Release(chosen);
    if (FAILED(hr) || !probe_device || !probe_context) {
        telemetry_snprintf(detail, detail_chars, "\"device_hr\":%ld", (long)hr);
        return FALSE;
    }
    char adapter_utf8[512] = {0}, adapter_escaped[1024];
    desc.Description[(sizeof(desc.Description) / sizeof(desc.Description[0])) - 1] = 0;
    if (!have_desc || !WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1,
            adapter_utf8, sizeof(adapter_utf8), NULL, NULL))
        memcpy(adapter_utf8, "unavailable", sizeof("unavailable"));
    json_escape(adapter_utf8, adapter_escaped, sizeof(adapter_escaped));
    telemetry_snprintf(detail, detail_chars,
        "\"status\":\"observed\",\"adapter_luid\":\"%08lx-%08lx\",\"adapter_desc\":\"%s\","
        "\"feature_level\":%u,\"isolated_from_game_device\":true",
        (unsigned long)desc.AdapterLuid.HighPart, (unsigned long)desc.AdapterLuid.LowPart,
        adapter_escaped, (unsigned)selected);
    return TRUE;
}

static BOOL create_projection_swapchain(void) {
    char detail[512];
    XrSwapchainCreateInfo swci; ZeroMemory(&swci, sizeof(swci));
    swci.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
    swci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    swci.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swci.sampleCount = 1;
    swci.width = g_eye_width;
    swci.height = g_eye_height;
    swci.faceCount = 1;
    swci.arraySize = g_view_count;
    swci.mipCount = 1;
    XrResult result = pxrCreateSwapchain(g_session, &swci, &g_swapchain_proj);
    if (XR_FAILED(result)) {
        log_xr_result(XR_NULL_HANDLE, "xrCreateSwapchain(proj)", result);
        g_swapchain_proj = 0;
        return FALSE;
    }
    XrSwapchainImageD3D11KHR images[MAX_IMAGES];
    memset(images, 0, sizeof(images));
    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_IMAGES; ++i) images[i].type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
    if (!XR_SUCCEEDED(pxrEnumerateSwapchainImages(g_swapchain_proj, MAX_IMAGES,
            &count, (XrSwapchainImageBaseHeader*)images)) || count > MAX_IMAGES) {
        write_disabled("proj_swapchain_image_enumeration", NULL);
        return FALSE;
    }
    g_proj_image_count = count;
    D3D11_RENDER_TARGET_VIEW_DESC rtvd;
    rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    rtvd.Texture2DArray.MipSlice = 0;
    rtvd.Texture2DArray.ArraySize = 1;
    for (uint32_t i = 0; i < count; ++i) {
        g_proj_images[i] = images[i].texture;
        D3D11_TEXTURE2D_DESC td;
        ID3D11Texture2D_GetDesc(g_proj_images[i], &td);
        rtvd.Format = td.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS ? DXGI_FORMAT_R8G8B8A8_UNORM : td.Format;
        for (uint32_t eye = 0; eye < g_view_count && eye < MAX_EYES; ++eye) {
            rtvd.Texture2DArray.FirstArraySlice = eye;
            ID3D11Device_CreateRenderTargetView(probe_device,
                (ID3D11Resource*)g_proj_images[i], &rtvd, &g_proj_rtvs[i][eye]);
        }
    }
    telemetry_snprintf(detail, sizeof(detail),
        "\"status\":\"observed\",\"images\":%u,\"array_size\":%u,\"width\":%u,\"height\":%u",
        count, swci.arraySize, swci.width, swci.height);
    write_record("CAP-004", "projection_swapchain_created", detail);
    return TRUE;
}

static BOOL open_shared_on_probe_device(void) {
    char detail[384];
    LONG generation = InterlockedCompareExchange(&desktop_mirror_generation, 0, 0);
    HANDLE handle = (HANDLE)InterlockedCompareExchangePointer((volatile PVOID*)&g_shared_handle, NULL, NULL);
    if (!handle || generation <= 0) {
        InterlockedExchange(&g_panel_frame_ready, 0);
        return FALSE;
    }
    if (xr_desktop_mirror_generation == generation && xr_desktop_mirror_handle == handle &&
        g_local_shared_tex && g_panel_tex && g_panel_srv && (!km_enabled || g_shared_km_probe))
        return TRUE;
    InterlockedExchange(&g_panel_frame_ready, 0);
    ID3D11Texture2D* replacement_local = NULL;
    IDXGIKeyedMutex* replacement_km = NULL;
    ID3D11Texture2D* replacement_cache = NULL;
    ID3D11ShaderResourceView* replacement_srv = NULL;
    HRESULT hr = ID3D11Device_OpenSharedResource(probe_device, handle,
        &iid_d3d11_texture2d, (void**)&replacement_local);
    if (FAILED(hr) || !replacement_local) {
        telemetry_snprintf(detail, sizeof(detail), "\"open_hr\":%ld", (long)hr);
        write_disabled("open_shared_resource", detail);
        return FALSE;
    }
    if (km_enabled) {
        hr = ID3D11Texture2D_QueryInterface(replacement_local, &iid_dxgi_keyed_mutex,
            (void**)&replacement_km);
        if (FAILED(hr) || !replacement_km) {
            write_disabled("probe_keyed_mutex_query", NULL);
            goto fail;
        }
    }
    D3D11_TEXTURE2D_DESC td;
    ID3D11Texture2D_GetDesc(replacement_local, &td);
    D3D11_TEXTURE2D_DESC panel_td = td;
    panel_td.Usage = D3D11_USAGE_DEFAULT;
    panel_td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    panel_td.CPUAccessFlags = 0;
    panel_td.MiscFlags = 0;
    HRESULT cache_hr = ID3D11Device_CreateTexture2D(probe_device, &panel_td, NULL, &replacement_cache);
    if (FAILED(cache_hr) || !replacement_cache) {
        telemetry_snprintf(detail, sizeof(detail), "\"panel_cache_hr\":%ld", (long)cache_hr);
        write_disabled("panel_cache_create", detail);
        goto fail;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
    ZeroMemory(&srvd, sizeof(srvd));
    srvd.Format = td.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS ? DXGI_FORMAT_R8G8B8A8_UNORM : td.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    srvd.Texture2D.MostDetailedMip = 0;
    HRESULT srv_hr = ID3D11Device_CreateShaderResourceView(probe_device,
        (ID3D11Resource*)replacement_cache, &srvd, &replacement_srv);
    if (FAILED(srv_hr) || !replacement_srv) goto fail;
    if (generation != InterlockedCompareExchange(&desktop_mirror_generation, 0, 0) ||
        handle != (HANDLE)InterlockedCompareExchangePointer(
            (volatile PVOID*)&g_shared_handle, NULL, NULL)) goto fail;
    if (g_panel_srv) ID3D11ShaderResourceView_Release(g_panel_srv);
    if (g_panel_tex) ID3D11Texture2D_Release(g_panel_tex);
    if (g_shared_km_probe) IDXGIKeyedMutex_Release(g_shared_km_probe);
    if (g_local_shared_tex) ID3D11Texture2D_Release(g_local_shared_tex);
    g_local_shared_tex = replacement_local;
    g_shared_km_probe = replacement_km;
    g_panel_tex = replacement_cache;
    g_panel_srv = replacement_srv;
    xr_desktop_mirror_generation = generation;
    xr_desktop_mirror_handle = handle;
    g_panel_width = td.Width;
    g_panel_height = td.Height;
    telemetry_snprintf(detail, sizeof(detail),
        "\"status\":\"observed\",\"generation\":%ld,\"cross_device_open\":true,"
        "\"width\":%u,\"height\":%u,\"format\":%u,"
        "\"local_cache_created\":%d,\"srv_created\":%d",
        (long)generation, td.Width, td.Height, (unsigned)td.Format, SUCCEEDED(cache_hr) ? 1 : 0,
        SUCCEEDED(srv_hr) ? 1 : 0);
    write_record("CAP-009", "desktop_mirror_generation_opened_on_probe", detail);
    return TRUE;
fail:
    if (replacement_srv) ID3D11ShaderResourceView_Release(replacement_srv);
    if (replacement_cache) ID3D11Texture2D_Release(replacement_cache);
    if (replacement_km) IDXGIKeyedMutex_Release(replacement_km);
    if (replacement_local) ID3D11Texture2D_Release(replacement_local);
    return FALSE;
}

static BOOL open_eye_pair_on_probe_device(void) {
    if (g_local_eye_tex[0] && g_local_eye_tex[1] && g_local_eye_km[0] && g_local_eye_km[1])
        return TRUE;
    if (!shared_eye_handle[0] || !shared_eye_handle[1]) return FALSE;
    for (int eye = 0; eye < 2; ++eye) {
        HRESULT hr = ID3D11Device_OpenSharedResource(probe_device, shared_eye_handle[eye],
            &iid_d3d11_texture2d, (void**)&g_local_eye_tex[eye]);
        if (FAILED(hr) || !g_local_eye_tex[eye]) goto fail;
        hr = ID3D11Texture2D_QueryInterface(g_local_eye_tex[eye], &iid_dxgi_keyed_mutex,
            (void**)&g_local_eye_km[eye]);
        if (FAILED(hr) || !g_local_eye_km[eye]) goto fail;
        D3D11_TEXTURE2D_DESC td;
        ID3D11Texture2D_GetDesc(g_local_eye_tex[eye], &td);
        if (td.Width != g_eye_width || td.Height != g_eye_height || td.ArraySize != 1 ||
            typed_format(td.Format) != DXGI_FORMAT_R8G8B8A8_UNORM) goto fail;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        td.CPUAccessFlags = 0;
        td.MiscFlags = 0;
        hr = ID3D11Device_CreateTexture2D(probe_device, &td, NULL, &g_eye_cache_tex[eye]);
        if (FAILED(hr) || !g_eye_cache_tex[eye]) goto fail;
        D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
        ZeroMemory(&srvd, sizeof(srvd));
        srvd.Format = typed_format(td.Format);
        srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvd.Texture2D.MipLevels = 1;
        hr = ID3D11Device_CreateShaderResourceView(probe_device,
            (ID3D11Resource*)g_eye_cache_tex[eye], &srvd, &g_eye_cache_srv[eye]);
        if (FAILED(hr) || !g_eye_cache_srv[eye]) goto fail;
    }
    write_record("IMMERSIVE-SOURCE", "shared_eye_pair_opened_on_xr",
        "\"status\":\"observed\",\"isolated_device\":true,\"pair_atomic\":true");
    return TRUE;
fail:
    for (int eye = 0; eye < 2; ++eye) {
        if (g_eye_cache_srv[eye]) { ID3D11ShaderResourceView_Release(g_eye_cache_srv[eye]); g_eye_cache_srv[eye] = NULL; }
        if (g_local_eye_km[eye]) { IDXGIKeyedMutex_Release(g_local_eye_km[eye]); g_local_eye_km[eye] = NULL; }
        if (g_local_eye_tex[eye]) { ID3D11Texture2D_Release(g_local_eye_tex[eye]); g_local_eye_tex[eye] = NULL; }
        if (g_eye_cache_tex[eye]) { ID3D11Texture2D_Release(g_eye_cache_tex[eye]); g_eye_cache_tex[eye] = NULL; }
    }
    write_disabled("open_shared_eye_pair", NULL);
    return FALSE;
}

static BOOL open_flat_source_on_probe_device(void) {
    if (g_local_flat_tex && g_local_flat_km && g_flat_cache_tex && g_flat_cache_srv)
        return TRUE;
    if (!shared_flat_handle) return FALSE;
    HRESULT hr = ID3D11Device_OpenSharedResource(probe_device, shared_flat_handle,
        &iid_d3d11_texture2d, (void**)&g_local_flat_tex);
    if (FAILED(hr) || !g_local_flat_tex) goto fail;
    hr = ID3D11Texture2D_QueryInterface(g_local_flat_tex, &iid_dxgi_keyed_mutex,
        (void**)&g_local_flat_km);
    if (FAILED(hr) || !g_local_flat_km) goto fail;
    D3D11_TEXTURE2D_DESC td;
    ID3D11Texture2D_GetDesc(g_local_flat_tex, &td);
    if (!td.Width || !td.Height || td.ArraySize != 1 || td.SampleDesc.Count != 1)
        goto fail;
    g_flat_width = td.Width;
    g_flat_height = td.Height;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = 0;
    td.MiscFlags = 0;
    hr = ID3D11Device_CreateTexture2D(probe_device, &td, NULL, &g_flat_cache_tex);
    if (FAILED(hr) || !g_flat_cache_tex) goto fail;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
    ZeroMemory(&srvd, sizeof(srvd));
    srvd.Format = typed_format(td.Format);
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    hr = ID3D11Device_CreateShaderResourceView(probe_device,
        (ID3D11Resource*)g_flat_cache_tex, &srvd, &g_flat_cache_srv);
    if (FAILED(hr) || !g_flat_cache_srv) goto fail;
    write_record("CTRL-002", "shared_flat_source_opened_on_xr",
        "\"status\":\"observed\",\"isolated_device\":true");
    return TRUE;
fail:
    if (g_flat_cache_srv) { ID3D11ShaderResourceView_Release(g_flat_cache_srv); g_flat_cache_srv = NULL; }
    if (g_flat_cache_tex) { ID3D11Texture2D_Release(g_flat_cache_tex); g_flat_cache_tex = NULL; }
    if (g_local_flat_km) { IDXGIKeyedMutex_Release(g_local_flat_km); g_local_flat_km = NULL; }
    if (g_local_flat_tex) { ID3D11Texture2D_Release(g_local_flat_tex); g_local_flat_tex = NULL; }
    g_flat_width = 0;
    g_flat_height = 0;
    write_disabled("open_shared_flat_source", NULL);
    return FALSE;
}

static void refresh_flat_cache_for_generation(LONG generation) {
    if (generation <= 0 ||
        InterlockedCompareExchange(&published_flat_generation, 0, 0) != generation ||
        !open_flat_source_on_probe_device()) return;
    if (SUCCEEDED(IDXGIKeyedMutex_AcquireSync(g_local_flat_km, 1, 0))) {
        ID3D11DeviceContext_CopyResource(probe_context,
            (ID3D11Resource*)g_flat_cache_tex, (ID3D11Resource*)g_local_flat_tex);
        IDXGIKeyedMutex_ReleaseSync(g_local_flat_km, 0);
        xr_flat_cache_generation = generation;
    }
}

#include "base_eye_output.h"
static BOOL draw_cached_eye_pair_to_projection(uint32_t projection_index,LONG generation,BOOL reused);
static BOOL copy_fresh_eye_pair_to_projection_body(uint32_t projection_index) {
    LONG generation = InterlockedCompareExchange(&published_eye_generation, 0, 0);
    LONG64 produced_tick = InterlockedCompareExchange64(&eye_pair_tick, 0, 0);
    LONG64 age_ms = (LONG64)GetTickCount64() - produced_tick;
    if (bp_transport_blocked(&bp_transport) || generation <= 0 || produced_tick <= 0 ||
        projection_index >= g_proj_image_count || !open_eye_pair_on_probe_device()) {
        if (xr_active_source_generation) {
            write_record("IMMERSIVE-SOURCE", "presentation_fallback",
                "\"status\":\"panel\",\"reason\":\"source_invalid_or_stale\"");
            xr_active_source_generation = 0;
        }
        return FALSE;
    }
    HRESULT acquire_result[3];
    LONG receipt_owner=game_pair_telemetry.owned?BP_TRANSPORT_OWN:BP_TRANSPORT_LIVE;
    LONG64 receipt_serial=game_pair_telemetry.pair_serial;
    if(bp_transport_pending(&bp_transport) &&
        !bp_transport_matches(&bp_transport,receipt_owner,generation,receipt_serial)) return FALSE;
    int acquired=bp_transport_pending(&bp_transport)?
        bp_keyed_pair_acquire(g_local_eye_km[0],g_local_eye_km[1],1,acquire_result):BP_KM_PENDING;
    if(acquired==BP_KM_BROKEN) {
        bp_transport_quarantine(&bp_transport);
        if(generation==InterlockedCompareExchange(&bp_published_generation,0,0)) bp_consumer_stop(generation);
        return FALSE;
    }
    if (acquired==BP_KM_READY) {
        for (UINT eye = 0; eye < 2; ++eye)
            ID3D11DeviceContext_CopyResource(probe_context,
                (ID3D11Resource*)g_eye_cache_tex[eye], (ID3D11Resource*)g_local_eye_tex[eye]);
        /* The producer cannot replace either texture while both mutexes are held.
           Attribute metadata to this copy, never to a later published pair. */
        xr_cache_telemetry = game_pair_telemetry;
        home_cached_views=game_pair_telemetry.owned?home_pair_views:(HomeRenderViews){0};
        HRESULT owned_right_release=IDXGIKeyedMutex_ReleaseSync(g_local_eye_km[1], 0);
        HRESULT owned_left_release=IDXGIKeyedMutex_ReleaseSync(g_local_eye_km[0], 0);
        if(owned_left_release!=S_OK || owned_right_release!=S_OK ||
            !bp_transport_ack(&bp_transport,receipt_owner,generation,receipt_serial)) {
            bp_transport_quarantine(&bp_transport);
            if(xr_cache_telemetry.owned) bp_consumer_stop(generation);
            return FALSE;
        }
        xr_live_cache_produced_tick = xr_cache_telemetry.owned ? 0 : (ULONGLONG)produced_tick;
        xr_eye_cache_generation = generation;
    } else {
        if (xr_eye_cache_generation != generation) return FALSE;
    }
    /* A stale published pair still owns key 1. Return/ack it before rejecting
       presentation, or the producer can never publish a fresh replacement.
       Recheck time after GPU transfer; this does not extend display freshness. */
    age_ms = (LONG64)GetTickCount64() - produced_tick;
    if(age_ms < 0 || age_ms > 500) return FALSE;
    if(xr_cache_telemetry.owned && (xr_cache_telemetry.generation!=generation ||
        InterlockedCompareExchange(&bp_published_generation,0,0)!=generation ||
        InterlockedCompareExchange(&published_eye_generation,0,0)!=generation)) return FALSE;
    if(xr_cache_telemetry.owned) {
        BpIdentity cached={(uint64_t)xr_cache_telemetry.generation,xr_cache_telemetry.source,
            xr_cache_telemetry.pose,xr_cache_telemetry.config,xr_cache_telemetry.capture_tick,
            xr_cache_telemetry.scene,0};
        if(!bp_consumer_current(&bp.c,&cached)) return FALSE;
        LONG64 cached_age=(LONG64)GetTickCount64()-(LONG64)xr_cache_telemetry.capture_tick;
        if(!xr_cache_telemetry.capture_tick || cached_age<0 || cached_age>500) return FALSE;
    }
    refresh_flat_cache_for_generation(generation);
    return draw_cached_eye_pair_to_projection(projection_index,generation,acquired!=BP_KM_READY);
}

/* Pure XR-owned cache draw. Lock-busy reuse never opens/reads a shared image,
   takes its mutex, refreshes a flat source, or reads mutable bp.c. */
static BOOL draw_cached_eye_pair_to_projection(uint32_t projection_index,LONG generation,BOOL reused) {
    BOOL owned=xr_cache_telemetry.owned;
    ID3D11PixelShader* eye_ps=bp_eye_output_shader(owned!=FALSE,g_ps,g_panel_ps);
    if(generation<=0 || xr_eye_cache_generation!=generation || xr_cache_telemetry.generation!=generation ||
        projection_index>=g_proj_image_count || !g_eye_cache_tex[0] || !g_eye_cache_tex[1] ||
        !g_eye_cache_srv[0] || !g_eye_cache_srv[1] || !g_proj_rtvs[projection_index][0] || !g_proj_rtvs[projection_index][1] ||
        !g_eye_copy_vs || !eye_ps || !g_sampler) return FALSE;
    BpLeaseSnapshot lease={0};BpIdentity cached={0};
    if(owned) {
        cached=(BpIdentity){(uint64_t)xr_cache_telemetry.generation,xr_cache_telemetry.source,
            xr_cache_telemetry.pose,xr_cache_telemetry.config,xr_cache_telemetry.capture_tick,xr_cache_telemetry.scene,0};
        if(!bp_lease_snapshot(&bp_consumer_lease,&lease)) return FALSE;
    }
    D3D11_VIEWPORT vp = {0, 0, (FLOAT)g_eye_width, (FLOAT)g_eye_height, 0, 1};
    ID3D11DeviceContext_RSSetViewports(probe_context, 1, &vp);
    ID3D11DeviceContext_IASetInputLayout(probe_context, NULL);
    ID3D11DeviceContext_IASetPrimitiveTopology(probe_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(probe_context, g_eye_copy_vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(probe_context, eye_ps, NULL, 0);
    ID3D11DeviceContext_PSSetSamplers(probe_context, 0, 1, &g_sampler);
    for (UINT eye = 0; eye < 2; ++eye) {
        ID3D11DeviceContext_OMSetRenderTargets(probe_context, 1,
            &g_proj_rtvs[projection_index][eye], NULL);
        ID3D11DeviceContext_PSSetShaderResources(probe_context, 0, 1, &g_eye_cache_srv[eye]);
        /* Final authorization includes active+both published generations and
           reads the sequence LAST. It authorizes this whole stereo draw. */
        if(eye==0 && owned && !bp_lease_authorize(&bp_consumer_lease,&lease,&cached,
            xr_cache_telemetry.capture_tick,GetTickCount64(),&published_eye_generation,&bp_published_generation)) return FALSE;
        if (eye == 0 && !owned) {
            ULONGLONG now = GetTickCount64();
            if (!xr_live_cache_produced_tick || now < xr_live_cache_produced_tick ||
                now - xr_live_cache_produced_tick > 500 || bp_transport_blocked(&bp_transport) ||
                InterlockedCompareExchange(&live_retired_through,0,0) >= generation ||
                InterlockedCompareExchange(&bp_published_generation,0,0) == generation ||
                InterlockedCompareExchange(&source_generation,0,0) != generation ||
                InterlockedCompareExchange(&published_eye_generation,0,0) != generation) return FALSE;
        }
        ID3D11DeviceContext_Draw(probe_context, 3, 0);
    }
    ID3D11ShaderResourceView* null_srv = NULL;
    ID3D11DeviceContext_PSSetShaderResources(probe_context, 0, 1, &null_srv);
    ++immersive_frames_total;
    stereo_observe_draw(&xr_frame_telemetry, &xr_cache_telemetry, projection_index, reused);
    if (xr_active_source_generation != generation) {
        xr_active_source_generation = generation;
        char detail[768];
        stereo_telemetry_detail(&xr_frame_telemetry, 0, detail, sizeof(detail));
        write_record("IMMERSIVE-SOURCE", "stereo_projection_draw_issued", detail);
        if(xr_cache_telemetry.owned) {
            char owned_detail[640];
            telemetry_snprintf(owned_detail,sizeof(owned_detail),
                "\"status\":\"draw_issued_not_acceptance\",\"generation\":%ld,\"pair_serial\":%lld,"
                "\"source\":%llu,\"scene\":%d,\"pose_epoch\":%llu,\"config_epoch\":%llu,"
                "\"capture_tick\":%llu,\"xr_frame_id\":%llu,\"source_pose_follow\":true,\"dynamic_6dof\":false",
                (long)xr_cache_telemetry.generation,(long long)xr_cache_telemetry.pair_serial,
                (unsigned long long)xr_cache_telemetry.source,xr_cache_telemetry.scene,
                (unsigned long long)xr_cache_telemetry.pose,(unsigned long long)xr_cache_telemetry.config,
                (unsigned long long)xr_cache_telemetry.capture_tick,xr_frame_telemetry.frame_id);
            write_record("OWN-BASE","base_pair_consumed",owned_detail);
        }
    }
    return TRUE;
}

static BOOL copy_fresh_eye_pair_to_projection(uint32_t projection_index) {
    /* Owned producer/retirement and XR CPU consumption share one interval.
       Never wait for a managed configuration or GPU query on the XR worker. */
    if(!TryAcquireSRWLockShared(&bp_lock)) {
        BOOL drawn=draw_cached_eye_pair_to_projection(projection_index,xr_eye_cache_generation,TRUE);
        static LONG reported_live_generation;
        if(drawn && !xr_cache_telemetry.owned && reported_live_generation!=xr_eye_cache_generation) {
            reported_live_generation=xr_eye_cache_generation;
            char detail[128];
            telemetry_snprintf(detail,sizeof detail,"\"stage\":\"live_cache_busy_reuse\",\"generation\":%ld",xr_eye_cache_generation);
            write_record("IMMERSIVE-SOURCE","live_cache_busy_reuse",detail);
        }
        return drawn;
    }
    LONG published=InterlockedCompareExchange(&published_eye_generation,0,0);
    BOOL owned=published>0 && published==InterlockedCompareExchange(&bp_published_generation,0,0);
    BOOL valid=!owned || (!bp.c.retiring && bp.c.phase==BP_ACTIVE &&
        published==(LONG)bp.c.identity.generation);
    BOOL result=valid?copy_fresh_eye_pair_to_projection_body(projection_index):FALSE;
    ReleaseSRWLockShared(&bp_lock);
    return result;
}

/* LH off-center perspective, D3D-style [0,1] depth; angles in radians */
static void build_projection(const XrFovf* fov, float znear, float zfar, float* m /*16, col-major*/) {
    float tl = tanf(fov->angleLeft), tr = tanf(fov->angleRight);
    float td = tanf(fov->angleDown), tu = tanf(fov->angleUp);
    ZeroMemory(m, sizeof(float) * 16);
    m[0] = 2.0f / (tr - tl);
    m[5] = 2.0f / (tu - td);
    /* mul(vp, position) uses a column vector. With the LH +Z view below,
       asymmetric-FOV shifts therefore live in row X/Y, column Z. */
    m[8] = -(tr + tl) / (tr - tl);
    m[9] = -(tu + td) / (tu - td);
    m[10] = zfar / (zfar - znear);
    m[11] = 1.0f;
    m[14] = -znear * zfar / (zfar - znear);
}

/* view = R^T with Z flipped to left-handed (+Z forward) for the LH projection */
static void build_view(const XrPosef* pose, float* m /*16, col-major*/) {
    float qx = pose->orientation.x, qy = pose->orientation.y,
          qz = pose->orientation.z, qw = pose->orientation.w;
    float r00 = 1 - 2*(qy*qy + qz*qz), r01 = 2*(qx*qy - qz*qw),   r02 = 2*(qx*qz + qy*qw);
    float r10 = 2*(qx*qy + qz*qw),   r11 = 1 - 2*(qx*qx + qz*qz), r12 = 2*(qy*qz - qx*qw);
    float r20 = 2*(qx*qz - qy*qw),   r21 = 2*(qy*qz + qx*qw),     r22 = 1 - 2*(qx*qx + qy*qy);
    m[0] = r00; m[1] = r01; m[2]  = r02;  m[3]  = 0;
    m[4] = r10; m[5] = r11; m[6]  = r12;  m[7]  = 0;
    m[8] = -r20; m[9] = -r21; m[10] = -r22; m[11] = 0;
    float px = pose->position.x, py = pose->position.y, pz = pose->position.z;
    m[12] = -(r00*px + r10*py + r20*pz);
    m[13] = -(r01*px + r11*py + r21*pz);
    m[14] = (r20*px + r21*py + r22*pz);
    m[15] = 1.0f;
}

static void mat_mul(float* out, const float* a, const float* b /* out = a*b, col-major */) {
    float r[16];
    for (int c = 0; c < 4; ++c)
        for (int ro = 0; ro < 4; ++ro) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a[k*4 + ro] * b[c*4 + k];
            r[c*4 + ro] = s;
        }
    memcpy(out, r, sizeof(r));
}

/* Build one centered, aspect-correct panel in VIEW space. The safe
   fraction keeps the same geometry inside both eyes' asymmetric FOVs. */
static BOOL update_panel_vertices(const XrView* views, uint32_t view_count,
        UINT source_width, UINT source_height, float* out_width, float* out_height) {
    if (!views || !view_count || !source_width || !source_height || !g_vb || !probe_context)
        return FALSE;
    const float distance = PANEL_DISTANCE_M;
    const float safe_fraction = PANEL_SAFE_FRACTION;
    float half_tan_x = 1000.0f, half_tan_y = 1000.0f;
    for (uint32_t eye = 0; eye < view_count && eye < MAX_EYES; ++eye) {
        float left = -tanf(views[eye].fov.angleLeft);
        float right = tanf(views[eye].fov.angleRight);
        float down = -tanf(views[eye].fov.angleDown);
        float up = tanf(views[eye].fov.angleUp);
        float hx = left < right ? left : right;
        float hy = down < up ? down : up;
        if (hx < half_tan_x) half_tan_x = hx;
        if (hy < half_tan_y) half_tan_y = hy;
    }
    if (!(half_tan_x > 0.0f) || !(half_tan_y > 0.0f)) return FALSE;
    float aspect = (float)source_width / (float)source_height;
    float hh = distance * safe_fraction * half_tan_y;
    float hw = hh * aspect;
    float max_hw = distance * safe_fraction * half_tan_x;
    if (hw > max_hw) {
        float scale = max_hw / hw;
        hw *= scale;
        hh *= scale;
    }
    PanelVertex verts[4] = {
        {-hw,  hh, -distance, 0, 0}, { hw,  hh, -distance, 1, 0},
        {-hw, -hh, -distance, 0, 1}, { hw, -hh, -distance, 1, 1},
    };
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(ID3D11DeviceContext_Map(probe_context, (ID3D11Resource*)g_vb,
            0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return FALSE;
    memcpy(mapped.pData, verts, sizeof(verts));
    ID3D11DeviceContext_Unmap(probe_context, (ID3D11Resource*)g_vb, 0);
    if (out_width) *out_width = hw * 2.0f;
    if (out_height) *out_height = hh * 2.0f;
    return TRUE;
}

static XrVector3f rotate_vector(const XrQuaternionf* q, XrVector3f v);

static XrVector3f transform_panel_point(const XrPosef* pose, XrVector3f local) {
    XrVector3f rotated = rotate_vector(&pose->orientation, local);
    rotated.x += pose->position.x;
    rotated.y += pose->position.y;
    rotated.z += pose->position.z;
    return rotated;
}

static BOOL update_aux_panel_vertices(const XrPosef* pose, UINT source_width,
        UINT source_height, float* out_width, float* out_height) {
    if (!pose || !source_width || !source_height || !g_vb || !probe_context)
        return FALSE;
    float height = AUX_PANEL_HEIGHT_M;
    float width = height * (float)source_width / (float)source_height;
    float hw = width * 0.5f, hh = height * 0.5f;
    XrVector3f corners[4] = {
        {-hw, hh, 0.0f}, {hw, hh, 0.0f}, {-hw, -hh, 0.0f}, {hw, -hh, 0.0f}
    };
    PanelVertex verts[4];
    for (int i = 0; i < 4; ++i) {
        XrVector3f world = transform_panel_point(pose, corners[i]);
        verts[i].x = world.x; verts[i].y = world.y; verts[i].z = world.z;
        verts[i].u = (i & 1) ? 1.0f : 0.0f;
        verts[i].v = (i & 2) ? 1.0f : 0.0f;
    }
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(ID3D11DeviceContext_Map(probe_context, (ID3D11Resource*)g_vb,
            0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return FALSE;
    memcpy(mapped.pData, verts, sizeof(verts));
    ID3D11DeviceContext_Unmap(probe_context, (ID3D11Resource*)g_vb, 0);
    if (out_width) *out_width = width;
    if (out_height) *out_height = height;
    return TRUE;
}

static BOOL build_cursor_vertices(const XrPosef* panel_pose, float panel_width,
        float panel_height, float u, float v, PanelVertex* verts) {
    if (!panel_pose || !verts || !(panel_width > 0.0f) || !(panel_height > 0.0f) ||
        !isfinite(u) || !isfinite(v) || u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
        return FALSE;
    float half_size = fminf(panel_width, panel_height) * CURSOR_HALF_SIZE_FRACTION;
    if (!(half_size > 0.0f) || !isfinite(half_size)) return FALSE;
    float cx = (u - 0.5f) * panel_width;
    float cy = (0.5f - v) * panel_height;
    XrVector3f corners[4] = {
        {cx - half_size, cy + half_size, CURSOR_SURFACE_OFFSET_M},
        {cx + half_size, cy + half_size, CURSOR_SURFACE_OFFSET_M},
        {cx - half_size, cy - half_size, CURSOR_SURFACE_OFFSET_M},
        {cx + half_size, cy - half_size, CURSOR_SURFACE_OFFSET_M}
    };
    for (int i = 0; i < 4; ++i) {
        XrVector3f world = transform_panel_point(panel_pose, corners[i]);
        verts[i].x = world.x; verts[i].y = world.y; verts[i].z = world.z;
        verts[i].u = (i & 1) ? 1.0f : 0.0f;
        verts[i].v = (i & 2) ? 1.0f : 0.0f;
    }
    return TRUE;
}

static BOOL update_cursor_vertices(const XrPosef* panel_pose, float panel_width,
        float panel_height, float u, float v) {
    if (!g_vb || !probe_context) return FALSE;
    PanelVertex verts[4];
    if (!build_cursor_vertices(panel_pose, panel_width, panel_height, u, v, verts))
        return FALSE;
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(ID3D11DeviceContext_Map(probe_context, (ID3D11Resource*)g_vb,
            0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return FALSE;
    memcpy(mapped.pData, verts, sizeof(verts));
    ID3D11DeviceContext_Unmap(probe_context, (ID3D11Resource*)g_vb, 0);
    return TRUE;
}

static XrVector3f rotate_vector(const XrQuaternionf* q, XrVector3f v) {
    XrVector3f t = {
        2.0f * (q->y * v.z - q->z * v.y),
        2.0f * (q->z * v.x - q->x * v.z),
        2.0f * (q->x * v.y - q->y * v.x)
    };
    XrVector3f r = {
        v.x + q->w * t.x + (q->y * t.z - q->z * t.y),
        v.y + q->w * t.y + (q->z * t.x - q->x * t.z),
        v.z + q->w * t.z + (q->x * t.y - q->y * t.x)
    };
    return r;
}

static BOOL panel_ray_hit(const XrPosef* aim, float panel_width, float panel_height,
        float* out_u, float* out_v) {
    if (!aim || !(panel_width > 0.0f) || !(panel_height > 0.0f)) return FALSE;
    XrVector3f forward = {0.0f, 0.0f, -1.0f};
    XrVector3f direction = rotate_vector(&aim->orientation, forward);
    if (!isfinite(direction.x) || !isfinite(direction.y) || !isfinite(direction.z) ||
        direction.z >= -0.0001f) return FALSE;
    float t = (-PANEL_DISTANCE_M - aim->position.z) / direction.z;
    if (!(t > 0.0f) || !isfinite(t)) return FALSE;
    float x = aim->position.x + direction.x * t;
    float y = aim->position.y + direction.y * t;
    float half_width = panel_width * 0.5f;
    float half_height = panel_height * 0.5f;
    if (x < -half_width || x > half_width || y < -half_height || y > half_height)
        return FALSE;
    if (out_u) *out_u = (x + half_width) / panel_width;
    if (out_v) *out_v = (half_height - y) / panel_height;
    return TRUE;
}

static BOOL panel_ray_hit_pose(const XrPosef* aim, const XrPosef* panel,
        float panel_width, float panel_height, float* out_u, float* out_v) {
    if (!aim || !panel) return FALSE;
    XrQuaternionf inverse = {
        -panel->orientation.x, -panel->orientation.y, -panel->orientation.z,
        panel->orientation.w
    };
    XrVector3f relative = {
        aim->position.x - panel->position.x,
        aim->position.y - panel->position.y,
        aim->position.z - panel->position.z
    };
    XrVector3f local_origin = rotate_vector(&inverse, relative);
    XrVector3f forward = {0.0f, 0.0f, -1.0f};
    XrVector3f world_direction = rotate_vector(&aim->orientation, forward);
    XrVector3f local_direction = rotate_vector(&inverse, world_direction);
    if (!isfinite(local_direction.z) || fabsf(local_direction.z) < 0.0001f)
        return FALSE;
    float t = -local_origin.z / local_direction.z;
    if (!(t > 0.0f) || !isfinite(t)) return FALSE;
    float x = local_origin.x + local_direction.x * t;
    float y = local_origin.y + local_direction.y * t;
    float half_width = panel_width * 0.5f, half_height = panel_height * 0.5f;
    if (x < -half_width || x > half_width || y < -half_height || y > half_height)
        return FALSE;
    if (out_u) *out_u = (x + half_width) / panel_width;
    if (out_v) *out_v = (half_height - y) / panel_height;
    return TRUE;
}

static BOOL game_window_is_foreground(void) {
    return g_game_window && IsWindow(g_game_window) &&
        GetForegroundWindow() == g_game_window;
}

static BOOL move_pointer_to_panel(float u, float v) {
    if (!game_window_is_foreground() || !isfinite(u) || !isfinite(v) ||
        u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return FALSE;
    RECT client;
    if (!GetClientRect(g_game_window, &client)) return FALSE;
    LONG width = client.right - client.left, height = client.bottom - client.top;
    if (width <= 1 || height <= 1) return FALSE;
    POINT point = {
        client.left + (LONG)lroundf(u * (float)(width - 1)),
        client.top + (LONG)lroundf(v * (float)(height - 1))
    };
    if (!ClientToScreen(g_game_window, &point)) return FALSE;
    int virtual_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int virtual_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int virtual_w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int virtual_h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (virtual_w <= 1 || virtual_h <= 1) return FALSE;
    INPUT input; ZeroMemory(&input, sizeof(input));
    input.type = INPUT_MOUSE;
    input.mi.dx = (LONG)(((int64_t)(point.x - virtual_x) * 65535) / (virtual_w - 1));
    input.mi.dy = (LONG)(((int64_t)(point.y - virtual_y) * 65535) / (virtual_h - 1));
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    return SendInput(1, &input, sizeof(input)) == 1;
}

static BOOL read_boolean_action_for_hand(XrAction action, XrPath hand, BOOL* active, BOOL* current) {
    XrActionStateGetInfo get; ZeroMemory(&get, sizeof(get));
    get.type = XR_TYPE_ACTION_STATE_GET_INFO;
    get.action = action;
    get.subactionPath = hand;
    XrActionStateBoolean state; ZeroMemory(&state, sizeof(state));
    state.type = XR_TYPE_ACTION_STATE_BOOLEAN;
    if (XR_FAILED(pxrGetActionStateBoolean(g_session, &get, &state))) return FALSE;
    if (active) *active = state.isActive;
    if (current) *current = state.currentState;
    return TRUE;
}

static BOOL read_boolean_action(XrAction action, BOOL* active, BOOL* current) {
    return read_boolean_action_for_hand(action,g_pointer_hand_path,active,current);
}

static BOOL camera_click_edge(XrAction action, XrPath hand, BOOL* armed) {
    BOOL active = FALSE, down = FALSE;
    if (!action || !read_boolean_action_for_hand(action,hand,&active,&down) || !active) {
        *armed = FALSE;
        return FALSE;
    }
    BOOL pressed = down && *armed;
    *armed = !down;
    return pressed;
}

static BOOL read_float_action_for_path(XrAction action, XrPath subaction_path,
        BOOL* active, float* current) {
    XrActionStateGetInfo get; ZeroMemory(&get, sizeof(get));
    get.type = XR_TYPE_ACTION_STATE_GET_INFO;
    get.action = action;
    get.subactionPath = subaction_path;
    XrActionStateFloat state; ZeroMemory(&state, sizeof(state));
    state.type = XR_TYPE_ACTION_STATE_FLOAT;
    if (XR_FAILED(pxrGetActionStateFloat(g_session, &get, &state))) return FALSE;
    if (active) *active = state.isActive;
    if (current) *current = state.currentState;
    return TRUE;
}

static BOOL read_float_action(XrAction action, BOOL* active, float* current) {
    return read_float_action_for_path(action, g_pointer_hand_path, active, current);
}

static BOOL read_vector2_action_for_path(XrAction action, XrPath subaction_path,
        BOOL* active, XrVector2f* current) {
    XrActionStateGetInfo get; ZeroMemory(&get, sizeof(get));
    get.type = XR_TYPE_ACTION_STATE_GET_INFO;
    get.action = action;
    get.subactionPath = subaction_path;
    XrActionStateVector2f state; ZeroMemory(&state, sizeof(state));
    state.type = XR_TYPE_ACTION_STATE_VECTOR2F;
    if (XR_FAILED(pxrGetActionStateVector2f(g_session, &get, &state))) return FALSE;
    if (active) *active = state.isActive;
    if (current) *current = state.currentState;
    return TRUE;
}

/* Positions are metres in the same LOCAL space as physical head translation. */
static Vec3 world_grab_delta(WorldGrab* grab, LONG generation, XrTime time,
        BOOL tracked, float squeeze, XrVector3f position) {
    Vec3 delta = {0};
    if (!tracked || generation <= 0 || !isfinite(squeeze) || !isfinite(position.x) ||
        !isfinite(position.y) || !isfinite(position.z)) {
        *grab = (WorldGrab){0};
        return delta;
    }
    if (generation != grab->generation || time <= grab->time || time-grab->time > 250000000)
        *grab = (WorldGrab){0};
    if (squeeze <= 0.25f) {
        grab->armed = TRUE;
        grab->holding = FALSE;
    } else if (grab->holding) {
        delta = (Vec3){grab->position.x-position.x, grab->position.y-position.y,
                       position.z-grab->position.z};
    } else if (grab->armed && squeeze >= 0.72f) {
        grab->holding = TRUE;
        grab->armed = FALSE;
    }
    grab->position = position;
    grab->generation = generation;
    grab->time = time;
    return delta;
}

static void sample_contact(unsigned side,BOOL eligible,XrVector3f pos,XrAction haptic,XrPath path) {
    if(!contact_consent || !contact_enabled)return;
    if(!TryAcquireSRWLockExclusive(&contact_lock))return;
    HandContact* slot=&contact[side];
    ULONGLONG now=GetTickCount64();
    LONG gen=eligible && !g_aux_panel_visible?xr_frame_telemetry.generation:0;
    BOOL pulse=gen && slot->pulse_gen==gen && now-slot->pulse_tick<=160;
    slot->hand=pos;slot->gen=gen;slot->tick=now;slot->pulse_gen=0;
    if(!gen)slot->surface_gen=0;
    ReleaseSRWLockExclusive(&contact_lock);
    if(pulse && contact_haptics && haptic_strength>0 && haptic_duration>0 && haptic && pxrApplyHapticFeedback){
        XrHapticActionInfo info={.type=XR_TYPE_HAPTIC_ACTION_INFO,.action=haptic,.subactionPath=path};
        XrHapticVibration vibration={.type=XR_TYPE_HAPTIC_VIBRATION,.duration=(XrDuration)(haptic_duration*1000000.0),.amplitude=haptic_strength/100.f,.frequency=XR_FREQUENCY_UNSPECIFIED};
        (void)pxrApplyHapticFeedback(g_session,&info,(const XrHapticBaseHeader*)&vibration);
    }
}
static Vec3 read_world_grab(XrTime time) {
    BOOL active = FALSE;
    float squeeze = 0;
    XrSpaceLocation location = {.type=XR_TYPE_SPACE_LOCATION};
    XrSpaceLocationFlags required = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
    BOOL tracked = g_world_grip_space && g_tracking_space &&
        XR_SUCCEEDED(pxrLocateSpace(g_world_grip_space, g_tracking_space, time, &location)) &&
        (location.locationFlags & required) == required;
    BOOL squeeze_ready=read_float_action_for_path(g_world_squeeze,g_pointer_hand_path,&active,&squeeze) && active;
    float activation=squeeze;
    BOOL can_grab=tracked && squeeze_ready;
    if(grab_with_trigger){
        BOOL trigger_active=FALSE;
        can_grab=tracked && read_float_action_for_path(g_trigger_action,g_pointer_hand_path,&trigger_active,&activation) && trigger_active;
    }
    sample_contact(0,tracked && squeeze<.25f && (!grab_with_trigger || activation<.25f),location.pose.position,g_contact_haptic,g_pointer_hand_path);
    XrSpaceLocation panel={.type=XR_TYPE_SPACE_LOCATION};
    float panel_grip=0;
    (void)read_float_action_for_path(g_panel_grip_value_action,g_panel_hand_path,&active,&panel_grip);
    BOOL panel_tracked=g_panel_grip_space && g_tracking_space &&
        panel_grip<.25f && XR_SUCCEEDED(pxrLocateSpace(g_panel_grip_space,g_tracking_space,time,&panel)) &&
        (panel.locationFlags&required)==required;
    sample_contact(1,panel_tracked,panel.pose.position,panel_haptic,g_panel_hand_path);
    BOOL valid=can_grab && !g_aux_panel_visible && (grab_panel_hand?panel_tracked:tracked);
    return world_grab_delta(&g_world_grab, xr_frame_telemetry.generation, time,
                            valid, activation, grab_panel_hand?panel.pose.position:location.pose.position);
}

static void update_controller_navigation(XrTime display_time,
        BOOL immersive_active, BOOL actions_synced, BOOL fresh_pose) {
    if (!fresh_pose || !immersive_active || !actions_synced || !g_controller_actions_ready ||
            g_session_state != (int)XR_SESSION_STATE_FOCUSED) {
        if (TryAcquireSRWLockExclusive(&contact_lock)) {
            ZeroMemory(contact,sizeof(contact));
            ReleaseSRWLockExclusive(&contact_lock);
        }
        g_click_generation = 0;
        g_return_button = (ReturnButton){0};
        g_world_grab = (WorldGrab){0};
        g_navigation_last_time = 0;
        g_navigation_turn_x_latched = FALSE;
        g_navigation_turn_y_latched = FALSE;
        return;
    }
    BOOL move_active = FALSE, turn_active = FALSE;
    XrVector2f move = {0.0f, 0.0f}, turn = {0.0f, 0.0f};
    BOOL left_moves=controller_hands_swapped != swap_move_sticks;
    XrAction move_action = left_moves ?
        g_left_stick_action : g_right_stick_action;
    XrPath move_path = left_moves ?
        g_left_hand_path : g_right_hand_path;
    XrAction turn_action = left_moves ?
        g_right_stick_action : g_left_stick_action;
    XrPath turn_path = left_moves ?
        g_right_hand_path : g_left_hand_path;
    if (!read_vector2_action_for_path(move_action, move_path,
            &move_active, &move) || !move_active)
        move = (XrVector2f){0.0f, 0.0f};
    if (!read_vector2_action_for_path(turn_action, turn_path,
            &turn_active, &turn) || !turn_active)
        turn = (XrVector2f){0.0f, 0.0f};

    float dt_seconds = 0.0f;
    if (g_navigation_last_time > 0 && display_time > g_navigation_last_time)
        dt_seconds = (float)((double)(display_time - g_navigation_last_time) * 0.000000001);
    g_navigation_last_time = display_time;
    LONG generation = xr_frame_telemetry.generation;
    if (generation != g_click_generation) {
        g_return_button = (ReturnButton){0};
        g_click_generation = generation;
    }
    BOOL active = FALSE, down = FALSE;
    BOOL readable = g_return_click && read_boolean_action_for_hand(g_return_click,g_left_hand_path,&active,&down);
    CameraGesture gesture = camera_return_gesture(&g_return_button, readable && active, down, display_time);
    BOOL reset = gesture == CAMERA_RETURN;
    Vec3 grab = read_world_grab(display_time);
    AcquireSRWLockExclusive(&eye_optics_lock);
    if (eye_optics.origin_valid && (grab.x || grab.y || grab.z)) {
        grab = quat_rotate(tracking_to_navigation(&eye_optics), grab);
        Vec3 p = eye_optics.artificial_position;
        p = (Vec3){p.x + grab.x, p.y + grab.y, p.z + grab.z};
        if (isfinite(p.x) && isfinite(p.y) && isfinite(p.z)) eye_optics.artificial_position = p;
    }
    uint32_t changed = apply_navigation_input(&eye_optics, move, turn, dt_seconds,
        &g_navigation_turn_x_latched, &g_navigation_turn_y_latched);
    if (gesture == CAMERA_RECENTER && generation > 0 && commit_immersive_environment_origin(&eye_optics)) {
        eye_optics.artificial_position = (Vec3){0};
        eye_optics.artificial_yaw = eye_optics.artificial_pitch = 0;
        eye_optics.return_orientation=eye_optics.current_center.orientation;
        eye_optics.return_orientation_valid=TRUE;
        immersive_entry_origin_committed = TRUE;
        g_world_grab = (WorldGrab){0};
    }
    if (reset && generation > 0 && eye_optics.origin_valid && InterlockedCompareExchange(&eye_optics_ready,0,0)) {
        eye_optics.return_orientation=eye_optics.current_center.orientation;
        eye_optics.return_orientation_valid=TRUE;
        eye_optics.artificial_yaw=eye_optics.artificial_pitch=0;
        Vec3 offset=physical_displacement(&eye_optics);
        eye_optics.artificial_position=(Vec3){-offset.x,-offset.y,-offset.z};
        g_world_grab = (WorldGrab){0};
        if (xr_cache_telemetry.owned) refresh_frozen_camera_anchor();
        else InterlockedExchange(&return_camera_generation,generation);
    }
    ReleaseSRWLockExclusive(&eye_optics_lock);
    if ((changed & NAV_STEP_MOVED) && !g_navigation_move_logged) {
        g_navigation_move_logged = TRUE;
        char detail[384];
        float effective_speed = effective_navigation_speed_mps(nav_locomotion_speed_mps,
            world_scale, nav_locomotion_scale_compensation_enabled);
        telemetry_snprintf(detail, sizeof(detail),
            "\"status\":\"observed\",\"hand\":\"%s\"," 
            "\"basis\":\"final_view_roll_free\",\"configured_speed_mps\":%.3f,"
            "\"effective_speed_mps\":%.3f,\"world_scale_compensation\":%s",
            controller_hands_swapped ? "left" : "right",
            nav_locomotion_speed_mps, effective_speed,
            nav_locomotion_scale_compensation_enabled ? "true" : "false");
        write_record("CTRL-006", "controller_locomotion_applied", detail);
    }
    if ((changed & NAV_STEP_SNAPPED) && !g_navigation_turn_logged) {
        g_navigation_turn_logged = TRUE;
        char detail[256];
        telemetry_snprintf(detail, sizeof(detail),
            "\"status\":\"observed\",\"hand\":\"%s\"," 
            "\"axes\":\"world_yaw_pitch\",\"snap_angle_degrees\":%.1f",
            controller_hands_swapped ? "right" : "left",
            nav_snap_angle_rad * 57.2957795f);
        write_record("CTRL-006", "controller_view_snap_applied", detail);
    }
}

/* Rotate into the eye frame before reflecting its depth into D3D view space. */
static void contact_eye_view(const XrPosef* pose,float* m){
    XrQuaternionf q=pose->orientation;
    Quat inverse=quat_conjugate((Quat){q.x,q.y,q.z,q.w});
    const Vec3 basis[]={{1,0,0},{0,1,0},{0,0,1},
        {-pose->position.x,-pose->position.y,-pose->position.z}};
    for(int i=0;i<4;i++){
        Vec3 v=quat_rotate(inverse,basis[i]);
        m[4*i]=v.x;m[4*i+1]=v.y;m[4*i+2]=-v.z;m[4*i+3]=i==3?1:0;
    }
}
static void draw_hand_sphere(UINT index,const XrView* eyes,XrVector3f point){
    if(!sphere_vs || !sphere_ps || !g_cb)return;
    D3D11_VIEWPORT viewport={0,0,(float)g_eye_width,(float)g_eye_height,0,1};
    ID3D11DeviceContext_RSSetViewports(probe_context,1,&viewport);
    ID3D11DeviceContext_RSSetState(probe_context,g_raster);
    ID3D11DeviceContext_VSSetShader(probe_context,sphere_vs,NULL,0);
    ID3D11DeviceContext_PSSetShader(probe_context,sphere_ps,NULL,0);
    ID3D11DeviceContext_VSSetConstantBuffers(probe_context,0,1,&g_cb);
    for(unsigned e=0;e<2;e++){
        float view[16],proj[16],vp[16],model[16]={0};contact_eye_view(&eyes[e].pose,view);
        build_projection(&eyes[e].fov,.05f,100.f,proj);
        model[0]=model[5]=model[10]=hand_radius_mm/1000.f;model[15]=1;
        for(unsigned j=0;j<3;j++)model[12+j]=view[j]*point.x+view[4+j]*point.y+view[8+j]*point.z+view[12+j];
        mat_mul(vp,proj,model);
        D3D11_MAPPED_SUBRESOURCE mapped;
        if(FAILED(ID3D11DeviceContext_Map(probe_context,(ID3D11Resource*)g_cb,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))continue;
        memcpy(mapped.pData,vp,sizeof(vp));ID3D11DeviceContext_Unmap(probe_context,(ID3D11Resource*)g_cb,0);
        ID3D11DeviceContext_OMSetRenderTargets(probe_context,1,&g_proj_rtvs[index][e],NULL);
        ID3D11DeviceContext_Draw(probe_context,16*12*6,0);
    }
}
static void draw_space_marker(UINT index,const XrView* eyes,XrVector3f point,BOOL surface) {
    for(UINT i=0;i<2;++i){
        float proj[16],view[16],vp[16];
        build_projection(&eyes[i].fov,.05f,100.f,proj);
        contact_eye_view(&eyes[i].pose,view); mat_mul(vp,proj,view);
        float w=vp[3]*point.x+vp[7]*point.y+vp[11]*point.z+vp[15];
        if(!(w>.05f && w<100.f))continue;
        float x=(vp[0]*point.x+vp[4]*point.y+vp[8]*point.z+vp[12])/w;
        float y=(vp[1]*point.x+vp[5]*point.y+vp[9]*point.z+vp[13])/w;
        if(!isfinite(x)||!isfinite(y)||fabsf(x)>1 || fabsf(y)>1)continue;
        float diameter=surface?.035f:.017f;
        float width=fmaxf(3,g_eye_width*.5f*proj[0]*diameter/w);
        float height=fmaxf(3,g_eye_height*.5f*proj[5]*diameter/w);
        D3D11_VIEWPORT viewport={(x+1)*.5f*g_eye_width-width*.5f,
            (1-y)*.5f*g_eye_height-height*.5f,width,height,surface?.75f:0,1};
        ID3D11DeviceContext_RSSetViewports(probe_context,1,&viewport);
        ID3D11DeviceContext_OMSetRenderTargets(probe_context,1,&g_proj_rtvs[index][i],NULL);
        ID3D11DeviceContext_Draw(probe_context,3,0);
    }
}
static void draw_controller_markers(UINT index,XrTime time,const XrView* eyes){
    if(!marker_ps || !g_eye_copy_vs || (!show_hands && !show_contact))return;
    ID3D11DeviceContext_IASetInputLayout(probe_context,NULL);
    ID3D11DeviceContext_IASetPrimitiveTopology(probe_context,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(probe_context,g_eye_copy_vs,NULL,0);
    ID3D11DeviceContext_PSSetShader(probe_context,marker_ps,NULL,0);
    XrSpace hands[]={g_world_grip_space,g_panel_grip_space};
    XrSpaceLocationFlags required=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
    if(show_hands)for(UINT h=0;h<2;++h){
        XrSpaceLocation loc={.type=XR_TYPE_SPACE_LOCATION};
        if(hands[h] && XR_SUCCEEDED(pxrLocateSpace(hands[h],g_space,time,&loc)) &&
            (loc.locationFlags&required)==required)draw_hand_sphere(index,eyes,loc.pose.position);
    }
    ID3D11DeviceContext_VSSetShader(probe_context,g_eye_copy_vs,NULL,0);
    ID3D11DeviceContext_PSSetShader(probe_context,marker_ps,NULL,0);
    if(!contact_consent || !contact_enabled || !show_contact || g_aux_panel_visible || !TryAcquireSRWLockShared(&contact_lock))return;
    ContactTargets visible_targets[2];memcpy(visible_targets,targets,sizeof targets);
    HandContact snapshot[2];memcpy(snapshot,contact,sizeof(snapshot));
    ReleaseSRWLockShared(&contact_lock);
    if(!g_tracking_space)return;
    XrSpaceLocation loc={.type=XR_TYPE_SPACE_LOCATION};
    required=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    if(XR_FAILED(pxrLocateSpace(g_tracking_space,g_space,time,&loc)) || (loc.locationFlags&required)!=required)return;
    int drawn_body=0;
    for(unsigned h=0;h<2;h++){
        ContactTargets* marks=&visible_targets[h];
        if(!marks->body || marks->body==drawn_body || marks->gen!=xr_frame_telemetry.generation ||
           snapshot[h].gen!=marks->gen || GetTickCount64()-marks->tick>240)continue;
        for(int i=0;i<marks->count;i++)
            draw_space_marker(index,eyes,transform_panel_point(&loc.pose,marks->points[i]),FALSE);
        drawn_body=marks->body;
    }
    for(unsigned i=0;i<2;++i){
        const HandContact* hand=&snapshot[i];
        if(hand->gen<=0 || hand->gen!=hand->surface_gen || hand->gen!=xr_frame_telemetry.generation ||
           GetTickCount64()-hand->surface_tick>160)continue;
        for(int k=0;k<CONTACT_MAX_TARGETS;k++)if(hand->surface_mask&(1u<<k)){
            draw_space_marker(index,eyes,transform_panel_point(&loc.pose,hand->surface_local[k]),TRUE);
        }
    }
}

/* Original auxiliary placement: follow grip translation; keep the surface readable as the wrist rotates. */
static BOOL panel_hand_mount(XrTime time,XrPosef* out){
    XrSpaceLocation grip={.type=XR_TYPE_SPACE_LOCATION};
    XrSpaceLocationFlags valid=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    if(!g_panel_grip_space || !g_space || XR_FAILED(pxrLocateSpace(g_panel_grip_space,g_space,time,&grip)) ||
       (grip.locationFlags&valid)!=valid)return FALSE;
    *out=(XrPosef){.orientation.w=1,.position=grip.pose.position};
    out->position.y+=AUX_PANEL_UP_OFFSET_M;
    out->position.z-=AUX_PANEL_FORWARD_OFFSET_M;
    return TRUE;
}
#include "vr_scale_menu.h"

static BOOL sync_controller_actions(void) {
    if (!g_controller_actions_ready ||
        g_session_state != (int)XR_SESSION_STATE_FOCUSED) return FALSE;
    XrActiveActionSet active = {g_controller_action_set, XR_NULL_PATH};
    XrActionsSyncInfo sync; ZeroMemory(&sync, sizeof(sync));
    sync.type = XR_TYPE_ACTIONS_SYNC_INFO;
    sync.countActiveActionSets = 1;
    sync.activeActionSets = &active;
    return XR_SUCCEEDED(pxrSyncActions(g_session, &sync));
}

static BOOL update_aux_panel_state(XrTime display_time, BOOL immersive_active,
        BOOL actions_synced, XrPosef* out_pose) {
    BOOL grip_active = FALSE;
    float grip_value = 0.0f;
    if (!actions_synced || !g_panel_grip_space || !g_space ||
        !read_float_action_for_path(g_panel_grip_value_action, g_panel_hand_path,
            &grip_active, &grip_value)) {
        g_aux_panel_visible = FALSE;
        g_panel_grip_was_down = FALSE;
        panel_short_armed = FALSE;
        release_synthetic_input();
        return FALSE;
    }
    BOOL grip_down = grip_active && grip_value >= 0.55f;
    if (!immersive_active) {
        if (g_aux_panel_visible) release_synthetic_input();
        g_aux_panel_visible = FALSE;
        g_panel_grip_was_down = grip_down;
        panel_short_armed = FALSE;
        return FALSE;
    }
    XrSpaceLocation location; ZeroMemory(&location, sizeof(location));
    location.type = XR_TYPE_SPACE_LOCATION;
    XrSpaceLocationFlags required =
        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
    BOOL pose_valid = XR_SUCCEEDED(pxrLocateSpace(g_panel_grip_space, g_space,
        display_time, &location)) && (location.locationFlags & required) == required;
    if (!pose_valid || !grip_active) panel_short_armed=FALSE;
    if (grip_down && !g_panel_grip_was_down && pose_valid) panel_short_armed=TRUE;
    if (!grip_down && g_panel_grip_was_down && panel_short_armed && pose_valid) {
        panel_short_armed=FALSE;
        g_aux_panel_visible = !g_aux_panel_visible;
        g_pointer_logged = FALSE;
        if (!g_aux_panel_visible) release_synthetic_input();
        char detail[160];
        telemetry_snprintf(detail, sizeof(detail),
            "\"status\":\"%s\",\"panel_hand\":\"%s\"",
            g_aux_panel_visible ? "visible" : "hidden",
            controller_hands_swapped ? "right" : "left");
        write_record("CTRL-002", "aux_panel_toggled", detail);
    }
    g_panel_grip_was_down = grip_down;
    if (!g_aux_panel_visible || !pose_valid || !out_pose) return FALSE;
    return panel_hand_mount(display_time,out_pose);
}

static void update_controller_input(XrTime display_time, BOOL panel_active,
        float panel_width, float panel_height, const XrPosef* panel_pose,
        BOOL actions_synced, BOOL auxiliary_panel) {
    if (!g_controller_actions_ready || !actions_synced || !panel_active || !panel_pose ||
        g_session_state != (int)XR_SESSION_STATE_FOCUSED || !g_space) {
        release_synthetic_input();
        return;
    }

    XrSpaceLocation location; ZeroMemory(&location, sizeof(location));
    location.type = XR_TYPE_SPACE_LOCATION;
    XrSpaceLocationFlags required =
        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
    float u = 0.0f, v = 0.0f;
    BOOL pointer_valid = XR_SUCCEEDED(pxrLocateSpace(g_aim_space, g_space, display_time, &location)) &&
        (location.locationFlags & required) == required &&
        panel_ray_hit_pose(&location.pose, panel_pose, panel_width, panel_height, &u, &v) &&
        game_window_is_foreground();

    BOOL trigger_active = FALSE;
    float trigger_value = 0.0f;
    BOOL primary_active = FALSE, primary_face_down = FALSE;
    BOOL back_active = FALSE, back_down = FALSE;
    read_float_action(g_trigger_action, &trigger_active, &trigger_value);
    read_boolean_action(g_primary_face_action, &primary_active, &primary_face_down);
    read_boolean_action(g_secondary_face_action, &back_active, &back_down);
    BOOL primary_now = pointer_valid &&
        ((trigger_active && trigger_value >= 0.55f) || (primary_active && primary_face_down));
    BOOL back_now = pointer_valid && back_active && back_down;

    if (pointer_valid) {
        float move_u = u, move_v = v;
        if (primary_now && !g_primary_down && g_last_pointer_valid) {
            move_u = g_last_pointer_u;
            move_v = g_last_pointer_v;
        }
        if (!move_pointer_to_panel(move_u, move_v)) {
            pointer_valid = FALSE;
        } else {
            /* Presentation mirrors the exact coordinate injected this frame.
               This preserves the pre-press latch while allowing held drags to
               keep the HMD cursor aligned with the moving Win32 pointer. */
            g_presented_pointer_u = move_u;
            g_presented_pointer_v = move_v;
            g_presented_pointer_valid = TRUE;
        }
    }
    if (!pointer_valid) {
        primary_now = FALSE;
        back_now = FALSE;
        g_last_pointer_valid = FALSE;
        g_presented_pointer_valid = FALSE;
    }
    if (pointer_valid && !g_pointer_logged) {
        g_pointer_logged = TRUE;
        char detail[192];
        telemetry_snprintf(detail, sizeof(detail),
            "\"status\":\"observed\",\"pointer_hand\":\"%s\",\"panel\":\"%s\"",
            controller_hands_swapped ? "left" : "right",
            auxiliary_panel ? "auxiliary_immersive" : "primary_flat");
        write_record(auxiliary_panel ? "CTRL-002" : "CTRL-001",
            "panel_pointer_observed", detail);
    }
    if (primary_now && !g_primary_down) {
        INPUT input; ZeroMemory(&input, sizeof(input));
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        if (SendInput(1, &input, sizeof(input)) == 1) {
            g_primary_down = TRUE;
            write_record("CTRL-001", "primary_click_pressed",
                "\"status\":\"observed\",\"deduplicated\":true");
        }
    } else if (!primary_now && g_primary_down) {
        release_primary_input();
    }
    if (back_now && !g_back_was_down) {
        INPUT inputs[2]; ZeroMemory(inputs, sizeof(inputs));
        inputs[0].type = inputs[1].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = inputs[1].ki.wVk = VK_ESCAPE;
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
        if (SendInput(2, inputs, sizeof(INPUT)) == 2)
            write_record("CTRL-001", "back_pressed",
                "\"status\":\"observed\",\"injection\":\"escape\"");
    }
    g_back_was_down = back_now;
    if (pointer_valid && !g_primary_down) {
        g_last_pointer_u = u;
        g_last_pointer_v = v;
        g_last_pointer_valid = TRUE;
    }
}

/* ---------------- minimal draw pipeline ---------------- */

static BOOL build_draw_pipeline(char* detail, size_t detail_chars) {
    write_record("CAP-009", "pipe_stage", "\"stage\":\"enter\"");
    if (!g_vs_blob || !g_ps_blob || !g_panel_ps_blob || !g_vs_dbg_blob || !g_ps_solid_blob || !g_eye_copy_vs_blob) {
        telemetry_snprintf(detail, detail_chars, "\"stage\":\"blobs_missing\"");
        return FALSE;
    }
    write_record("CAP-009", "pipe_stage", "\"stage\":\"using_precompiled_blobs\"");
    {
        char ptrs[384];
        telemetry_snprintf(ptrs, sizeof(ptrs),
            "\"ptrs\":{\"device\":\"%p\",\"vs_blob\":\"%p\",\"ps_blob\":\"%p\","
            "\"vs_dbg_blob\":\"%p\",\"ps_solid_blob\":\"%p\"}",
            (void*)probe_device, (void*)g_vs_blob, (void*)g_ps_blob,
            (void*)g_vs_dbg_blob, (void*)g_ps_solid_blob);
        write_record("CAP-009", "pipe_stage", ptrs);
    }
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    HRESULT hr = ID3D11Device_CreateVertexShader(probe_device,
        ID3D10Blob_GetBufferPointer(g_vs_blob), ID3D10Blob_GetBufferSize(g_vs_blob), NULL, &g_vs);
    write_record("CAP-009", "pipe_stage", "\"stage\":\"vs_created\"");
    if (SUCCEEDED(hr))
        hr = ID3D11Device_CreateInputLayout(probe_device, layout, 2,
            ID3D10Blob_GetBufferPointer(g_vs_blob), ID3D10Blob_GetBufferSize(g_vs_blob), &g_il);
    write_record("CAP-009", "pipe_stage", "\"stage\":\"il_created\"");
    if (SUCCEEDED(hr))
        hr = ID3D11Device_CreatePixelShader(probe_device,
            ID3D10Blob_GetBufferPointer(g_ps_blob), ID3D10Blob_GetBufferSize(g_ps_blob), NULL, &g_ps);
    write_record("CAP-009", "pipe_stage", "\"stage\":\"ps_created\"");
    if (SUCCEEDED(hr))
        hr = ID3D11Device_CreatePixelShader(probe_device,
            ID3D10Blob_GetBufferPointer(g_panel_ps_blob), ID3D10Blob_GetBufferSize(g_panel_ps_blob),
            NULL, &g_panel_ps);
    write_record("CAP-009", "pipe_stage", "\"stage\":\"panel_ps_created\"");
    if (SUCCEEDED(hr))
        hr = ID3D11Device_CreateVertexShader(probe_device,
            ID3D10Blob_GetBufferPointer(g_vs_dbg_blob), ID3D10Blob_GetBufferSize(g_vs_dbg_blob), NULL, &g_vs_dbg);
    write_record("CAP-009", "pipe_stage", "\"stage\":\"vs_dbg_created\"");
    if(sphere_vs_blob && !sphere_vs)(void)ID3D11Device_CreateVertexShader(probe_device,ID3D10Blob_GetBufferPointer(sphere_vs_blob),ID3D10Blob_GetBufferSize(sphere_vs_blob),NULL,&sphere_vs);
    if(sphere_ps_blob && !sphere_ps)(void)ID3D11Device_CreatePixelShader(probe_device,ID3D10Blob_GetBufferPointer(sphere_ps_blob),ID3D10Blob_GetBufferSize(sphere_ps_blob),NULL,&sphere_ps);
    if (marker_blob && !marker_ps)
        (void)ID3D11Device_CreatePixelShader(probe_device,ID3D10Blob_GetBufferPointer(marker_blob),
            ID3D10Blob_GetBufferSize(marker_blob),NULL,&marker_ps);
    if (SUCCEEDED(hr))
        hr = ID3D11Device_CreatePixelShader(probe_device,
            ID3D10Blob_GetBufferPointer(g_ps_solid_blob), ID3D10Blob_GetBufferSize(g_ps_solid_blob), NULL, &g_ps_solid);
    write_record("CAP-009", "pipe_stage", "\"stage\":\"ps_solid_created\"");
    if (SUCCEEDED(hr))
        hr = ID3D11Device_CreateVertexShader(probe_device,
            ID3D10Blob_GetBufferPointer(g_eye_copy_vs_blob), ID3D10Blob_GetBufferSize(g_eye_copy_vs_blob),
            NULL, &g_eye_copy_vs);
    if (FAILED(hr)) { telemetry_snprintf(detail, detail_chars, "\"stage\":\"shader_create\",\"hr\":%ld", (long)hr); return FALSE; }

    /* panel quad: dynamic VB, aspect-corrected per frame from the captured
       game frame dimensions (portrait 852x1514 must not be squeezed) */
    PanelVertex verts[4] = {
        {-1,  1, -2, 0, 0}, { 1,  1, -2, 1, 0},
        {-1, -1, -2, 0, 1}, { 1, -1, -2, 1, 1},
    };
    D3D11_BUFFER_DESC vb_desc; ZeroMemory(&vb_desc, sizeof(vb_desc));
    vb_desc.ByteWidth = sizeof(verts);
    vb_desc.Usage = D3D11_USAGE_DYNAMIC;
    vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    D3D11_SUBRESOURCE_DATA vb_data = { verts, 0, 0 };
    hr = ID3D11Device_CreateBuffer(probe_device, &vb_desc, &vb_data, &g_vb);
    if (FAILED(hr)) { telemetry_snprintf(detail, detail_chars, "\"stage\":\"vb\",\"hr\":%ld", (long)hr); return FALSE; }

    D3D11_BUFFER_DESC cb_desc; ZeroMemory(&cb_desc, sizeof(cb_desc));
    cb_desc.ByteWidth = 64;
    cb_desc.Usage = D3D11_USAGE_DYNAMIC;
    cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = ID3D11Device_CreateBuffer(probe_device, &cb_desc, NULL, &g_cb);
    if (FAILED(hr)) { telemetry_snprintf(detail, detail_chars, "\"stage\":\"cb\",\"hr\":%ld", (long)hr); return FALSE; }

    D3D11_SAMPLER_DESC sdesc; ZeroMemory(&sdesc, sizeof(sdesc));
    sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sdesc.AddressU = sdesc.AddressV = sdesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sdesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sdesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = ID3D11Device_CreateSamplerState(probe_device, &sdesc, &g_sampler);
    if (FAILED(hr)) { telemetry_snprintf(detail, detail_chars, "\"stage\":\"sampler\",\"hr\":%ld", (long)hr); return FALSE; }
    /* the projected quad can wind either way depending on eye/pose; never cull */
    D3D11_RASTERIZER_DESC rdesc; ZeroMemory(&rdesc, sizeof(rdesc));
    rdesc.FillMode = D3D11_FILL_SOLID;
    rdesc.CullMode = D3D11_CULL_NONE;
    hr = ID3D11Device_CreateRasterizerState(probe_device, &rdesc, &g_raster);
    if (FAILED(hr)) { telemetry_snprintf(detail, detail_chars, "\"stage\":\"raster\",\"hr\":%ld", (long)hr); return FALSE; }
    return TRUE;
}

typedef HRESULT (__stdcall *PFN_D3DCompile_)(LPCSTR, SIZE_T, LPCSTR, const void*, void*,
    LPCSTR, LPCSTR, UINT, UINT, void**);

static BOOL compile_all_shaders(void) {
    HMODULE compiler = LoadLibraryA("D3DCompiler_47.dll");
    if (!compiler) return FALSE;
    PFN_D3DCompile_ compile;
    typedef HRESULT (__stdcall *PFN_D3DCompile_)(LPCSTR, SIZE_T, LPCSTR, const void*, void*,
        LPCSTR, LPCSTR, UINT, UINT, void**);
    compile = (PFN_D3DCompile_)(void*)GetProcAddress(compiler, "D3DCompile");
    if (!compile) { FreeLibrary(compiler); return FALSE; }
    HRESULT hr = compile(g_vs_src, strlen(g_vs_src), NULL, NULL, NULL, "main", "vs_5_0", 0, 0, (void**)&g_vs_blob);
    if (SUCCEEDED(hr)) hr = compile(g_ps_src, strlen(g_ps_src), NULL, NULL, NULL, "main", "ps_5_0", 0, 0, (void**)&g_ps_blob);
    if (SUCCEEDED(hr)) hr = compile(g_panel_ps_src, strlen(g_panel_ps_src), NULL, NULL, NULL, "main", "ps_5_0", 0, 0, (void**)&g_panel_ps_blob);
    if (SUCCEEDED(hr)) hr = compile(g_vs_dbg_src, strlen(g_vs_dbg_src), NULL, NULL, NULL, "main", "vs_5_0", 0, 0, (void**)&g_vs_dbg_blob);
    if(!sphere_vs_blob)(void)compile(sphere_vertex_shader,strlen(sphere_vertex_shader),NULL,NULL,NULL,"main","vs_5_0",0,0,(void**)&sphere_vs_blob);
    if(!sphere_ps_blob)(void)compile(sphere_pixel_shader,strlen(sphere_pixel_shader),NULL,NULL,NULL,"main","ps_5_0",0,0,(void**)&sphere_ps_blob);
    if (!marker_blob) (void)compile(marker_shader,strlen(marker_shader),NULL,NULL,NULL,"main","ps_5_0",0,0,(void**)&marker_blob);
    if (SUCCEEDED(hr)) hr = compile(g_ps_solid_src, strlen(g_ps_solid_src), NULL, NULL, NULL, "main", "ps_5_0", 0, 0, (void**)&g_ps_solid_blob);
    if (SUCCEEDED(hr)) hr = compile(g_eye_copy_vs_src, strlen(g_eye_copy_vs_src), NULL, NULL, NULL, "main", "vs_5_0", 0, 0, (void**)&g_eye_copy_vs_blob);
    /* NEVER FreeLibrary the compiler: the returned blobs' vtables live in
       D3DCompiler_47.dll and must stay mapped for the blob lifetime
       (crashing AV at blob vtable+0x10 otherwise) */
    return SUCCEEDED(hr) && g_vs_blob && g_ps_blob && g_panel_ps_blob && g_vs_dbg_blob && g_ps_solid_blob && g_eye_copy_vs_blob;
}

/* ---------------- XR worker ---------------- */

/* The session thread owns all LOCAL handles. Keep each change until the first
   locate at or after its time, including multiple announcements in one poll. */
typedef struct LocalChange {
    struct LocalChange* next;
    XrTime time;
    BOOL valid;
    XrPosef new_in_old;
} LocalChange;

static BOOL queue_local_change(LocalChange** head,const XrEventDataReferenceSpaceChangePending* event) {
    LocalChange* item=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*item));
    if (!item) return FALSE;
    item->time=event->changeTime;
    item->valid=event->poseValid;
    if (item->valid) item->new_in_old=event->poseInPreviousSpace;
    while (*head && (*head)->time<=item->time) head=&(*head)->next;
    item->next=*head;
    *head=item;
    return TRUE;
}

static BOOL offset_after_change(XrPosef prior,XrPosef delta,XrPosef* next) {
    XrQuaternionf q=delta.orientation, p=prior.orientation;
    float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    if (!isfinite(norm) || fabsf(norm-1.f)>.01f) return FALSE;
    Quat inverse=quat_conjugate((Quat){q.x,q.y,q.z,q.w});
    Vec3 v=quat_rotate(inverse,(Vec3){prior.position.x-delta.position.x,
        prior.position.y-delta.position.y,prior.position.z-delta.position.z});
    if (!isfinite(v.x)||!isfinite(v.y)||!isfinite(v.z)) return FALSE;
    Quat r=quat_multiply(inverse,(Quat){p.x,p.y,p.z,p.w});
    *next=(XrPosef){{r.x,r.y,r.z,r.w},{v.x,v.y,v.z}};
    return TRUE;
}

enum { XR_RUN_DONE, XR_RUN_LOST, XR_RUN_UNAVAILABLE };
static BOOL lost_xr(XrResult result){
    return result==XR_ERROR_INSTANCE_LOST || result==XR_ERROR_SESSION_LOST || result==XR_SESSION_LOSS_PENDING;
}
static DWORD WINAPI xr_worker(LPVOID unused) {
    (void)unused;
    static HMODULE loader;
    WCHAR loader_path[MAX_PATH];
    if (!find_loader_dll(loader_path, MAX_PATH)) {
        write_disabled("openxr_loader_not_found", NULL);
        return 0;
    }
    if (!loader) loader = LoadLibraryW(loader_path);
    if (!loader) {
        char extra[320];
        char escaped[MAX_PATH * 2];
        char narrow[MAX_PATH] = {0};
        if (!WideCharToMultiByte(CP_UTF8, 0, loader_path, -1, narrow, MAX_PATH, NULL, NULL))
            memcpy(narrow, "[path_too_long]", sizeof("[path_too_long]"));
        json_escape(narrow, escaped, sizeof(escaped));
        telemetry_snprintf(extra, sizeof(extra), "\"loader_last_error\":%lu,\"loader_path\":\"%s\"",
            GetLastError(), escaped);
        write_disabled("openxr_loader_load_failed", extra);
        return 0;
    }
    if (!resolve_all(loader)) {
        write_disabled("openxr_resolve_missing_export", NULL);
        return 0;
    }

    XrRuntimeState state; ZeroMemory(&state, sizeof(state));

    XrApplicationInfo app; ZeroMemory(&app, sizeof(app));
    strcpy_s(app.applicationName, XR_MAX_APPLICATION_NAME_SIZE, "Umamusume VR Immersive");
    app.applicationVersion = 1;
    strcpy_s(app.engineName, XR_MAX_ENGINE_NAME_SIZE, "umavr-immersive");
    app.apiVersion = XR_API_VERSION_1_0;
    const char* extensions[] = {"XR_KHR_D3D11_enable"};
    XrInstanceCreateInfo ici; ZeroMemory(&ici, sizeof(ici));
    ici.type = XR_TYPE_INSTANCE_CREATE_INFO;
    ici.applicationInfo = app;
    ici.enabledApiLayerCount = 0;
    ici.enabledExtensionCount = 1;
    ici.enabledExtensionNames = extensions;
    XrResult result = pxrCreateInstance(&ici, &state.instance);
    if (XR_FAILED(result)) {
        log_xr_result(XR_NULL_HANDLE, "xrCreateInstance", result);
        return result==XR_ERROR_RUNTIME_UNAVAILABLE || lost_xr(result)?XR_RUN_UNAVAILABLE:XR_RUN_DONE;
    }
    state.instance_alive = TRUE;
    create_controller_actions(state.instance);

    char detail[640];
    XrInstanceProperties props; ZeroMemory(&props, sizeof(props));
    props.type = XR_TYPE_INSTANCE_PROPERTIES;
    if (XR_SUCCEEDED(pxrGetInstanceProperties(state.instance, &props))) {
        char escaped_name[XR_MAX_RUNTIME_NAME_SIZE * 2];
        json_escape(props.runtimeName, escaped_name, sizeof(escaped_name));
        telemetry_snprintf(detail, sizeof(detail),
            "\"status\":\"observed\",\"runtime_name\":\"%s\",\"runtime_version\":\"%u.%u.%u\"",
            escaped_name, XR_VERSION_MAJOR(props.runtimeVersion), XR_VERSION_MINOR(props.runtimeVersion),
            XR_VERSION_PATCH(props.runtimeVersion));
        write_record(PROBE_ID, "instance_properties", detail);
    }

    XrSystemGetInfo sgi; ZeroMemory(&sgi, sizeof(sgi));
    sgi.type = XR_TYPE_SYSTEM_GET_INFO;
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    result = pxrGetSystem(state.instance, &sgi, &state.system);
    if (XR_FAILED(result)) {
        log_xr_result(state.instance, "xrGetSystem", result);
        destroy_xr_state(&state);
        return result==XR_ERROR_FORM_FACTOR_UNAVAILABLE || lost_xr(result)?XR_RUN_UNAVAILABLE:XR_RUN_DONE;
    }

    XrSystemProperties sys_props; ZeroMemory(&sys_props, sizeof(sys_props));
    sys_props.type = XR_TYPE_SYSTEM_PROPERTIES;
    if (XR_SUCCEEDED(pxrGetSystemProperties(state.instance, state.system, &sys_props))) {
        telemetry_snprintf(detail, sizeof(detail),
            "\"status\":\"observed\",\"system_id\":\"%llu\",\"vendor_id\":%u,\"max_layers\":%u,"
            "\"orientation_tracking\":%d,\"position_tracking\":%d",
            (unsigned long long)sys_props.systemId, sys_props.vendorId,
            sys_props.graphicsProperties.maxLayerCount,
            sys_props.trackingProperties.orientationTracking ? 1 : 0,
            sys_props.trackingProperties.positionTracking ? 1 : 0);
        write_record(PROBE_ID, "system_properties", detail);
    }

    XrViewConfigurationView views_config[MAX_EYES];
    memset(views_config, 0, sizeof(views_config));
    for (uint32_t i = 0; i < MAX_EYES; ++i) views_config[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
    result = pxrEnumerateViewConfigurationViews(state.instance, state.system,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, MAX_EYES, &g_view_count, views_config);
    if (XR_FAILED(result) || g_view_count == 0 || g_view_count > MAX_EYES) {
        write_disabled("view_configuration_views", NULL);
        destroy_xr_state(&state);
        return 0;
    }
    uint32_t recommended_eye_width = views_config[0].recommendedImageRectWidth;
    uint32_t recommended_eye_height = views_config[0].recommendedImageRectHeight;
    uint32_t max_eye_width = views_config[0].maxImageRectWidth;
    uint32_t max_eye_height = views_config[0].maxImageRectHeight;
    if (!recommended_eye_width || !recommended_eye_height ||
        !max_eye_width || !max_eye_height || recommended_eye_width > 8192 ||
        recommended_eye_height > 8192 || recommended_eye_width > max_eye_width ||
        recommended_eye_height > max_eye_height) {
        write_disabled("unusable_recommended_resolution", NULL);
        destroy_xr_state(&state);
        return 0;
    }
    float requested_scale = eye_render_scale;
    float selected_scale = requested_scale;
    const char* scale_status = eye_render_scale_setting_loaded ? "observed" : "defaulted";
    const char* fallback_reason = "none";
    if (!isfinite(selected_scale) || selected_scale < EYE_RENDER_SCALE_MIN ||
        selected_scale > EYE_RENDER_SCALE_MAX) {
        selected_scale = 1.0f;
        scale_status = "fallback";
        fallback_reason = "outside_valid_range";
    }
    uint32_t selected_eye_width = (uint32_t)floorf((float)recommended_eye_width * selected_scale + 0.5f);
    uint32_t selected_eye_height = (uint32_t)floorf((float)recommended_eye_height * selected_scale + 0.5f);
    if (!selected_eye_width || !selected_eye_height || selected_eye_width > max_eye_width ||
        selected_eye_height > max_eye_height || selected_eye_width > 8192 || selected_eye_height > 8192) {
        selected_scale = 1.0f;
        selected_eye_width = recommended_eye_width;
        selected_eye_height = recommended_eye_height;
        scale_status = "fallback";
        fallback_reason = "runtime_dimension_limit";
    }
    eye_render_scale = selected_scale;
    scale_recommended_w=(LONG)recommended_eye_width; scale_recommended_h=(LONG)recommended_eye_height;
    scale_max_width=(LONG)max_eye_width; scale_max_height=(LONG)max_eye_height;
    InterlockedExchange(&scale_native,(LONG)lroundf(selected_scale*100));
    InterlockedExchange(&scale_requested,scale_native);
    g_eye_width = selected_eye_width;
    g_eye_height = selected_eye_height;
    telemetry_snprintf(detail, sizeof(detail),
        "\"status\":\"%s\",\"requested_scale\":%.3f,\"selected_scale\":%.3f,"
        "\"recommended_width\":%u,\"recommended_height\":%u,"
        "\"selected_width\":%u,\"selected_height\":%u,"
        "\"max_width\":%u,\"max_height\":%u,\"fallback_reason\":\"%s\","
        "\"expensive_warning\":%s,\"pc_output_unchanged\":true",
        scale_status, requested_scale, selected_scale, recommended_eye_width,
        recommended_eye_height, selected_eye_width, selected_eye_height,
        max_eye_width, max_eye_height, fallback_reason,
        selected_scale > EYE_RENDER_SCALE_EXPENSIVE ? "true" : "false");
    write_record("PERF-001", "eye_render_resolution", detail);
    InterlockedExchange(&requested_eye_width, (LONG)g_eye_width);
    InterlockedExchange(&requested_eye_height, (LONG)g_eye_height);

    XrEnvironmentBlendMode blend_modes[8];
    uint32_t blend_count = 0;
    if (XR_SUCCEEDED(pxrEnumerateEnvironmentBlendModes(state.instance, state.system,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 8, &blend_count, blend_modes)) && blend_count > 0) {
        g_blend_mode = blend_modes[0];
    } else {
        g_blend_mode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    }

    PFN_xrGetD3D11GraphicsRequirementsKHR p_get_requirements = NULL;
    result = pxrGetInstanceProcAddr(state.instance, "xrGetD3D11GraphicsRequirementsKHR",
        (PFN_xrVoidFunction*)&p_get_requirements);
    if (XR_FAILED(result) || !p_get_requirements) {
        write_disabled("graphics_requirements_proc_missing", NULL);
        destroy_xr_state(&state);
        return 0;
    }
    XrGraphicsRequirementsD3D11KHR requirements;
    ZeroMemory(&requirements, sizeof(requirements));
    requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR;
    result = p_get_requirements(state.instance, state.system, &requirements);
    if (XR_FAILED(result)) {
        log_xr_result(state.instance, "xrGetD3D11GraphicsRequirementsKHR", result);
        destroy_xr_state(&state);
        return 0;
    }

    char device_detail[512];
    if (!create_probe_device(&requirements.adapterLuid, device_detail, sizeof(device_detail))) {
        telemetry_snprintf(detail, sizeof(detail), "\"status\":\"failed\",\"stage\":\"probe_device_creation\",%s",
            device_detail);
        write_record(PROBE_ID, "disabled", detail);
        destroy_xr_state(&state);
        return 0;
    }
    telemetry_snprintf(detail, sizeof(detail),
        "\"status\":\"observed\",\"runtime_adapter_luid\":\"%08lx-%08lx\",%s",
        (unsigned long)requirements.adapterLuid.HighPart, (unsigned long)requirements.adapterLuid.LowPart,
        device_detail);
    write_record("CAP-005", "probe_device_created", detail);

    XrGraphicsBindingD3D11KHR binding; ZeroMemory(&binding, sizeof(binding));
    binding.type = XR_TYPE_GRAPHICS_BINDING_D3D11_KHR;
    binding.device = probe_device;
    XrSessionCreateInfo sci; ZeroMemory(&sci, sizeof(sci));
    sci.type = XR_TYPE_SESSION_CREATE_INFO;
    sci.next = &binding;
    sci.systemId = state.system;
    result = pxrCreateSession(state.instance, &sci, &g_session);
    if (XR_FAILED(result)) {
        log_xr_result(state.instance, "xrCreateSession", result);
        destroy_xr_state(&state);
        return lost_xr(result)?XR_RUN_UNAVAILABLE:XR_RUN_DONE;
    }
    telemetry_snprintf(detail, sizeof(detail),
        "\"status\":\"observed\",\"bound_device\":\"%p\",\"device_owner\":\"probe_isolated\"",
        (void*)probe_device);
    write_record("CAP-005", "session_created", detail);
    if (g_controller_actions_configured) attach_controller_actions();

    /* wait for the game-side shared texture to appear (bounded) */
    ULONGLONG shared_deadline = GetTickCount64() + (fast_mode ? 6000 : 60000);
    while (!InterlockedCompareExchangePointer((volatile PVOID*)&g_shared_handle, NULL, NULL)) {
        if (GetTickCount64() > shared_deadline) {
            write_disabled("shared_texture_wait_timeout", NULL);
            destroy_xr_state(&state);
            return 0;
        }
        Sleep(fast_mode ? 10 : 50);
    }
    if (!create_projection_swapchain()) {
        destroy_xr_state(&state);
        return 0;
    }
    if (!open_shared_on_probe_device()) {
        destroy_xr_state(&state);
        return 0;
    }
    if (!build_draw_pipeline(detail, sizeof(detail))) {
        write_record(PROBE_ID, "disabled", detail);
        destroy_xr_state(&state);
        return 0;
    }
    telemetry_snprintf(detail, sizeof(detail), "\"status\":\"observed\",\"stage\":\"draw_pipeline_ready\"");
    write_record("CAP-009", "lifecycle_step", detail);

    ULONGLONG deadline = GetTickCount64() + (fast_mode ? 8000 : 300000);
    ULONGLONG begun_tick = 0;
    BOOL runtime_lost=FALSE;
    LocalChange* changes=NULL;
    XrPosef local_offset={.orientation.w=1};
    XrTime local_valid_since=0;
    BOOL replace_space=FALSE, reseed_local=FALSE;
    XrTime space_retry=0;
    BOOL exit_requested = FALSE;
    BOOL running = TRUE;
    while (running) {
        XrEventDataBuffer event_data; ZeroMemory(&event_data, sizeof(event_data));
        event_data.type = XR_TYPE_EVENT_DATA_BUFFER;
        XrResult poll = pxrPollEvent(state.instance, &event_data);
        while (poll == XR_SUCCESS) {
            switch (event_data.type) {
                case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
                    const XrEventDataReferenceSpaceChangePending* change=(const void*)&event_data;
                    if (change->session==g_session && change->referenceSpaceType==XR_REFERENCE_SPACE_TYPE_LOCAL &&
                        !queue_local_change(&changes,change)) {
                        write_disabled("reference_change_queue_allocation",NULL);
                        running=FALSE;
                    }
                    break;
                }
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                    XrEventDataSessionStateChanged* changed =
                        (XrEventDataSessionStateChanged*)(void*)&event_data;
                    g_session_state = (int)changed->state;
                    if (changed->state != XR_SESSION_STATE_FOCUSED)
                        release_synthetic_input();
                    telemetry_snprintf(detail, sizeof(detail),
                        "\"status\":\"observed\",\"session_state\":%d,\"time_ns\":%lld",
                        g_session_state, (long long)changed->time);
                    write_record("CAP-005", "session_state_changed", detail);
                    if (changed->state == XR_SESSION_STATE_READY && !g_begun) {
                        XrSessionBeginInfo bi; ZeroMemory(&bi, sizeof(bi));
                        bi.type = XR_TYPE_SESSION_BEGIN_INFO;
                        bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                        XrResult br = pxrBeginSession(g_session, &bi);
                        if (XR_SUCCEEDED(br)) {
                            g_begun = TRUE;
                            begun_tick = GetTickCount64();
                            XrReferenceSpaceCreateInfo rsi; ZeroMemory(&rsi, sizeof(rsi));
                            rsi.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
                            /* The primary flat panel follows the viewer. A LOCAL-space
                               quad is world-fixed and leaves the FOV when the user turns. */
                            rsi.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
                            rsi.poseInReferenceSpace.orientation.w = 1.0f;
                            XrResult sr = pxrCreateReferenceSpace(g_session, &rsi, &g_space);
                            if (XR_FAILED(sr)) {
                                log_xr_result(state.instance, "xrCreateReferenceSpace", sr);
                                running = FALSE;
                            } else {
                                write_record("CAP-009", "panel_space_created",
                                    "\"status\":\"observed\",\"reference_space\":\"VIEW\"," 
                                    "\"placement\":\"viewer_locked\"");
                                rsi.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
                                sr = pxrCreateReferenceSpace(g_session, &rsi, &g_tracking_space);
                                if (XR_FAILED(sr)) {
                                    g_tracking_space = 0;
                                    log_xr_result(state.instance, "xrCreateTrackingSpace", sr);
                                } else {
                                    write_record("IMMERSIVE-001", "tracking_space_created",
                                        "\"status\":\"observed\",\"reference_space\":\"LOCAL\"");
                                }
                            }
                        } else {
                            log_xr_result(state.instance, "xrBeginSession", br);
                            running = FALSE;
                        }
                    } else if (changed->state == XR_SESSION_STATE_STOPPING && g_begun) {
                        pxrEndSession(g_session);
                        g_begun = FALSE;
                    } else if (changed->state == XR_SESSION_STATE_EXITING ||
                               changed->state == XR_SESSION_STATE_LOSS_PENDING) {
                        runtime_lost=changed->state==XR_SESSION_STATE_LOSS_PENDING;
                        running = FALSE;
                    }
                    break;
                }
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                    runtime_lost=TRUE;
                    running = FALSE;
                    break;
                default:
                    break;
            }
            if (!running) break;
            ZeroMemory(&event_data, sizeof(event_data));
            event_data.type = XR_TYPE_EVENT_DATA_BUFFER;
            poll = pxrPollEvent(state.instance, &event_data);
        }
        if(lost_xr(poll)){runtime_lost=TRUE;break;}
        if (!running || (!begun_tick && GetTickCount64() > deadline)) {
            if (running) write_disabled("session_not_ready_timeout", NULL);
            break;
        }
        if (fast_mode && g_begun && !exit_requested &&
            GetTickCount64() - begun_tick > 5000) {
            exit_requested = TRUE;
            pxrEndSession(g_session);
            g_begun = FALSE;
            running = FALSE;
        }
        if (!g_begun || g_session_state < (int)XR_SESSION_STATE_READY ||
            g_session_state > (int)XR_SESSION_STATE_FOCUSED) {
            Sleep(fast_mode ? 5 : 10);
            continue;
        }

        XrFrameWaitInfo fwi; ZeroMemory(&fwi, sizeof(fwi));
        fwi.type = XR_TYPE_FRAME_WAIT_INFO;
        XrFrameState fs; ZeroMemory(&fs, sizeof(fs));
        fs.type = XR_TYPE_FRAME_STATE;
        XrResult wait_result=pxrWaitFrame(g_session,&fwi,&fs);
        if(lost_xr(wait_result)){runtime_lost=TRUE;break;}
        if (XR_FAILED(wait_result)) {
            write_disabled("xrWaitFrame", NULL);
            break;
        }
        if(!local_valid_since)local_valid_since=fs.predictedDisplayTime;
        while (changes && changes->time<=fs.predictedDisplayTime) {
            LocalChange* due=changes;
            changes=due->next;
            if (!due->valid || !offset_after_change(local_offset,due->new_in_old,&local_offset)) {
                local_offset=(XrPosef){.orientation.w=1};
                reseed_local=TRUE;
            }
            HeapFree(GetProcessHeap(),0,due);
            local_valid_since=fs.predictedDisplayTime;
            replace_space=TRUE;
            space_retry=0;
        }
        if (replace_space && fs.predictedDisplayTime>=space_retry) {
            if (g_tracking_space) { pxrDestroySpace(g_tracking_space); g_tracking_space=XR_NULL_HANDLE; }
            XrReferenceSpaceCreateInfo info={.type=XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
                .referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL,.poseInReferenceSpace=local_offset};
            XrResult replaced=pxrCreateReferenceSpace(g_session,&info,&g_tracking_space);
            if (XR_FAILED(replaced)) {
                g_tracking_space=XR_NULL_HANDLE;
                InterlockedExchange(&eye_optics_ready,0);
                space_retry=fs.predictedDisplayTime+1000000000;
                log_xr_result(state.instance,"rebaseTrackingSpace",replaced);
            } else {
                replace_space=FALSE;
                write_record("POSE","local_space_rebased",reseed_local?"\"reseed\":true":"\"reseed\":false");
            }
            g_world_grab=(WorldGrab){0};
            g_return_button=(ReturnButton){0};
            AcquireSRWLockExclusive(&contact_lock);
            ZeroMemory(&contact,sizeof(contact));
            ReleaseSRWLockExclusive(&contact_lock);
            if(reseed_local) InterlockedExchange(&eye_optics_ready,0);
        }
        XrFrameBeginInfo fbi; ZeroMemory(&fbi, sizeof(fbi));
        fbi.type = XR_TYPE_FRAME_BEGIN_INFO;
        XrResult begin_result=pxrBeginFrame(g_session,&fbi);
        if(lost_xr(begin_result)){runtime_lost=TRUE;break;}
        if (XR_FAILED(begin_result)) {
            write_disabled("xrBeginFrame", NULL);
            break;
        }

        ZeroMemory(&xr_frame_telemetry, sizeof(xr_frame_telemetry));
        xr_frame_telemetry.frame_id = ++xr_observed_frame_id;
        xr_frame_telemetry.predicted_time = fs.predictedDisplayTime;

        BOOL visible = g_session_state == (int)XR_SESSION_STATE_VISIBLE ||
                       g_session_state == (int)XR_SESSION_STATE_FOCUSED;
        BOOL actions_synced = sync_controller_actions();
        if(!actions_synced || g_session_state!=(int)XR_SESSION_STATE_FOCUSED)g_follow_armed=FALSE;
        else if(camera_click_edge(g_follow_click,g_right_hand_path,&g_follow_armed)){
            LONG token=toggle_camera_follow(InterlockedCompareExchange(&camera_follow_default_on,0,0));
            write_record("POSE-002","camera_follow_click",token&1?"\"stage\":\"frozen\"":"\"stage\":\"following\"");
        }
        BOOL navigation_updated = FALSE;
        uint32_t layer_count = 0;
        BOOL controller_panel_active = FALSE;
        BOOL controller_panel_auxiliary = FALSE;
        BOOL fresh_pose = FALSE;
        float controller_panel_width = 0.0f, controller_panel_height = 0.0f;
        XrPosef controller_panel_pose; ZeroMemory(&controller_panel_pose, sizeof(controller_panel_pose));
        controller_panel_pose.orientation.w = 1.0f;
        controller_panel_pose.position.z = -PANEL_DISTANCE_M;
        XrCompositionLayerProjectionView proj_views[MAX_EYES];
        XrCompositionLayerProjection proj; ZeroMemory(&proj, sizeof(proj));
        const XrCompositionLayerBaseHeader* layer_ptrs[2];

        /* background projection layer: dim neutral clear + panel draw */
        if (fs.shouldRender && visible) {
            XrViewLocateInfo li; ZeroMemory(&li, sizeof(li));
            li.type = XR_TYPE_VIEW_LOCATE_INFO;
            li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            li.displayTime = fs.predictedDisplayTime;
            li.space = g_space;
            XrViewState view_state; ZeroMemory(&view_state, sizeof(view_state));
            view_state.type = XR_TYPE_VIEW_STATE;
            XrView located[MAX_EYES];
            memset(located, 0, sizeof(located));
            for (uint32_t i = 0; i < MAX_EYES; ++i) located[i].type = XR_TYPE_VIEW;
            uint32_t located_count = 0;
            XrViewStateFlags required_view_flags =
                XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT;
            if (XR_SUCCEEDED(pxrLocateViews(g_session, &li, &view_state, MAX_EYES, &located_count, located)) &&
                located_count == MAX_EYES &&
                (view_state.viewStateFlags & required_view_flags) == required_view_flags) {
                /* VIEW-relative poses are intentionally identity-like so the flat
                   panel remains viewer-locked. Physical head motion must come from
                   a stable LOCAL space; sharing VIEW here makes every pose delta 0. */
                if (g_tracking_space) {
                    XrViewLocateInfo tracking_li = li;
                    tracking_li.space = g_tracking_space;
                    XrViewState tracking_state; ZeroMemory(&tracking_state, sizeof(tracking_state));
                    tracking_state.type = XR_TYPE_VIEW_STATE;
                    XrView tracking_views[MAX_EYES];
                    memset(tracking_views, 0, sizeof(tracking_views));
                    for (uint32_t i = 0; i < MAX_EYES; ++i) tracking_views[i].type = XR_TYPE_VIEW;
                    uint32_t tracking_count = 0;
                    if (XR_SUCCEEDED(pxrLocateViews(g_session, &tracking_li, &tracking_state,
                            MAX_EYES, &tracking_count, tracking_views)) &&
                        tracking_count == MAX_EYES &&
                        (tracking_state.viewStateFlags & required_view_flags) == required_view_flags) {
                        float dx = tracking_views[1].pose.position.x - tracking_views[0].pose.position.x;
                        float dy = tracking_views[1].pose.position.y - tracking_views[0].pose.position.y;
                        float dz = tracking_views[1].pose.position.z - tracking_views[0].pose.position.z;
                        float half_ipd = 0.5f * sqrtf(dx*dx + dy*dy + dz*dz);
                        if (half_ipd >= 0.02f && half_ipd <= 0.05f) {
                            XrPosef center = tracking_views[0].pose;
                            center.position.x = 0.5f * (tracking_views[0].pose.position.x + tracking_views[1].pose.position.x);
                            center.position.y = 0.5f * (tracking_views[0].pose.position.y + tracking_views[1].pose.position.y);
                            center.position.z = 0.5f * (tracking_views[0].pose.position.z + tracking_views[1].pose.position.z);
                            AcquireSRWLockExclusive(&eye_optics_lock);
                            eye_optics.fov[0] = located[0].fov;
                            eye_optics.fov[1] = located[1].fov;
                            eye_optics.half_ipd_m = half_ipd;
                            fresh_pose = (tracking_state.viewStateFlags & (XR_VIEW_STATE_POSITION_TRACKED_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT)) ==
                                (XR_VIEW_STATE_POSITION_TRACKED_BIT|XR_VIEW_STATE_ORIENTATION_TRACKED_BIT);
                            if(reseed_local && fresh_pose){
                                eye_optics.origin_center=center;
                                eye_optics.origin_center.orientation=(XrQuaternionf){0,0,0,1};
                                eye_optics.origin_valid=TRUE;
                                immersive_entry_origin_committed=TRUE;
                                eye_optics.return_orientation_valid=FALSE;
                                reseed_local=FALSE;
                            }
                            eye_optics.render_views=(HomeRenderViews){.eye={tracking_views[0].pose,tracking_views[1].pose},.space=g_tracking_space,.time=fs.predictedDisplayTime};
                            eye_optics.current_center = center;
                            if (!eye_optics.origin_valid) {
                                eye_optics.origin_center = center;
                                eye_optics.origin_valid = TRUE;
                            }
                            ReleaseSRWLockExclusive(&eye_optics_lock);
                            InterlockedExchange(&eye_optics_ready, !reseed_local);
                        }
                    }
                }
                XrSwapchainImageAcquireInfo ai; ZeroMemory(&ai, sizeof(ai));
                ai.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
                uint32_t pidx = 0;
                if (XR_SUCCEEDED(pxrAcquireSwapchainImage(g_swapchain_proj, &ai, &pidx)) &&
                    pidx < g_proj_image_count) {
                    XrSwapchainImageWaitInfo wi; ZeroMemory(&wi, sizeof(wi));
                    wi.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
                    wi.timeout = 100000000LL;
                    if (XR_SUCCEEDED(pxrWaitSwapchainImage(g_swapchain_proj, &wi))) {
                        float bg[4] = {0.02f, 0.02f, 0.04f, 1.0f};
                        for (uint32_t eye = 0; eye < located_count && eye < MAX_EYES; ++eye) {
                            ID3D11RenderTargetView* rtv = g_proj_rtvs[pidx][eye];
                            if (!rtv) continue;
                            ID3D11DeviceContext_ClearRenderTargetView(probe_context, rtv, bg);
                            XrCompositionLayerProjectionView* v = &proj_views[eye];
                            v->type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
                            v->subImage.swapchain = g_swapchain_proj;
                            v->subImage.imageRect.offset.x = 0;
                            v->subImage.imageRect.offset.y = 0;
                            v->subImage.imageRect.extent.width = g_eye_width;
                            v->subImage.imageRect.extent.height = g_eye_height;
                            v->subImage.imageArrayIndex = eye;
                            v->pose = located[eye].pose;
                            v->fov = located[eye].fov;
                        }
                        BOOL immersive_drawn = copy_fresh_eye_pair_to_projection(pidx);
                        BOOL desktop_mirror_ready = open_shared_on_probe_device();
                        /* Refresh a probe-owned latest-complete-frame cache without
                           blocking either render loop. Draws never retain the shared
                           keyed mutex and can reuse the previous complete frame. */
                        BOOL panel_drawn = FALSE;
                        /* Drain even while Home is immersive. Otherwise key 1 can
                           hold a pre-menu frame for the entire immersive interval. */
                        BOOL refreshed=FALSE;
                        if(desktop_mirror_ready && g_panel_tex && g_local_shared_tex &&
                           (!km_enabled || (g_shared_km_probe &&
                            IDXGIKeyedMutex_AcquireSync(g_shared_km_probe,1,0)==S_OK))){
                            ID3D11DeviceContext_CopyResource(probe_context,
                                (ID3D11Resource*)g_panel_tex,(ID3D11Resource*)g_local_shared_tex);
                            refreshed=!km_enabled || IDXGIKeyedMutex_ReleaseSync(g_shared_km_probe,0)==S_OK;
                            if(refreshed) InterlockedExchange(&g_panel_frame_ready,1);
                        }
                        if(menu_prefer_panel(immersive_drawn,desktop_mirror_ready,refreshed) && immersive_drawn){
                            immersive_drawn=FALSE;
                            for(uint32_t eye=0;eye<located_count && eye<MAX_EYES;eye++)
                                ID3D11DeviceContext_ClearRenderTargetView(probe_context,g_proj_rtvs[pidx][eye],bg);
                        }
                        XrSpace submission_space=g_space;
                        if(immersive_drawn && xr_cache_telemetry.owned && g_tracking_space &&
                           home_cached_views.space==g_tracking_space && home_cached_views.time>=local_valid_since){
                            XrSpaceLocation local_in_view={.type=XR_TYPE_SPACE_LOCATION};
                            XrSpaceLocationFlags valid=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
                            if(XR_SUCCEEDED(pxrLocateSpace(g_tracking_space,g_space,fs.predictedDisplayTime,&local_in_view)) &&
                               (local_in_view.locationFlags&valid)==valid){
                                submission_space=g_tracking_space;
                                xr_frame_telemetry.render_pose_time=home_cached_views.time;
                                for(UINT e=0;e<2;e++){
                                    XrPosef captured=home_cached_views.eye[e];
                                    proj_views[e].pose=captured;
                                    proj_views[e].fov=located[e].fov=home_cached_views.fov[e];
                                    located[e].pose.position=transform_panel_point(&local_in_view.pose,captured.position);
                                    XrQuaternionf a=local_in_view.pose.orientation,b=captured.orientation;
                                    Quat q=quat_multiply((Quat){a.x,a.y,a.z,a.w},(Quat){b.x,b.y,b.z,b.w});
                                    located[e].pose.orientation=(XrQuaternionf){q.x,q.y,q.z,q.w};
                                }
                            }
                        }
                        scale_menu_input(fs.predictedDisplayTime,immersive_drawn && fresh_pose,actions_synced && g_session_state==(int)XR_SESSION_STATE_FOCUSED);
                        update_controller_navigation(fs.predictedDisplayTime,
                            immersive_drawn && !scale_menu.open && !scale_menu.swallow, actions_synced, fresh_pose);
                        navigation_updated = TRUE;
                        XrPosef auxiliary_panel_pose;
                        BOOL auxiliary_panel_presented = update_aux_panel_state(
                            fs.predictedDisplayTime, immersive_drawn && !scale_menu.open, actions_synced,
                            &auxiliary_panel_pose);
                        BOOL settings_panel=scale_menu.open && scale_menu.tracked && scale_menu_label();
                        if(settings_panel){auxiliary_panel_pose=scale_menu.pose;auxiliary_panel_presented=TRUE;}
                        if ((settings_panel || (desktop_mirror_ready && g_panel_srv && (!immersive_drawn || auxiliary_panel_presented))) &&
                                g_vs && g_panel_ps && g_il && g_vb &&
                                g_cb && g_sampler && g_raster) {
                            /* FLAT and IMMERSIVE auxiliary presentation share the
                               same actual PC-visible desktop mirror. A replacement
                               generation is rejected until its handle/cache opens. */
                            ID3D11ShaderResourceView* presented_panel_srv = settings_panel?scale_menu.label:g_panel_srv;
                            ID3D11PixelShader* presented_panel_ps = g_panel_ps;
                            UINT presented_panel_width = settings_panel?MENU_W:g_panel_width;
                            UINT presented_panel_height = settings_panel?MENU_H:g_panel_height;
                            BOOL presented_panel_ready =
                                settings_panel || InterlockedCompareExchange(&g_panel_frame_ready, 0, 0) != 0;
                            if (!settings_panel && auxiliary_panel_presented && immersive_drawn &&
                                    InterlockedCompareExchange(&g_aux_panel_source_logged, 1, 0) == 0)
                                write_record("CTRL-002", "aux_panel_source_selected",
                                    "\"status\":\"observed\",\"source\":\"pc_visible_desktop_mirror\"," 
                                    "\"reason\":\"complete_flat_screen_required\"");
                            float panel_w = 0.0f, panel_h = 0.0f;
                            BOOL geometry_ready = auxiliary_panel_presented ?
                                update_aux_panel_vertices(&auxiliary_panel_pose,
                                    presented_panel_width, presented_panel_height,
                                    &panel_w, &panel_h) :
                                update_panel_vertices(located, located_count, presented_panel_width,
                                    presented_panel_height, &panel_w, &panel_h);
                            if (geometry_ready && presented_panel_ready) {
                                UINT stride = 20, offset = 0;
                                ID3D11DeviceContext_IASetPrimitiveTopology(probe_context,
                                    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                                ID3D11DeviceContext_IASetVertexBuffers(probe_context, 0, 1, &g_vb, &stride, &offset);
                                ID3D11DeviceContext_IASetInputLayout(probe_context, g_il);
                                ID3D11DeviceContext_VSSetShader(probe_context, g_vs, NULL, 0);
                                ID3D11DeviceContext_PSSetShader(probe_context, presented_panel_ps, NULL, 0);
                                ID3D11DeviceContext_VSSetConstantBuffers(probe_context, 0, 1, &g_cb);
                                ID3D11DeviceContext_PSSetSamplers(probe_context, 0, 1, &g_sampler);
                                D3D11_VIEWPORT vp = {0, 0, (FLOAT)g_eye_width, (FLOAT)g_eye_height, 0, 1};
                                ID3D11DeviceContext_RSSetViewports(probe_context, 1, &vp);
                                ID3D11DeviceContext_RSSetState(probe_context, g_raster);
                                float first_vp[16];
                                for (uint32_t eye = 0; eye < located_count && eye < MAX_EYES; ++eye) {
                                    float proj_m[16], view_m[16], vp_m[16];
                                    build_projection(&located[eye].fov, 0.05f, 100.0f, proj_m);
                                    build_view(&located[eye].pose, view_m);
                                    mat_mul(vp_m, proj_m, view_m);
                                    D3D11_MAPPED_SUBRESOURCE mapped;
                                    if (SUCCEEDED(ID3D11DeviceContext_Map(probe_context,
                                            (ID3D11Resource*)g_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                                        if (copied_frames_total == 0)
                                            memcpy(first_vp, vp_m, sizeof(first_vp));
                                        memcpy(mapped.pData, vp_m, sizeof(vp_m));
                                        ID3D11DeviceContext_Unmap(probe_context, (ID3D11Resource*)g_cb, 0);
                                        ID3D11DeviceContext_OMSetRenderTargets(probe_context, 1,
                                            &g_proj_rtvs[pidx][eye], NULL);
                                        ID3D11DeviceContext_PSSetShaderResources(probe_context, 0, 1,
                                            &presented_panel_srv);
                                        ID3D11DeviceContext_Draw(probe_context, 4, 0);
                                        panel_drawn = TRUE;
                                        if (copied_frames_total == 0) {
                                            char vpbuf[256];
                                            telemetry_snprintf(vpbuf, sizeof(vpbuf),
                                                "\"vp_diag\":[%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f]",
                                                first_vp[0], first_vp[5], first_vp[10], first_vp[14],
                                                first_vp[2], first_vp[6], first_vp[8], first_vp[9]);
                                            write_record("CAP-009", "vp_diag", vpbuf);
                                        }
                                    }
                                }
                                if (panel_drawn) {
                                    controller_panel_active = !settings_panel;
                                    controller_panel_auxiliary = auxiliary_panel_presented;
                                    controller_panel_width = panel_w;
                                    controller_panel_height = panel_h;
                                    if (auxiliary_panel_presented)
                                        controller_panel_pose = auxiliary_panel_pose;
                                    /* The accepted mapped pointer coordinate is the
                                       single owner for input and the HMD cursor. It
                                       intentionally trails ray sampling by one frame,
                                       preserving the trigger pre-press latch exactly. */
                                    if ((settings_panel?scale_menu.hit:g_presented_pointer_valid) && g_ps_solid &&
                                            update_cursor_vertices(&controller_panel_pose,
                                                panel_w, panel_h,
                                                settings_panel?scale_menu.u:g_presented_pointer_u,
                                                settings_panel?scale_menu.v:g_presented_pointer_v)) {
                                        ID3D11DeviceContext_PSSetShader(probe_context,
                                            g_ps_solid, NULL, 0);
                                        for (uint32_t eye = 0;
                                                eye < located_count && eye < MAX_EYES; ++eye) {
                                            float proj_m[16], view_m[16], vp_m[16];
                                            build_projection(&located[eye].fov,
                                                0.05f, 100.0f, proj_m);
                                            build_view(&located[eye].pose, view_m);
                                            mat_mul(vp_m, proj_m, view_m);
                                            D3D11_MAPPED_SUBRESOURCE cursor_mapped;
                                            if (SUCCEEDED(ID3D11DeviceContext_Map(probe_context,
                                                    (ID3D11Resource*)g_cb, 0,
                                                    D3D11_MAP_WRITE_DISCARD, 0,
                                                    &cursor_mapped))) {
                                                memcpy(cursor_mapped.pData, vp_m, sizeof(vp_m));
                                                ID3D11DeviceContext_Unmap(probe_context,
                                                    (ID3D11Resource*)g_cb, 0);
                                                ID3D11DeviceContext_OMSetRenderTargets(probe_context,
                                                    1, &g_proj_rtvs[pidx][eye], NULL);
                                                ID3D11DeviceContext_Draw(probe_context, 4, 0);
                                            }
                                        }
                                        if (!g_cursor_logged) {
                                            g_cursor_logged = TRUE;
                                            write_record("CTRL-003", "hmd_cursor_composited",
                                                auxiliary_panel_presented ?
                                                    "\"status\":\"observed\",\"panel\":\"auxiliary_immersive\"" :
                                                    "\"status\":\"observed\",\"panel\":\"primary_flat\"");
                                        }
                                    }
                                    ++copied_frames_total;
                                    if (copied_frames_total == 1) {
                                        telemetry_snprintf(detail, sizeof(detail),
                                            "\"status\":\"observed\",\"first_draw_width\":%u,\"first_draw_height\":%u,"
                                            "\"panel_distance_m\":%.2f,\"panel_width_m\":%.3f,"
                                            "\"panel_height_m\":%.3f,\"placement\":\"viewer_locked\"",
                                            bb_width, bb_height, PANEL_DISTANCE_M, panel_w, panel_h);
                                        write_record("CAP-009", "first_frame_copied_to_panel", detail);
                                    }
                                }
                            }
                        }
                        static int prior_route=-1;
                        int route=immersive_drawn?2:panel_drawn?1:0;
                        if(route!=prior_route){
                            const char* stage=route==2?"stereo_draw":route==1?"flat_draw":"clear_only";
                            char status[300];
                            telemetry_snprintf(status,sizeof status,"\"stage\":\"%s\",\"state\":%d,\"generation\":%ld,\"mirror_open\":%s,\"mirror_ready\":%s,\"width\":%u,\"height\":%u",
                                stage,prior_route,(long)xr_desktop_mirror_generation,desktop_mirror_ready?"true":"false",
                                g_panel_frame_ready?"true":"false",g_panel_width,g_panel_height);
                            write_record("XR-PANEL","presentation_fallback",status);prior_route=route;
                        }
                        XrSwapchainImageReleaseInfo rel; ZeroMemory(&rel, sizeof(rel));
                        rel.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
                        if(immersive_drawn && actions_synced && fresh_pose && !scale_menu.open && !auxiliary_panel_presented &&
                           g_session_state==(int)XR_SESSION_STATE_FOCUSED && located_count==2)
                            draw_controller_markers(pidx,fs.predictedDisplayTime,located);
                        XrResult release_result = pxrReleaseSwapchainImage(g_swapchain_proj, &rel);
                        xr_frame_telemetry.release_called = TRUE;
                        xr_frame_telemetry.release_result = (long)release_result;
                        proj.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
                        proj.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                        proj.space = submission_space;
                        proj.viewCount = located_count;
                        proj.views = proj_views;
                        layer_ptrs[layer_count++] = (const XrCompositionLayerBaseHeader*)&proj;
                        xr_frame_telemetry.stereo_layer = immersive_drawn;
                    }
                }
            }
        }

        if (!navigation_updated)
            update_controller_navigation(fs.predictedDisplayTime, FALSE, actions_synced, FALSE);
        if (!navigation_updated) scale_cancel();
        if (scale_menu.swallow) {
            BOOL a=FALSE, primary=FALSE, back=FALSE;float trigger=0;
            if (actions_synced && read_boolean_action(g_primary_face_action,&a,&primary) &&
                read_boolean_action(g_secondary_face_action,&a,&back) &&
                read_float_action(g_trigger_action,&a,&trigger) && !primary && !back && trigger<.35f)
                scale_menu.swallow=FALSE;
        }
        update_controller_input(fs.predictedDisplayTime, controller_panel_active && !scale_menu.open && !scale_menu.swallow,
            controller_panel_width, controller_panel_height, &controller_panel_pose,
            actions_synced, controller_panel_auxiliary);

        XrFrameEndInfo fei; ZeroMemory(&fei, sizeof(fei));
        fei.type = XR_TYPE_FRAME_END_INFO;
        fei.displayTime = fs.predictedDisplayTime;
        fei.environmentBlendMode = g_blend_mode;
        fei.layerCount = layer_count;
        fei.layers = layer_count ? layer_ptrs : NULL;
        XrResult endr = pxrEndFrame(g_session, &fei);
        if(lost_xr(endr)){runtime_lost=TRUE;break;}
        xr_frame_telemetry.end_called = TRUE;
        xr_frame_telemetry.layer_count = layer_count;
        BOOL stereo_accepted=stereo_end_accepted(&xr_frame_telemetry, (long)endr);
        cadence_step(stereo_accepted,xr_frame_telemetry.generation,
            xr_cache_telemetry.owned?scale_native:xr_cache_telemetry.render_percent,
            xr_cache_telemetry.owned,xr_frame_telemetry.pair_serial,fs.predictedDisplayTime);
        if (stereo_accepted) {
            ++stereo_end_accepted_total;
            scale_menu_accept();
            if (stereo_end_logged_generation != xr_frame_telemetry.generation) {
                char frame_detail[768];
                stereo_telemetry_detail(&xr_frame_telemetry, (long)endr, frame_detail, sizeof(frame_detail));
                write_record("IMMERSIVE-SOURCE", "first_stereo_frame_end_accepted", frame_detail);
                stereo_end_logged_generation = xr_frame_telemetry.generation;
            }
        } else if (XR_FAILED(endr) || (xr_frame_telemetry.eye_mask &&
                (endr != XR_SUCCESS || xr_frame_telemetry.release_result != XR_SUCCESS))) {
            char frame_detail[768];
            stereo_telemetry_detail(&xr_frame_telemetry, (long)endr, frame_detail, sizeof(frame_detail));
            write_record("IMMERSIVE-SOURCE", "stereo_frame_end_failed", frame_detail);
        }
        if (XR_FAILED(endr)) {
            log_xr_result(state.instance, "xrEndFrame", endr);
            break;
        }
        if (!g_endframe_logged) {
            g_endframe_logged = TRUE;
            write_record("CAP-005", "lifecycle_step", "\"status\":\"observed\",\"stage\":\"end_frame_ok\"");
        }
        ++g_submitted_frames;
        if ((g_submitted_frames % 900) == 0) {
            telemetry_snprintf(detail, sizeof(detail),
                "\"status\":\"observed\",\"submitted_frames\":%llu,\"copied_frames\":%llu,"
                "\"panel_drawn_frames\":%llu,\"immersive_frames\":%llu,\"scene_hooks\":%ld,\"scene_updates\":%ld,\"scene_home\":%ld,"
                "\"scene_owner_updates\":%ld,\"scene_owner_home\":%ld",
                g_submitted_frames, produced_frames, copied_frames_total, immersive_frames_total,
                InterlockedCompareExchange(&scene_hook_calls,0,0), InterlockedCompareExchange(&scene_updates,0,0),
                InterlockedCompareExchange(&scene_home_updates,0,0), InterlockedCompareExchange(&scene_owner_updates,0,0),
                InterlockedCompareExchange(&scene_owner_home_updates,0,0));
            write_record("IMMERSIVE-001", "presentation_progress", detail);
        }
    }

    if(!runtime_lost) InterlockedExchange(&capture_active, 0);
    Sleep(100);
    while(changes){LocalChange* next=changes->next;HeapFree(GetProcessHeap(),0,changes);changes=next;}
    cadence_report("session_end");cadence=(StereoCadence){0};
    destroy_xr_state(&state);

    telemetry_snprintf(detail, sizeof(detail),
        "\"status\":\"summary\",\"submitted_frames\":%llu,\"captured_frames\":%llu,"
        "\"immersive_frames\":%llu,\"stereo_end_accepted\":%llu,\"final_session_state\":%d",
        g_submitted_frames, produced_frames, immersive_frames_total, stereo_end_accepted_total, g_session_state);
    write_record("IMMERSIVE-001", "session_summary", detail);
    return runtime_lost?XR_RUN_LOST:XR_RUN_DONE;
}

/* ---------------- selftest: two devices + keyed mutex + pixel verify ---------------- */

static void release_probe_pipeline(void);
static DWORD WINAPI xr_worker_with_callback_stop(LPVOID unused) {
    BOOL reconnecting=FALSE;
    DWORD delay=1000,result;
    for(;;){
        result=xr_worker(unused);
        if(result==XR_RUN_LOST) reconnecting=TRUE;
        release_probe_pipeline();
        if(probe_context){ID3D11DeviceContext_Release(probe_context);probe_context=NULL;}
        if(probe_device){ID3D11Device_Release(probe_device);probe_device=NULL;}
        scale_menu=(struct ScaleMenu){0};
        panel_short_armed=FALSE;
        if(!reconnecting || result==XR_RUN_DONE)break;
        write_record("XR","reconnect_wait","\"status\":\"waiting_for_runtime\"");
        Sleep(delay);
        delay=delay<16000?delay*2:16000;
    }
    InterlockedExchange(&capture_active,0);
    /* Covers early startup failure as well as ordinary session exit. This
       gates callback delivery only; it cannot cancel an in-flight registration. */
    ef_disarm(&end_frame_bridge);
    InterlockedOr(&bp_stop_requested,1);
    return result;
}

static void release_probe_pipeline(void) {
    if (g_panel_srv) { ID3D11ShaderResourceView_Release(g_panel_srv); g_panel_srv = NULL; }
    if (g_panel_tex) { ID3D11Texture2D_Release(g_panel_tex); g_panel_tex = NULL; }
    InterlockedExchange(&g_panel_frame_ready, 0);
    if (g_sampler) { ID3D11SamplerState_Release(g_sampler); g_sampler = NULL; }
    if (g_raster) { ID3D11RasterizerState_Release(g_raster); g_raster = NULL; }
    if (g_cb) { ID3D11Buffer_Release(g_cb); g_cb = NULL; }
    if (g_vb) { ID3D11Buffer_Release(g_vb); g_vb = NULL; }
    if (g_il) { ID3D11InputLayout_Release(g_il); g_il = NULL; }
    if(sphere_vs){ID3D11VertexShader_Release(sphere_vs);sphere_vs=NULL;}
    if(sphere_ps){ID3D11PixelShader_Release(sphere_ps);sphere_ps=NULL;}
    if(marker_ps){ID3D11PixelShader_Release(marker_ps);marker_ps=NULL;}
    if (g_ps_solid) { ID3D11PixelShader_Release(g_ps_solid); g_ps_solid = NULL; }
    if (g_eye_copy_vs) { ID3D11VertexShader_Release(g_eye_copy_vs); g_eye_copy_vs = NULL; }
    if(scale_menu.label){ID3D11ShaderResourceView_Release(scale_menu.label);scale_menu.label=NULL;}
    if (g_panel_ps) { ID3D11PixelShader_Release(g_panel_ps); g_panel_ps = NULL; }
    if (g_ps) { ID3D11PixelShader_Release(g_ps); g_ps = NULL; }
    if (g_vs_dbg) { ID3D11VertexShader_Release(g_vs_dbg); g_vs_dbg = NULL; }
    if (g_vs) { ID3D11VertexShader_Release(g_vs); g_vs = NULL; }
}

static DWORD WINAPI selftest_worker(LPVOID unused) {
    (void)unused;
    char detail[768];
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0}, selected = 0, selected_b = 0;
    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
        levels, 1, D3D11_SDK_VERSION, &probe_device, &selected, &probe_context);
    if (FAILED(hr))
        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0,
            levels, 1, D3D11_SDK_VERSION, &probe_device, &selected, &probe_context);
    if (FAILED(hr) || !probe_device || !probe_context) {
        telemetry_snprintf(detail, sizeof(detail), "\"selftest\":\"fail\",\"stage\":\"device\",\"hr\":%ld", (long)hr);
        write_record(PROBE_ID, "selftest", detail); return 0;
    }
    /* second device simulating the game side, exactly like production */
    ID3D11Device* dev_a = NULL;
    ID3D11DeviceContext* ctx_a = NULL;
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
        levels, 1, D3D11_SDK_VERSION, &dev_a, &selected_b, &ctx_a);
    if (FAILED(hr) || !dev_a || !ctx_a) {
        telemetry_snprintf(detail, sizeof(detail), "\"selftest\":\"fail\",\"stage\":\"device_a\",\"hr\":%ld", (long)hr);
        write_record(PROBE_ID, "selftest", detail); return 0;
    }
    if (!build_draw_pipeline(detail, sizeof(detail))) {
        char pipeline_detail[768];
        telemetry_snprintf(pipeline_detail, sizeof(pipeline_detail), "\"selftest\":\"fail\",\"stage\":\"pipeline\",%s", detail);
        write_record(PROBE_ID, "selftest", pipeline_detail); return 0;
    }

    /* Display-encoded 0.5 mid-gray exercises the panel-only sRGB EOTF. */
    D3D11_TEXTURE2D_DESC sd; ZeroMemory(&sd, sizeof(sd));
    sd.Width = 32; sd.Height = 64; sd.MipLevels = 1; sd.ArraySize = 1;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.SampleDesc.Count = 1;
    sd.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    sd.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDHANDLE;
    ID3D11Texture2D* src_tex_a = NULL;
    if (FAILED(ID3D11Device_CreateTexture2D(dev_a, &sd, NULL, &src_tex_a)) || !src_tex_a) {
        write_record(PROBE_ID, "selftest", "\"selftest\":\"fail\",\"stage\":\"src_tex\"");
        return 0;
    }
    IDXGIKeyedMutex* km = NULL;
    if (FAILED(ID3D11Texture2D_QueryInterface(src_tex_a, &iid_dxgi_keyed_mutex, (void**)&km)) || !km) {
        write_record(PROBE_ID, "selftest", "\"selftest\":\"fail\",\"stage\":\"src_km\"");
        return 0;
    }
    {
        ID3D11RenderTargetView* src_rtv = NULL;
        if (SUCCEEDED(ID3D11Device_CreateRenderTargetView(dev_a, (ID3D11Resource*)src_tex_a, NULL, &src_rtv))) {
            if (SUCCEEDED(IDXGIKeyedMutex_AcquireSync(km, 0, INFINITE))) {
                float encoded_midgray[4] = {0.5f, 0.5f, 0.5f, 1};
                ID3D11DeviceContext_ClearRenderTargetView(ctx_a, src_rtv, encoded_midgray);
                IDXGIKeyedMutex_ReleaseSync(km, 1);
                ctx_a->lpVtbl->Flush(ctx_a);
            }
            ID3D11RenderTargetView_Release(src_rtv);
        }
    }
    HANDLE shared_handle = NULL;
    {
        IDXGIResource* res = NULL;
        if (SUCCEEDED(ID3D11Texture2D_QueryInterface(src_tex_a, &iid_dxgi_resource, (void**)&res)) && res) {
            IDXGIResource_GetSharedHandle(res, &shared_handle);
            IDXGIResource_Release(res);
        }
    }
    if (!shared_handle) {
        write_record(PROBE_ID, "selftest", "\"selftest\":\"fail\",\"stage\":\"shared_handle\"");
        return 0;
    }
    if (FAILED(ID3D11Device_OpenSharedResource(probe_device, shared_handle,
            &iid_d3d11_texture2d, (void**)&g_local_shared_tex)) || !g_local_shared_tex) {
        write_record(PROBE_ID, "selftest", "\"selftest\":\"fail\",\"stage\":\"open_shared\"");
        return 0;
    }
    D3D11_TEXTURE2D_DESC panel_sd = sd;
    panel_sd.MiscFlags = 0;
    panel_sd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(ID3D11Device_CreateTexture2D(probe_device, &panel_sd, NULL, &g_panel_tex)) ||
        !g_panel_tex) {
        write_record(PROBE_ID, "selftest", "\"selftest\":\"fail\",\"stage\":\"panel_cache\"");
        return 0;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
    srvd.Format = sd.Format; srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1; srvd.Texture2D.MostDetailedMip = 0;
    if (FAILED(ID3D11Device_CreateShaderResourceView(probe_device,
            (ID3D11Resource*)g_panel_tex, &srvd, &g_panel_srv))) {
        write_record(PROBE_ID, "selftest", "\"selftest\":\"fail\",\"stage\":\"srv\"");
        return 0;
    }
    /* production draws under the keyed mutex; use the PROBE device's own
       interface of the shared mutex */
    if (FAILED(ID3D11Texture2D_QueryInterface(g_local_shared_tex, &iid_dxgi_keyed_mutex,
            (void**)&g_shared_km_probe)) || !g_shared_km_probe) {
        write_record(PROBE_ID, "selftest", "\"selftest\":\"fail\",\"stage\":\"probe_km\"");
        return 0;
    }
    km_enabled = 1;

    /* render target on the probe device (must NOT inherit shared flags) */
    sd.Width = 64; sd.Height = 64; sd.MiscFlags = 0;
    ID3D11Texture2D* rt_tex = NULL;
    if (FAILED(ID3D11Device_CreateTexture2D(probe_device, &sd, NULL, &rt_tex)) || !rt_tex) {
        write_record(PROBE_ID, "selftest", "\"selftest\":\"fail\",\"stage\":\"rt_tex\"");
        return 0;
    }
    ID3D11RenderTargetView* rtv = NULL;
    if (FAILED(ID3D11Device_CreateRenderTargetView(probe_device, (ID3D11Resource*)rt_tex, NULL, &rtv))) {
        write_record(PROBE_ID, "selftest", "\"selftest\":\"fail\",\"stage\":\"rtv\"");
        return 0;
    }

    /* draw: centered quad with symmetric +-45 degree FOV. */
    XrFovf fov = {-0.7853982f, 0.7853982f, 0.7853982f, -0.7853982f};
    XrPosef identity; ZeroMemory(&identity, sizeof(identity)); identity.orientation.w = 1.0f;
    XrView test_view; ZeroMemory(&test_view, sizeof(test_view));
    test_view.type = XR_TYPE_VIEW; test_view.pose = identity; test_view.fov = fov;
    float panel_w = 0.0f, panel_h = 0.0f;
    BOOL geometry_ok = update_panel_vertices(&test_view, 1, 32, 64, &panel_w, &panel_h);
    BOOL aspect_ok = geometry_ok && panel_h > 0.0f &&
        fabsf(panel_w / panel_h - 0.5f) < 0.001f;
    BOOL geometry_scale_ok = geometry_ok &&
        fabsf(panel_w - 1.5f) < 0.001f && fabsf(panel_h - 3.0f) < 0.001f;
    float pointer_u = 0.0f, pointer_v = 0.0f;
    BOOL pointer_center_hit = panel_ray_hit(&identity, panel_w, panel_h,
        &pointer_u, &pointer_v);
    XrPosef outside_aim = identity;
    outside_aim.position.x = panel_w;
    BOOL controller_pointer_math_ok = pointer_center_hit &&
        fabsf(pointer_u - 0.5f) < 0.001f && fabsf(pointer_v - 0.5f) < 0.001f &&
        !panel_ray_hit(&outside_aim, panel_w, panel_h, NULL, NULL);
    XrPosef aux_test_pose = identity;
    aux_test_pose.position.z = -1.0f;
    float aux_u = 0.0f, aux_v = 0.0f;
    BOOL aux_panel_math_ok = panel_ray_hit_pose(&identity, &aux_test_pose,
        0.275f, 0.55f, &aux_u, &aux_v) &&
        fabsf(aux_u - 0.5f) < 0.001f && fabsf(aux_v - 0.5f) < 0.001f &&
        !panel_ray_hit_pose(&outside_aim, &aux_test_pose,
            0.275f, 0.55f, NULL, NULL);
    XrPosef cursor_pose = identity;
    cursor_pose.position.z = -PANEL_DISTANCE_M;
    PanelVertex cursor_verts[4];
    float expected_cursor_half_size =
        fminf(panel_w, panel_h) * CURSOR_HALF_SIZE_FRACTION;
    BOOL cursor_geometry_ok = build_cursor_vertices(&cursor_pose, panel_w, panel_h,
        0.5f, 0.5f, cursor_verts) &&
        fabsf(0.5f * (cursor_verts[0].x + cursor_verts[1].x)) < 0.001f &&
        fabsf(0.5f * (cursor_verts[0].y + cursor_verts[2].y)) < 0.001f &&
        fabsf((cursor_verts[1].x - cursor_verts[0].x) -
            2.0f * expected_cursor_half_size) < 0.0001f &&
        fabsf((cursor_verts[0].y - cursor_verts[2].y) -
            2.0f * expected_cursor_half_size) < 0.0001f &&
        fabsf(cursor_verts[0].z - (-PANEL_DISTANCE_M + CURSOR_SURFACE_OFFSET_M)) < 0.001f;
    XrFovf asymmetric_fov = {-0.65f, 0.85f, 0.75f, -0.55f};
    float asymmetric_proj[16];
    build_projection(&asymmetric_fov, 0.05f, 100.0f, asymmetric_proj);
    float left_ndc = asymmetric_proj[0] * tanf(asymmetric_fov.angleLeft) + asymmetric_proj[8];
    float right_ndc = asymmetric_proj[0] * tanf(asymmetric_fov.angleRight) + asymmetric_proj[8];
    float down_ndc = asymmetric_proj[5] * tanf(asymmetric_fov.angleDown) + asymmetric_proj[9];
    float up_ndc = asymmetric_proj[5] * tanf(asymmetric_fov.angleUp) + asymmetric_proj[9];
    BOOL asymmetric_fov_ok = fabsf(left_ndc + 1.0f) < 0.001f &&
        fabsf(right_ndc - 1.0f) < 0.001f && fabsf(down_ndc + 1.0f) < 0.001f &&
        fabsf(up_ndc - 1.0f) < 0.001f && fabsf(asymmetric_proj[2]) < 0.001f &&
        fabsf(asymmetric_proj[6]) < 0.001f;
    Matrix4x4 unity_proj;
    const float unity_near = 0.3f, unity_far = 777.0f;
    build_unity_eye_projection(&asymmetric_fov, unity_near, unity_far, &unity_proj);
    float unity_left_ndc = unity_proj.m[0] * tanf(asymmetric_fov.angleLeft) - unity_proj.m[8];
    float unity_right_ndc = unity_proj.m[0] * tanf(asymmetric_fov.angleRight) - unity_proj.m[8];
    float unity_down_ndc = unity_proj.m[5] * tanf(asymmetric_fov.angleDown) - unity_proj.m[9];
    float unity_up_ndc = unity_proj.m[5] * tanf(asymmetric_fov.angleUp) - unity_proj.m[9];
    BOOL unity_projection_ok = fabsf(unity_left_ndc + 1.0f) < 0.001f &&
        fabsf(unity_right_ndc - 1.0f) < 0.001f &&
        fabsf(unity_down_ndc + 1.0f) < 0.001f && fabsf(unity_up_ndc - 1.0f) < 0.001f &&
        fabsf(unity_proj.m[10] + (unity_far + unity_near) /
            (unity_far - unity_near)) < 0.0001f &&
        fabsf(unity_proj.m[14] + (2.0f * unity_far * unity_near) /
            (unity_far - unity_near)) < 0.0001f;
    EyeOptics pose_test; ZeroMemory(&pose_test, sizeof(pose_test));
    pose_test.origin_valid = TRUE;
    pose_test.origin_center.orientation.w = 1.0f;
    pose_test.current_center.orientation.w = 1.0f;
    pose_test.current_center.position.x = 0.1f;
    pose_test.current_center.position.y = 0.2f;
    pose_test.current_center.position.z = -0.3f;
    pose_test.half_ipd_m = 0.032f;
    Vec3 pose_base_position = {1.0f, 2.0f, 3.0f};
    Quat pose_base_rotation = {0, 0, 0, 1};
    Vec3 pose_left, pose_right;
    Quat pose_rotation;
    BOOL pose_translation_ok = compose_unity_eye_pose(&pose_base_position,
        &pose_base_rotation, FALSE, &pose_test, 1.0f, 0, &pose_left, &pose_rotation) &&
        compose_unity_eye_pose(&pose_base_position, &pose_base_rotation,
            FALSE, &pose_test, 1.0f, 1, &pose_right, &pose_rotation) &&
        fabsf(pose_left.x - 1.068f) < 0.001f && fabsf(pose_right.x - 1.132f) < 0.001f &&
        fabsf(pose_left.y - 2.2f) < 0.001f && fabsf(pose_left.z - 3.3f) < 0.001f;
    BOOL pose_world_scale_ok = compose_unity_eye_pose(&pose_base_position,
        &pose_base_rotation, FALSE, &pose_test, 4.0f, 0, &pose_left, &pose_rotation) &&
        compose_unity_eye_pose(&pose_base_position, &pose_base_rotation,
            FALSE, &pose_test, 4.0f, 1, &pose_right, &pose_rotation) &&
        fabsf(pose_left.x - 1.017f) < 0.001f && fabsf(pose_right.x - 1.033f) < 0.001f &&
        fabsf(pose_left.y - 2.05f) < 0.001f && fabsf(pose_left.z - 3.075f) < 0.001f;
    const float sin15 = 0.25881905f, cos15 = 0.96592583f;
    pose_test.current_center.position = pose_test.origin_center.position;
    pose_test.current_center.orientation = (XrQuaternionf){0, 0, sin15, cos15};
    BOOL pose_roll_ok = compose_unity_eye_pose(&pose_base_position, &pose_base_rotation,
        FALSE, &pose_test, 1.0f, 0, &pose_left, &pose_rotation);
    float test_yaw, test_pitch, test_roll;
    quat_to_yaw_pitch_roll(pose_rotation, &test_yaw, &test_pitch, &test_roll);
    pose_roll_ok = pose_roll_ok && fabsf(test_yaw) < 0.001f && fabsf(test_pitch) < 0.001f &&
        fabsf(test_roll - 0.52359878f) < 0.001f;
    const float sin22_5 = 0.38268343f, cos22_5 = 0.92387953f;
    pose_test.origin_center.position = (XrVector3f){-3.0f, 4.0f, 7.0f};
    pose_test.current_center.position = (XrVector3f){0.1f, 0.2f, -0.3f};
    pose_test.current_center.orientation = (XrQuaternionf){0, 0, sin15, cos15};
    BOOL pose_environment_origin_ok = commit_immersive_environment_origin(&pose_test) &&
        fabsf(pose_test.origin_center.position.x - 0.1f) < 0.001f &&
        fabsf(pose_test.origin_center.position.y - 0.2f) < 0.001f &&
        fabsf(pose_test.origin_center.position.z + 0.3f) < 0.001f &&
        fabsf(pose_test.origin_center.orientation.x) < 0.001f &&
        fabsf(pose_test.origin_center.orientation.y) < 0.001f &&
        fabsf(pose_test.origin_center.orientation.z) < 0.001f &&
        fabsf(pose_test.origin_center.orientation.w - 1.0f) < 0.001f &&
        compose_unity_eye_pose(&pose_base_position, &pose_base_rotation,
            FALSE, &pose_test, 1.0f, 0, &pose_left, &pose_rotation);
    quat_to_yaw_pitch_roll(pose_rotation, &test_yaw, &test_pitch, &test_roll);
    pose_environment_origin_ok = pose_environment_origin_ok &&
        fabsf(test_yaw) < 0.001f && fabsf(test_pitch) < 0.001f &&
        fabsf(test_roll - 0.52359878f) < 0.001f;
    pose_test.current_center.orientation = (XrQuaternionf){0, sin22_5, 0, cos22_5};
    EyeOptics returned=pose_test;
    returned.current_center.orientation=(XrQuaternionf){.2f,.3f,-.1f,.92736185f};
    returned.return_orientation=returned.current_center.orientation;returned.return_orientation_valid=TRUE;
    float return_yaw,return_pitch,return_roll;
    physical_orientation_deltas(&returned,&return_yaw,&return_pitch,&return_roll);
    pose_environment_origin_ok=pose_environment_origin_ok && fabsf(return_yaw)<.0001f &&
        fabsf(return_pitch)<.0001f;
    returned.current_center.orientation=(XrQuaternionf){0,sin22_5,0,cos22_5};
    physical_orientation_deltas(&returned,&return_yaw,&return_pitch,&return_roll);
    pose_environment_origin_ok=pose_environment_origin_ok && fabsf(return_roll)<.0001f;
    returned.return_orientation=(XrQuaternionf){0,0,0,1};
    returned.artificial_yaw=1.57079633f;
    Vec3 side_grab=quat_rotate(tracking_to_navigation(&returned),(Vec3){0,0,1});
    pose_environment_origin_ok=pose_environment_origin_ok && fabsf(side_grab.x-1)<.001f && fabsf(side_grab.z)<.001f;
    BOOL pose_yaw_ok = compose_unity_eye_pose(&pose_base_position, &pose_base_rotation,
        FALSE, &pose_test, 1.0f, 0, &pose_left, &pose_rotation);
    quat_to_yaw_pitch_roll(pose_rotation, &test_yaw, &test_pitch, &test_roll);
    pose_yaw_ok = pose_yaw_ok && fabsf(test_yaw + 0.78539816f) < 0.001f &&
        fabsf(test_pitch) < 0.001f && fabsf(test_roll) < 0.001f;
    Quat tilted_origin = quat_from_yaw_pitch_roll(0.0f, 0.17453293f, 0.52359878f);
    Quat tilted_current = quat_from_yaw_pitch_roll(0.78539816f, 0.17453293f, 0.52359878f);
    pose_test.origin_center.orientation = (XrQuaternionf){
        -tilted_origin.x, -tilted_origin.y, tilted_origin.z, tilted_origin.w};
    pose_test.current_center.orientation = (XrQuaternionf){
        -tilted_current.x, -tilted_current.y, tilted_current.z, tilted_current.w};
    BOOL pose_tilt_isolation_ok = compose_unity_eye_pose(&pose_base_position,
        &pose_base_rotation, FALSE, &pose_test, 1.0f, 0, &pose_left, &pose_rotation);
    quat_to_yaw_pitch_roll(pose_rotation, &test_yaw, &test_pitch, &test_roll);
    pose_tilt_isolation_ok = pose_tilt_isolation_ok &&
        fabsf(test_yaw - 0.78539816f) < 0.001f &&
        fabsf(test_pitch) < 0.001f && fabsf(test_roll) < 0.001f;
    pose_test.origin_center.position = (XrVector3f){0.0f, 0.0f, 0.0f};
    pose_test.current_center.position = pose_test.origin_center.position;
    pose_test.origin_center.orientation = (XrQuaternionf){0, 0, 0, 1};
    pose_test.current_center.orientation = pose_test.origin_center.orientation;
    pose_test.half_ipd_m = 0.0f;
    pose_test.artificial_position = (Vec3){0.25f, -0.1f, 0.5f};
    pose_test.artificial_yaw = NAV_SNAP_ANGLE_DEFAULT_RAD;
    pose_test.artificial_pitch = -0.5f * NAV_SNAP_ANGLE_DEFAULT_RAD;
    BOOL artificial_pose_ok = compose_unity_eye_pose(&pose_base_position,
        &pose_base_rotation, FALSE, &pose_test, 1.0f, 0, &pose_left, &pose_rotation);
    quat_to_yaw_pitch_roll(pose_rotation, &test_yaw, &test_pitch, &test_roll);
    artificial_pose_ok = artificial_pose_ok &&
        fabsf(pose_left.x - 1.25f) < 0.001f &&
        fabsf(pose_left.y - 1.9f) < 0.001f &&
        fabsf(pose_left.z - 3.5f) < 0.001f &&
        fabsf(test_yaw - NAV_SNAP_ANGLE_DEFAULT_RAD) < 0.001f &&
        fabsf(test_pitch + 0.5f * NAV_SNAP_ANGLE_DEFAULT_RAD) < 0.001f &&
        fabsf(test_roll) < 0.001f;
    EyeOptics navigation_test; ZeroMemory(&navigation_test, sizeof(navigation_test));
    navigation_test.origin_valid = TRUE;
    navigation_test.origin_center.orientation.w = 1.0f;
    navigation_test.current_center.orientation.w = 1.0f;
    BOOL navigation_x_latched = FALSE, navigation_y_latched = FALSE;
    uint32_t navigation_changed = apply_navigation_input(&navigation_test,
        (XrVector2f){0.0f, 1.0f}, (XrVector2f){1.0f, 0.0f}, 0.1f,
        &navigation_x_latched, &navigation_y_latched);
    float snapped_yaw = navigation_test.artificial_yaw;
    uint32_t held_changed = apply_navigation_input(&navigation_test,
        (XrVector2f){0.0f, 0.0f}, (XrVector2f){1.0f, 0.0f}, 0.0f,
        &navigation_x_latched, &navigation_y_latched);
    float effective_test_speed = effective_navigation_speed_mps(nav_locomotion_speed_mps,
        world_scale, nav_locomotion_scale_compensation_enabled);
    BOOL locomotion_scale_compensation_math_ok =
        fabsf(effective_navigation_speed_mps(5.0f, 2.0f, TRUE) - 5.0f) < 0.001f &&
        fabsf(effective_navigation_speed_mps(5.0f, 4.0f, TRUE) - 10.0f) < 0.001f &&
        fabsf(effective_navigation_speed_mps(7.5f, 8.0f, FALSE) - 7.5f) < 0.001f &&
        fabsf(effective_navigation_speed_mps(12.5f, 8.0f, TRUE) - 50.0f) < 0.001f &&
        effective_navigation_speed_mps(-1.0f, 2.0f, TRUE) == 0.0f;
    WorldGrab grab_test = {0};
    XrVector3f hand = {0};
    (void)world_grab_delta(&grab_test,1,1,TRUE,0,hand);
    (void)world_grab_delta(&grab_test,1,2,TRUE,1,hand);
    hand = (XrVector3f){0.25f,-0.5f,1.0f};
    Vec3 grabbed = world_grab_delta(&grab_test,1,3,TRUE,1,hand);
    BOOL grab_ok = grabbed.x==-0.25f && grabbed.y==0.5f && grabbed.z==1.0f;
    (void)world_grab_delta(&grab_test,2,4,TRUE,1,hand);
    grab_ok = grab_ok && !grab_test.holding && !grab_test.armed;
    (void)world_grab_delta(&grab_test,2,5,TRUE,0,hand);
    (void)world_grab_delta(&grab_test,2,6,TRUE,1,hand);
    (void)world_grab_delta(&grab_test,2,250000007,TRUE,1,hand);
    grab_ok = grab_ok && !grab_test.holding && !grab_test.armed;
    ReturnButton button = {0};
    BOOL gesture_ok = camera_return_gesture(&button,TRUE,TRUE,1)==CAMERA_NONE &&
        camera_return_gesture(&button,TRUE,FALSE,2)==CAMERA_NONE &&
        camera_return_gesture(&button,TRUE,TRUE,3)==CAMERA_NONE &&
        camera_return_gesture(&button,TRUE,FALSE,4)==CAMERA_RETURN &&
        camera_return_gesture(&button,TRUE,TRUE,5)==CAMERA_NONE &&
        camera_return_gesture(&button,TRUE,TRUE,800000005)==CAMERA_RECENTER &&
        camera_return_gesture(&button,TRUE,FALSE,800000006)==CAMERA_NONE &&
        camera_return_gesture(&button,TRUE,TRUE,800000007)==CAMERA_NONE &&
        camera_return_gesture(&button,FALSE,TRUE,800000008)==CAMERA_NONE &&
        camera_return_gesture(&button,TRUE,FALSE,800000009)==CAMERA_NONE;
    XrPosef shifted={.orientation={0,.70710678f,0,.70710678f},.position={2,3,4}};
    XrPosef offset={.orientation.w=1}, restored;
    BOOL reference_ok=offset_after_change(offset,shifted,&offset) &&
        fabsf(offset.position.x-4)<.001f && fabsf(offset.position.y+3)<.001f &&
        fabsf(offset.position.z+2)<.001f &&
        offset_after_change((XrPosef){.orientation.w=1},offset,&restored) &&
        fabsf(restored.position.x-2)<.001f && fabsf(restored.position.z-4)<.001f &&
        fabsf(restored.orientation.y-shifted.orientation.y)<.001f;
    BOOL navigation_math_ok = reference_ok && gesture_ok && grab_ok && artificial_pose_ok && locomotion_scale_compensation_math_ok &&
        (navigation_changed & NAV_STEP_MOVED) != 0 &&
        (navigation_changed & NAV_STEP_SNAPPED) != 0 &&
        held_changed == 0 && navigation_x_latched && !navigation_y_latched &&
        fabsf(snapped_yaw - nav_snap_angle_rad) < 0.001f &&
        fabsf(navigation_test.artificial_yaw - snapped_yaw) < 0.001f &&
        fabsf(navigation_test.artificial_position.x -
            sinf(nav_snap_angle_rad) * effective_test_speed * 0.1f) < 0.001f &&
        fabsf(navigation_test.artificial_position.y) < 0.001f &&
        fabsf(navigation_test.artificial_position.z -
            cosf(nav_snap_angle_rad) * effective_test_speed * 0.1f) < 0.001f;
    pose_test.origin_center.orientation = (XrQuaternionf){0, 0, 0, 1};
    pose_test.current_center.orientation = (XrQuaternionf){0, 0, sin15, cos15};
    pose_test.artificial_yaw = 0.0f;
    pose_test.artificial_pitch = 0.0f;
    Quat authored_follow_rotation = quat_from_yaw_pitch_roll(0.2f, -0.1f, 0.3f);
    BOOL authored_rotation_follow_ok = compose_unity_eye_pose(&pose_base_position,
        &authored_follow_rotation, TRUE, &pose_test, 1.0f, 0, &pose_left, &pose_rotation);
    quat_to_yaw_pitch_roll(pose_rotation, &test_yaw, &test_pitch, &test_roll);
    authored_rotation_follow_ok = authored_rotation_follow_ok &&
        fabsf(test_yaw - 0.2f) < 0.001f && fabsf(test_pitch + 0.1f) < 0.001f &&
        fabsf(test_roll - (0.3f + 0.52359878f)) < 0.001f;
    BOOL pose_composition_ok = pose_translation_ok && pose_world_scale_ok &&
        pose_roll_ok && pose_environment_origin_ok && pose_yaw_ok &&
        pose_tilt_isolation_ok && authored_rotation_follow_ok;
    BOOL parsed_true = FALSE, parsed_false = TRUE, malformed_value = TRUE, duplicate_value = TRUE;
    BOOL pose_config_parse_ok =
        parse_json_bool_setting("{\"umaVrLiveCameraFollow\":true}",
            "umaVrLiveCameraFollow", &parsed_true) && parsed_true &&
        parse_json_bool_setting("{ \"umaVrLiveCameraFollow\" : false }",
            "umaVrLiveCameraFollow", &parsed_false) && !parsed_false &&
        !parse_json_bool_setting("{\"umaVrLiveCameraFollow\":1}",
            "umaVrLiveCameraFollow", &malformed_value) &&
        !parse_json_bool_setting("{\"umaVrLiveCameraFollow\":true,\"umaVrLiveCameraFollow\":false}",
            "umaVrLiveCameraFollow", &duplicate_value);
    float parsed_scale = 0.0f, malformed_scale = 0.0f, duplicate_scale = 0.0f;
    BOOL render_scale_config_parse_ok =
        parse_json_float_setting("{\"umaVrEyeRenderScale\":0.75}",
            "umaVrEyeRenderScale", &parsed_scale) && fabsf(parsed_scale - 0.75f) < 0.0001f &&
        !parse_json_float_setting("{\"umaVrEyeRenderScale\":\"0.75\"}",
            "umaVrEyeRenderScale", &malformed_scale) &&
        !parse_json_float_setting("{\"umaVrEyeRenderScale\":0.75,\"umaVrEyeRenderScale\":1.0}",
            "umaVrEyeRenderScale", &duplicate_scale);
    BOOL render_scale_policy_ok =
        (uint32_t)floorf(2496.0f * 0.75f + 0.5f) == 1872u &&
        (uint32_t)floorf(2688.0f * 0.75f + 0.5f) == 2016u &&
        EYE_RENDER_SCALE_MIN == 0.50f && EYE_RENDER_SCALE_MAX == 1.50f &&
        EYE_RENDER_SCALE_EXPENSIVE == 1.25f;
    float parsed_world_scale = 0.0f;
    BOOL world_scale_config_parse_ok =
        parse_json_float_setting("{\"worldScale\":8.0}",
            "worldScale", &parsed_world_scale) &&
        fabsf(parsed_world_scale - 8.0f) < 0.0001f &&
        WORLD_SCALE_DEFAULT == 1.0f && WORLD_SCALE_MIN == 0.55f;
    float proj_m[16], view_m[16], vp_m[16];
    build_projection(&fov, 0.05f, 100.0f, proj_m);
    build_view(&identity, view_m);
    mat_mul(vp_m, proj_m, view_m);
    {
        char dbgvs[8] = {0};
        if (GetEnvironmentVariableA("UMAVR_PANEL_DEBUG_VS", dbgvs, sizeof(dbgvs)) > 0 && dbgvs[0] == '2') {
            float simple[16] = {0.5f,0,0,0, 0,0.5f,0,0, 0,0,0.5f,0, 0,0,1,1};
            memcpy(vp_m, simple, sizeof(simple));
        }
    }
    D3D11_MAPPED_SUBRESOURCE mapped;
    BOOL drawn = FALSE;
    BOOL km_held = SUCCEEDED(IDXGIKeyedMutex_AcquireSync(g_shared_km_probe, 1, 500));
    if (km_held) {
        ID3D11DeviceContext_CopyResource(probe_context,
            (ID3D11Resource*)g_panel_tex, (ID3D11Resource*)g_local_shared_tex);
        IDXGIKeyedMutex_ReleaseSync(g_shared_km_probe, 0);
        InterlockedExchange(&g_panel_frame_ready, 1);
    }
    if (km_held && geometry_ok && SUCCEEDED(ID3D11DeviceContext_Map(probe_context, (ID3D11Resource*)g_cb,
            0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, vp_m, sizeof(vp_m));
        ID3D11DeviceContext_Unmap(probe_context, (ID3D11Resource*)g_cb, 0);
        UINT stride = 20, offset = 0;
        ID3D11DeviceContext_IASetPrimitiveTopology(probe_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        ID3D11DeviceContext_IASetVertexBuffers(probe_context, 0, 1, &g_vb, &stride, &offset);
        ID3D11DeviceContext_IASetInputLayout(probe_context, g_il);
        ID3D11DeviceContext_VSSetShader(probe_context, g_vs, NULL, 0);
        ID3D11DeviceContext_PSSetShader(probe_context, g_panel_ps, NULL, 0);
        ID3D11DeviceContext_VSSetConstantBuffers(probe_context, 0, 1, &g_cb);
        ID3D11DeviceContext_PSSetSamplers(probe_context, 0, 1, &g_sampler);
        ID3D11DeviceContext_PSSetShaderResources(probe_context, 0, 1, &g_panel_srv);
        D3D11_VIEWPORT vp = {0, 0, 64, 64, 0, 1};
        ID3D11DeviceContext_RSSetViewports(probe_context, 1, &vp);
        float bg[4] = {0, 0, 1, 1};
        ID3D11DeviceContext_ClearRenderTargetView(probe_context, rtv, bg);
        ID3D11DeviceContext_OMSetRenderTargets(probe_context, 1, &rtv, NULL);
        ID3D11DeviceContext_Draw(probe_context, 4, 0);
        drawn = TRUE;
    }

    BOOL pass = FALSE;
    float center_r = -1, corner_b = -1, center_g = -1;
    if (drawn) {
        ID3D11DeviceContext_Flush(probe_context);
        D3D11_TEXTURE2D_DESC rtd; ZeroMemory(&rtd, sizeof(rtd));
        rtd.Width = 64; rtd.Height = 64; rtd.MipLevels = 1; rtd.ArraySize = 1;
        rtd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; rtd.SampleDesc.Count = 1;
        rtd.Usage = D3D11_USAGE_STAGING; rtd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ID3D11Texture2D* staging = NULL;
        if (SUCCEEDED(ID3D11Device_CreateTexture2D(probe_device, &rtd, NULL, &staging))) {
            ID3D11DeviceContext_CopyResource(probe_context, (ID3D11Resource*)staging, (ID3D11Resource*)rt_tex);
            D3D11_MAPPED_SUBRESOURCE m;
            if (SUCCEEDED(ID3D11DeviceContext_Map(probe_context, (ID3D11Resource*)staging,
                    0, D3D11_MAP_READ, 0, &m))) {
                const unsigned char* px = (const unsigned char*)m.pData;
                const unsigned char* c = px + (32 * (size_t)m.RowPitch + 32 * 4);
                const unsigned char* k = px + (2 * (size_t)m.RowPitch + 2 * 4);
                center_g = c[1] / 255.0f; center_r = c[0] / 255.0f; corner_b = k[2] / 255.0f;
                BOOL color_eotf_ok = c[0] >= 50 && c[0] <= 60 &&
                    c[1] >= 50 && c[1] <= 60 && c[2] >= 50 && c[2] <= 60;
                pass = aspect_ok && geometry_scale_ok && controller_pointer_math_ok &&
                    aux_panel_math_ok && cursor_geometry_ok &&
                    asymmetric_fov_ok && unity_projection_ok && pose_composition_ok &&
                    pose_config_parse_ok && render_scale_config_parse_ok &&
                    render_scale_policy_ok && world_scale_config_parse_ok &&
                    marker_ps && sphere_ps && sphere_vs && navigation_math_ok &&
                    color_eotf_ok && k[2] > 200 && k[0] < 80;
                ID3D11DeviceContext_Unmap(probe_context, (ID3D11Resource*)staging, 0);
            }
            staging->lpVtbl->Release(staging);
        }
    }
    telemetry_snprintf(detail, sizeof(detail),
        "\"selftest\":\"%s\",\"center_rgb\":[%.2f,%.2f,0.00],\"corner_blue\":%.2f,"
        "\"drawn\":%d,\"km_held\":%d,\"geometry_ok\":%d,\"aspect_ok\":%d,"
        "\"geometry_scale_ok\":%d,\"controller_pointer_math_ok\":%d,"
        "\"aux_panel_math_ok\":%d,\"cursor_geometry_ok\":%d,"
        "\"asymmetric_fov_ok\":%d,\"unity_projection_ok\":%d,\"pose_composition_ok\":%d,"
        "\"pose_config_parse_ok\":%d,\"render_scale_config_parse_ok\":%d,"
        "\"render_scale_policy_ok\":%d,\"world_scale_config_parse_ok\":%d,"
        "\"navigation_math_ok\":%d,"
        "\"color_eotf_ok\":%d,"
        "\"panel_width_m\":%.3f,\"panel_height_m\":%.3f",
        pass ? "pass" : "fail", center_r, center_g, corner_b, drawn ? 1 : 0,
        km_held ? 1 : 0, geometry_ok ? 1 : 0, aspect_ok ? 1 : 0,
        geometry_scale_ok ? 1 : 0, controller_pointer_math_ok ? 1 : 0,
        aux_panel_math_ok ? 1 : 0, cursor_geometry_ok ? 1 : 0,
        asymmetric_fov_ok ? 1 : 0,
        unity_projection_ok ? 1 : 0, pose_composition_ok ? 1 : 0,
        pose_config_parse_ok ? 1 : 0, render_scale_config_parse_ok ? 1 : 0,
        render_scale_policy_ok ? 1 : 0, world_scale_config_parse_ok ? 1 : 0,
        navigation_math_ok ? 1 : 0,
        pass && center_r >= 0.19f && center_r <= 0.24f ? 1 : 0, panel_w, panel_h);
    write_record(PROBE_ID, "selftest", detail);

    if (rtv) ID3D11RenderTargetView_Release(rtv);
    if (rt_tex) ID3D11Texture2D_Release(rt_tex);
    ID3D11Texture2D_Release(src_tex_a);
    km->lpVtbl->Release(km);
    release_probe_pipeline();
    if (g_shared_km_probe) { IDXGIKeyedMutex_Release(g_shared_km_probe); g_shared_km_probe = NULL; }
    if (g_local_shared_tex) { ID3D11Texture2D_Release(g_local_shared_tex); g_local_shared_tex = NULL; }
    ctx_a->lpVtbl->Release(ctx_a);
    dev_a->lpVtbl->Release(dev_a);
    probe_context->lpVtbl->Release(probe_context); probe_context = NULL;
    probe_device->lpVtbl->Release(probe_device); probe_device = NULL;
    return 0;
}

/* ---------------- DLL entry ---------------- */

static BOOL parse_json_bool_setting(const char* text, const char* key, BOOL* value) {
    if (!text || !key || !value) return FALSE;
    char needle[96];
    int needle_length = snprintf(needle, sizeof(needle), "\"%s\"", key);
    if (needle_length <= 2 || (size_t)needle_length >= sizeof(needle)) return FALSE;
    const char* match = strstr(text, needle);
    if (!match || strstr(match + needle_length, needle)) return FALSE;
    const char* cursor = match + needle_length;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    if (*cursor++ != ':') return FALSE;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    if (strncmp(cursor, "true", 4) == 0) {
        cursor += 4;
        *value = TRUE;
    } else if (strncmp(cursor, "false", 5) == 0) {
        cursor += 5;
        *value = FALSE;
    } else {
        return FALSE;
    }
    return *cursor == 0 || *cursor == ',' || *cursor == '}' || *cursor == ' ' ||
        *cursor == '\t' || *cursor == '\r' || *cursor == '\n';
}

static BOOL parse_json_float_setting(const char* text, const char* key, float* value) {
    if (!text || !key || !value) return FALSE;
    char needle[96];
    int needle_length = snprintf(needle, sizeof(needle), "\"%s\"", key);
    if (needle_length <= 2 || (size_t)needle_length >= sizeof(needle)) return FALSE;
    const char* match = strstr(text, needle);
    if (!match || strstr(match + needle_length, needle)) return FALSE;
    const char* cursor = match + needle_length;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    if (*cursor++ != ':') return FALSE;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') ++cursor;
    char* end = NULL;
    float parsed = strtof(cursor, &end);
    if (end == cursor || !isfinite(parsed)) return FALSE;
    if (!(*end == 0 || *end == ',' || *end == '}' || *end == ' ' ||
            *end == '\t' || *end == '\r' || *end == '\n')) return FALSE;
    *value = parsed;
    return TRUE;
}

static BOOL apply_runtime_settings_text(const char* text, BOOL versioned_settings) {
    if (!text) return FALSE;
    if (versioned_settings) {
        float schema_version = 0.0f;
        if (!parse_json_float_setting(text, "schemaVersion", &schema_version) ||
                (fabsf(schema_version - 1.0f) > 0.0001f &&
                 fabsf(schema_version - 2.0f) > 0.0001f &&
                 fabsf(schema_version - 3.0f) > 0.0001f &&
                 fabsf(schema_version - 4.0f) > 0.0001f &&
                  fabsf(schema_version - 5.0f) > 0.0001f &&
                  fabsf(schema_version - 6.0f) > 0.0001f &&
                  fabsf(schema_version - 7.0f) > 0.0001f &&
                  fabsf(schema_version - 8.0f) > 0.0001f &&
                  fabsf(schema_version - 9.0f) > 0.0001f &&
                  fabsf(schema_version - 10.0f) > 0.0001f)) {
            return FALSE;
        }
        if (schema_version < 10.0f) nav_locomotion_scale_compensation_enabled = FALSE;
    }

    BOOL loaded = FALSE;
    BOOL value = FALSE;
    const char* camera_key = versioned_settings ? "liveCameraFollow" : "umaVrLiveCameraFollow";
    if (parse_json_bool_setting(text, camera_key, &value)) {
        live_camera_follow = value;
        live_camera_follow_setting_loaded = TRUE;
        loaded = TRUE;
    }
    float scale = 1.0f;
    const char* scale_key = versioned_settings ? "eyeRenderScale" : "umaVrEyeRenderScale";
    if (parse_json_float_setting(text, scale_key, &scale)) {
        eye_render_scale = scale;
        eye_render_scale_setting_loaded = TRUE;
        loaded = TRUE;
    }
    if (versioned_settings) {
        float requested_world_scale = WORLD_SCALE_DEFAULT;
        if (parse_json_float_setting(text, "worldScale", &requested_world_scale) &&
                requested_world_scale >= WORLD_SCALE_MIN) {
            world_scale = requested_world_scale;
            world_scale_setting_loaded = TRUE;
            loaded = TRUE;
        }
        if (parse_json_bool_setting(text, "locomotionEnabled", &value)) {
            nav_locomotion_enabled = value;
            loaded = TRUE;
        }
        float speed = NAV_LOCOMOTION_SPEED_DEFAULT_MPS;
        if (parse_json_float_setting(text, "locomotionSpeed", &speed) && speed >= 0.0f) {
            nav_locomotion_speed_mps = speed;
            loaded = TRUE;
        }
        if (parse_json_bool_setting(text, "locomotionScaleCompensationEnabled", &value)) {
            nav_locomotion_scale_compensation_enabled = value;
            loaded = TRUE;
        }
        if (parse_json_bool_setting(text, "snapTurnEnabled", &value)) {
            nav_snap_turn_enabled = value;
            loaded = TRUE;
        }
        float degrees = 30.0f;
        if (parse_json_float_setting(text, "snapTurnAngleDegrees", &degrees) &&
                degrees >= 15.0f && degrees <= 90.0f) {
            nav_snap_angle_rad = degrees * 0.01745329252f;
            loaded = TRUE;
        }
        float zone;
        if(parse_json_float_setting(text,"movementDeadzone",&zone) && zone>=0 && zone<=.9f)move_deadzone=zone;
        if(parse_json_bool_setting(text,"contactEnabled",&value))contact_enabled=value;
        if(parse_json_bool_setting(text,"contactMotion",&value))contact_motion=value;
        if(parse_json_float_setting(text,"contactStrength",&zone) && zone>=0 && zone<=100)contact_strength=zone;
        if(parse_json_float_setting(text,"contactTilt",&zone) && zone>=0 && zone<=100)contact_tilt=zone;
        if(parse_json_float_setting(text,"contactDuration",&zone) && zone>=0.5 && zone<=2)contact_duration=zone;
        if(parse_json_float_setting(text,"contactReach",&zone) && zone>=50 && zone<=200)contact_reach=zone;
        if(parse_json_float_setting(text,"contactHandRadiusMm",&zone) && zone>=10 && zone<=50)hand_radius_mm=zone;
        if(parse_json_float_setting(text,"contactHoldMs",&zone) && zone>=0 && zone<=250)contact_hold_ms=zone;
        if(parse_json_bool_setting(text,"contactHaptics",&value))contact_haptics=value;
        if(parse_json_float_setting(text,"hapticStrength",&zone) && zone>=0 && zone<=100)haptic_strength=zone;
        if(parse_json_float_setting(text,"hapticDuration",&zone) && zone>=0 && zone<=250)haptic_duration=zone;
        if(parse_json_bool_setting(text,"swapMovementSticks",&value))swap_move_sticks=value;
        if(parse_json_bool_setting(text,"grabTracksPanelHand",&value))grab_panel_hand=value;
        if(parse_json_bool_setting(text,"grabUsesTrigger",&value))grab_with_trigger=value;
        if (parse_json_bool_setting(text, "showControllerHands", &value)) show_hands=value;
        if (parse_json_bool_setting(text, "showContactLocations", &value)) show_contact=value;
        if (parse_json_bool_setting(text, "navigationHandsSwapped", &value)) {
            controller_hands_swapped = value;
            loaded = TRUE;
        }
        LONG disabled_mask;
        if(read_home_vfx_mask(text,&disabled_mask))InterlockedExchange(&home_vfx_disabled,disabled_mask);
        if(parse_json_bool_setting(text,"homePostProcessingEnabled",&value))InterlockedExchange(&home_vfx_allowed,value);
        if (parse_json_bool_setting(text, "postProcessingEnabled", &value)) {
            post_processing_enabled = value;
            loaded = TRUE;
        }
        if (parse_json_bool_setting(text, "blurEnabled", &value)) {
            blur_effect_enabled = value;
            loaded = TRUE;
        }
        if (parse_json_bool_setting(text, "depthOfFieldEnabled", &value)) {
            depth_of_field_enabled = value;
            loaded = TRUE;
        }
        if (parse_json_bool_setting(text, "diffusionEnabled", &value)) {
            diffusion_effect_enabled = value;
            loaded = TRUE;
        }
        if (parse_json_bool_setting(text, "bloomEnabled", &value)) {
            bloom_effect_enabled = value;
            loaded = TRUE;
        }
        if (parse_json_bool_setting(text, "globalFogEnabled", &value)) {
            global_fog_effect_enabled = value;
            loaded = TRUE;
        }
        if (parse_json_bool_setting(text, "lensDistortionEnabled", &value)) {
            lens_distortion_effect_enabled = value;
            loaded = TRUE;
        }
        if (parse_json_bool_setting(text, "radialBlurEnabled", &value)) {
            radial_blur_effect_enabled = value;
            loaded = TRUE;
        }
#define LOAD_VFX_BOOL(key, target) do { \
            if (parse_json_bool_setting(text, key, &value)) { target = value; loaded = TRUE; } \
        } while (0)
        LOAD_VFX_BOOL("sunShaftsEnabled", sun_shafts_effect_enabled);
        LOAD_VFX_BOOL("indirectLightShaftsEnabled", indirect_light_shafts_effect_enabled);
        LOAD_VFX_BOOL("transmittedLightEnabled", transmitted_light_effect_enabled);
        LOAD_VFX_BOOL("dofDiffusionBloomOverlayEnabled", dof_diffusion_bloom_overlay_enabled);
        LOAD_VFX_BOOL("tiltShiftEnabled", tilt_shift_effect_enabled);
        LOAD_VFX_BOOL("fluctuationEnabled", fluctuation_effect_enabled);
        LOAD_VFX_BOOL("chromaticAberrationEnabled", chromatic_aberration_effect_enabled);
        LOAD_VFX_BOOL("toneCurveEnabled", tone_curve_effect_enabled);
        LOAD_VFX_BOOL("exposureEnabled", exposure_effect_enabled);
        LOAD_VFX_BOOL("colorCorrectionEnabled", color_correction_effect_enabled);
        LOAD_VFX_BOOL("colorGradingEnabled", color_grading_effect_enabled);
        LOAD_VFX_BOOL("bgBlurEnabled", bg_blur_effect_enabled);
        LOAD_VFX_BOOL("vortexEnabled", vortex_effect_enabled);
        LOAD_VFX_BOOL("filmRollEnabled", film_roll_effect_enabled);
        LOAD_VFX_BOOL("hatchingEnabled", hatching_effect_enabled);
        LOAD_VFX_BOOL("letterBoxEnabled", letter_box_effect_enabled);
        LOAD_VFX_BOOL("rainSplashEnabled", rain_splash_effect_enabled);
#undef LOAD_VFX_BOOL
    }
    return loaded;
}

static BOOL load_runtime_settings_file(const WCHAR* path, BOOL versioned_settings) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    LARGE_INTEGER size;
    BOOL loaded = FALSE;
    if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart <= 1024 * 1024) {
        DWORD bytes = (DWORD)size.QuadPart;
        char* text = (char*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)bytes + 1);
        DWORD read = 0;
        if (text && ReadFile(file, text, bytes, &read, NULL) && read == bytes) {
            text[bytes] = 0;
            loaded = apply_runtime_settings_text(text, versioned_settings);
        }
        if (text) HeapFree(GetProcessHeap(), 0, text);
    }
    CloseHandle(file);
    return loaded;
}

static BOOL load_runtime_settings(void) {
    WCHAR path[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (!length || length >= MAX_PATH) return FALSE;
    DWORD slash = length;
    while (slash > 0 && path[slash - 1] != L'\\' && path[slash - 1] != L'/') --slash;

    const WCHAR settings_name[] = L"vrmod\\config\\settings.json";
    if (slash + (DWORD)(sizeof(settings_name) / sizeof(settings_name[0])) <= MAX_PATH) {
        memcpy(path + slash, settings_name, sizeof(settings_name));
        if (load_runtime_settings_file(path, TRUE)) return TRUE;
    }

    const WCHAR config_name[] = L"config.json";
    if (slash + (DWORD)(sizeof(config_name) / sizeof(config_name[0])) > MAX_PATH) return FALSE;
    memcpy(path + slash, config_name, sizeof(config_name));
    return load_runtime_settings_file(path, FALSE);
}

static BOOL read_contact_consent(void) {
    WCHAR path[MAX_PATH];
    const WCHAR suffix[]=L"\\UmaVR\\contact-consent.txt";
    DWORD len=GetEnvironmentVariableW(L"LOCALAPPDATA",path,MAX_PATH);
    if(!len || len>MAX_PATH-(sizeof suffix/sizeof *suffix))return FALSE;
    memcpy(path+len,suffix,sizeof suffix);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return FALSE;
    const char token[]="uma-contact-v1";
    char data[sizeof token]; DWORD count=0;
    BOOL ok=ReadFile(file,data,sizeof data,&count,NULL) && count==sizeof token-1 &&
        !memcmp(data,token,sizeof token-1);
    CloseHandle(file);
    return ok;
}

static DWORD WINAPI shader_init_worker(LPVOID unused) {
    (void)unused;
    telemetry_init();
    write_record(PROBE_ID, "startup", "\"status\":\"loaded\",\"mode\":\"panel_fail_open_plus_live_stereo\"");
    AddVectoredExceptionHandler(1, panel_veh);
    if (!compile_all_shaders()) {
        write_record(PROBE_ID, "disabled", "\"status\":\"failed\",\"stage\":\"shader_precompile\"");
        return 0;
    }
    write_record("CAP-009", "pipe_stage", "\"stage\":\"shaders_precompiled\"");
    contact_consent=read_contact_consent();
    (void)load_runtime_settings();
    saved_world_scale=world_scale;saved_snap_degrees=nav_snap_angle_rad*57.2957795f;
    saved_hand_swap=controller_hands_swapped;saved_start_follow=live_camera_follow;
    LONG mask=0;
    for(unsigned i=0;i<VFX_COUNT;i++)if(live_vfx_options[i].game_value && *live_vfx_options[i].game_value) mask|=1L<<i;
    InterlockedExchange(&live_vfx_mask,mask);
    write_record("POSE-002", "camera_follow_config",
        live_camera_follow ?
            "\"status\":\"observed\",\"context\":\"Live\",\"position_follow\":true,\"rotation_follow\":true,\"authored_roll\":true" :
            (live_camera_follow_setting_loaded ?
                "\"status\":\"observed\",\"context\":\"Live\",\"position_follow\":false,\"rotation_follow\":false" :
                "\"status\":\"defaulted\",\"context\":\"Live\",\"position_follow\":false,\"rotation_follow\":false"));
    {
        char detail[192];
        telemetry_snprintf(detail, sizeof(detail),
            "\"status\":\"%s\",\"world_scale\":%.3f,\"translation_multiplier\":%.3f,"
            "\"coupling\":\"head_translation+locomotion+eye_offset\","
            "\"authored_pose_scaled\":false,\"rotation_scaled\":false",
            world_scale_setting_loaded ? "observed" : "defaulted",
            world_scale, 1.0f / world_scale);
        write_record("POSE-003", "world_scale_config", detail);
    }
    {
        char detail[640];
        float effective_speed = effective_navigation_speed_mps(nav_locomotion_speed_mps,
            world_scale, nav_locomotion_scale_compensation_enabled);
        telemetry_snprintf(detail, sizeof(detail),
            "\"status\":\"observed\",\"locomotion_enabled\":%s," 
            "\"locomotion_speed_mps\":%.3f,\"effective_speed_mps\":%.3f,"
            "\"world_scale_compensation\":%s,\"speed_reference_world_scale\":%.2f,"
            "\"snap_turn_enabled\":%s,"
            "\"snap_angle_degrees\":%.1f,\"locomotion_hand\":\"%s\"," 
            "\"view_turn_hand\":\"%s\",\"hands_swapped\":%s",
            nav_locomotion_enabled ? "true" : "false", nav_locomotion_speed_mps,
            effective_speed,
            nav_locomotion_scale_compensation_enabled ? "true" : "false",
            NAV_LOCOMOTION_SCALE_REFERENCE,
            nav_snap_turn_enabled ? "true" : "false", nav_snap_angle_rad * 57.2957795f,
            controller_hands_swapped ? "left" : "right",
            controller_hands_swapped ? "right" : "left",
            controller_hands_swapped ? "true" : "false");
        write_record("CTRL-006", "navigation_config", detail);
    }
    {
        unsigned enabled_slots =
            (unsigned)blur_effect_enabled + (unsigned)global_fog_effect_enabled +
            (unsigned)sun_shafts_effect_enabled + (unsigned)indirect_light_shafts_effect_enabled +
            (unsigned)transmitted_light_effect_enabled + (unsigned)dof_diffusion_bloom_overlay_enabled +
            (unsigned)tilt_shift_effect_enabled + (unsigned)radial_blur_effect_enabled +
            (unsigned)fluctuation_effect_enabled + (unsigned)lens_distortion_effect_enabled +
            (unsigned)chromatic_aberration_effect_enabled + (unsigned)tone_curve_effect_enabled +
            (unsigned)exposure_effect_enabled + (unsigned)color_correction_effect_enabled +
            (unsigned)color_grading_effect_enabled + (unsigned)bg_blur_effect_enabled +
            (unsigned)vortex_effect_enabled + 1u +
            (unsigned)film_roll_effect_enabled + (unsigned)hatching_effect_enabled +
            (unsigned)letter_box_effect_enabled + (unsigned)rain_splash_effect_enabled;
        char detail[384];
        telemetry_snprintf(detail, sizeof(detail),
            "\"status\":\"observed\",\"post_processing_enabled\":%s,"
            "\"effect_slot_count\":22,\"enabled_slot_count\":%u,"
            "\"dof_enabled\":%s,\"diffusion_enabled\":%s,\"bloom_enabled\":%s,"
            "\"enabled_semantics\":\"preserve_authored\"",
            post_processing_enabled ? "true" : "false",
            enabled_slots,
            depth_of_field_enabled ? "true" : "false",
            diffusion_effect_enabled ? "true" : "false",
            bloom_effect_enabled ? "true" : "false");
        write_record("SETTINGS-008", "vfx_config", detail);
    }
    {
        char st[8] = {0};
        if (GetEnvironmentVariableA("UMAVR_PANEL_SELFTEST", st, sizeof(st)) > 0 && st[0] == '1') {
            HANDLE t = CreateThread(NULL, 0, selftest_worker, NULL, 0, NULL);
            if (t) CloseHandle(t);
            return 0;
        }
    }
    HANDLE t1 = CreateThread(NULL, 0, capture_worker, NULL, 0, NULL);
    if (t1) CloseHandle(t1);
    HANDLE t2 = CreateThread(NULL, 0, xr_worker_with_callback_stop, NULL, 0, NULL);
    if (t2) CloseHandle(t2);
    HANDLE t3 = CreateThread(NULL, 0, unity_adapter_worker, NULL, 0, NULL);
    if (t3) CloseHandle(t3);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        g_module = instance;
        attach_tick = GetTickCount64();
        {
            char flag[8] = {0};
            DWORD got = GetEnvironmentVariableA("UMAVR_PROBE_FAST", flag, sizeof(flag));
            fast_mode = got > 0 && flag[0] == '1';
        }
        HANDLE init_t = CreateThread(NULL, 0, shader_init_worker, NULL, 0, NULL);
        if (init_t) CloseHandle(init_t);
        (void)instance;
    }
    return TRUE;
}

