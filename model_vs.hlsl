// NOTE: Matrices are stored row-major. For multiplication with vectors (e.g.
// in shader code), treat vectors as column vectors where the order of
// transformations on the vector is read right-to-left.
// Thus, projection * view * model * position_vector, where model = T * R * S.

cbuffer frame_data : register(b0)
{
    float4x4 projection_view_mtx;
    float3 light_dir;
    float3 camera_pos;
}

cbuffer obj_data : register(b1)
{
    float4x4 model_mtx;
}

struct vs_input
{
    float3 vertex_pos : POSITION;
    float2 vertex_tc : TEXCOORD;
    float3 vertex_norm: NORMAL;
    float4 vertex_tan: TANGENT;
};

struct vs_output
{
    float4 vertex_pos : SV_POSITION;
    float2 vertex_tc : TEXCOORD;
    float3 light_dir : POSITION0;
    float3 view_dir : POSITION1;
    float3x3 tbn_mtx : TBN_MATRIX;
};

vs_output main(vs_input i)
{
    vs_output o;
    o.vertex_pos = mul(mul(projection_view_mtx, model_mtx), float4(i.vertex_pos, 1.0f));

    o.vertex_tc = i.vertex_tc;

    o.light_dir = light_dir; // towards target

    o.view_dir = (float3)mul(model_mtx, float4(i.vertex_pos, 1.0f)) - camera_pos; // towards target

    // Construct the TBN matrix.
    // NOTE: Translation is ignored by casting to a 3x3 matrix.
    // NOTE: This assumes uniform scaling. For non-uniform scaling, use the
    // inverse transpose to undo the model matrix's scale transform but
    // preserve its rotation.
    float3 normal = normalize(mul((float3x3)model_mtx, i.vertex_norm));
    float3 tangent = normalize(mul((float3x3)model_mtx, i.vertex_tan.xyz));
    float3 bitangent = normalize(mul(cross(normal, tangent), i.vertex_tan.w));
    o.tbn_mtx = transpose(float3x3(tangent, bitangent, normal));

    return o;
}
