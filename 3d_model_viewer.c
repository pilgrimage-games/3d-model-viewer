#define PG_APP_NAME "3D Model Viewer"
#define PG_APP_GPU_RENDERING
#define PG_APP_IMGUI

#define CAMERA_AUTO_ROTATION_RATE 30.0f    // degrees/sec
#define CAMERA_MANUAL_ROTATION_RATE 135.0f // degrees/sec

#if defined(WINDOWS)
#include <windows/pg_windows.h>
#else
static_assert(0, "no supported platform is defined");
#endif

typedef enum
{
    GRAPHICS_BUFFER_PER_FRAME_CB,
    GRAPHICS_BUFFER_VERTEX_SB,
    GRAPHICS_BUFFER_INDEX_SB,
    GRAPHICS_BUFFER_MATERIAL_PROPERTIES_SB,
    GRAPHICS_BUFFER_COUNT
} graphics_buffer;

// NOTE: This represents constant buffer data, which requires 16-byte alignment.
// This may require padding a struct member to 16 bytes.
typedef struct
{
    u32 vertex_offset;
    u32 index_offset;
    u32 material_id;
    u32 texture_id;
    pg_f32_4x4 global_transform;
} constants_cb;

// NOTE: This represents constant buffer data, which requires 16-byte alignment.
// This may require padding a struct member to 16 bytes.
typedef struct
{
    pg_f32_4x4 world_from_model;
    pg_f32_4x4 clip_from_world;
    pg_f32_3x camera_pos;
    f32 padding0;
} per_frame_cb;

typedef struct
{
    b8 fullscreen;
    b8 vsync;
    b8 wireframe_mode;
    b8 auto_rotate;
    u32 model_id;
    u32 animation_id;
    f32 running_animation_time; // in ms
    pg_f32_2x previous_cursor_position;
    pg_f32_3x scaling;
    pg_f32_3x rotation;
    pg_f32_3x translation;
    pg_camera camera;                   // align: 4
    pg_graphics_api gfx_api;            // align: 4
    pg_graphics_api supported_gfx_apis; // align: 4
    pg_graphics_metrics* metrics;
} application_state;

typedef struct
{
    u32 model_id_last_frame;
    u32 max_vertex_count;
    u32 max_index_count;
    u32 max_material_count;
    u32 total_texture_count;
} models_metadata;

typedef enum
{
    MODEL_NONE,
    MODEL_ABSTRACT_RAINBOW_TRANSLUCENT_PENDANT,
    MODEL_BAKER_AND_THE_BRIDGE,
    MODEL_BOX_ANIMATED,
    MODEL_CORSET,
    MODEL_DAMAGED_HELMET,
    MODEL_FTM,
    MODEL_METAL_ROUGH_SPHERES,
    MODEL_PLAYSTATION_1,
    MODEL_WATER_BOTTLE,
    MODEL_COUNT
} asset_type_model;

GLOBAL c8* model_names[] = {"None",
                            "Abstract Rainbow Translucent Pendant",
                            "Baker and the Bridge",
                            "Box Animated",
                            "Corset",
                            "Damaged Helmet",
                            "Ftm",
                            "Metal-Rough Spheres",
                            "PlayStation 1",
                            "Water Bottle"};

GLOBAL pg_config config = {.gamepad_count = 1,
                           .input_repeat_rate = 750.0f,
                           .permanent_mem_size = 1024u * PG_MEBIBYTE,
                           .transient_mem_size = 128u * PG_KIBIBYTE,
                           .min_gpu_mem_size = 512u * PG_MEBIBYTE};

GLOBAL application_state app_state
    = {.vsync = true,
       .auto_rotate = true,
       .model_id = MODEL_FTM,
       .camera = {.arcball = true, .up_axis = {.y = 1.0f}}};

FUNCTION void
reset_view(void)
{
    app_state.auto_rotate = true;
    app_state.scaling = (pg_f32_3x){0};
    app_state.rotation = (pg_f32_3x){0};
    app_state.translation = (pg_f32_3x){0};
    app_state.camera.position
        = (pg_f32_3x){.x = PG_PI / 2.0f, .y = PG_PI / 2.0f, .z = 6.0f};
    switch (app_state.model_id)
    {
        case MODEL_ABSTRACT_RAINBOW_TRANSLUCENT_PENDANT:
        {
            app_state.scaling = pg_f32_3x_pack(0.8f);
            break;
        }
        case MODEL_BAKER_AND_THE_BRIDGE:
        {
            app_state.scaling = pg_f32_3x_pack(0.06f);
            app_state.camera.position.y = PG_PI / 3.0f;
            break;
        }
        case MODEL_BOX_ANIMATED:
        {
            app_state.scaling = pg_f32_3x_pack(0.45f);
            app_state.camera.position.y = PG_PI / 4.0f;
            break;
        }
        case MODEL_CORSET:
        {
            app_state.scaling = pg_f32_3x_pack(35.0f);
            app_state.translation.y = -1.0f;
            break;
        }
        case MODEL_DAMAGED_HELMET:
        {
            app_state.scaling = pg_f32_3x_pack(1.125f);
            break;
        }
        case MODEL_FTM:
        {
            app_state.scaling = pg_f32_3x_pack(0.13f);
            app_state.rotation.y = 135.0f;
            app_state.camera.position.y = PG_PI / 2.5f;
            break;
        }
        case MODEL_METAL_ROUGH_SPHERES:
        {
            app_state.scaling = pg_f32_3x_pack(0.2f);
            break;
        }
        case MODEL_PLAYSTATION_1:
        {
            app_state.scaling = pg_f32_3x_pack(0.5f);
            app_state.rotation.x = 120.0f;
            app_state.rotation.z = 270.0f;
            break;
        }
        case MODEL_WATER_BOTTLE:
        {
            app_state.scaling = pg_f32_3x_pack(8.0f);
            app_state.rotation.y = 225.0f;
            break;
        }
    }
}

FUNCTION void
imgui_ui(void)
{
#if defined(PG_APP_IMGUI)
    pg_imgui_graphics_header(app_state.supported_gfx_apis,
                             app_state.metrics,
                             &app_state.gfx_api,
                             &app_state.fullscreen,
                             &app_state.vsync,
                             &app_state.wireframe_mode);

    b8 model_selection_active
        = ImGui_CollapsingHeader("Models", ImGuiTreeNodeFlags_DefaultOpen);
    if (model_selection_active)
    {
        ImGui_SeparatorText("Model Selection");
        u32 model_id = app_state.model_id;
        for (asset_type_model i = 1; i < MODEL_COUNT; i += 1)
        {
            ImGui_RadioButtonIntPtr(model_names[i],
                                    (s32*)&app_state.model_id,
                                    i);
        }
        if (app_state.model_id != model_id)
        {
            reset_view();
        }
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
        ImGui_Text("[View]: Toggle Fullscreen");
    }
#endif
}

FUNCTION void
init_app(pg_assets* assets,
         pg_scratch_allocator* permanent_mem,
         models_metadata* metadata,
         pg_graphics_renderer_data* renderer_data,
         pg_error* err)
{
    b8 ok = true;

    // Get models metadata.
    for (u32 i = 0; i < assets->model_count; i += 1)
    {
        pg_asset_model* model = &assets->models[i];

        if (model->vertex_count > metadata->max_vertex_count)
        {
            metadata->max_vertex_count = model->vertex_count;
        }

        if (model->index_count > metadata->max_index_count)
        {
            metadata->max_index_count = model->index_count;
        }

        if (model->material_count > metadata->max_material_count)
        {
            metadata->max_material_count = model->material_count;
        }

        for (u32 j = 0; j < model->material_count; j += 1)
        {
            metadata->total_texture_count += model->materials[j].texture_count;
        }
    }

    // Initialize renderer data.
    {
        pg_graphics_buffer_data buffer_data[]
            = {{.id = GRAPHICS_BUFFER_PER_FRAME_CB,
                .shader_stage = PG_SHADER_STAGE_VERTEX,
                .max_elem_count = 1,
                .elem_size = sizeof(per_frame_cb)},
               {.id = GRAPHICS_BUFFER_VERTEX_SB,
                .shader_stage = PG_SHADER_STAGE_VERTEX,
                .max_elem_count = metadata->max_vertex_count,
                .elem_size = sizeof(pg_vertex)},
               {.id = GRAPHICS_BUFFER_INDEX_SB,
                .shader_stage = PG_SHADER_STAGE_VERTEX,
                .max_elem_count = metadata->max_index_count,
                .elem_size = sizeof(PG_GRAPHICS_INDEX_TYPE)},
               {.id = GRAPHICS_BUFFER_MATERIAL_PROPERTIES_SB,
                .shader_stage = PG_SHADER_STAGE_PIXEL,
                .max_elem_count = metadata->max_material_count,
                .elem_size = sizeof(pg_asset_material_properties)}};
        static_assert(CAP(buffer_data) == GRAPHICS_BUFFER_COUNT);

        *renderer_data = (pg_graphics_renderer_data){
            .wireframe = app_state.wireframe_mode,
            .render_target_srgb = true,
            .depth_buffer_bit_count = 32,
            .constant_count = sizeof(constants_cb) / sizeof(u32),
            .buffer_count = CAP(buffer_data),
            .max_texture_count = assets->model_count
                                 * metadata->max_material_count
                                 * PG_TEXTURE_TYPE_COUNT};

        ok &= pg_scratch_alloc(permanent_mem,
                               renderer_data->buffer_count
                                   * sizeof(pg_graphics_buffer_data),
                               alignof(pg_graphics_buffer_data),
                               &renderer_data->buffer_data);
        if (!ok)
        {
            PG_ERROR_MAJOR("failed to get memory for buffer data");
        }
        ok &= pg_copy(
            buffer_data,
            renderer_data->buffer_count * sizeof(pg_graphics_buffer_data),
            renderer_data->buffer_data,
            renderer_data->buffer_count * sizeof(pg_graphics_buffer_data));
        if (!ok)
        {
            PG_ERROR_MAJOR("failed to copy buffer data to renderer data");
        }
    }

    reset_view();
}

FUNCTION void
update_app(pg_assets* assets,
           pg_input* input,
           models_metadata* metadata,
           pg_f32_2x render_res,
           pg_scratch_allocator* transient_mem,
           pg_graphics_renderer_data* renderer_data,
           pg_error* err)
{
    b8 ok = true;

    f32 frame_time = app_state.metrics->cpu_last_frame_time;

    // Process input.
    pg_f32_2x cursor_delta = {0};
    {
        // Left/Up: Previous Model
        if (pg_button_active(&input->kbd.left, config.input_repeat_rate)
            || pg_button_active(&input->kbd.up, config.input_repeat_rate)
            || pg_button_active(&input->gp[0].left, config.input_repeat_rate)
            || pg_button_active(&input->gp[0].up, config.input_repeat_rate))
        {
            if (app_state.model_id == 1)
            {
                app_state.model_id = assets->model_count - 1;
            }
            else
            {
                app_state.model_id -= 1;
            }
            reset_view();
        }

        // Right/Down: Next Model
        if (pg_button_active(&input->kbd.right, config.input_repeat_rate)
            || pg_button_active(&input->kbd.down, config.input_repeat_rate)
            || pg_button_active(&input->gp[0].right, config.input_repeat_rate)
            || pg_button_active(&input->gp[0].down, config.input_repeat_rate))
        {
            if (app_state.model_id == assets->model_count - 1)
            {
                app_state.model_id = 1;
            }
            else
            {
                app_state.model_id += 1;
            }
            reset_view();
        }

        // Gamepad: Toggle Fullscreen
        if (pg_button_active(&input->gp[0].view, config.input_repeat_rate))
        {
            app_state.fullscreen = !app_state.fullscreen;
        }

        b8 mouse_active = true;
#if defined(PG_APP_IMGUI)
        if (ImGui_GetIO()->WantCaptureMouse)
        {
            mouse_active = false;
        }
#endif

        if (mouse_active && pg_button_active(&input->mouse.left, frame_time)
            && (!pg_f32_2x_eq(app_state.previous_cursor_position,
                              (pg_f32_2x){0})))

        {
            f32 cursor_multiplier = 100.0f;
            cursor_delta = pg_f32_2x_mul(
                pg_f32_2x_sub(input->mouse.cursor,
                              app_state.previous_cursor_position),
                pg_f32_2x_pack(cursor_multiplier));
        }

        if (cursor_delta.x != 0.0f || cursor_delta.y != 0.0f
            || input->gp[0].rs.x != 0.0f || input->gp[0].rs.y != 0.0f)
        {
            app_state.auto_rotate = false;
        }
    }

    pg_asset_model* curr_model = &assets->models[app_state.model_id];

    // Animate.
    {
        if (app_state.model_id != metadata->model_id_last_frame)
        {
            app_state.running_animation_time = 0.0f;
        }

        if (app_state.animation_id < curr_model->animation_count)
        {
            app_state.running_animation_time += frame_time;

            // Loop the animation.
            if (app_state.running_animation_time
                > curr_model->animations[app_state.animation_id]
                      .total_animation_time)
            {
                app_state.running_animation_time
                    -= curr_model->animations[app_state.animation_id]
                           .total_animation_time;
            }
        }
    }

    // Simulate.
    {
        // Rotate camera.
        f32 auto_rotation_rate = CAMERA_AUTO_ROTATION_RATE / PG_MS_IN_S;
        f32 manual_rotation_rate = CAMERA_MANUAL_ROTATION_RATE / PG_MS_IN_S;
        if (app_state.auto_rotate)
        {
            app_state.camera.position.x
                += pg_f32_deg_to_rad(auto_rotation_rate * frame_time);
        }
        else
        {
            app_state.camera.position.x += pg_f32_deg_to_rad(
                manual_rotation_rate * cursor_delta.x * frame_time);
            app_state.camera.position.y += pg_f32_deg_to_rad(
                manual_rotation_rate * cursor_delta.y * frame_time);
            app_state.camera.position.x += pg_f32_deg_to_rad(
                manual_rotation_rate * input->gp[0].rs.x * frame_time);
            app_state.camera.position.y += pg_f32_deg_to_rad(
                manual_rotation_rate * input->gp[0].rs.y * frame_time);
        }

        // Zoom camera.
        f32 zoom_rate = 1.0f / 150.0f;
        app_state.camera.position.z
            -= zoom_rate * (u8)input->mouse.forward.pressed * frame_time;
        app_state.camera.position.z
            += zoom_rate * (u8)input->mouse.back.pressed * frame_time;
        app_state.camera.position.z -= zoom_rate * input->gp[0].rt * frame_time;
        app_state.camera.position.z += zoom_rate * input->gp[0].lt * frame_time;

        pg_camera_clamp((pg_f32_2x){.min = 0.0f, .max = 2.0f * PG_PI},
                        (pg_f32_2x){.min = 0.0f, .max = PG_PI},
                        (pg_f32_2x){.min = 2.0f, .max = 10.0f},
                        true,
                        &app_state.camera);
    }

    // Generate matrices.
    pg_f32_3x camera_position
        = pg_camera_get_cartesian_position(&app_state.camera);
    pg_f32_4x4 world_from_model = pg_f32_4x4_world_from_model(
        app_state.scaling,
        pg_f32_4x_euler_to_quaternion(app_state.rotation),
        app_state.translation);
    pg_f32_4x4 clip_from_view = pg_f32_4x4_clip_from_view_perspective(
        27.0f,
        render_res.width / render_res.height,
        0.01f,
        16.0f);
    pg_f32_4x4 view_from_world
        = pg_f32_4x4_view_from_world(camera_position,
                                     app_state.camera.focal_point,
                                     app_state.camera.up_axis);
    pg_f32_4x4 view_from_model
        = pg_f32_4x4_mul(view_from_world, world_from_model);

    // Get drawables.
    pg_graphics_drawables drawables = {0};
    {
        pg_asset_model models[] = {*curr_model};
        u32 model_ids[] = {app_state.model_id};
        u32 animation_ids[] = {app_state.animation_id};
        f32 running_animation_times[] = {app_state.running_animation_time};
        pg_asset_model_get_drawables(models,
                                     model_ids,
                                     animation_ids,
                                     running_animation_times,
                                     CAP(models),
                                     &view_from_model,
                                     transient_mem,
                                     &drawables,
                                     err);
    }

    // Update renderer data.
    {
        // Update buffers.
        for (graphics_buffer gb = 0; gb < GRAPHICS_BUFFER_COUNT; gb += 1)
        {
            switch (gb)
            {
                case GRAPHICS_BUFFER_PER_FRAME_CB:
                {
                    pg_f32_4x4 clip_from_world
                        = pg_f32_4x4_mul(clip_from_view, view_from_world);

                    per_frame_cb* per_frame;
                    ok &= pg_scratch_alloc(transient_mem,
                                           sizeof(per_frame_cb),
                                           alignof(per_frame_cb),
                                           &per_frame);
                    if (!ok)
                    {
                        PG_ERROR_MAJOR("failed to get memory for per frame "
                                       "constant buffer");
                    }
                    *per_frame
                        = (per_frame_cb){.world_from_model = world_from_model,
                                         .clip_from_world = clip_from_world,
                                         .camera_pos = camera_position};

                    renderer_data->buffer_data[gb].elem_count = 1;
                    renderer_data->buffer_data[gb].buffer = per_frame;

                    break;
                }
                case GRAPHICS_BUFFER_VERTEX_SB:
                {
                    if (app_state.model_id != metadata->model_id_last_frame)
                    {
                        renderer_data->buffer_data[gb].elem_count
                            = curr_model->vertex_count;
                        renderer_data->buffer_data[gb].buffer
                            = curr_model->vertices;
                    }
                    break;
                }
                case GRAPHICS_BUFFER_INDEX_SB:
                {
                    if (app_state.model_id != metadata->model_id_last_frame)
                    {
                        renderer_data->buffer_data[gb].elem_count
                            = curr_model->index_count;
                        renderer_data->buffer_data[gb].buffer
                            = curr_model->indices;
                    }
                    break;
                }
                case GRAPHICS_BUFFER_MATERIAL_PROPERTIES_SB:
                {
                    pg_asset_material_properties* material_properties;
                    ok &= pg_scratch_alloc(
                        transient_mem,
                        curr_model->material_count
                            * sizeof(pg_asset_material_properties),
                        alignof(pg_asset_material_properties),
                        &material_properties);
                    if (!ok)
                    {
                        PG_ERROR_MAJOR(
                            "failed to get memory for material properties");
                    }

                    for (u32 i = 0; i < curr_model->material_count; i += 1)
                    {
                        material_properties[i]
                            = curr_model->materials[i].properties;
                    }

                    renderer_data->buffer_data[gb].elem_count
                        = curr_model->material_count;
                    renderer_data->buffer_data[gb].buffer = material_properties;

                    break;
                }
            }
        }

        // Declare (required and optional) textures for upcoming frame.
        {
            ok &= pg_scratch_alloc(transient_mem,
                                   metadata->total_texture_count
                                       * sizeof(pg_graphics_texture_data),
                                   alignof(pg_graphics_texture_data),
                                   &renderer_data->texture_data);
            if (!ok)
            {
                PG_ERROR_MAJOR("failed to get memory for texture data");
            }

            // Consider textures for the current model required and textures
            // for all other models in priority order (i.e. +/-1, +/-2, etc)
            // optional.
            u32 required_texture_count = 0;
            u32 optional_texture_count = 0;
            if (app_state.model_id != metadata->model_id_last_frame)
            {
                s32 offset = 0;
                b8 positive = true;
                for (u32 i = 0; i < assets->model_count; i += 1)
                {
                    s32 model_id = 0;
                    if (i == 0)
                    {
                        model_id = (s32)app_state.model_id;
                    }
                    else if (positive)
                    {
                        model_id = ((s32)app_state.model_id + offset)
                                   % (s32)assets->model_count;
                    }
                    else
                    {
                        model_id = ((s32)app_state.model_id - offset)
                                   % (s32)assets->model_count;
                        if (model_id < 0)
                        {
                            model_id += assets->model_count;
                        }
                    }

                    pg_asset_model* model = &assets->models[model_id];
                    for (u32 j = 0; j < model->material_count; j += 1)
                    {
                        for (u32 k = 0; k < model->materials[j].texture_count;
                             k += 1)
                        {
                            renderer_data
                                ->texture_data[required_texture_count
                                               + optional_texture_count]
                                = (pg_graphics_texture_data){
                                    .id = (u32)pg_3d_to_1d_index(
                                        model->materials[j].textures[k].type,
                                        j,
                                        model_id,
                                        PG_TEXTURE_TYPE_COUNT,
                                        metadata->max_material_count),
                                    .texture
                                    = &(model->materials[j].textures[k])};

                            if (i == 0)
                            {
                                required_texture_count += 1;
                            }
                            else
                            {
                                optional_texture_count += 1;
                            }
                        }
                    }

                    if (i != 0)
                    {
                        positive = !positive;
                    }

                    if (positive)
                    {
                        offset += 1;
                    }
                }
            }

            renderer_data->required_texture_count = required_texture_count;
            renderer_data->optional_texture_count = optional_texture_count;
        }

        // Set draw data.
        {
            ok &= pg_scratch_alloc(transient_mem,
                                   drawables.drawable_count
                                       * sizeof(pg_graphics_draw_data),
                                   alignof(pg_graphics_draw_data),
                                   &renderer_data->draw_data);
            if (!ok)
            {
                PG_ERROR_MAJOR("failed to get memory for draw data");
            }

            for (u32 i = 0; i < drawables.drawable_count; i += 1)
            {
                pg_graphics_drawable* d = &drawables.drawables[i];
                constants_cb* constants;
                ok &= pg_scratch_alloc(transient_mem,
                                       sizeof(constants_cb),
                                       alignof(constants_cb),
                                       &constants);
                if (!ok)
                {
                    PG_ERROR_MAJOR("failed to get memory for constants "
                                   "constant buffer");
                }
                *constants
                    = (constants_cb){.vertex_offset = d->vertex_offset,
                                     .index_offset = d->index_offset,
                                     .material_id = d->material_id,
                                     .texture_id = (u32)pg_3d_to_1d_index(
                                         0,
                                         d->material_id,
                                         d->art_id,
                                         PG_TEXTURE_TYPE_COUNT,
                                         metadata->max_material_count),
                                     .global_transform = d->global_transform};

                renderer_data->draw_data[i] = (pg_graphics_draw_data){
                    .opaque
                    = i < drawables.opaque_drawable_count ? true : false,
                    .vertex_count = d->index_count,
                    .instance_count = 1,
                    .start_texture_id = constants->texture_id,
                    .texture_count = PG_TEXTURE_TYPE_COUNT,
                    .constants = constants};
            }

            renderer_data->wireframe = app_state.wireframe_mode;
            renderer_data->draw_count = drawables.drawable_count;
        }
    }
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

    models_metadata metadata = {0};

    pg_windows_init_window(&windows.window,
                           inst,
                           config.fixed_aspect_ratio_width,
                           config.fixed_aspect_ratio_height,
                           &app_state.fullscreen,
                           &err);
    pg_windows_init_mem(&windows,
                        config.permanent_mem_size,
                        config.transient_mem_size,
                        &err);
    pg_assets* assets = pg_assets_read_pga(&windows.permanent_mem,
                                           &pg_windows_read_file,
                                           &err);
    pg_assets_verify(assets, 0, 0, MODEL_COUNT, 0, &err);
    static_assert(CAP(model_names) == MODEL_COUNT);

    init_app(assets,
             &windows.permanent_mem,
             &metadata,
             &windows.gfx.renderer_data,
             &err);

    pg_windows_init_graphics(&windows,
                             config.min_gpu_mem_size,
                             windows.gfx.renderer_data,
                             app_state.vsync,
                             &app_state.gfx_api,
                             &app_state.supported_gfx_apis,
                             &err);
    pg_windows_init_metrics(&windows.metrics, &err);
    app_state.metrics = &windows.metrics.gfx_metrics;

    while (windows.msg.message != WM_QUIT)
    {
        if (PeekMessageW(&windows.msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&windows.msg);
            DispatchMessageW(&windows.msg);
            continue;
        }

        pg_graphics_api gfx_api = app_state.gfx_api;

        app_state.previous_cursor_position = windows.input.mouse.cursor;
        pg_windows_update_input(&windows,
                                config.gamepad_count,
                                &app_state.fullscreen,
                                &err);

        update_app(assets,
                   &windows.input,
                   &metadata,
                   windows.window.render_res,
                   &windows.transient_mem,
                   &windows.gfx.renderer_data,
                   &err);
        metadata.model_id_last_frame = app_state.model_id;

        pg_windows_update_graphics(&windows,
                                   app_state.gfx_api,
                                   windows.gfx.renderer_data,
                                   app_state.fullscreen,
                                   app_state.vsync,
                                   &imgui_ui,
                                   &err);

        pg_windows_update_metrics(&windows.metrics, &err);

        if (app_state.gfx_api != gfx_api)
        {
            metadata.model_id_last_frame = 0;
            pg_windows_reload_graphics(&windows,
                                       inst,
                                       config.min_gpu_mem_size,
                                       windows.gfx.renderer_data,
                                       config.fixed_aspect_ratio_width,
                                       config.fixed_aspect_ratio_height,
                                       &app_state.fullscreen,
                                       app_state.vsync,
                                       &app_state.gfx_api,
                                       &app_state.supported_gfx_apis,
                                       &err);
        }

        pg_scratch_free(&windows.transient_mem);
    }

    pg_windows_release(&windows);

    return 0;
}
#endif
