#define PG_APP_NAME "3D Model Viewer"
#define PG_APP_GFX_API
#define PG_APP_IMGUI

#define DEFAULT_CAMERA_Z_POSITION 6.0f

#define MAX_OBJECT_COUNT 1u

#if defined(WINDOWS)
#include <windows/pg_windows.h>
#else
static_assert(0, "no supported platform is defined");
#endif

typedef enum
{
    ART_MODEL_BAKER_AND_THE_BRIDGE,
    ART_MODEL_CORSET,
    ART_MODEL_DAMAGED_HELMET,
    ART_MODEL_FTM,
    ART_MODEL_METAL_ROUGH_SPHERES,
    ART_MODEL_PLAYSTATION_1,
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
} object_data;

typedef struct
{
    b8 vsync;
    b8 auto_rotate;
    b8 mouse_rotation;
    u32 art_id;
    f32 fps;
    f32 frame_time;
    pg_gfx_api gfx_api;     // align: 4
    pg_f32_2x mouse_cursor; // align: 4
    pg_f32_3x rotation;     // align: 4
    pg_f32_3x light_dir;    // align: 4
    pg_camera camera;       // align: 4
    pg_assets assets;       // align: 8 (ptr)
} application_state;

GLOBAL pg_config config = {.gfx_force_widescreen = true,
                           .input_repeat_rate = 750.0f,
                           .app_fixed_time_step = (1.0f / 60.0f) * PG_MS_IN_S,
                           .app_permanent_mem_size = 1u * PG_GIBIBYTE,
                           .app_transient_mem_size = 1u * PG_KIBIBYTE,
                           .gfx_mem_size = 50u * PG_KIBIBYTE};

// NOTE: Camera position is stored as a spherical coordinate (zoom, theta, phi).
GLOBAL application_state app_state
    = {.vsync = true,
       .auto_rotate = false,   // TODO
       .mouse_rotation = true, // TODO
       .gfx_api = PG_GFX_API_D3D12,
       .light_dir = {.x = 0.0f, .y = 0.0f, .z = -1.0f},
       .camera = {.theta = PG_CAMERA_DEFAULT_THETA,
                  .phi = PG_CAMERA_DEFAULT_PHI,
                  .up_axis = {.y = 1.0f}}};

void
reset_view(void)
{
    switch (app_state.art_id)
    {
        case ART_MODEL_FTM:
        {
            app_state.rotation = (pg_f32_3x){.x = 0.0f, .y = 180.0f, .z = 0.0f};
            camera_zoom = 1.0f;
            break;
        }
        case ART_MODEL_PLAYSTATION_1:
        {
            app_state.rotation
                = (pg_f32_3x){.x = 90.0f, .y = 0.0f, .z = 270.0f};
            camera_zoom = PG_CAMERA_DEFAULT_ZOOM;
            break;
        }
        default:
        {
            app_state.rotation = (pg_f32_3x){.x = 0.0f, .y = 0.0f, .z = 0.0f};
            camera_zoom = PG_CAMERA_DEFAULT_ZOOM;
            break;
        }
    }
    app_state.camera.theta = PG_CAMERA_DEFAULT_THETA;
    app_state.camera.phi = PG_CAMERA_DEFAULT_PHI;
    app_state.auto_rotate = true;
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

    b8 model_controls_active
        = ImGui_CollapsingHeader("Model Controls",
                                 ImGuiTreeNodeFlags_DefaultOpen);
    if (model_controls_active)
    {
        ImGui_Checkbox("Auto-Rotate", (bool*)&app_state.auto_rotate);
        ImGui_Checkbox("Mouse Rotation", (bool*)&app_state.mouse_rotation);
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
    if (pg_button_pressed(&input->kbd.right,
                          config.input_repeat_rate,
                          frame_time)
        || pg_button_pressed(&input->kbd.down,
                             config.input_repeat_rate,
                             frame_time))
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

    // Every fixed time step, update model and camera rotation.
    if (*running_time_step >= config.app_fixed_time_step)
    {
        // Left Mouse Click and Mouse Movement: Rotate camera about origin.
        if (app_state.mouse_rotation)
        {
            app_state.auto_rotate = false;

            if (pg_button_pressed(&input->mouse.left, frame_time, frame_time))
            {
                pg_f32_2x mouse_cursor_delta
                    = pg_f32_2x_sub(app_state.mouse_cursor,
                                    input->mouse.cursor);
                f32 cursor_x_delta = mouse_cursor_delta.x >= 0.0f
                                         ? mouse_cursor_delta.x
                                         : -mouse_cursor_delta.x;
                f32 cursor_y_delta = mouse_cursor_delta.y >= 0.0f
                                         ? mouse_cursor_delta.y
                                         : -mouse_cursor_delta.y;

                if (cursor_x_delta > cursor_y_delta)
                {
                    // NOTE: Phi must be in the range [0, 2*PG_PI)
                    // Move cursor to the left, move camera right.
                    if (mouse_cursor_delta.x > 0.0f)
                    {
                        app_state.camera.phi -= PG_PI * PG_CAMERA_ROTATION_RATE;
                        if (app_state.camera.phi < 0.0f)
                        {
                            app_state.camera.phi = (2.0f * PG_PI);
                        }
                    }
                    // Move cursor to the right, move camera left.
                    if (mouse_cursor_delta.x < 0.0f)
                    {
                        app_state.camera.phi += PG_PI * PG_CAMERA_ROTATION_RATE;
                        if (app_state.camera.phi > (2.0f * PG_PI))
                        {
                            app_state.camera.phi = 0.0f;
                        }
                    }
                }
                else
                {
                    // NOTE: Theta must be in the range [0, PG_PI]
                    // Move cursor down, move camera up.
                    if (mouse_cursor_delta.y > 0.0f)
                    {
                        app_state.camera.theta
                            -= (PG_PI / 2.0f) * PG_CAMERA_ROTATION_RATE;
                        if (app_state.camera.theta < 0.001f)
                        {
                            app_state.camera.theta = 0.001f;
                        }
                    }
                    // Move cursor up, move camera down.
                    if (mouse_cursor_delta.y < 0.0f)
                    {
                        app_state.camera.theta
                            += (PG_PI / 2.0f) * PG_CAMERA_ROTATION_RATE;
                        if (app_state.camera.theta > PG_PI - 0.001f)
                        {
                            app_state.camera.theta = PG_PI - 0.001f;
                        }
                    }
                }
            }
        }
        pg_f32_3x new_camera_position
            = {.x = pg_f32_sin(app_state.camera.theta)
                    * pg_f32_cos(app_state.camera.phi),
               .y = pg_f32_cos(app_state.camera.theta),
               .z = pg_f32_sin(app_state.camera.theta)
                    * pg_f32_sin(app_state.camera.phi)};
        app_state.camera.position
            = pg_f32_3x_mul_scalar(new_camera_position, (f32)camera_zoom);

        if (app_state.auto_rotate)
        {
            app_state.rotation.y += 0.5f;
            if (app_state.rotation.y > 359.0f)
            {
                app_state.rotation.y = 0.0f;
            }
        }

        *running_time_step -= config.app_fixed_time_step;
    }

    frame_data fd = {.projection_mtx = *projection_mtx,
                     .view_mtx = pg_f32_4x4_look_at(&app_state.camera),
                     .light_dir = app_state.light_dir,
                     .camera_pos = app_state.camera.position};
    object_data od
        = {.model_mtx = pg_f32_4x4_place(model_scaling[app_state.art_id],
                                         app_state.rotation,
                                         (pg_f32_3x){0})};
    pg_dynamic_cb_data_update(dynamic_cb_data, &fd, &od, MAX_OBJECT_COUNT, err);
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
    if (app_state.assets.art_count != ART_COUNT)
    {
        err.log(&err,
                PG_ERROR_MAJOR,
                "wWinMain: unexpected number of art assets");
    }

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

        app_state.mouse_cursor = windows.input.mouse.cursor;

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
                                   &app_state.art_id,
                                   MAX_OBJECT_COUNT,
                                   &dynamic_cb_data,
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
