#define PG_APP_NAME "3D Model Viewer"
#define PG_APP_GPU_RENDERING
#define PG_APP_IMGUI

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

typedef enum
{
    INPUT_ACTION_TYPE_NONE,
    INPUT_ACTION_TYPE_NEXT,
    INPUT_ACTION_TYPE_PREVIOUS,
    INPUT_ACTION_TYPE_ROTATE,
    INPUT_ACTION_TYPE_ZOOM_IN,
    INPUT_ACTION_TYPE_ZOOM_OUT,
    INPUT_ACTION_TYPE_ZOOM,
    INPUT_ACTION_TYPE_COUNT
} input_action_type;

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
    f32 repeat_rate;
    input_action_type type; // align: 4
} input_action;

typedef struct
{
    b8 fullscreen;
    b8 vsync;
    b8 wireframe_mode;
    b8 auto_rotate;
    u32 model_id;
    u32 animation_id;
    f32 running_animation_time; // in ms
    pg_f32_3x scaling;
    pg_f32_3x rotation;
    pg_f32_3x translation;
    pg_camera camera;                                         // align: 4
    input_action input_action_map[PG_INPUT_EVENT_TYPE_COUNT]; // align: 4
    pg_graphics_api gfx_api;                                  // align: 4
    pg_graphics_api supported_gfx_apis;                       // align: 4
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
                           .input_queue_event_count = 10,
                           .permanent_mem_size = PG_MEBIBYTE(1024),
                           .transient_mem_size = PG_KIBIBYTE(128),
                           .min_gpu_mem_size = PG_MEBIBYTE(512)};

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

    if (app_state.model_id == MODEL_ABSTRACT_RAINBOW_TRANSLUCENT_PENDANT)
    {
        app_state.scaling = pg_f32_3x_pack(0.8f);
    }
    else if (app_state.model_id == MODEL_BAKER_AND_THE_BRIDGE)
    {
        app_state.scaling = pg_f32_3x_pack(0.06f);
        app_state.camera.position.y = PG_PI / 3.0f;
    }
    else if (app_state.model_id == MODEL_BOX_ANIMATED)
    {
        app_state.scaling = pg_f32_3x_pack(0.45f);
        app_state.camera.position.y = PG_PI / 4.0f;
    }
    else if (app_state.model_id == MODEL_CORSET)
    {
        app_state.scaling = pg_f32_3x_pack(35.0f);
        app_state.translation.y = -1.0f;
    }
    else if (app_state.model_id == MODEL_DAMAGED_HELMET)
    {
        app_state.scaling = pg_f32_3x_pack(1.125f);
    }
    else if (app_state.model_id == MODEL_FTM)
    {
        app_state.scaling = pg_f32_3x_pack(0.13f);
        app_state.rotation.y = 135.0f;
        app_state.camera.position.y = PG_PI / 2.5f;
    }
    else if (app_state.model_id == MODEL_METAL_ROUGH_SPHERES)
    {
        app_state.scaling = pg_f32_3x_pack(0.2f);
    }
    else if (app_state.model_id == MODEL_PLAYSTATION_1)
    {
        app_state.scaling = pg_f32_3x_pack(0.5f);
        app_state.rotation.x = 120.0f;
        app_state.rotation.z = 270.0f;
    }
    else if (app_state.model_id == MODEL_WATER_BOTTLE)
    {
        app_state.scaling = pg_f32_3x_pack(8.0f);
        app_state.rotation.y = 225.0f;
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
        ImGui_Text("[Scroll]: Zoom In/Zoom Out");
    }

    b8 keyboard_controls_active
        = ImGui_CollapsingHeader("Keyboard Controls",
                                 ImGuiTreeNodeFlags_DefaultOpen);
    if (keyboard_controls_active)
    {
        ImGui_Text("[W/A/Left/Up]: Previous Model");
        ImGui_Text("[D/S/Right/Down]: Next Model");
        ImGui_Text("[Alt+Enter]: Toggle Fullscreen");
    }

    b8 gamepad_controls_active
        = ImGui_CollapsingHeader("Gamepad Controls",
                                 ImGuiTreeNodeFlags_DefaultOpen);
    if (gamepad_controls_active)
    {
        ImGui_Text("[D-Pad Left/Up]: Previous Model");
        ImGui_Text("[D-Pad Right/Down]: Next Model");
        ImGui_Text("[Left/Right Stick]: Rotate");
        ImGui_Text("[Right Trigger/Left Trigger]: Zoom In/Zoom Out");
    }
#endif
}

FUNCTION void
init_app(pg_assets* assets,
         pg_scratch_allocator* permanent_mem,
         models_metadata* metadata,
         pg_input_queue* input_queue,
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

    // Initialize input queue.
    ok &= pg_scratch_alloc(permanent_mem,
                           config.input_queue_event_count
                               * sizeof(pg_input_event),
                           alignof(pg_input_event),
                           &input_queue->events);
    if (!ok)
    {
        PG_ERROR_MAJOR("failed to get memory for input queue");
    }
    input_queue->event_count = config.input_queue_event_count;

    // Set input action map.
    for (pg_input_event_type et = 0; et < PG_INPUT_EVENT_TYPE_COUNT; et += 1)
    {
        input_action* at = &app_state.input_action_map[et];
        switch (et)
        {
            case PG_KEYBOARD_S:
            case PG_KEYBOARD_DOWN:
            case PG_GAMEPAD_DOWN:
            case PG_KEYBOARD_D:
            case PG_KEYBOARD_RIGHT:
            case PG_GAMEPAD_RIGHT:
            {
                at->repeat_rate = PG_MILLISECOND(1.0f / 2.0f);
                at->type = INPUT_ACTION_TYPE_NEXT;
                break;
            }
            case PG_KEYBOARD_W:
            case PG_KEYBOARD_UP:
            case PG_GAMEPAD_UP:
            case PG_KEYBOARD_A:
            case PG_KEYBOARD_LEFT:
            case PG_GAMEPAD_LEFT:
            {
                at->repeat_rate = PG_MILLISECOND(1.0f / 2.0f);
                at->type = INPUT_ACTION_TYPE_PREVIOUS;
                break;
            }
            case PG_MOUSE_MOVED:
            case PG_GAMEPAD_LS_MOVED:
            case PG_GAMEPAD_RS_MOVED:
            {
                at->type = INPUT_ACTION_TYPE_ROTATE;
                break;
            }
            case PG_GAMEPAD_LT:
            {
                at->type = INPUT_ACTION_TYPE_ZOOM_OUT;
                break;
            }
            case PG_GAMEPAD_RT:
            {
                at->type = INPUT_ACTION_TYPE_ZOOM_IN;
                break;
            }
            case PG_MOUSE_SCROLLED:
            {
                at->type = INPUT_ACTION_TYPE_ZOOM;
                break;
            }
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
process_action(input_action_type at, pg_f32_2x event_value, pg_error* err)
{
    f32 frame_time = app_state.metrics->cpu_last_frame_time;

    switch (at)
    {
        case INPUT_ACTION_TYPE_NEXT:
        {
            app_state.model_id = (app_state.model_id == MODEL_COUNT - 1)
                                     ? 1
                                     : app_state.model_id + 1;
            reset_view();
            break;
        }
        case INPUT_ACTION_TYPE_PREVIOUS:
        {
            app_state.model_id = (app_state.model_id == 1)
                                     ? MODEL_COUNT - 1
                                     : app_state.model_id - 1;
            reset_view();
            break;
        }
        case INPUT_ACTION_TYPE_ROTATE:
        {
            app_state.auto_rotate = false;

            f32 manual_rotation_rate = 135.0f; // degrees/ms
            pg_f32_2x rotation_speed
                = pg_f32_2x_mul(pg_f32_2x_pack((manual_rotation_rate
                                                * (1.0f / PG_MILLISECOND(1)))
                                               * frame_time),
                                event_value);
            app_state.camera.position.x += pg_f32_deg_to_rad(rotation_speed.x);
            app_state.camera.position.y += pg_f32_deg_to_rad(rotation_speed.y);
            pg_camera_clamp((pg_f32_2x){.min = 0.0f, .max = 2.0f * PG_PI},
                            (pg_f32_2x){.min = 0.0f, .max = PG_PI},
                            (pg_f32_2x){.min = 2.0f, .max = 10.0f},
                            true,
                            &app_state.camera);
            break;
        }
        case INPUT_ACTION_TYPE_ZOOM_IN:
        case INPUT_ACTION_TYPE_ZOOM_OUT:
        case INPUT_ACTION_TYPE_ZOOM:
        {
            if (at == INPUT_ACTION_TYPE_ZOOM_OUT)
            {
                event_value.x *= -1.0f;
            }
            f32 zoom_rate = 1.0f / 150.0f;
            f32 zoom_speed = zoom_rate * event_value.x * frame_time;
            app_state.camera.position.z -= zoom_speed;
            pg_camera_clamp((pg_f32_2x){.min = 0.0f, .max = 2.0f * PG_PI},
                            (pg_f32_2x){.min = 0.0f, .max = PG_PI},
                            (pg_f32_2x){.min = 2.0f, .max = 10.0f},
                            true,
                            &app_state.camera);
            break;
        }
        default:
        {
            PG_ERROR_MINOR("unexpected input action type");
            break;
        }
    }
}

FUNCTION void
update_app(pg_assets* assets,
           pg_input_queue* iq,
           models_metadata* metadata,
           pg_f32_2x render_res,
           pg_scratch_allocator* transient_mem,
           pg_graphics_renderer_data* renderer_data,
           pg_error* err)
{
    b8 ok = true;

    f32 frame_time = app_state.metrics->cpu_last_frame_time;

    // Process input.
    {
        // Process inputs in event queue.
        for (iq->read_idx; iq->read_idx != iq->write_idx;
             iq->read_idx = (iq->read_idx + 1) % iq->event_count)
        {
            pg_input_event ie = iq->events[iq->read_idx];
            input_action* ia = &app_state.input_action_map[ie.event_type];

            // Skip any input event that does not have a mapped input action.
            if (!ia->type)
            {
                continue;
            }

            pg_f32_2x event_value = ie.value;
            {
                // Handle mouse click-to-drag.
                if (ia->type == INPUT_ACTION_TYPE_ROTATE
                    && ie.input_type == PG_INPUT_TYPE_MOUSE)
                {
                    // Skip if left mouse button is not held down.
                    if (iq->duration_held[PG_MOUSE_LEFT] == 0.0f)
                    {
                        continue;
                    }

                    // Compute the vector from the previous mouse position to
                    // the current one.
                    b8 drag = false;
                    for (u32 read_idx = (iq->read_idx + iq->event_count - 1)
                                        % iq->event_count;
                         read_idx != iq->write_idx;
                         read_idx
                         = (read_idx + iq->event_count - 1) % iq->event_count)
                    {
                        pg_input_event prev_ie = iq->events[read_idx];

                        // Don't look past the start of the chord.
                        if (prev_ie.event_type == PG_MOUSE_LEFT)
                        {
                            break;
                        }

                        if (prev_ie.event_type == PG_MOUSE_MOVED)
                        {
                            f32 cursor_multipler = 100.0f;
                            event_value = pg_f32_2x_mul(
                                pg_f32_2x_sub(ie.value, prev_ie.value),
                                pg_f32_2x_pack(cursor_multipler));
                            drag = true;
                            break;
                        }
                    }

                    // Skip if mouse not dragged.
                    if (!drag)
                    {
                        continue;
                    }
                }

                // Handle mouse scrolling.
                if (ia->type == INPUT_ACTION_TYPE_ZOOM
                    && ie.event_type == PG_MOUSE_SCROLLED)
                {
                    f32 scroll_multiplier = 5.0f;
                    event_value
                        = pg_f32_2x_mul(ie.value,
                                        pg_f32_2x_pack(scroll_multiplier));
                }
            }

            if (iq->duration_held[ie.event_type] > 0.0f)
            {
                process_action(ia->type, event_value, err);
            }
        }

        // Process held inputs.
        for (pg_input_event_type et = 0; et < CAP(iq->duration_held); et += 1)
        {
            input_action* ia = &app_state.input_action_map[et];

            if (ia->repeat_rate != 0.0f
                && iq->duration_held[et] > ia->repeat_rate)
            {
                process_action(ia->type, pg_f32_2x_pack(0.0f), err);
                iq->duration_held[et] -= ia->repeat_rate;
            }
        }
    }

    pg_asset_model* curr_model = &assets->models[app_state.model_id];

    // Animate.
    {
        if (app_state.auto_rotate)
        {
            f32 auto_rotation_rate = 30.0f; // degrees/sec
            f32 rotation_speed
                = (auto_rotation_rate * (1.0f / PG_MILLISECOND(1)))
                  * frame_time;
            app_state.camera.position.x += pg_f32_deg_to_rad(rotation_speed);
            pg_camera_clamp((pg_f32_2x){.min = 0.0f, .max = 2.0f * PG_PI},
                            (pg_f32_2x){.min = 0.0f, .max = PG_PI},
                            (pg_f32_2x){.min = 2.0f, .max = 10.0f},
                            true,
                            &app_state.camera);
        }

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
            if (gb == GRAPHICS_BUFFER_PER_FRAME_CB)
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
            }
            else if (gb == GRAPHICS_BUFFER_VERTEX_SB)
            {
                if (app_state.model_id != metadata->model_id_last_frame)
                {
                    renderer_data->buffer_data[gb].elem_count
                        = curr_model->vertex_count;
                    renderer_data->buffer_data[gb].buffer
                        = curr_model->vertices;
                }
            }
            else if (gb == GRAPHICS_BUFFER_INDEX_SB)
            {
                if (app_state.model_id != metadata->model_id_last_frame)
                {
                    renderer_data->buffer_data[gb].elem_count
                        = curr_model->index_count;
                    renderer_data->buffer_data[gb].buffer = curr_model->indices;
                }
            }
            else if (gb == GRAPHICS_BUFFER_MATERIAL_PROPERTIES_SB)
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

    pg_windows_init_window(&windows,
                           inst,
                           config.fixed_aspect_ratio_width,
                           config.fixed_aspect_ratio_height,
                           &app_state.fullscreen,
                           &err);
    pg_windows_init_memory(&windows,
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
             &windows.input_queue,
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

        pg_windows_update_input(&windows, config.gamepad_count, &err);

        update_app(assets,
                   &windows.input_queue,
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
