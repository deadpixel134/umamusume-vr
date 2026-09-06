param([string]$ProbeDll = "", [string]$ZigPath = "")
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$workspace = (Resolve-Path (Join-Path $root "..\..")).Path
if ([string]::IsNullOrWhiteSpace($ProbeDll)) { $ProbeDll = Join-Path $root "package\umavr_immersive.dll" }
$ProbeDll = (Resolve-Path -LiteralPath $ProbeDll).Path
$manifest = Get-Content -LiteralPath (Join-Path (Split-Path $ProbeDll) "BUILD_MANIFEST.json") -Raw | ConvertFrom-Json
if ($manifest.candidate_id -ne "IMMERSIVE-001" -or $manifest.artifact_sha256 -ne (Get-FileHash -Algorithm SHA256 $ProbeDll).Hash) { throw "package manifest/hash mismatch" }
$sourceText = Get-Content -LiteralPath (Join-Path $root "src\immersive.c") -Raw
if ($sourceText.Contains("Internal_CloneSingleWithParent") -or $sourceText.Contains("clone_with_parent")) {
    throw "FAIL-014 regression: runtime whole-owner cloning must remain absent"
}
$snapshotIndex = $sourceText.IndexOf("Il2CppObject* saved_target = camera_get_target(owner)")
$renderIndex = $sourceText.IndexOf("render(owner, mi_camera_render);", $snapshotIndex)
$restoreIndex = $sourceText.IndexOf("set_target(owner, saved_target, mi_camera_set_target);", $renderIndex)
$generationRenderIndex = $sourceText.IndexOf("if (!render_authored_eye_pair(owner)) goto fail;")
$generationIndex = $sourceText.IndexOf("InterlockedIncrement(&source_generation)", $generationRenderIndex)
if ($snapshotIndex -lt 0 -or $renderIndex -le $snapshotIndex -or $restoreIndex -le $renderIndex -or
    $generationRenderIndex -lt 0 -or $generationIndex -le $generationRenderIndex -or
    -not $sourceText.Contains("camera_get_near_clip(owner)") -or
    -not $sourceText.Contains("camera_get_far_clip(owner)") -or
    -not $sourceText.Contains("build_unity_eye_projection(&optics.fov[eye], saved_near_clip,") -or
    -not $sourceText.Contains("const Vec3* base_position = live_camera_follow ? &saved_position : &authored_pose_anchor_position") -or
    -not $sourceText.Contains("const Quat* base_rotation = live_camera_follow ? &saved_rotation : &authored_pose_anchor_rotation") -or
    -not $sourceText.Contains("compose_unity_eye_pose(base_position, base_rotation, live_camera_follow,") -or
    -not $sourceText.Contains("&optics, world_scale, eye") -or
    -not $sourceText.Contains("float inverse_world_scale = 1.0f / perceived_world_scale") -or
    -not $sourceText.Contains("unity_delta.x *= inverse_world_scale") -or
    -not $sourceText.Contains("optics->half_ipd_m : -optics->half_ipd_m) * inverse_world_scale") -or
    -not $sourceText.Contains("authored_pose_anchor_valid = FALSE") -or
    -not $sourceText.Contains('"current_authored_world_pose_plus_physical_hmd"') -or
    -not $sourceText.Contains('"entry_anchor_plus_physical_hmd"') -or
    -not $sourceText.Contains('"tracking_origin":"first_immersive_entry"') -or
    -not $sourceText.Contains("if (!immersive_entry_origin_committed && eye_optics.origin_valid)") -or
    -not $sourceText.Contains("commit_immersive_environment_origin(&eye_optics)") -or
    -not $sourceText.Contains("optics->origin_center.position = optics->current_center.position") -or
    -not $sourceText.Contains("optics->origin_center.orientation = (XrQuaternionf){0.0f, 0.0f, 0.0f, 1.0f}") -or
    -not $sourceText.Contains("immersive_entry_origin_committed = FALSE") -or
    -not $sourceText.Contains("rsi.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW") -or
    -not $sourceText.Contains("rsi.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL") -or
    -not $sourceText.Contains("tracking_li.space = g_tracking_space") -or
    $sourceText -match "(?m)^\s*li\.space = g_tracking_space" -or
    -not $sourceText.Contains("physicalRollDelta") -and -not $sourceText.Contains("physical_roll") -or
    -not $sourceText.Contains('p_resolve_icall("UnityEngine.Transform::get_position_Injected(UnityEngine.Vector3&)")') -or
    -not $sourceText.Contains('p_resolve_icall("UnityEngine.Transform::get_rotation_Injected(UnityEngine.Quaternion&)")') -or
    -not $sourceText.Contains('p_resolve_icall("UnityEngine.Transform::set_position_Injected(UnityEngine.Vector3&)")') -or
    -not $sourceText.Contains('p_resolve_icall("UnityEngine.Transform::set_rotation_Injected(UnityEngine.Quaternion&)")') -or
    -not $sourceText.Contains("transform_get_position(transform, &saved_position)") -or
    -not $sourceText.Contains("transform_get_rotation(transform, &saved_rotation)") -or
    -not $sourceText.Contains("transform_set_position(transform, &eye_position)") -or
    -not $sourceText.Contains("transform_set_rotation(transform, &eye_rotation)") -or
    -not $sourceText.Contains("transform_set_position(transform, &saved_position)") -or
    -not $sourceText.Contains("transform_set_rotation(transform, &saved_rotation)") -or
    -not $sourceText.Contains("transform_get_position(owner_transform, &authored_pose_anchor_position)") -or
    -not $sourceText.Contains("transform_get_rotation(owner_transform, &authored_pose_anchor_rotation)") -or
    -not $sourceText.Contains("InterlockedCompareExchange(&eye_render_in_progress, 0, 0) == 0")) {
    throw "authored render/pose-space ordering and restoration regression"
}
$eyeShareStart = $sourceText.IndexOf("static BOOL ensure_shared_eye_pair")
$eyeShareEnd = $sourceText.IndexOf("static BOOL ensure_shared_flat_source", $eyeShareStart)
$eyeShareText = if ($eyeShareStart -ge 0 -and $eyeShareEnd -gt $eyeShareStart) {
    $sourceText.Substring($eyeShareStart, $eyeShareEnd - $eyeShareStart)
} else { "" }
if (-not $sourceText.Contains('il2cpp_string_equals_ascii(scene_name, "Live")') -or
    -not $sourceText.Contains('component_is(c, "Gallop.Live", "LiveTimelineCamera")') -or
    -not $sourceText.Contains('component_is(c, "Gallop", "LiveImageEffect")') -or
    -not $sourceText.Contains('component_is(c, "Gallop.Live", "MultiCameraFinalComposite")') -or
    -not $sourceText.Contains("AUTHORED_EYE_LIVE_REQUIRED") -or
    -not $sourceText.Contains('\"owner_profiles\":\"Live\"') -or
    -not $sourceText.Contains('write_disabled("shared_eye_pair_reuse_contract"') -or
    [string]::IsNullOrWhiteSpace($eyeShareText) -or
    $eyeShareText.Contains("get_native_texture") -or
    $sourceText.Contains('il2cpp_string_equals_ascii(scene_name, "Race")') -or
    $sourceText.Contains('component_is(components->vector[i], "Gallop", "RaceImageEffect")') -or
    $sourceText.Contains('il2cpp_string_equals_ascii(scene_name, "Story")') -or
    $sourceText.Contains('component_is(components->vector[i], "Gallop", "StoryImageEffect")') -or
    $sourceText.Contains('il2cpp_string_equals_ascii(scene_name, "LiveTheater")')) {
    throw "accepted Live owner/fail-open regression"
}
$homeSceneStart = $sourceText.IndexOf('BOOL home_scene = il2cpp_string_equals_ascii(scene_name, "Home");')
$homeSceneEnd = $sourceText.IndexOf('if (!live_scene && !home_scene)', $homeSceneStart)
$homeSceneText = if ($homeSceneStart -ge 0 -and $homeSceneEnd -gt $homeSceneStart) {
    $sourceText.Substring($homeSceneStart, $homeSceneEnd - $homeSceneStart)
} else { "" }
if ([string]::IsNullOrWhiteSpace($homeSceneText) -or
    -not $homeSceneText.Contains('if (home_scene)') -or
    -not $homeSceneText.Contains('if (current_authored_owner) destroy_eye_unity_objects();') -or
    -not $sourceText.Contains('BOOL home_contract = FALSE;') -or
    -not $sourceText.Contains('FAIL-036: the metadata-selected OnRender native pointer is shared') -or
    $sourceText.Contains('is_current_home_render_owner') -or
    $sourceText.Contains('home_cached_camera_field') -or
    $sourceText.Contains('effect->klass != home_image_effect_class') -or
    $sourceText.Contains('STEREO-010-OBS-001') -or
    $sourceText.Contains('home_on_render_calls') -or
    -not $sourceText.Contains('source_serial != game_published_source_serial') -or
    -not $sourceText.Contains('InterlockedCompareExchange(&eye_pair_capture_ready, 0, 0)')) {
    throw "Home PANEL fallback / rejected shared-hook isolation regression"
}
if ($sourceText.Contains("eye_generation_gpu_ready") -or
    $sourceText.Contains("eye_copy_ready_query") -or
    $sourceText.Contains("D3D11_ASYNC_GETDATA_DONOTFLUSH")) {
    throw "rejected Race GPU-fence route must remain absent"
}
if (-not $sourceText.Contains('APPLY_ZERO(blur_effect_enabled, "BlurOptimized", "IsEnable", 1, 2)') -or
    -not $sourceText.Contains('APPLY_ZERO(FALSE, "GlobalFog", "IsDistanceFog", 1, 2)') -or
    -not $sourceText.Contains('APPLY_ZERO(FALSE, "GlobalFog", "IsHeightFog", 1, 2)') -or
    -not $sourceText.Contains('APPLY_ZERO(lens_distortion_effect_enabled, "LensDistortion", "Intensity", 4, 12)') -or
    -not $sourceText.Contains('APPLY_ZERO(radial_blur_effect_enabled, "RadialBlur", "RadialBlurPower", 4, 12)') -or
    -not $sourceText.Contains('APPLY_ZERO(tilt_shift_effect_enabled, "TiltShift", "MaxBlurSize", 4, 12)') -or
    $sourceText.Contains('AuraParamDic') -or
    $sourceText.Contains('auraEnabled') -or
    -not $sourceText.Contains('APPLY_ZERO(hatching_effect_enabled, "Hatching", "BlendAlpha", 4, 12)') -or
    $sourceText.Contains('vfx_effect_contract') -or
    -not $sourceText.Contains('find_vfx_field(current->klass, "IsEnable")') -or
    -not $sourceText.Contains('VfxMasterOverride vfx_master_override') -or
    -not $sourceText.Contains('component_mask == AUTHORED_EYE_CAMERA_DATA') -or
    -not $sourceText.Contains('apply_vfx_overrides(camera_data)') -or
    -not $sourceText.Contains('find_vfx_field(camera_data->klass, "ImageEffectParameter")') -or
    -not $sourceText.Contains('"il2cpp_class_get_parent"') -or
    -not $sourceText.Contains('owner = il2cpp_class_get_parent ? il2cpp_class_get_parent(owner) : NULL;') -or
    -not $sourceText.Contains('"il2cpp_field_get_value"') -or
    -not $sourceText.Contains('"il2cpp_field_set_value"') -or
    -not $sourceText.Contains('"il2cpp_field_get_value_object"') -or
    -not $sourceText.Contains('"il2cpp_field_get_type"') -or
    -not $sourceText.Contains('"il2cpp_type_get_type"') -or
    -not $sourceText.Contains('"il2cpp_object_unbox"') -or
    -not $sourceText.Contains('original_objects_mutated\":false') -or
    -not $sourceText.Contains('restore_vfx_overrides();') -or
    -not $sourceText.Contains('parse_json_bool_setting(text, "postProcessingEnabled"') -or
    -not $sourceText.Contains('parse_json_bool_setting(text, "blurEnabled"') -or
    -not $sourceText.Contains('parse_json_bool_setting(text, "depthOfFieldEnabled"') -or
    -not $sourceText.Contains('parse_json_bool_setting(text, "diffusionEnabled"') -or
    -not $sourceText.Contains('parse_json_bool_setting(text, "bloomEnabled"') -or
    -not $sourceText.Contains('parse_json_bool_setting(text, "globalFogEnabled"') -or
    -not $sourceText.Contains('parse_json_bool_setting(text, "lensDistortionEnabled"') -or
    -not $sourceText.Contains('parse_json_bool_setting(text, "radialBlurEnabled"') -or
    -not $sourceText.Contains('VfxFieldOverride vfx_overrides[32]') -or
    -not $sourceText.Contains('for (unsigned i = 0; i < 32; ++i)') -or
    $sourceText.Contains("il2cpp_property_set_value")) {
    throw "SETTINGS-008 proved VFX master/individual snapshot-write-restore contract regression"
}
if (-not $sourceText.Contains('FieldInfo* effect_field = find_vfx_field(current_parameter->klass, effect_name);') -or
    -not $sourceText.Contains('il2cpp_field_get_value_object(effect_field, current_parameter)') -or
    -not $sourceText.Contains('il2cpp_field_set_value(boxed, resolved->value_field, (void*)value);') -or
    -not $sourceText.Contains('void* unboxed = il2cpp_object_unbox(boxed);') -or
    -not $sourceText.Contains('il2cpp_field_set_value(resolved->parameter, resolved->effect_field, unboxed);') -or
    $sourceText.Contains('commit_vfx_parameter') -or
    $sourceText.Contains('outer_write_back') -or
    $sourceText.Contains('_originalImageEffectParameter') -or
    $sourceText.Contains('vfx_boolean_field') -or
    $sourceText.Contains('find_vfx_field(original_parameter->klass, effect_name)') -or
    $sourceText.Contains('vfx_parameter_class_matches') -or
    -not $sourceText.Contains('!il2cpp_class_get_parent ||') -or
    $sourceText.Contains('current_parameter->klass != original_parameter->klass') -or
    $sourceText.Contains('current->klass != original->klass')) {
    throw "SETTINGS-008 inherited outer/current-original class resolution regression"
}

if ($sourceText.Contains('hooked_aura_execute') -or
    $sourceText.Contains('hooked_blur_optimize_execute') -or
    $sourceText.Contains('hooked_bg_blur_execute') -or
    $sourceText.Contains('hooked_radial_blur_execute') -or
    $sourceText.Contains('blur_consumer_hook')) {
    throw "inactive renderer Execute hooks must remain removed"
}
if (-not $sourceText.Contains('"IsDisableDofTemporary", 1, 2, item') -or
    -not $sourceText.Contains('uint8_t disabled = 1;') -or
    -not $sourceText.Contains('write_vfx_value(item, &disabled)') -or
    -not $sourceText.Contains('item->applied = TRUE;')) {
    throw "focus/DoF temporary-disable snapshot-write-restore contract is incomplete"
}
if (-not $sourceText.Contains('APPLY_ZERO(dof_diffusion_bloom_overlay_enabled,') -or
    -not $sourceText.Contains('"DofDiffuionBloomOverlay", "IsEnableOldDof", 1, 2);')) {
    throw "rejected OldDoF mapping must remain rolled back"
}
$restoreVfxIndex = $sourceText.IndexOf("restore_vfx_overrides();", $sourceText.IndexOf("static void invalidate_eye_source_locked"))
$retireVfxIndex = $sourceText.IndexOf("InterlockedExchange(&published_eye_generation", $restoreVfxIndex)
if ($restoreVfxIndex -lt 0 -or $retireVfxIndex -le $restoreVfxIndex) {
    throw "SETTINGS-008 VFX restore must precede generation retirement"
}
$requiredExportsStart = $sourceText.IndexOf('if (!p_domain_get || !p_domain_get_assemblies')
$requiredExportsEnd = $sourceText.IndexOf('write_disabled("unity_il2cpp_exports"', $requiredExportsStart)
$requiredExports = if ($requiredExportsStart -ge 0 -and $requiredExportsEnd -gt $requiredExportsStart) {
    $sourceText.Substring($requiredExportsStart, $requiredExportsEnd - $requiredExportsStart)
} else { "" }
if ([string]::IsNullOrWhiteSpace($requiredExports) -or
    $requiredExports.Contains("il2cpp_class_get_fields") -or
    $requiredExports.Contains("il2cpp_field_get_value_object") -or
    $requiredExports.Contains("il2cpp_method_get_name")) {
    throw "SETTINGS-005 optional metadata exports must not disable the accepted Unity adapter"
}
if (-not $sourceText.Contains("g_panel_ps_src") -or
    -not $sourceText.Contains("PSSetShader(probe_context, g_panel_ps, NULL, 0)") -or
    -not $sourceText.Contains("PSSetShader(probe_context, g_ps, NULL, 0)")) {
    throw "panel color shader / accepted immersive eye-copy separation regression"
}
if (-not $sourceText.Contains("xrCreateActionSet") -or
    -not $sourceText.Contains("xrSuggestInteractionProfileBindings") -or
    -not $sourceText.Contains("xrAttachSessionActionSets") -or
    -not $sourceText.Contains("xrSyncActions") -or
    -not $sourceText.Contains("xrGetActionStateFloat") -or
    -not $sourceText.Contains('"/user/hand/right/input/trigger/value"') -or
    -not $sourceText.Contains("xrLocateSpace(g_aim_space, g_space") -or
    -not $sourceText.Contains("panel_ray_hit_pose(&location.pose") -or
    -not $sourceText.Contains("GetForegroundWindow() == g_game_window") -or
    -not $sourceText.Contains("(trigger_active && trigger_value >= 0.55f) || (primary_active && primary_face_down)") -or
    -not $sourceText.Contains("release_synthetic_input();") -or
    -not $sourceText.Contains("controller_panel_active = TRUE")) {
    throw "CTRL-001 action/pointer/dedup/foreground/release ownership regression"
}
if (-not $sourceText.Contains("g_last_pointer_valid") -or
    -not $sourceText.Contains("move_u = g_last_pointer_u") -or
    -not $sourceText.Contains("move_v = g_last_pointer_v")) {
    throw "CTRL-001 pre-press pointer latch regression"
}
if (-not $sourceText.Contains("XR_ACTION_TYPE_VECTOR2F_INPUT") -or
    -not $sourceText.Contains("xrGetActionStateVector2f") -or
    -not $sourceText.Contains('"/user/hand/right/input/thumbstick"') -or
    -not $sourceText.Contains('"/user/hand/left/input/thumbstick"') -or
    -not $sourceText.Contains("update_controller_navigation(fs.predictedDisplayTime,") -or
    -not $sourceText.Contains("base_yaw + optics->artificial_yaw + physical_yaw") -or
    -not $sourceText.Contains("unity_delta.x += optics->artificial_position.x") -or
    -not $sourceText.Contains('\"basis\":\"final_view_roll_free') -or
    -not $sourceText.Contains('\"snap_angle_degrees\":%.1f') -or
    -not $sourceText.Contains("eye_optics.artificial_position = (Vec3){0.0f, 0.0f, 0.0f}") -or
    -not $sourceText.Contains("g_right_stick_action = g_left_stick_action = 0")) {
    throw "CTRL-005 controller navigation action/final-pose/lifetime ownership regression"
}
if (-not $sourceText.Contains("build_cursor_vertices") -or
    -not $sourceText.Contains("#define CURSOR_HALF_SIZE_FRACTION 0.018f") -or
    -not $sourceText.Contains("2.0f * expected_cursor_half_size") -or
    -not $sourceText.Contains("update_cursor_vertices(&controller_panel_pose") -or
    -not $sourceText.Contains("g_presented_pointer_u = move_u") -or
    -not $sourceText.Contains("g_presented_pointer_v = move_v") -or
    -not $sourceText.Contains("g_presented_pointer_valid = FALSE") -or
    -not $sourceText.Contains("g_presented_pointer_u,") -or
    -not $sourceText.Contains("g_presented_pointer_v" ) -or
    -not $sourceText.Contains('"CTRL-003", "hmd_cursor_composited"') -or
    -not $sourceText.Contains('float radius_sq = dot(centered, centered);') -or
    -not $sourceText.Contains('if (radius_sq > 0.25) discard;') -or
    -not $sourceText.Contains('radius_sq > 0.1024') -or
    -not $sourceText.Contains("ID3D11DeviceContext_PSSetShader(probe_context,") -or
    -not $sourceText.Contains("g_ps_solid, NULL, 0")) {
    throw "CTRL-003/CTRL-004 authoritative mapped-coordinate circular HMD cursor composition regression"
}
if (-not $sourceText.Contains('live_camera_follow ? &saved_position : &authored_pose_anchor_position') -or
    -not $sourceText.Contains('live_camera_follow ? &saved_rotation : &authored_pose_anchor_rotation') -or
    -not $sourceText.Contains('base_rotation, live_camera_follow,') -or
    -not $sourceText.Contains('(authored_rotation_follow ? base_roll : 0.0f) + physical_roll') -or
    $sourceText.Contains('live_entry_roll_bias') -or
    -not $sourceText.Contains('pose_environment_origin_ok') -or
    -not $sourceText.Contains('optics->navigation_base_yaw + optics->artificial_yaw + physical_yaw') -or
    -not $sourceText.Contains('authored_rotation_follow_ok') -or
    -not $sourceText.Contains('physical_orientation_deltas(optics,') -or
    -not $sourceText.Contains('pose_tilt_isolation_ok') -or
    -not $sourceText.Contains('versioned_settings ? "liveCameraFollow" : "umaVrLiveCameraFollow"') -or
    -not $sourceText.Contains('\"rotation_follow\":true') -or
    -not $sourceText.Contains('\"pose_config_parse_ok\":%d')) {
    throw "POSE-002 LIVE full-pose-follow/config/rotation-ownership regression"
}
if (-not $sourceText.Contains('#define WORLD_SCALE_MIN 0.55f') -or
    $sourceText.Contains('#define WORLD_SCALE_MAX') -or
    $sourceText.Contains('requested_world_scale <=') -or
    -not $sourceText.Contains('parse_json_float_setting(text, "worldScale"') -or
    -not $sourceText.Contains('write_record("POSE-003", "world_scale_config", detail)') -or
    -not $sourceText.Contains('\"world_scale_config_parse_ok\":%d')) {
    throw "POSE-003 world-scale bounds/config/coupled pose regression"
}
if (-not $sourceText.Contains('#define EYE_RENDER_SCALE_MIN 0.50f') -or
    -not $sourceText.Contains('#define EYE_RENDER_SCALE_MAX 1.50f') -or
    -not $sourceText.Contains('#define EYE_RENDER_SCALE_EXPENSIVE 1.25f') -or
    -not $sourceText.Contains('versioned_settings ? "eyeRenderScale" : "umaVrEyeRenderScale"') -or
    -not $sourceText.Contains('fallback_reason = "outside_valid_range"') -or
    -not $sourceText.Contains('fallback_reason = "runtime_dimension_limit"') -or
    -not $sourceText.Contains('write_record("PERF-001", "eye_render_resolution", detail)') -or
    -not $sourceText.Contains('\"pc_output_unchanged\":true') -or
    -not $sourceText.Contains('\"render_scale_config_parse_ok\":%d') -or
    -not $sourceText.Contains('\"render_scale_policy_ok\":%d')) {
    throw "PERF-001 eye render scale/config/fallback/telemetry regression"
}
if (-not $sourceText.Contains('L"vrmod\\config\\settings.json"') -or
    -not $sourceText.Contains('parse_json_float_setting(text, "schemaVersion"') -or
    -not $sourceText.Contains('versioned_settings ? "liveCameraFollow" : "umaVrLiveCameraFollow"') -or
    -not $sourceText.Contains('versioned_settings ? "eyeRenderScale" : "umaVrEyeRenderScale"') -or
    -not $sourceText.Contains('if (load_runtime_settings_file(path, TRUE)) return TRUE;') -or
    -not $sourceText.Contains('return load_runtime_settings_file(path, FALSE);')) {
    throw "SETTINGS-001 versioned settings/runtime compatibility regression"
}
if (-not $sourceText.Contains('parse_json_bool_setting(text, "locomotionEnabled"') -or
    -not $sourceText.Contains('parse_json_float_setting(text, "locomotionSpeed"') -or
    -not $sourceText.Contains('parse_json_bool_setting(text, "snapTurnEnabled"') -or
    -not $sourceText.Contains('parse_json_float_setting(text, "snapTurnAngleDegrees"') -or
    -not $sourceText.Contains('parse_json_bool_setting(text, "navigationHandsSwapped"') -or
    -not $sourceText.Contains('controller_hands_swapped ?') -or
    -not $sourceText.Contains('move_action, move_path') -or
    -not $sourceText.Contains('turn_action, turn_path') -or
    -not $sourceText.Contains('nav_locomotion_enabled && dt_seconds > 0.0f') -or
    -not $sourceText.Contains('nav_snap_turn_enabled && fabsf(turn.x)') -or
    -not $sourceText.Contains('write_record("CTRL-006", "navigation_config", detail)')) {
    throw "CTRL-006 navigation hand-swap/settings/runtime contract regression"
}
if (-not $sourceText.Contains('g_pointer_hand_path = controller_hands_swapped ?') -or
    -not $sourceText.Contains('g_panel_hand_path = controller_hands_swapped ?') -or
    -not $sourceText.Contains('"/user/hand/left/input/aim/pose"') -or
    -not $sourceText.Contains('"/user/hand/left/input/trigger/value"') -or
    -not $sourceText.Contains('"/user/hand/left/input/x/click"') -or
    -not $sourceText.Contains('"/user/hand/left/input/y/click"') -or
    -not $sourceText.Contains('"/user/hand/right/input/grip/pose"') -or
    -not $sourceText.Contains('"/user/hand/right/input/squeeze/value"') -or
    -not $sourceText.Contains('"pointer_aim", "Pointer Aim", g_pointer_hand_path') -or
    -not $sourceText.Contains('"panel_grip_pose", "Panel Grip Pose", g_panel_hand_path') -or
    -not $sourceText.Contains('space.subactionPath = g_pointer_hand_path') -or
    -not $sourceText.Contains('space.subactionPath = g_panel_hand_path') -or
    -not $sourceText.Contains('read_float_action_for_path(g_panel_grip_value_action, g_panel_hand_path') -or
    -not $sourceText.Contains('write_record("CTRL-007", "controller_role_actions_created", detail)') -or
    -not $sourceText.Contains('controller_hands_swapped ? "X" : "A"') -or
    -not $sourceText.Contains('controller_hands_swapped ? "Y" : "B"')) {
    throw "CTRL-007 full primary/secondary controller role-swap contract regression"
}
if (-not $sourceText.Contains('"/user/hand/left/input/grip/pose"') -or
    -not $sourceText.Contains('"/user/hand/left/input/squeeze/value"') -or
    -not $sourceText.Contains("g_aux_panel_visible = !g_aux_panel_visible") -or
    -not $sourceText.Contains("grip_down && !g_panel_grip_was_down") -or
    -not $sourceText.Contains("update_aux_panel_vertices(&auxiliary_panel_pose") -or
    -not $sourceText.Contains("(!immersive_drawn || auxiliary_panel_presented)") -or
    -not $sourceText.Contains("if (g_aux_panel_visible) release_synthetic_input();") -or
    -not $sourceText.Contains("controller_panel_auxiliary = auxiliary_panel_presented") -or
    -not $sourceText.Contains('auxiliary_immersive')) {
    throw "CTRL-002 auxiliary panel/grip-edge/presentation/release ownership regression"
}
if (-not $sourceText.Contains("quick.OutputWindow != g_game_window") -or
    -not $sourceText.Contains("mirror_matches_backbuffer(bb)") -or
    -not $sourceText.Contains("InterlockedIncrement(&desktop_mirror_generation)") -or
    -not $sourceText.Contains("xr_desktop_mirror_generation == generation") -or
    -not $sourceText.Contains("xr_desktop_mirror_handle == handle") -or
    -not $sourceText.Contains("desktop_mirror_ready && (!immersive_drawn || auxiliary_panel_presented)") -or
    -not $sourceText.Contains("presented_panel_width = g_panel_width") -or
    -not $sourceText.Contains("presented_panel_height = g_panel_height") -or
    -not $sourceText.Contains('\"source\":\"pc_visible_desktop_mirror\"') -or
    $sourceText.Contains("set_target(owner, flat_render_texture, mi_camera_set_target)") -or
    $sourceText.Contains('\"source\":\"authored_live_monoscopic\"') -or
    $sourceText.Contains('\"source\":\"authored_live_left_eye\"') -or
    -not $sourceText.Contains("&presented_panel_srv")) {
    throw "CTRL-002 generation-safe PC-visible desktop mirror ownership regression"
}
$pe = & (Join-Path $workspace "vrmod\bootstrap_probe\tests\Inspect-Pe.ps1") -Path $ProbeDll
if ($pe.machine -ne "0x8664" -or $pe.pe_magic -ne "0x020B" -or $pe.entry_point_rva -eq "0x00000000") { throw "PE identity failed" }
$flags = [Convert]::ToUInt16($pe.dll_characteristics.Substring(2),16)
if (($flags -band 0x0160) -ne 0x0160) { throw "ASLR/NX flags missing" }
if ([string]::IsNullOrWhiteSpace($ZigPath)) { $ZigPath = Join-Path $workspace ".tmp\zig-0.16.0\zig-x86_64-windows-0.16.0\zig.exe" }
$zig = (Resolve-Path -LiteralPath $ZigPath).Path
$testRoot = Join-Path $workspace (".tmp\immersive-tests\" + [Guid]::NewGuid().ToString("N")); New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$cache = Join-Path $workspace ".tmp\zig-cache\immersive-tests"; New-Item -ItemType Directory -Path "$cache\global","$cache\local" -Force | Out-Null
$env:ZIG_GLOBAL_CACHE_DIR = "$cache\global"; $env:ZIG_LOCAL_CACHE_DIR = "$cache\local"
$fakeGame = Join-Path $testRoot "immersive_fake_game.exe"
& $zig cc -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror (Join-Path $PSScriptRoot "fake_game.c") -ld3d11 -ldxgi -luser32 -o $fakeGame
if ($LASTEXITCODE) { throw "fake game build failed" }

function Read-Records([string]$LocalAppData) {
    @(Get-Content -LiteralPath (Join-Path $LocalAppData "UmaVR\immersive-001.log") | ForEach-Object { $_ | ConvertFrom-Json })
}

# Scenario A: deterministic loader-missing override -> capture may run but XR product evidence must not
$env:UMAVR_PROBE_FAST = "1"
$env:UMAVR_OPENXR_LOADER = Join-Path $testRoot "no-such-openxr_loader.dll"
$env:LOCALAPPDATA = Join-Path $testRoot "run-a-localappdata"; New-Item -ItemType Directory -Path $env:LOCALAPPDATA -Force | Out-Null
& $fakeGame $ProbeDll
if ($LASTEXITCODE) { throw "fake game (A) failed: $LASTEXITCODE" }
$recordsA = Read-Records $env:LOCALAPPDATA
if (@($recordsA | Where-Object event -eq "startup").Count -lt 1) { throw "startup record missing" }
$xrDisabled = @($recordsA | Where-Object { $_.event -eq "disabled" -and $_.stage -like "openxr_loader*" })
if ($xrDisabled.Count -ne 1) { throw "expected exactly one loader-stage disabled record, got $($xrDisabled.Count)" }
$xrProduct = @($recordsA | Where-Object { $_.evidence_id -in @("CAP-004","CAP-005") })
if ($xrProduct.Count -ne 0) { throw "XR product evidence recorded without a usable XR runtime" }
if ((Get-Item -LiteralPath (Join-Path $env:LOCALAPPDATA "UmaVR\immersive-001.log")).Length -gt 65536 -or $recordsA.Count -gt 256) { throw "telemetry bounds failed" }
Remove-Item Env:\UMAVR_OPENXR_LOADER

# Scenario S: production-equivalent two-device/cache/draw selftest with a
# portrait source. This must prove both pixel coverage and preserved aspect.
$versionedConfig = Join-Path $testRoot "vrmod\config"
New-Item -ItemType Directory -Path $versionedConfig -Force | Out-Null
Set-Content -LiteralPath (Join-Path $versionedConfig "settings.json") -Encoding utf8NoBOM -Value `
    '{"schemaVersion":10,"liveCameraFollow":true,"worldScale":8.0,"eyeRenderScale":0.75,"locomotionEnabled":true,"locomotionSpeed":12.5,"locomotionScaleCompensationEnabled":true,"snapTurnEnabled":true,"snapTurnAngleDegrees":45,"navigationHandsSwapped":true,"postProcessingEnabled":true,"blurEnabled":false,"depthOfFieldEnabled":false,"diffusionEnabled":true,"bloomEnabled":true,"globalFogEnabled":true,"lensDistortionEnabled":true,"radialBlurEnabled":false,"sunShaftsEnabled":true,"indirectLightShaftsEnabled":true,"transmittedLightEnabled":true,"dofDiffusionBloomOverlayEnabled":true,"tiltShiftEnabled":false,"fluctuationEnabled":true,"chromaticAberrationEnabled":true,"toneCurveEnabled":true,"exposureEnabled":true,"colorCorrectionEnabled":true,"colorGradingEnabled":true,"bgBlurEnabled":false,"vortexEnabled":true,"filmRollEnabled":true,"hatchingEnabled":true,"letterBoxEnabled":true,"rainSplashEnabled":true}'
$env:UMAVR_PANEL_SELFTEST = "1"
$env:LOCALAPPDATA = Join-Path $testRoot "run-s-localappdata"; New-Item -ItemType Directory -Path $env:LOCALAPPDATA -Force | Out-Null
& $fakeGame $ProbeDll
if ($LASTEXITCODE) { throw "fake game (S) failed: $LASTEXITCODE" }
$recordsS = Read-Records $env:LOCALAPPDATA
$cameraConfig = @($recordsS | Where-Object { $_.event -eq "camera_follow_config" -and $_.context -eq "Live" })
if ($cameraConfig.Count -ne 1 -or $cameraConfig[0].position_follow -ne $true -or
    $cameraConfig[0].rotation_follow -ne $true -or $cameraConfig[0].authored_roll -ne $true) {
    throw "SETTINGS-001 versioned Live Camera Follow runtime load failed"
}
$worldScaleConfig = @($recordsS | Where-Object { $_.event -eq "world_scale_config" })
if ($worldScaleConfig.Count -ne 1 -or $worldScaleConfig[0].status -ne "observed" -or
    [Math]::Abs([double]$worldScaleConfig[0].world_scale - 8.0) -gt 0.001 -or
    [Math]::Abs([double]$worldScaleConfig[0].translation_multiplier - 0.125) -gt 0.001 -or
    $worldScaleConfig[0].authored_pose_scaled -ne $false -or
    $worldScaleConfig[0].rotation_scaled -ne $false) {
    throw "POSE-003 versioned world scale runtime load failed"
}
$navigationConfig = @($recordsS | Where-Object { $_.event -eq "navigation_config" })
if ($navigationConfig.Count -ne 1 -or $navigationConfig[0].locomotion_enabled -ne $true -or
    [Math]::Abs([double]$navigationConfig[0].locomotion_speed_mps - 12.5) -gt 0.001 -or
    [Math]::Abs([double]$navigationConfig[0].effective_speed_mps - 50.0) -gt 0.001 -or
    $navigationConfig[0].world_scale_compensation -ne $true -or
    [Math]::Abs([double]$navigationConfig[0].speed_reference_world_scale - 2.0) -gt 0.001 -or
    $navigationConfig[0].snap_turn_enabled -ne $true -or
    [Math]::Abs([double]$navigationConfig[0].snap_angle_degrees - 45.0) -gt 0.01 -or
    $navigationConfig[0].hands_swapped -ne $true -or
    $navigationConfig[0].locomotion_hand -ne "left" -or
    $navigationConfig[0].view_turn_hand -ne "right") {
    throw "CTRL-006 versioned navigation hand-swap runtime load failed"
}
$vfxConfig = @($recordsS | Where-Object { $_.event -eq "vfx_config" })
if ($vfxConfig.Count -ne 1 -or $vfxConfig[0].post_processing_enabled -ne $true -or
    $vfxConfig[0].effect_slot_count -ne 22 -or
    $vfxConfig[0].enabled_slot_count -ne 18 -or
    $vfxConfig[0].dof_enabled -ne $false -or
    $vfxConfig[0].diffusion_enabled -ne $true -or
    $vfxConfig[0].bloom_enabled -ne $true -or
    $vfxConfig[0].enabled_semantics -ne "preserve_authored") {
    throw "VFX-001 versioned selective VFX runtime load failed"
}
$selftests = @($recordsS | Where-Object event -eq "selftest")
if ($selftests.Count -ne 1 -or $selftests[0].selftest -ne "pass" -or
    $selftests[0].geometry_ok -ne 1 -or $selftests[0].aspect_ok -ne 1 -or
    $selftests[0].geometry_scale_ok -ne 1 -or
    $selftests[0].controller_pointer_math_ok -ne 1 -or
    $selftests[0].aux_panel_math_ok -ne 1 -or
    $selftests[0].cursor_geometry_ok -ne 1 -or
    $selftests[0].asymmetric_fov_ok -ne 1 -or
    $selftests[0].unity_projection_ok -ne 1 -or
    $selftests[0].pose_composition_ok -ne 1 -or
    $selftests[0].world_scale_config_parse_ok -ne 1 -or
    $selftests[0].navigation_math_ok -ne 1 -or
    $selftests[0].color_eotf_ok -ne 1) {
    throw "panel selftest/aspect/scale/controller-pointer/aux-panel/cursor/asymmetric-FOV/Unity-clip/pose/navigation/color-EOTF verification failed"
}
Remove-Item Env:\UMAVR_PANEL_SELFTEST

# Scenario B: real loader smoke with an animated fake game; liveness asserted,
# panel pipeline details depend on whether a headset is streaming right now
$env:UMAVR_PROBE_FAST = "1"
$env:LOCALAPPDATA = Join-Path $testRoot "run-b-localappdata"; New-Item -ItemType Directory -Path $env:LOCALAPPDATA -Force | Out-Null
& $fakeGame $ProbeDll
if ($LASTEXITCODE) { throw "fake game (B) failed: $LASTEXITCODE" }
$recordsB = Read-Records $env:LOCALAPPDATA
if (@($recordsB | Where-Object event -eq "startup").Count -lt 1) { throw "real-loader startup missing" }
$copies = @($recordsB | Where-Object event -eq "first_frame_copied_to_panel")
$submits = @($recordsB | Where-Object event -eq "frame_progress")
Write-Host "[info] scenario B records=$($recordsB.Count) copies=$($copies.Count) progress_records=$($submits.Count)"

# Install/rollback contract on a synthetic game root
$installRoot = Join-Path $testRoot "install-target"; New-Item -ItemType Directory -Path $installRoot -Force | Out-Null
Set-Content -LiteralPath (Join-Path $installRoot "config.json") -Encoding utf8NoBOM -Value `
    '{"enableConsole":false,"customData":"preserve-me","_externalDlls":[]}'
[IO.File]::WriteAllBytes((Join-Path $installRoot "umamusume.exe"), [byte[]](0x4D,0x5A))
[IO.File]::WriteAllBytes((Join-Path $installRoot "localify.dll"), [byte[]](0x4D,0x5A))
$before = (Get-FileHash -Algorithm SHA256 (Join-Path $installRoot "config.json")).Hash
& (Join-Path $root "tools\Prepare-Test.ps1") -GameRoot $installRoot -PackageDll $ProbeDll | Out-Null
$prepared = Get-Content (Join-Path $installRoot "config.json") -Raw | ConvertFrom-Json
if (-not ($prepared.PSObject.Properties.Name -contains "externalDlls") -or -not ($prepared.PSObject.Properties.Name -contains "_externalDlls") -or
    -not ($prepared.PSObject.Properties.Name -contains "umaVrLiveCameraFollow") -or $prepared.umaVrLiveCameraFollow -ne $true -or
    -not ($prepared.PSObject.Properties.Name -contains "umaVrEyeRenderScale") -or $prepared.umaVrEyeRenderScale -ne 0.75) { throw "prepare schema preservation failed" }
& (Join-Path $root "tools\Rollback-Test.ps1") -GameRoot $installRoot | Out-Null
$after = (Get-FileHash -Algorithm SHA256 (Join-Path $installRoot "config.json")).Hash
if ($after -ne $before) { throw "byte-exact rollback failed" }
& (Join-Path $root "tools\Prepare-Test.ps1") -GameRoot $installRoot -PackageDll $ProbeDll | Out-Null
Add-Content -LiteralPath (Join-Path $installRoot "config.json") -Value " " -NoNewline
$refused = $false
try { & (Join-Path $root "tools\Rollback-Test.ps1") -GameRoot $installRoot | Out-Null } catch { $refused = $_.Exception.Message.Contains("changed after preparation") }
if (-not $refused) { throw "rollback conflict refusal failed" }

[pscustomobject]@{ test_root=$testRoot; records_a=$recordsA.Count; loader_disabled_stage=$xrDisabled[0].stage;
  selftest=$selftests[0].selftest; aspect_ok=$selftests[0].aspect_ok;
  geometry_scale_ok=$selftests[0].geometry_scale_ok;
  controller_pointer_math_ok=$selftests[0].controller_pointer_math_ok;
  aux_panel_math_ok=$selftests[0].aux_panel_math_ok;
  cursor_geometry_ok=$selftests[0].cursor_geometry_ok;
  asymmetric_fov_ok=$selftests[0].asymmetric_fov_ok;
  unity_projection_ok=$selftests[0].unity_projection_ok;
  pose_composition_ok=$selftests[0].pose_composition_ok;
  world_scale_config_parse_ok=$selftests[0].world_scale_config_parse_ok;
  navigation_math_ok=$selftests[0].navigation_math_ok;
  color_eotf_ok=$selftests[0].color_eotf_ok;
  real_loader_records=$recordsB.Count;
  log_bytes_a=(Get-Item -LiteralPath (Join-Path $testRoot "run-a-localappdata\UmaVR\immersive-001.log")).Length;
  package_hash=$manifest.artifact_sha256; rollback_restored=$true; rollback_conflict_refused=$refused }
