#define PI 3.14159265359f

struct vertex
{
    float3 position;
    float2 tex_coord;
    float3 normal;
    float4 tangent;
};

struct pixel
{
    uint material_id : MATERIAL_ID;
    float4 position : SV_POSITION; // clip space
    float2 tex_coord : TEXCOORD;   // texture space
    float3 normal : NORMAL;        // world space
    float3 tangent : TANGENT;      // world space
    float3 bitangent : BITANGENT;  // world space
    float3 light_dir : POSITION0;  // world space (towards target)
    float3 view_dir : POSITION1;   // world space (towards target)
};

struct material_properties
{
    uint normal_mapping;
    float alpha_cutoff;
    float metallic_factor;
    float roughness_factor;
    uint alpha_mode;
    float4 base_color_factor;
};

// Vertex Shader Resources
cbuffer per_frame : register(b0)
{
    float4x4 clip_from_world;
    float3 light_dir;
    float3 camera_pos;
}
cbuffer per_model : register(b1)
{
    float4x4 world_from_model;
}
cbuffer per_mesh : register(b2)
{
    uint material_id;
}
StructuredBuffer<vertex> vertices : register(t0);
StructuredBuffer<uint> indices : register(t1);

// Pixel Shader Resources
StructuredBuffer<material_properties> properties : register(t2);
Texture2D textures[4] : TEXTURE : register(t3);
SamplerState ss : SAMPLER : register(s0);

pixel vs(uint index_id : SV_VertexID)
{
    uint vertex_id = indices[index_id];
    vertex v = vertices[vertex_id];

    pixel p;
    p.material_id = material_id;
    p.position = mul(clip_from_world, mul(world_from_model, float4(v.position, 1.0f)));

    p.tex_coord = v.tex_coord;

    // NOTE: Translation is ignored by casting to a 3x3 matrix.
    // NOTE: This assumes uniform scaling. For non-uniform scaling, use the
    // inverse transpose to undo the model matrix's scale transform but
    // preserve its rotation.
    p.normal = normalize(mul((float3x3)world_from_model, v.normal));
    p.tangent = normalize(mul((float3x3)world_from_model, v.tangent.xyz));

    // Reorthogonalize tangent with respect to normal using Gram-Schmidt
    p.tangent = normalize(p.tangent - (dot(p.tangent, p.normal) * p.normal));

    p.bitangent = normalize(mul(cross(p.normal, p.tangent), v.tangent.w));

    p.light_dir = light_dir;

    p.view_dir = (float3)mul(world_from_model, float4(v.position, 1.0f)) - camera_pos;

    return p;
}

// Fresnel Reflectance using Schlick approximation
// The fresnel equation approximates the percentage of light reflected.
float3 fresnel_schlick(float v_dot_h, float3 f0)
{
    return f0 + ((1.0f - f0) * pow(1.0f - v_dot_h, 5.0f));
}

// Normal Distribution Function using GGX
// The normal distribution function (NDF) describes the orientation of
// microfacet normals, which determines the shape of highlights and changes how
// rough or smooth a surface appears.
float ndf_ggx(float n_dot_h, float roughness)
{
    float alpha = pow(roughness, 2.0f);
    float alpha_sq = pow(alpha, 2.0f);
    float denom = PI * pow(pow(n_dot_h, 2.0f) * (alpha_sq - 1.0f) + 1.0f, 2.0f);

    return alpha_sq / denom;
}

// Schlick-GGX G1 function
float g1_schlick_ggx(float n_dot_x, float k)
{
    return n_dot_x / (n_dot_x * (1.0f - k) + k);
}

// Geometry Term using Smith method
// The geometry term approximates the probability that light is occluded by
// microfacets.
float geometry_smith(float n_dot_l, float n_dot_v, float roughness)
{
    float r = roughness + 1.0f;
    float k = pow(r, 2.0f) / 8.0f;

    return g1_schlick_ggx(n_dot_l, k) * g1_schlick_ggx(n_dot_v, k);
}

// NOTE: pg_alpha_mode values: PG_AM_OPAQUE (0), PG_AM_MASK (1), PG_AM_BLEND (2)
float4 get_albedo(pixel p, material_properties mp)
{
    float4 albedo = textures[0].Sample(ss, p.tex_coord) * mp.base_color_factor;

    if (mp.alpha_mode == 1 && albedo.a < mp.alpha_cutoff)
    {
        discard;
    }

    uint has_alpha = max(mp.alpha_mode, 1) - 1;
    albedo.a = (albedo.a * has_alpha) + (1.0f * (1 - has_alpha));

    return albedo;
}

float3 get_normal(pixel p, material_properties mp)
{
    // NOTE: Normals are remapped from [0, 1] to [-1, 1] and transformed from
    // tangent space to world space.
    // NOTE: glTF normal maps are +Y/Green-up.
    // green on bottom = hole, green on top = bump
    float3 normal = textures[2].Sample(ss, p.tex_coord).rgb;
    normal = (normal * 2.0f) - 1.0f;
    float3x3 tbn = transpose(float3x3(p.tangent, p.bitangent, p.normal));
    normal = mul(tbn, normal);

    // If normal mapping is disabled, use world-space vertex normal.
    normal = (normal * mp.normal_mapping) + (p.normal * (1 - mp.normal_mapping));

    return normal;
}

float4 ps(pixel p) : SV_TARGET
{
    material_properties mp = properties[p.material_id];

    // Sample textures.
    float4 albedo = get_albedo(p, mp);
    float roughness = textures[1].Sample(ss, p.tex_coord).g * mp.roughness_factor;
    float metallic = textures[1].Sample(ss, p.tex_coord).b * mp.metallic_factor;
    float3 normal = get_normal(p, mp);
    float3 emissive = textures[3].Sample(ss, p.tex_coord).rgb;

    // Microfacet BRDF using Cook-Torrance model
    // The bidirectional reflective distribution function (BRDF) is the
    // approximation of the light reflected off a surface given its material
    // properties.
    // NOTE: `f0` is the base reflectivity when looking directly at the surface
    // (p.e. 0 degree angle between `n` and `v`).
    float3 n = normalize(normal);
    float3 l = normalize(-p.light_dir);
    float3 v = normalize(-p.view_dir);
    float3 h = normalize(l + v);
    float n_dot_v = max(0.0f, dot(n, v));
    float n_dot_l = max(0.0f, dot(n, l));
    float n_dot_h = max(0.0f, dot(n, h));
    float v_dot_h = max(0.0f, dot(v, h));
    float3 dielectric_fresnel_factor = float3(0.04f, 0.04f, 0.04f);
    float3 f0 = lerp(dielectric_fresnel_factor, albedo.rgb, metallic);
    float3 f = fresnel_schlick(v_dot_h, f0);
    float d = ndf_ggx(n_dot_h, roughness);
    float g = geometry_smith(n_dot_l, n_dot_v, roughness);
    float3 refracted_light = float3(1.0f, 1.0f, 1.0f) - f; // all non-reflected light
    float dielectric = 1.0f - metallic; // diffuse BRDF = 0.0f for metals
    float3 diffuse_brdf = (albedo.rgb * refracted_light * dielectric) / PI;
    float3 specular_brdf = (f * d * g) / max(0.0001f, 4.0f * n_dot_l * n_dot_v);
    float3 brdf = diffuse_brdf + specular_brdf;

    float3 light_color = float3(1.0f, 1.0f, 1.0f);
    float attenuation = 1.0f; // attenuation = 1.0f for directional lights
    float3 radiance = light_color * attenuation;

    float3 ambient = albedo.rgb * float3(0.35f, 0.35f, 0.35f);

    float3 color = (brdf * radiance * n_dot_l) + ambient + emissive;

    return float4(color, albedo.a);
}
