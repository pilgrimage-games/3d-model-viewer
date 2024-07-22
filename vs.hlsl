cbuffer frame_data : register(b0)
{
    float4x4 clip_from_world;
    float3 light_dir;
    float3 camera_pos;
}

cbuffer entity_data : register(b1)
{
    float4x4 world_from_model;
}

struct vs_input
{
    float3 position : POSITION;  // model space
    float2 tex_coord : TEXCOORD; // texture space
    float3 normal: NORMAL;       // tangent space
    float4 tangent: TANGENT;     // tangent space
};

struct vs_output
{
    float4 position : SV_POSITION; // clip space
    float2 tex_coord : TEXCOORD;   // texture space
    float3 normal : NORMAL;        // world space
    float3 tangent : TANGENT;      // world space
    float3 bitangent : BITANGENT;  // world space
    float3 light_dir : POSITION0;  // world space (towards target)
    float3 view_dir : POSITION1;   // world space (towards target)
};

vs_output main(vs_input i)
{
    vs_output o;
    o.position = mul(clip_from_world, mul(world_from_model, float4(i.position, 1.0f)));

    o.tex_coord = i.tex_coord;

    // NOTE: Translation is ignored by casting to a 3x3 matrix.
    // NOTE: This assumes uniform scaling. For non-uniform scaling, use the
    // inverse transpose to undo the model matrix's scale transform but
    // preserve its rotation.
    o.normal = normalize(mul((float3x3)world_from_model, i.normal));
    o.tangent = normalize(mul((float3x3)world_from_model, i.tangent.xyz));

    // Reorthogonalize tangent with respect to normal using Gram-Schmidt
    o.tangent = normalize(o.tangent - (dot(o.tangent, o.normal) * o.normal));

    o.bitangent = normalize(mul(cross(o.normal, o.tangent), i.tangent.w));

    o.light_dir = light_dir;

    o.view_dir = (float3)mul(world_from_model, float4(i.position, 1.0f)) - camera_pos;

    return o;
}
