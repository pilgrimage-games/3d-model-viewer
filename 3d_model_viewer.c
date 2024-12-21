#define PG_APP_NAME "3D Model Viewer"
#define PG_APP_D3D11
#define PG_APP_D3D12
#define PG_APP_OPENGL
#define PG_APP_VULKAN
#define PG_APP_IMGUI

#define CAMERA_AUTO_ROTATION_RATE 30.0f    // degrees/sec
#define CAMERA_MANUAL_ROTATION_RATE 180.0f // degrees/sec

#if defined(WINDOWS)
#include <windows/pg_windows.h>
#else
static_assert(0, "no supported platform is defined");
#endif

typedef enum
{
    GRAPHICS_RESOURCE_PER_DRAW_CONSTANTS,
    GRAPHICS_RESOURCE_PER_FRAME_CB,
    GRAPHICS_RESOURCE_VERTEX_SB,
    GRAPHICS_RESOURCE_INDEX_SB,
    GRAPHICS_RESOURCE_MATERIAL_PROPERTIES_SB,
    GRAPHICS_RESOURCE_MATERIAL,
    GRAPHICS_RESOURCE_COUNT
} graphics_resources;

// NOTE: This represents constant buffer data, which requires 16-byte alignment.
// This may require padding a struct member to 16 bytes.
typedef struct
{
    pg_f32_4x4 world_from_model;
    pg_f32_4x4 clip_from_world;
    pg_f32_3x light_dir;
    f32 padding0;
    pg_f32_3x camera_pos;
    f32 padding1;
} per_frame_cb;

// NOTE: This represents constant buffer data, which requires 16-byte alignment.
// This may require padding a struct member to 16 bytes.
typedef struct
{
    u32 material_id;
    u32 texture_offset;
    u32 vertex_offset;
    u32 index_offset;
} constants_cb;

typedef struct
{
    b8 vsync;
    b8 auto_rotate;
    u32 model_id;
    f32 fps;
    f32 frame_time;
    f32 center_zoom;
    f32 running_simulation_time; // in ms
    pg_f32_3x rotation;
    pg_f32_3x light_dir;
    pg_camera camera;        // align: 4
    pg_graphics_api gfx_api; // align: 4
} application_state;

typedef struct
{
    u32 model_id_last_frame;
    u32 max_vertex_count;
    u32 max_index_count;
    u32 max_material_count;
    u32 max_mesh_count;
    pg_f32_3x* scaling;
} models_metadata;

typedef enum
{
    MODEL_NONE,
    MODEL_BAKER_AND_THE_BRIDGE,
    MODEL_CORSET,
    MODEL_DAMAGED_HELMET,
    MODEL_FTM,
    MODEL_METAL_ROUGH_SPHERES,
    MODEL_PLAYSTATION_1,
    MODEL_SHIP_IN_A_BOTTLE,
    MODEL_WATER_BOTTLE,
    MODEL_COUNT
} asset_type_model;

GLOBAL c8* model_names[] = {"None",
                            "Baker and the Bridge",
                            "Corset",
                            "Damaged Helmet",
                            "Ftm",
                            "Metal Rough Spheres",
                            "PlayStation 1",
                            "Ship in a Bottle",
                            "Water Bottle"};

GLOBAL pg_config config = {.gamepad_count = 1,
                           .input_repeat_rate = 750.0f,
                           .simulation_time_step = (1.0f / 480.0f) * PG_MS_IN_S,
                           .permanent_mem_size = 1250u * PG_MEBIBYTE,
                           .transient_mem_size = 100u * PG_KIBIBYTE,
                           .gfx_cpu_mem_size = 500u * PG_MEBIBYTE,
                           .gfx_gpu_mem_size = 750u * PG_MEBIBYTE};

GLOBAL application_state app_state
    = {.vsync = true,
       .auto_rotate = true,
       .model_id = MODEL_BAKER_AND_THE_BRIDGE,
       .light_dir = {.x = -0.5f, .y = 0.0f, .z = -1.0f},
       .camera = {.arcball = true, .up_axis = {.y = 1.0f}},
       .gfx_api = PG_GRAPHICS_API_D3D11};

FUNCTION void
reset_view(void)
{
    app_state.auto_rotate = true;
    app_state.rotation = (pg_f32_3x){.x = 0.0f, .y = 0.0f, .z = 0.0f};
    app_state.camera.position
        = (pg_f32_3x){.x = PG_PI / 2.0f, .y = PG_PI / 2.0f, .z = 6.0f};
    switch (app_state.model_id)
    {
        case MODEL_BAKER_AND_THE_BRIDGE:
        {
            app_state.camera.position.y = PG_PI / 3.0f;
            break;
        }
        case MODEL_CORSET:
        {
            app_state.camera.position.y = PG_PI / 3.0f;
            break;
        }
        case MODEL_DAMAGED_HELMET:
        {
            break;
        }
        case MODEL_FTM:
        {
            app_state.rotation.y = 135.0f;
            app_state.camera.position.y = PG_PI / 2.5f;
            app_state.camera.position.z = 1.25f;
            break;
        }
        case MODEL_METAL_ROUGH_SPHERES:
        {
            break;
        }
        case MODEL_PLAYSTATION_1:
        {
            app_state.rotation.x = 90.0f;
            app_state.rotation.z = 270.0f;
            break;
        }
        case MODEL_SHIP_IN_A_BOTTLE:
        {
            app_state.camera.position.x = (3.0f * PG_PI) / 2.0f;
            app_state.camera.position.y = PG_PI / 2.0f;
            break;
        }
        case MODEL_WATER_BOTTLE:
        {
            app_state.camera.position.x = (3.0f * PG_PI) / 2.0f;
            app_state.camera.position.y = PG_PI / 2.0f;
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
init_app(pg_assets* assets,
         pg_scratch_allocator* permanent_mem,
         models_metadata* metadata,
         pg_graphics_message** init_gfx_msgs,
         u32* init_gfx_msg_count,
         pg_error* err)
{
    b8 ok = true;

    ok &= pg_scratch_alloc(permanent_mem,
                           assets->model_count * sizeof(pg_f32_3x),
                           alignof(pg_f32_3x),
                           &metadata->scaling);
    if (!ok)
    {
        err->log(err,
                 PG_ERROR_MAJOR,
                 "init_app: failed to get memory for model scaling metadata");
    }

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

        if (model->mesh_count > metadata->max_mesh_count)
        {
            metadata->max_mesh_count = model->mesh_count;
        }

        if (model->material_count > metadata->max_material_count)
        {
            metadata->max_material_count = model->material_count;
        }

        f32 normalized_scale = 0.0f;
        for (u32 j = 0; j < model->mesh_count; j += 1)
        {
            for (u32 k = 0; k < CAP(model->meshes[j].min_position.e); k += 1)
            {
                if (-model->meshes[j].min_position.e[k] > normalized_scale)
                {
                    normalized_scale = -model->meshes[j].min_position.e[k];
                }
            }

            for (u32 k = 0; k < CAP(model->meshes[j].max_position.e); k += 1)
            {
                if (model->meshes[j].max_position.e[k] > normalized_scale)
                {
                    normalized_scale = model->meshes[j].max_position.e[k];
                }
            }
        }

        normalized_scale
            = (normalized_scale == 0.0f) ? 1.0f : 1.0f / normalized_scale;
        metadata->scaling[i] = (pg_f32_3x){.x = normalized_scale,
                                           .y = normalized_scale,
                                           .z = normalized_scale};
    }

    // Create initialization graphics messages.
    {
        pg_graphics_message gfx_msgs[] = {
            {.type = PG_GRAPHICS_MESSAGE_TYPE_CONSTANTS,
             .constants_msg
             = {.id = GRAPHICS_RESOURCE_PER_DRAW_CONSTANTS,
                .shader_register = 0,
                .shader_stage = PG_SHADER_STAGE_VERTEX | PG_SHADER_STAGE_PIXEL,
                .elem_count = sizeof(constants_cb) / sizeof(u32)}},
            {.type = PG_GRAPHICS_MESSAGE_TYPE_BUFFER,
             .buffer_msg = {.id = GRAPHICS_RESOURCE_PER_FRAME_CB,
                            .shader_register = 1,
                            .shader_stage = PG_SHADER_STAGE_VERTEX,
                            .elem_count = 1,
                            .elem_size = sizeof(per_frame_cb)}},
            {.type = PG_GRAPHICS_MESSAGE_TYPE_BUFFER,
             .buffer_msg = {.id = GRAPHICS_RESOURCE_VERTEX_SB,
                            .shader_register = 0,
                            .shader_stage = PG_SHADER_STAGE_VERTEX,
                            .elem_count = metadata->max_vertex_count,
                            .elem_size = sizeof(pg_vertex)}},
            {.type = PG_GRAPHICS_MESSAGE_TYPE_BUFFER,
             .buffer_msg = {.id = GRAPHICS_RESOURCE_INDEX_SB,
                            .shader_register = 1,
                            .shader_stage = PG_SHADER_STAGE_VERTEX,
                            .elem_count = metadata->max_index_count,
                            .elem_size = sizeof(PG_GRAPHICS_INDEX_TYPE)}},
            {.type = PG_GRAPHICS_MESSAGE_TYPE_BUFFER,
             .buffer_msg = {.id = GRAPHICS_RESOURCE_MATERIAL_PROPERTIES_SB,
                            .shader_register = 2,
                            .shader_stage = PG_SHADER_STAGE_PIXEL,
                            .elem_count = metadata->max_material_count,
                            .elem_size = sizeof(pg_asset_material_properties)}},
            {.type = PG_GRAPHICS_MESSAGE_TYPE_MATERIAL,
             .material_msg
             = {.id = GRAPHICS_RESOURCE_MATERIAL,
                .shader_register = 3,
                .shader_register_space = 1,
                .max_material_count = metadata->max_material_count,
                .max_art_count = MODEL_COUNT,
                .texture_count = PG_TEXTURE_TYPE_COUNT}}};
        static_assert(CAP(gfx_msgs) == GRAPHICS_RESOURCE_COUNT);

        ok &= pg_scratch_alloc(permanent_mem,
                               GRAPHICS_RESOURCE_COUNT
                                   * sizeof(pg_graphics_message),
                               alignof(pg_graphics_message),
                               init_gfx_msgs);
        ok &= pg_copy(gfx_msgs,
                      GRAPHICS_RESOURCE_COUNT * sizeof(pg_graphics_message),
                      *init_gfx_msgs,
                      GRAPHICS_RESOURCE_COUNT * sizeof(pg_graphics_message));
        if (!ok)
        {
            err->log(
                err,
                PG_ERROR_MAJOR,
                "init_app: failed to copy initialization graphics messages");
        }

        *init_gfx_msg_count = CAP(gfx_msgs);
    }

    reset_view();
}

FUNCTION void
update_app(pg_assets* assets,
           pg_input* input,
           models_metadata* metadata,
           pg_graphics_message* init_gfx_msgs,
           pg_f32_2x previous_cursor_position,
           pg_f32_2x render_res,
           pg_scratch_allocator* transient_mem,
           pg_graphics_message** update_gfx_msgs,
           u32* update_gfx_msg_count,
           pg_error* err)
{
    b8 ok = true;

    // Left/Up: Previous Model
    if (pg_button_pressed(&input->kbd.left, config.input_repeat_rate)
        || pg_button_pressed(&input->kbd.up, config.input_repeat_rate)
        || pg_button_pressed(&input->gp[0].left, config.input_repeat_rate)
        || pg_button_pressed(&input->gp[0].up, config.input_repeat_rate))
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
    if (pg_button_pressed(&input->kbd.right, config.input_repeat_rate)
        || pg_button_pressed(&input->kbd.down, config.input_repeat_rate)
        || pg_button_pressed(&input->gp[0].right, config.input_repeat_rate)
        || pg_button_pressed(&input->gp[0].down, config.input_repeat_rate))
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

    b8 mouse_active = true;
#if defined(PG_APP_IMGUI)
    if (ImGui_GetIO()->WantCaptureMouse)
    {
        mouse_active = false;
    }
#endif

    pg_f32_2x cursor_delta = {0};
    if (mouse_active
        && pg_button_pressed(&input->mouse.left, app_state.frame_time))
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

    while (app_state.running_simulation_time != 0.0f)
    {
        f32 dt = config.simulation_time_step;
        if (app_state.running_simulation_time < config.simulation_time_step)
        {
            dt = app_state.running_simulation_time;
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

        app_state.running_simulation_time -= dt;
    }

    pg_camera_clamp(
        (pg_f32_2x){.min = 0.0f, .max = 2.0f * PG_PI},
        (pg_f32_2x){.min = 0.0f, .max = PG_PI},
        (pg_f32_2x){.min = 0.0f, .max = app_state.center_zoom * 2.0f},
        true,
        &app_state.camera);

    // Create update graphics messages.
    {
        pg_asset_model* model = &assets->models[app_state.model_id];

        // (1): Update per_frame CB
        // (3): When model changes,
        //      Update vertex SB/index SB/material properties SB
        // ---
        // (4): Set per_frame CB/vertex SB/index SB/material properties SB
        // (2): Set opaque and non-opaque draw properties
        // (3/mesh): Set per draw constants/material
        //           Draw
        u32 max_update_gfx_msg_count = 1 + 3 + 4 + 2 + (3 * model->mesh_count);
        ok &= pg_scratch_alloc(transient_mem,
                               max_update_gfx_msg_count
                                   * sizeof(pg_graphics_message),
                               alignof(pg_graphics_message),
                               update_gfx_msgs);
        if (!ok)
        {
            err->log(err,
                     PG_ERROR_MAJOR,
                     "update_app: failed to get memory for update graphics "
                     "messages");
        }

        pg_graphics_message* gfx_msgs = *update_gfx_msgs;
        u32 gfx_msg_count = 0;

        // Generate matrices.
        pg_f32_3x camera_position
            = pg_camera_get_cartesian_position(&app_state.camera);
        pg_f32_4x4 world_from_model
            = pg_f32_4x4_world_from_model(metadata->scaling[app_state.model_id],
                                          app_state.rotation,
                                          (pg_f32_3x){0});
        pg_f32_4x4 clip_from_view = pg_f32_4x4_clip_from_view_perspective(
            27.0f,
            render_res.width / render_res.height,
            0.1f,
            100.0f);
        pg_f32_4x4 view_from_world
            = pg_f32_4x4_view_from_world(camera_position,
                                         app_state.camera.focal_point,
                                         app_state.camera.up_axis);

        // Create and sort graphics properties.
        pg_graphics_properties* opaque_gp;
        pg_graphics_properties* non_opaque_gp;
        pg_graphics_properties* gp;
        u32 opaque_gp_count = 0;
        u32 non_opaque_gp_count = 0;
        u32 gp_count = 0;
        {
            ok &= pg_scratch_alloc(transient_mem,
                                   metadata->max_mesh_count
                                       * sizeof(pg_graphics_properties),
                                   alignof(pg_graphics_properties),
                                   &opaque_gp);
            ok &= pg_scratch_alloc(transient_mem,
                                   metadata->max_mesh_count
                                       * sizeof(pg_graphics_properties),
                                   alignof(pg_graphics_properties),
                                   &non_opaque_gp);
            ok &= pg_scratch_alloc(transient_mem,
                                   metadata->max_mesh_count
                                       * sizeof(pg_graphics_properties),
                                   alignof(pg_graphics_properties),
                                   &gp);
            if (!ok)
            {
                err->log(
                    err,
                    PG_ERROR_MAJOR,
                    "update_app: failed to get memory for graphics properties");
            }

            pg_f32_4x4 view_from_model
                = pg_f32_4x4_mul(view_from_world, world_from_model);
            for (u32 i = 0; i < model->mesh_count; i += 1)
            {
                u32 material_id = model->meshes[i].material_id;
                pg_graphics_properties curr_gp
                    = {.art_id = app_state.model_id,
                       .mesh_id = i,
                       .material_id = material_id,
                       .camera_dist
                       = pg_asset_mesh_get_camera_dist(model->meshes + i,
                                                       view_from_model)};

                if (model->materials[material_id].properties.alpha_mode
                    == PG_ALPHA_MODE_OPAQUE)
                {
                    opaque_gp[opaque_gp_count] = curr_gp;
                    opaque_gp_count += 1;
                }
                else
                {
                    non_opaque_gp[non_opaque_gp_count] = curr_gp;
                    non_opaque_gp_count += 1;
                }
            }

            pg_sort(opaque_gp,
                    opaque_gp_count,
                    sizeof(pg_graphics_properties),
                    &pg_sort_graphics_properties_front_to_back);
            pg_sort(non_opaque_gp,
                    non_opaque_gp_count,
                    sizeof(pg_graphics_properties),
                    &pg_sort_graphics_properties_back_to_front);

            gp_count = opaque_gp_count + non_opaque_gp_count;
            ok &= pg_copy(opaque_gp,
                          opaque_gp_count * sizeof(pg_graphics_properties),
                          gp,
                          gp_count * sizeof(pg_graphics_properties));
            ok &= pg_copy(non_opaque_gp,
                          non_opaque_gp_count * sizeof(pg_graphics_properties),
                          gp + opaque_gp_count,
                          gp_count * sizeof(pg_graphics_properties));
            if (!ok)
            {
                err->log(err,
                         PG_ERROR_MAJOR,
                         "update_app: failed to copy opaque/non-opaque "
                         "graphics properties into unified list");
            }
        }

        // Update per frame constant buffer.
        {
            pg_f32_4x4 clip_from_world
                = pg_f32_4x4_mul(clip_from_view, view_from_world);
            per_frame_cb per_frame = {.world_from_model = world_from_model,
                                      .clip_from_world = clip_from_world,
                                      .light_dir = app_state.light_dir,
                                      .camera_pos = camera_position};

            per_frame_cb* data;
            ok &= pg_scratch_alloc(transient_mem,
                                   sizeof(per_frame_cb),
                                   alignof(per_frame_cb),
                                   &data);
            if (!ok)
            {
                err->log(err,
                         PG_ERROR_MAJOR,
                         "update_app: failed to get memory for per frame "
                         "constant buffer");
            }

            ok &= pg_copy(&per_frame,
                          sizeof(per_frame_cb),
                          data,
                          sizeof(per_frame_cb));
            if (!ok)
            {
                err->log(err,
                         PG_ERROR_MAJOR,
                         "update_app: failed to copy per frame constant buffer "
                         "data");
            }

            gfx_msgs[gfx_msg_count]
                = init_gfx_msgs[GRAPHICS_RESOURCE_PER_FRAME_CB];
            gfx_msgs[gfx_msg_count].buffer_msg.data = data;
            gfx_msg_count += 1;
        }

        // If model changed, update model data.
        if (metadata->model_id_last_frame != app_state.model_id)
        {
            // Update vertex/index structured buffers.
            {
                gfx_msgs[gfx_msg_count]
                    = init_gfx_msgs[GRAPHICS_RESOURCE_VERTEX_SB];
                gfx_msgs[gfx_msg_count].buffer_msg.elem_count
                    = model->vertex_count;
                gfx_msgs[gfx_msg_count].buffer_msg.data = model->vertices;
                gfx_msg_count += 1;

                gfx_msgs[gfx_msg_count]
                    = init_gfx_msgs[GRAPHICS_RESOURCE_INDEX_SB];
                gfx_msgs[gfx_msg_count].buffer_msg.elem_count
                    = model->index_count;
                gfx_msgs[gfx_msg_count].buffer_msg.data = model->indices;
                gfx_msg_count += 1;
            }

            // Update material properties structured buffer.
            {
                pg_asset_material_properties* material_properties;
                ok &= pg_scratch_alloc(
                    transient_mem,
                    model->material_count
                        * sizeof(pg_asset_material_properties),
                    alignof(pg_asset_material_properties),
                    &material_properties);
                if (!ok)
                {
                    err->log(err,
                             PG_ERROR_MAJOR,
                             "update_app: failed to get memory for material "
                             "properties");
                }

                for (u32 i = 0; i < model->material_count; i += 1)
                {
                    material_properties[i] = model->materials[i].properties;
                }

                gfx_msgs[gfx_msg_count]
                    = init_gfx_msgs[GRAPHICS_RESOURCE_MATERIAL_PROPERTIES_SB];
                gfx_msgs[gfx_msg_count].buffer_msg.elem_count
                    = model->material_count;
                gfx_msgs[gfx_msg_count].buffer_msg.data = material_properties;
                gfx_msg_count += 1;
            }
        }

        // Set per frame constant buffer.
        gfx_msgs[gfx_msg_count] = init_gfx_msgs[GRAPHICS_RESOURCE_PER_FRAME_CB];
        gfx_msg_count += 1;

        // Set vertex structured buffer.
        gfx_msgs[gfx_msg_count] = init_gfx_msgs[GRAPHICS_RESOURCE_VERTEX_SB];
        gfx_msg_count += 1;

        // Set index structured buffer.
        gfx_msgs[gfx_msg_count] = init_gfx_msgs[GRAPHICS_RESOURCE_INDEX_SB];
        gfx_msg_count += 1;

        // Set material properties structured buffer.
        gfx_msgs[gfx_msg_count]
            = init_gfx_msgs[GRAPHICS_RESOURCE_MATERIAL_PROPERTIES_SB];
        gfx_msg_count += 1;

        u32 material_id = metadata->max_material_count;
        for (u32 i = 0; i < gp_count; i += 1)
        {
            // Set draw properties.
            if (i == 0)
            {
                gfx_msgs[gfx_msg_count] = (pg_graphics_message){
                    .type = PG_GRAPHICS_MESSAGE_TYPE_DRAW_PROPERTIES,
                    .draw_properties_msg
                    = {.triangle_strip = false, .opaque = true}};
                gfx_msg_count += 1;
            }
            else if (i == opaque_gp_count)
            {
                gfx_msgs[gfx_msg_count] = (pg_graphics_message){
                    .type = PG_GRAPHICS_MESSAGE_TYPE_DRAW_PROPERTIES,
                    .draw_properties_msg
                    = {.triangle_strip = false, .opaque = false}};
                gfx_msg_count += 1;
            }

            // Set per draw constants.
            {
                constants_cb constants = {
                    .material_id = gp[i].material_id,
                    .texture_offset
                    = (u32)pg_3d_to_1d_index(0,
                                             gp[i].material_id,
                                             gp[i].art_id,
                                             PG_TEXTURE_TYPE_COUNT,
                                             metadata->max_material_count),
                    .vertex_offset = model->meshes[gp[i].mesh_id].vertex_offset,
                    .index_offset = model->meshes[gp[i].mesh_id].index_offset};

                constants_cb* data;
                ok &= pg_scratch_alloc(transient_mem,
                                       sizeof(constants_cb),
                                       alignof(constants_cb),
                                       &data);
                if (!ok)
                {
                    err->log(err,
                             PG_ERROR_MAJOR,
                             "update_app: failed to get memory for constants "
                             "constant buffer");
                }

                ok &= pg_copy(&constants,
                              sizeof(constants_cb),
                              data,
                              sizeof(constants_cb));
                if (!ok)
                {
                    err->log(err,
                             PG_ERROR_MAJOR,
                             "update_app: failed to copy constants constant "
                             "buffer data");
                }

                gfx_msgs[gfx_msg_count]
                    = init_gfx_msgs[GRAPHICS_RESOURCE_PER_DRAW_CONSTANTS];
                gfx_msgs[gfx_msg_count].constants_msg.data = data;
                gfx_msg_count += 1;
            }

            // Set material.
            if (gp[i].material_id != material_id)
            {
                gfx_msgs[gfx_msg_count]
                    = init_gfx_msgs[GRAPHICS_RESOURCE_MATERIAL];
                gfx_msgs[gfx_msg_count].material_msg.material_id
                    = gp[i].material_id;
                gfx_msgs[gfx_msg_count].material_msg.art_id = gp[i].art_id;
                gfx_msgs[gfx_msg_count].material_msg.textures
                    = model->materials[gp[i].material_id].textures;
                gfx_msg_count += 1;

                material_id = gp[i].material_id;
            }

            // Draw.
            gfx_msgs[gfx_msg_count] = (pg_graphics_message){
                .type = PG_GRAPHICS_MESSAGE_TYPE_DRAW,
                .draw_msg
                = {.vertex_count = model->meshes[gp[i].mesh_id].index_count,
                   .instance_count = 1}};
            gfx_msg_count += 1;
        }

        *update_gfx_msg_count = gfx_msg_count;
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

    pg_graphics_message* init_gfx_msgs = 0;
    u32 init_gfx_msg_count = 0;
    pg_graphics_message* update_gfx_msgs = 0;
    u32 update_gfx_msg_count = 0;

    pg_windows_init_window(&windows.window,
                           inst,
                           config.fixed_aspect_ratio_width,
                           config.fixed_aspect_ratio_height,
                           &err);
    pg_windows_init_mem(&windows,
                        config.permanent_mem_size,
                        config.transient_mem_size,
                        &err);
    pg_assets* assets = pg_assets_read_pga(&windows.permanent_mem,
                                           &pg_windows_read_file,
                                           &err);
    pg_assets_verify(assets, 0, 0, 0, MODEL_COUNT, 0, &err);
    static_assert(CAP(model_names) == MODEL_COUNT);

    init_app(assets,
             &windows.permanent_mem,
             &metadata,
             &init_gfx_msgs,
             &init_gfx_msg_count,
             &err);

    pg_windows_init_graphics(&windows,
                             app_state.gfx_api,
                             init_gfx_msgs,
                             init_gfx_msg_count,
                             config.gfx_cpu_mem_size,
                             config.gfx_gpu_mem_size,
                             app_state.vsync,
                             &err);
    pg_windows_init_metrics(&windows.metrics, &err);

    while (windows.msg.message != WM_QUIT)
    {
        if (PeekMessageW(&windows.msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&windows.msg);
            DispatchMessageW(&windows.msg);
            continue;
        }

        pg_graphics_api gfx_api = app_state.gfx_api;
        pg_f32_2x previous_cursor_position = windows.input.mouse.cursor;

        pg_windows_update_input(&windows, config.gamepad_count, &err);

        update_app(assets,
                   &windows.input,
                   &metadata,
                   init_gfx_msgs,
                   previous_cursor_position,
                   windows.window.render_res,
                   &windows.transient_mem,
                   &update_gfx_msgs,
                   &update_gfx_msg_count,
                   &err);
        metadata.model_id_last_frame = app_state.model_id;

        pg_windows_update_graphics(&windows,
                                   app_state.gfx_api,
                                   update_gfx_msgs,
                                   update_gfx_msg_count,
                                   app_state.vsync,
                                   &imgui_ui,
                                   &err);

        pg_windows_update_metrics(&windows.metrics, &err);
        app_state.fps = windows.metrics.fps;
        app_state.frame_time = windows.metrics.frame_time;
        app_state.running_simulation_time += app_state.frame_time;

        if (app_state.gfx_api != gfx_api)
        {
            metadata.model_id_last_frame = 0;
            pg_windows_reload_graphics(&windows,
                                       inst,
                                       app_state.gfx_api,
                                       init_gfx_msgs,
                                       init_gfx_msg_count,
                                       config.gfx_cpu_mem_size,
                                       config.gfx_gpu_mem_size,
                                       config.fixed_aspect_ratio_width,
                                       config.fixed_aspect_ratio_height,
                                       app_state.vsync,
                                       &err);
        }

        pg_scratch_free(&windows.transient_mem);
    }

    pg_windows_release(&windows);

    return 0;
}
#endif
