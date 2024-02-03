#define PG_APP_NAME "3D Model Viewer"
#define PG_APP_GFX_API
#define PG_APP_IMGUI

#define CAMERA_AUTO_ROTATION_RATE 30.0f    // degrees/sec
#define CAMERA_MANUAL_ROTATION_RATE 180.0f // degrees/sec

#define MAX_ENTITY_COUNT 1u

#if defined(WINDOWS)
#include <windows/pg_windows.h>
#else
static_assert(0, "no supported platform is defined");
#endif

typedef enum
{
    ART_MODEL_ALPHA_BLEND_TEST,
    ART_MODEL_BAKER_AND_THE_BRIDGE,
    ART_MODEL_CORSET,
    ART_MODEL_DAMAGED_HELMET,
    ART_MODEL_FTM,
    ART_MODEL_HEARTPIECE,
    ART_MODEL_METAL_ROUGH_SPHERES,
    ART_MODEL_PLAYSTATION_1,
    ART_MODEL_TEST_OPACITY_2,
    ART_MODEL_WATER_BOTTLE,
    ART_COUNT
} asset_type_art;

// NOTE: This represents constant buffer data so struct members must not cross
// a 16-byte boundary and struct alignment must be to 256 bytes.
typedef struct
{
    pg_f32_4x4 projection_mtx;
    pg_f32_4x4 view_mtx;
    pg_f32_3x light_dir;
    f32 padding_0;
    pg_f32_3x camera_pos;
    f32 padding_1[25];
} frame_data;

// NOTE: This represents constant buffer data so struct members must not cross
// a 16-byte boundary and struct alignment must be to 256 bytes.
typedef struct
{
    pg_f32_4x4 model_mtx;
    f32 padding_0[48];
} entity_data;

typedef struct
{
    b8 vsync;
    b8 auto_rotate;
    u32 art_id;
    f32 fps;
    f32 frame_time;
    f32 center_zoom;
    pg_gfx_api gfx_api;  // align: 4
    pg_f32_3x rotation;  // align: 4
    pg_f32_3x light_dir; // align: 4
    pg_camera camera;    // align: 4
    pg_assets assets;    // align: 8 (ptr)
} application_state;

GLOBAL pg_config config = {.input_gamepad_count = 1,
                           .input_repeat_rate = 750.0f,
                           .fixed_time_step = (1.0f / 480.0f) * PG_MS_IN_S,
                           .permanent_mem_size = 2u * PG_GIBIBYTE,
                           .transient_mem_size = 100u * PG_KIBIBYTE,
                           .gfx_mem_size = 50u * PG_KIBIBYTE};

GLOBAL application_state app_state
    = {.vsync = true,
       .auto_rotate = true,
       .gfx_api = PG_GFX_API_D3D12,
       .light_dir = {.x = -0.5f, .y = 0.0f, .z = -1.0f},
       .camera = {.arcball = true, .up_axis = {.y = 1.0f}}};

void
reset_view(void)
{
    app_state.camera.position.x = PG_PI / 2.0f;
    app_state.camera.position.y = PG_PI / 2.0f;
    app_state.camera.position.z = 6.0f;
    app_state.auto_rotate = true;
    switch (app_state.art_id)
    {
        case ART_MODEL_FTM:
        {
            app_state.rotation = (pg_f32_3x){.x = 0.0f, .y = 180.0f, .z = 0.0f};
            app_state.camera.position.z = 1.0f;
            break;
        }
        case ART_MODEL_PLAYSTATION_1:
        {
            app_state.rotation
                = (pg_f32_3x){.x = 90.0f, .y = 0.0f, .z = 270.0f};
            break;
        }
        default:
        {
            app_state.rotation = (pg_f32_3x){.x = 0.0f, .y = 0.0f, .z = 0.0f};
            break;
        }
    }
    app_state.center_zoom = app_state.camera.position.z;
}

FUNCTION void
imgui_ui(void)
{
#if defined(PG_APP_IMGUI)
    pg_imgui_graphics_header(&app_state.gfx_api,
                             app_state.fps,
                             app_state.frame_time);

    b8 model_selection_active
        = ImGui_CollapsingHeader("Model Selection",
                                 ImGuiTreeNodeFlags_DefaultOpen);
    if (model_selection_active)
    {
        u32 model_id = app_state.art_id;
        for (u32 i = 0; i < app_state.assets.art_count; i += 1)
        {
            ImGui_RadioButtonIntPtr(app_state.assets.art[i].name,
                                    (s32*)&app_state.art_id,
                                    i);
        }
        if (app_state.art_id != model_id)
        {
            reset_view();
        }
    }

    b8 lighting_controls_active
        = ImGui_CollapsingHeader("Lighting Controls",
                                 ImGuiTreeNodeFlags_DefaultOpen);
    if (lighting_controls_active)
    {
        ImGui_SliderFloat("X Direction", &app_state.light_dir.x, -1.0f, 1.0f);
        ImGui_SliderFloat("Y Direction", &app_state.light_dir.y, -1.0f, 1.0f);
        ImGui_SliderFloat("Z Direction", &app_state.light_dir.z, -1.0f, 1.0f);
    }

    b8 mouse_controls_active
        = ImGui_CollapsingHeader("Mouse Controls",
                                 ImGuiTreeNodeFlags_DefaultOpen);
    if (mouse_controls_active)
    {
        ImGui_Text("[Left Click + Drag]: Rotate");
        ImGui_Text("[Forward/Back]: Zoom In/Zoom Out");
    }

    b8 keyboard_controls_active
        = ImGui_CollapsingHeader("Keyboard Controls",
                                 ImGuiTreeNodeFlags_DefaultOpen);
    if (keyboard_controls_active)
    {
        ImGui_Text("[Left/Up]: Previous Model");
        ImGui_Text("[Right/Down]: Next Model");
        ImGui_Text("[Alt+Enter]: Toggle Fullscreen");
    }

    b8 gamepad_controls_active
        = ImGui_CollapsingHeader("Gamepad Controls",
                                 ImGuiTreeNodeFlags_DefaultOpen);
    if (gamepad_controls_active)
    {
        ImGui_Text("[D-Pad Left/Up]: Previous Model");
        ImGui_Text("[D-Pad Right/Down]: Next Model");
        ImGui_Text("[Right Thumbstick]: Rotate");
        ImGui_Text("[Right Trigger/Left Trigger]: Zoom In/Zoom Out");
    }
#endif
}

FUNCTION void
init_app(pg_dynamic_cb_data* dynamic_cb_data,
         pg_f32_3x** model_scaling,
         pg_arena* permanent_mem,
         pg_error* err)
{
    b8 ok = true;

    pg_dynamic_cb_data_create(sizeof(frame_data),
                              sizeof(entity_data),
                              MAX_ENTITY_COUNT,
                              permanent_mem,
                              dynamic_cb_data,
                              err);

    // Get normalized scaling for all models.
    ok = pg_arena_push(model_scaling,
                       permanent_mem,
                       app_state.assets.art_count * sizeof(pg_f32_3x),
                       alignof(pg_f32_3x));
    if (!ok)
    {
        err->log(err,
                 PG_ERROR_MAJOR,
                 "init_app_state: failed to get memory for model scaling");
    }
    for (u32 i = 0; i < app_state.assets.art_count; i += 1)
    {
        f32 norm_scale = 0.0f;
        for (u32 j = 0; j < app_state.assets.art[i].mesh_count; j += 1)
        {
            pg_mesh* mesh = &app_state.assets.art[i].meshes[j];
            for (u32 k = 0; k < mesh->vertex_count; k += 1)
            {
                pg_f32_3x position = mesh->vertices[k].position;
                for (u32 l = 0; l < CAP(position.e); l += 1)
                {
                    f32 v = position.e[l];
                    if (v > norm_scale)
                    {
                        norm_scale = v;
                    }
                    if (-v > norm_scale)
                    {
                        norm_scale = -v;
                    }
                }
            }
        }
        norm_scale = norm_scale != 0.0f ? 1.0f / norm_scale : 1.0f;
        (*model_scaling)[i]
            = (pg_f32_3x){.x = norm_scale, .y = norm_scale, .z = norm_scale};
    }

    reset_view();
}

FUNCTION void
update_app(pg_input* input,
           pg_f32_2x previous_cursor_position,
           pg_drawable_mesh** meshes,
           u32* mesh_count,
           pg_dynamic_cb_data* dynamic_cb_data,
           pg_f32_3x* model_scaling,
           pg_arena* transient_mem,
           f32* running_time_step,
           pg_error* err)
{
    // Left/Up: Previous Model
    if (pg_button_pressed(&input->kbd.left, config.input_repeat_rate)
        || pg_button_pressed(&input->kbd.up, config.input_repeat_rate)
        || pg_button_pressed(&input->gp[0].left, config.input_repeat_rate)
        || pg_button_pressed(&input->gp[0].up, config.input_repeat_rate))
    {
        if (app_state.art_id == 0)
        {
            app_state.art_id = ART_COUNT - 1;
        }
        else
        {
            app_state.art_id -= 1;
        }
        reset_view();
    }

    // Right/Down: Next Model
    if (pg_button_pressed(&input->kbd.right, config.input_repeat_rate)
        || pg_button_pressed(&input->kbd.down, config.input_repeat_rate)
        || pg_button_pressed(&input->gp[0].right, config.input_repeat_rate)
        || pg_button_pressed(&input->gp[0].down, config.input_repeat_rate))
    {
        if (app_state.art_id == app_state.assets.art_count - 1)
        {
            app_state.art_id = 0;
        }
        else
        {
            app_state.art_id += 1;
        }
        reset_view();
    }

    pg_f32_2x cursor_delta = {0};
    if (pg_button_pressed(&input->mouse.left, app_state.frame_time)
        && !(ImGui_GetIO()->WantCaptureMouse))
    {
        cursor_delta = pg_f32_2x_mul_scalar(
            pg_f32_2x_sub(input->mouse.cursor, previous_cursor_position),
            100.0f);
    }

    if (cursor_delta.x != 0.0f || cursor_delta.y != 0.0f
        || input->gp[0].rs.x != 0.0f || input->gp[0].rs.y != 0.0f)
    {
        app_state.auto_rotate = false;
    }

    while (*running_time_step != 0.0f)
    {
        f32 dt = config.fixed_time_step;
        if (*running_time_step < dt)
        {
            dt = *running_time_step;
        }

        // Rotate camera.
        f32 auto_rotation_rate = CAMERA_AUTO_ROTATION_RATE / PG_MS_IN_S;
        f32 manual_rotation_rate = CAMERA_MANUAL_ROTATION_RATE / PG_MS_IN_S;
        if (app_state.auto_rotate)
        {
            app_state.camera.position.x
                += pg_f32_deg_to_rad(auto_rotation_rate * dt);
        }
        else
        {
            app_state.camera.position.x += pg_f32_deg_to_rad(
                manual_rotation_rate * cursor_delta.x * dt);
            app_state.camera.position.y += pg_f32_deg_to_rad(
                manual_rotation_rate * cursor_delta.y * dt);
            app_state.camera.position.x += pg_f32_deg_to_rad(
                manual_rotation_rate * input->gp[0].rs.x * dt);
            app_state.camera.position.y += pg_f32_deg_to_rad(
                manual_rotation_rate * input->gp[0].rs.y * dt);
        }

        // Zoom camera.
        f32 zoom_rate = app_state.center_zoom / 1000.0f;
        app_state.camera.position.z
            -= zoom_rate * (u8)input->mouse.forward.pressed * dt;
        app_state.camera.position.z
            += zoom_rate * (u8)input->mouse.back.pressed * dt;
        app_state.camera.position.z -= zoom_rate * input->gp[0].rt * dt;
        app_state.camera.position.z += zoom_rate * input->gp[0].lt * dt;

        *running_time_step -= dt;
    }

    pg_camera_clamp(
        (pg_f32_2x){.min = 0.0f, .max = 2.0f * PG_PI},
        (pg_f32_2x){.min = 0.0f, .max = PG_PI},
        (pg_f32_2x){.min = 0.0f, .max = app_state.center_zoom * 2.0f},
        true,
        &app_state.camera);
    pg_f32_3x camera_position
        = pg_camera_get_cartesian_position(&app_state.camera);

    frame_data fd
        = {.projection_mtx
           = pg_f32_4x4_perspective(27.0f, 16.0f / 9.0f, 0.1f, 100.0f),
           .view_mtx = pg_f32_4x4_look_at(camera_position,
                                          app_state.camera.focal_point,
                                          app_state.camera.up_axis),
           .light_dir = app_state.light_dir,
           .camera_pos = camera_position};
    entity_data ed
        = {.model_mtx = pg_f32_4x4_place(model_scaling[app_state.art_id],
                                         app_state.rotation,
                                         (pg_f32_3x){0})};
    pg_dynamic_cb_data_update(dynamic_cb_data, &fd, &ed, MAX_ENTITY_COUNT, err);

    pg_assets_get_drawable_meshes(&app_state.art_id,
                                  1,
                                  &app_state.assets,
                                  &fd.view_mtx,
                                  &ed.model_mtx,
                                  transient_mem,
                                  meshes,
                                  mesh_count,
                                  err);
}

#if defined(WINDOWS)
s32 WINAPI
wWinMain(HINSTANCE inst, HINSTANCE prev_inst, WCHAR* cmd_args, s32 show_code)
{
    (void)prev_inst;
    (void)cmd_args;
    (void)show_code;

    pg_windows windows = {0};
    pg_error err
        = {.log = &pg_windows_error_log, .write_file = &pg_windows_write_file};

    pg_dynamic_cb_data dynamic_cb_data = {0};
    pg_f32_3x* model_scaling = 0;
    pg_drawable_mesh* meshes = 0;
    u32 mesh_count = 0;

    pg_windows_window_init(&windows, &config, inst, &err);
    pg_windows_init_mem(&windows, &config, &err);
    pg_assets_read_pga(&windows.permanent_mem,
                       &pg_windows_read_file,
                       &app_state.assets,
                       &err);
    if (app_state.assets.art_count != ART_COUNT)
    {
        err.log(&err,
                PG_ERROR_MAJOR,
                "wWinMain: unexpected number of art assets");
    }

    init_app(&dynamic_cb_data, &model_scaling, &windows.permanent_mem, &err);

    pg_windows_init_graphics(&windows,
                             app_state.gfx_api,
                             &app_state.assets,
                             &dynamic_cb_data,
                             app_state.vsync,
                             config.gfx_mem_size,
                             &err);
    pg_windows_metrics_init(&windows, &err);

    f32 running_time_step = config.fixed_time_step;
    while (windows.msg.message != WM_QUIT)
    {
        if (PeekMessageW(&windows.msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&windows.msg);
            DispatchMessageW(&windows.msg);
            continue;
        }

        pg_gfx_api gfx_api = app_state.gfx_api;
        pg_f32_2x previous_cursor_position = windows.input.mouse.cursor;

        pg_windows_update_input(&windows, &config, &err);

        update_app(&windows.input,
                   previous_cursor_position,
                   &meshes,
                   &mesh_count,
                   &dynamic_cb_data,
                   model_scaling,
                   &windows.transient_mem,
                   &running_time_step,
                   &err);

        pg_windows_update_graphics(&windows,
                                   app_state.gfx_api,
                                   meshes,
                                   mesh_count,
                                   &dynamic_cb_data,
                                   app_state.vsync,
                                   &imgui_ui,
                                   &err);

        pg_windows_metrics_update(&windows, &err);
        app_state.fps = windows.metrics.fps;
        app_state.frame_time = windows.metrics.frame_time;
        running_time_step += app_state.frame_time;

        if (app_state.gfx_api != gfx_api)
        {
            pg_windows_reload_graphics(&windows,
                                       &config,
                                       inst,
                                       app_state.gfx_api,
                                       &app_state.assets,
                                       &dynamic_cb_data,
                                       app_state.vsync,
                                       config.gfx_mem_size,
                                       &err);
        }

        pg_arena_flush(&windows.transient_mem);
    }

    pg_windows_release(&windows);

    return 0;
}
#endif
