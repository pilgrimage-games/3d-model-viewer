#define PG_APP_NAME "3D Model Viewer"
#define PG_APP_GFX_API
#define PG_APP_IMGUI

#define MAX_OBJECT_COUNT 1u

#if defined(WINDOWS)
#include <windows/pg_windows.h>
#else
static_assert(0, "no supported platform is defined");
#endif

// NOTE: This represents constant buffer data so struct members must not cross
// a 16-byte boundary and struct alignment must be to 256 bytes.
typedef struct
{
    pg_f32_4x4 projection_view_mtx;
    pg_f32_3x light_dir;
    f32 padding_0;
    pg_f32_3x camera_pos;
    f32 padding_1[41];
} frame_data;

// NOTE: This represents constant buffer data so struct members must not cross
// a 16-byte boundary and struct alignment must be to 256 bytes.
typedef struct
{
    pg_f32_4x4 model_mtx;
    f32 padding_0[48];
} object_data;

typedef struct
{
    b8 vsync;
    b8 auto_rotate;
    f32 fps;
    f32 frame_time;
    pg_gfx_api gfx_api;  // align: 4
    pg_f32_3x rotation;  // align: 4
    pg_f32_3x light_dir; // align: 4
    pg_camera camera;    // align: 4
    pg_assets assets;    // align: 8 (ptr)
    pg_art art;          // align: 8 (ptr)
} application_state;

GLOBAL pg_config config = {.gfx_force_widescreen = true,
                           .input_repeat_rate = 750.0f,
                           .app_fixed_time_step = (1.0f / 60.0f) * PG_MS_IN_S,
                           .app_permanent_mem_size = 250u * PG_MEBIBYTE,
                           .app_transient_mem_size = 1u * PG_KIBIBYTE,
                           .gfx_mem_size = 50u * PG_KIBIBYTE};

GLOBAL application_state app_state
    = {.vsync = true,
       .auto_rotate = true,
       .gfx_api = PG_GFX_API_D3D12,
       .light_dir = {.x = 0.0f, .y = 0.0f, .z = -1.0f},
       .camera = {.position = {.z = 8.0f}, .up_axis = {.y = 1.0f}}};

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
        for (u32 i = 0; i < app_state.assets.model_count; i += 1)
        {
            ImGui_RadioButtonIntPtr(app_state.assets.models[i].name,
                                    (s32*)&app_state.art.ids[0],
                                    i);
        }
    }

    b8 model_controls_active
        = ImGui_CollapsingHeader("Model Controls",
                                 ImGuiTreeNodeFlags_DefaultOpen);
    if (model_controls_active)
    {
        f32 x_rotation_rad = pg_f32_deg_to_rad(app_state.rotation.x);
        ImGui_SliderAngleEx("X Rotation",
                            &x_rotation_rad,
                            0.0f,
                            360.0f,
                            "%.0f°",
                            0);
        app_state.rotation.x = pg_f32_rad_to_deg(x_rotation_rad);
        if (ImGui_IsItemActive())
        {
            app_state.auto_rotate = false;
        }

        f32 y_rotation_rad = pg_f32_deg_to_rad(app_state.rotation.y);
        ImGui_SliderAngleEx("Y Rotation",
                            &y_rotation_rad,
                            0.0f,
                            360.0f,
                            "%.0f°",
                            0);
        app_state.rotation.y = pg_f32_rad_to_deg(y_rotation_rad);
        if (ImGui_IsItemActive())
        {
            app_state.auto_rotate = false;
        }

        f32 z_rotation_rad = pg_f32_deg_to_rad(app_state.rotation.z);
        ImGui_SliderAngleEx("Z Rotation",
                            &z_rotation_rad,
                            0.0f,
                            360.0f,
                            "%.0f°",
                            0);
        app_state.rotation.z = pg_f32_rad_to_deg(z_rotation_rad);
        if (ImGui_IsItemActive())
        {
            app_state.auto_rotate = false;
        }

        ImGui_Checkbox("Auto-Rotate", (bool*)&app_state.auto_rotate);
    }

    b8 camera_controls_active
        = ImGui_CollapsingHeader("Camera Controls",
                                 ImGuiTreeNodeFlags_DefaultOpen);
    if (camera_controls_active)
    {
        ImGui_SliderFloat("Z Position (Zoom)",
                          &app_state.camera.position.z,
                          0.001f,
                          16.0f);
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

    b8 keyboard_controls_active
        = ImGui_CollapsingHeader("Keyboard Controls",
                                 ImGuiTreeNodeFlags_DefaultOpen);
    if (keyboard_controls_active)
    {
        ImGui_Text("[Left/Up]: Previous Model");
        ImGui_Text("[Right/Down]: Next Model");
        ImGui_Text("[Alt+Enter]: Toggle Fullscreen");
    }
#endif
}

FUNCTION void
init_app(pg_dynamic_cb_data* dynamic_cb_data,
         pg_f32_4x4* projection_mtx,
         pg_f32_3x** model_scaling,
         pg_arena* permanent_mem,
         pg_error* err)
{
    b8 ok = true;

    ok = pg_arena_push(&app_state.art.ids,
                       permanent_mem,
                       sizeof(u32),
                       alignof(u32));
    if (!ok)
    {
        err->log(err,
                 PG_ERROR_MAJOR,
                 "init_app_state: failed to get memory for art ids");
    }
    app_state.art.model_count = MAX_OBJECT_COUNT;

    pg_dynamic_cb_data_create(sizeof(frame_data),
                              sizeof(object_data),
                              MAX_OBJECT_COUNT,
                              permanent_mem,
                              dynamic_cb_data,
                              err);
    *projection_mtx = pg_f32_4x4_perspective(27.0f, 16.0f / 9.0f, 0.1f, 100.0f);

    // Get normalized scaling for all models.
    ok = pg_arena_push(model_scaling,
                       permanent_mem,
                       app_state.assets.model_count * sizeof(pg_f32_3x),
                       alignof(pg_f32_3x));
    if (!ok)
    {
        err->log(err,
                 PG_ERROR_MAJOR,
                 "init_app_state: failed to get memory for model scaling");
    }
    for (u32 i = 0; i < app_state.assets.model_count; i += 1)
    {
        f32 norm_scale = 0.0f;
        for (u32 j = 0; j < app_state.assets.models[i].mesh_count; j += 1)
        {
            pg_model_mesh* m = &app_state.assets.models[i].meshes[j];
            for (u32 k = 0; k < m->vertex_count; k += 1)
            {
                pg_f32_3x position = m->vertices[k].position;
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
}

FUNCTION void
update_app(pg_input* input,
           pg_dynamic_cb_data* dynamic_cb_data,
           pg_f32_4x4* projection_mtx,
           pg_f32_3x* model_scaling,
           f32 frame_time,
           f32* running_time_step,
           pg_error* err)
{
    // Left/Up: Previous Model
    if (pg_button_pressed(&input->kbd.left,
                          config.input_repeat_rate,
                          frame_time)
        || pg_button_pressed(&input->kbd.up,
                             config.input_repeat_rate,
                             frame_time))
    {
        if (app_state.art.ids[0] == 0)
        {
            app_state.art.ids[0] = app_state.assets.model_count - 1;
        }
        else
        {
            app_state.art.ids[0] -= 1;
        }
    }

    // Right/Down: Next Model
    if (pg_button_pressed(&input->kbd.right,
                          config.input_repeat_rate,
                          frame_time)
        || pg_button_pressed(&input->kbd.down,
                             config.input_repeat_rate,
                             frame_time))
    {
        if (app_state.art.ids[0] == app_state.assets.model_count - 1)
        {
            app_state.art.ids[0] = 0;
        }
        else
        {
            app_state.art.ids[0] += 1;
        }
    }

    // Every fixed time step, update model rotation.
    if (*running_time_step >= config.app_fixed_time_step)
    {
        if (app_state.auto_rotate)
        {
            app_state.rotation.y += 1.0f;
            if (app_state.rotation.y > 359.0f)
            {
                app_state.rotation.y = 0.0f;
            }
        }

        *running_time_step -= config.app_fixed_time_step;
    }

    pg_f32_4x4 view_mtx = pg_f32_4x4_look_at(&app_state.camera);
    frame_data fd
        = {.projection_view_mtx = pg_f32_4x4_mul(*projection_mtx, view_mtx),
           .light_dir = app_state.light_dir,
           .camera_pos = app_state.camera.position};
    object_data od
        = {.model_mtx = pg_f32_4x4_place(model_scaling[app_state.art.ids[0]],
                                         app_state.rotation,
                                         (pg_f32_3x){0})};
    pg_dynamic_cb_data_update(dynamic_cb_data,
                              &fd,
                              &od,
                              app_state.art.model_count,
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
    pg_f32_4x4 projection_mtx = {0};
    pg_f32_3x* model_scaling = 0;

    pg_windows_window_init(&windows, &config, inst, &err);
    pg_windows_init_mem(&windows, &config, &err);
    pg_assets_read_pga(&windows.permanent_mem,
                       &pg_windows_read_file,
                       &app_state.assets,
                       &err);

    init_app(&dynamic_cb_data,
             &projection_mtx,
             &model_scaling,
             &windows.permanent_mem,
             &err);

    pg_windows_init_graphics(&windows,
                             app_state.gfx_api,
                             &app_state.assets,
                             &dynamic_cb_data,
                             app_state.vsync,
                             config.gfx_mem_size,
                             &err);
    pg_windows_metrics_init(&windows, &err);

    f32 running_time_step = config.app_fixed_time_step;
    while (windows.msg.message != WM_QUIT)
    {
        if (PeekMessageW(&windows.msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&windows.msg);
            DispatchMessageW(&windows.msg);
            continue;
        }

        pg_gfx_api gfx_api = app_state.gfx_api;

        pg_windows_update_input(&windows, &config, &err);

        update_app(&windows.input,
                   &dynamic_cb_data,
                   &projection_mtx,
                   model_scaling,
                   windows.metrics.frame_time,
                   &running_time_step,
                   &err);

        pg_windows_update_graphics(&windows,
                                   app_state.gfx_api,
                                   &app_state.assets,
                                   &dynamic_cb_data,
                                   app_state.art,
                                   app_state.vsync,
                                   &imgui_ui,
                                   &err);

        pg_windows_metrics_update(&windows, &err);
        running_time_step += windows.metrics.frame_time;
        app_state.fps = windows.metrics.fps;
        app_state.frame_time = windows.metrics.frame_time;

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
