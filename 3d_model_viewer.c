#define PG_APP_NAME "3D Model Viewer"
#define PG_APP_GFX_API
#define PG_APP_IMGUI

#if defined(WINDOWS)
#include <windows/pg_windows.h>
#else
static_assert(0, "no supported platform is defined");
#endif

typedef struct
{
    b8 vsync;
    b8 auto_rotate;
    f32 fps;
    f32 frame_time;
    f32 running_time_step;
    pg_gfx_api gfx_api;       // align: 4
    pg_art art;               // align: 4
    pg_f32_3x rotation;       // align: 4
    pg_f32_3x light_dir;      // align: 4
    pg_frame_data frame_data; // align: 4
    pg_obj_data obj_data;     // align: 4
    pg_f32_3x* model_scaling;
    pg_assets* assets;
} application_state;

pg_config config = {.gfx_force_widescreen = true,
                    .input_repeat_rate = 750.0f,
                    .app_fixed_time_step = (1.0f / 60.0f) * PG_MS_IN_S,
                    .app_permanent_mem_size = 200u * PG_MEBIBYTE,
                    .app_transient_mem_size = 25u * PG_KIBIBYTE,
                    .gfx_mem_size = 50u * PG_KIBIBYTE};

pg_camera camera = {.position = {.z = 8.0f}, .up_axis = {.y = 1.0f}};

application_state app_state
    = {.vsync = true,
       .auto_rotate = true,
       .gfx_api = PG_GFX_API_D3D12,
       .light_dir = {.x = -0.5f, .y = 0.0f, .z = -1.0f}};

void
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
        for (u32 i = 0; i < app_state.assets->model_count; i += 1)
        {
            ImGui_RadioButtonIntPtr(app_state.assets->models[i].name,
                                    (s32*)&app_state.art.ids[0],
                                    i);
        }
    }

    b8 model_controls_active
        = ImGui_CollapsingHeader("Model Controls",
                                 ImGuiTreeNodeFlags_DefaultOpen);
    if (model_controls_active)
    {
        f32 y_rotation_rad = pg_f32_deg_to_rad(app_state.rotation.y);
        ImGui_SliderAngle("Rotation", &y_rotation_rad);
        app_state.rotation.y = pg_f32_rad_to_deg(y_rotation_rad);
        b8 manual_rotation = ImGui_IsItemActive();
        if (manual_rotation)
        {
            app_state.auto_rotate = false;
        }
        ImGui_Checkbox("Auto-Rotate", &app_state.auto_rotate);
    }

    b8 lighting_controls_active
        = ImGui_CollapsingHeader("Lighting Controls",
                                 ImGuiTreeNodeFlags_DefaultOpen);
    if (lighting_controls_active)
    {
        ImGui_SliderFloat3("Light Direction",
                           app_state.light_dir.e,
                           -1.0f,
                           1.0f);
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

void
update_app_state(pg_input* input, pg_assets* assets, f32 frame_time)
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
            app_state.art.ids[0] = assets->model_count - 1;
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
        if (app_state.art.ids[0] == assets->model_count - 1)
        {
            app_state.art.ids[0] = 0;
        }
        else
        {
            app_state.art.ids[0] += 1;
        }
    }

    if (app_state.running_time_step >= config.app_fixed_time_step)
    {
        // Update model rotation.
        if (app_state.auto_rotate)
        {
            app_state.rotation.y += 1.0f;
            if (app_state.rotation.y > 359.0f)
            {
                app_state.rotation.y = -360.0f;
            }
        }

        app_state.running_time_step -= config.app_fixed_time_step;
    }

    app_state.frame_data.light_dir = app_state.light_dir;
    app_state.obj_data.model_mtx
        = pg_f32_4x4_place(app_state.model_scaling[app_state.art.ids[0]],
                           app_state.rotation,
                           (pg_f32_3x){0});
}

void
init_app_state(pg_arena* permanent_mem, pg_assets* assets, pg_error* err)
{
    b8 ok = true;

    app_state.running_time_step = config.app_fixed_time_step;

    ok = pg_arena_push(&app_state.art.ids,
                       permanent_mem,
                       sizeof(u32),
                       alignof(u32));
    if (!ok)
    {
        err->log(err,
                 PG_ERR_MAJOR,
                 "init_app_state: failed to get memory for art ids");
    }
    app_state.art.model_count = 1;

    pg_f32_4x4 projection_mtx
        = pg_f32_4x4_perspective(27.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    pg_f32_4x4 view_mtx = pg_f32_4x4_look_at(&camera);
    ok = pg_arena_push(&app_state.model_scaling,
                       permanent_mem,
                       assets->model_count * sizeof(pg_f32_3x),
                       alignof(pg_f32_3x));
    if (!ok)
    {
        err->log(err,
                 PG_ERR_MAJOR,
                 "init_app_state: failed to get memory for model scaling");
    }

    app_state.frame_data = (pg_frame_data){
        .projection_view_mtx = pg_f32_4x4_mul(projection_mtx, view_mtx),
        .light_dir = app_state.light_dir,
        .camera_pos = camera.position};
    app_state.obj_data
        = (pg_obj_data){.model_mtx = pg_f32_4x4_place(
                            app_state.model_scaling[app_state.art.ids[0]],
                            app_state.rotation,
                            (pg_f32_3x){0})};

    // Get normalized scaling for all models.
    for (u32 i = 0; i < assets->model_count; i += 1)
    {
        pg_file_model* model = &assets->models[i];
        f32 norm_scale = 0.0f;
        for (u32 j = 0; j < model->mesh_count; j += 1)
        {
            for (u32 k = 0; k < model->meshes[j].vertex_count; k += 1)
            {
                pg_f32_3x position = model->meshes[j].vertices[k].position;
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
        app_state.model_scaling[i]
            = (pg_f32_3x){.x = norm_scale, .y = norm_scale, .z = norm_scale};
    }

    app_state.assets = assets;
}

#if defined(WINDOWS)
s32 WINAPI
wWinMain(HINSTANCE inst, HINSTANCE prev_inst, WCHAR* cmd_args, s32 show_code)
{
    (void)prev_inst;
    (void)cmd_args;
    (void)show_code;

    pg_windows windows = {0};
    pg_assets assets = {0};
    pg_error err
        = {.log = &pg_windows_error_log, .write_file = &pg_windows_write_file};

    b8 ok = pg_windows_init_mem(&windows, &config);
    if (!ok)
    {
        err.log(&err, PG_ERR_MAJOR, "wWinMain: failed to initialize memory");
    }
    ok = pg_windows_init_assets(&windows, &assets);
    if (!ok)
    {
        err.log(&err, PG_ERR_MAJOR, "wWinMain: failed to initialize assets");
    }
    ok = pg_windows_window_init(&windows, &config, inst, true);
    if (!ok)
    {
        err.log(&err, PG_ERR_MAJOR, "wWinMain: failed to initialize window");
    }

    init_app_state(&windows.permanent_mem, &assets, &err);

    ok = pg_windows_init_graphics(&windows,
                                  app_state.gfx_api,
                                  &assets,
                                  app_state.vsync,
                                  config.gfx_mem_size);
    if (!ok)
    {
        err.log(&err, PG_ERR_MAJOR, "wWinMain: failed to initialize graphics");
    }
    ok = pg_windows_metrics_init(&windows);
    if (!ok)
    {
        err.log(&err, PG_ERR_MINOR, "wWinMain: failed to initialize metrics");
    }

    while (windows.msg.message != WM_QUIT)
    {
        if (PeekMessageW(&windows.msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&windows.msg);
            DispatchMessageW(&windows.msg);
            continue;
        }

        pg_gfx_api gfx_api = app_state.gfx_api;

        ok = pg_windows_update_input(&windows, &config);
        if (!ok)
        {
            err.log(&err, PG_ERR_MAJOR, "wWinMain: failed to update input");
        }

        update_app_state(&windows.input, &assets, windows.metrics.frame_time);

        ok = pg_windows_update_graphics(&windows,
                                        app_state.gfx_api,
                                        &assets,
                                        app_state.art,
                                        &app_state.frame_data,
                                        &app_state.obj_data,
                                        app_state.vsync,
                                        &imgui_ui);
        if (!ok)
        {
            err.log(&err, PG_ERR_MAJOR, "wWinMain: failed to update graphics");
        }

        ok = pg_windows_metrics_update(&windows);
        if (!ok)
        {
            err.log(&err, PG_ERR_MINOR, "wWinMain: failed to update metrics");
        }
        app_state.running_time_step += windows.metrics.frame_time;
        app_state.fps = windows.metrics.fps;
        app_state.frame_time = windows.metrics.frame_time;

        if (app_state.gfx_api != gfx_api)
        {
            ok = pg_windows_reload_graphics(&windows,
                                            &config,
                                            inst,
                                            app_state.gfx_api,
                                            &assets,
                                            app_state.vsync,
                                            config.gfx_mem_size);
            if (!ok)
            {
                err.log(&err,
                        PG_ERR_MAJOR,
                        "wWinMain: failed to reload graphics");
            }
        }

        pg_arena_flush(&windows.transient_mem);
    }

    pg_windows_release(&windows);

    return 0;
}
#endif
