#define PI 3.14159265359f

cbuffer material_factors : register(b2)
{
    float4 base_color_factor;
    float metallic_factor;
    float roughness_factor;
    bool normal_mapping;
}

struct ps_input
{
    float4 position : SV_POSITION; // clip space
    float2 tex_coord : TEXCOORD;   // texture space
    float3 normal : NORMAL;        // world space
    float3 tangent : TANGENT;      // world space
    float3 bitangent : BITANGENT;  // world space
    float3 light_dir : POSITION0;  // world space (towards target)
    float3 view_dir : POSITION1;   // world space (towards target)
};

Texture2D tex[4] : TEXTURE : register(t0);
SamplerState ss : SAMPLER : register(s0);

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

float4 main(ps_input i) : SV_TARGET
{
    // Sample textures.
    // NOTE: Normals are remapped from [0, 1] to [-1, 1] and transformed from
    // tangent space to world space.
    float3 albedo = tex[0].Sample(ss, i.tex_coord).rgb * base_color_factor.rgb;
    float roughness = tex[1].Sample(ss, i.tex_coord).g * roughness_factor;
    float metallic = tex[1].Sample(ss, i.tex_coord).b * metallic_factor;
    float3 normal = i.normal;
    if (normal_mapping)
    {
        // NOTE: glTF normal maps are +Y/Green-up.
        // green on bottom = hole, green on top = bump
        normal = tex[2].Sample(ss, i.tex_coord).rgb;
        normal = (normal * 2.0f) - 1.0f;
        float3x3 tbn_mtx = transpose(float3x3(i.tangent, i.bitangent, i.normal));
        normal = mul(tbn_mtx, normal);
    }
    float3 emissive = tex[3].Sample(ss, i.tex_coord).rgb;

    // Microfacet BRDF using Cook-Torrance model
    // The bidirectional reflective distribution function (BRDF) is the
    // approximation of the light reflected off a surface given its material
    // properties.
    // NOTE: `f0` is the base reflectivity when looking directly at the surface
    // (i.e. 0 degree angle between `n` and `v`).
    float3 n = normalize(normal);
    float3 l = normalize(-i.light_dir);
    float3 v = normalize(-i.view_dir);
    float3 h = normalize(l + v);
    float n_dot_v = max(0.0f, dot(n, v));
    float n_dot_l = max(0.0f, dot(n, l));
    float n_dot_h = max(0.0f, dot(n, h));
    float v_dot_h = max(0.0f, dot(v, h));
    float3 dielectric_fresnel_factor = float3(0.04f, 0.04f, 0.04f);
    float3 f0 = lerp(dielectric_fresnel_factor, albedo, metallic);
    float3 f = fresnel_schlick(v_dot_h, f0);
    float d = ndf_ggx(n_dot_h, roughness);
    float g = geometry_smith(n_dot_l, n_dot_v, roughness);
    float3 refracted_light = float3(1.0f, 1.0f, 1.0f) - f; // all non-reflected light
    float dielectric = 1.0f - metallic; // diffuse BRDF = 0.0f for metals
    float3 diffuse_brdf = (albedo * refracted_light * dielectric) / PI;
    float3 specular_brdf = (f * d * g) / max(0.0001f, 4.0f * n_dot_l * n_dot_v);
    float3 brdf = diffuse_brdf + specular_brdf;

    float3 light_color = float3(1.0f, 1.0f, 1.0f);
    float attenuation = 1.0f; // attenuation = 1.0f for directional lights
    float3 radiance = light_color * attenuation;

    float3 ambient = albedo * float3(0.03f, 0.03f, 0.03f);

    float3 color = (brdf * radiance * n_dot_l) + ambient + emissive;

    return float4(color, 1.0f);
}
