cbuffer Constants : register(b0)
{
    row_major float4x4 transform;
    float4 params;
}

struct VertexData
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
    float4 color    : COLOR;
};

struct PixelData
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color    : COLOR0;
};

Texture2D InputTexture : register(t0);
SamplerState InputSampler : register(s0);

PixelData Vertex(VertexData vertex)
{
    PixelData output;
    output.position = mul(float4(vertex.position, 1.0f), transform);
    output.texcoord = vertex.texcoord;
    output.color = vertex.color;
    return output;
}

#define stb_unlerp(t,a,b) (((t) - (a)) / ((b) - (a)))

float stb_linear_remap(float x, float x_min, float x_max, float out_min, float out_max)
{
   return lerp(out_min, out_max, stb_unlerp(x, x_min, x_max));
}

float4 Pixel(PixelData pixel) : SV_Target
{
    float sample = InputTexture.Sample(InputSampler, pixel.texcoord).r;
    sample *= 255.0f;
    float sdfDist = stb_linear_remap(sample, params.x, params.x + params.y, 0.0f, 1.0f);
    float pixDist = sdfDist * params.z;
    float alpha = stb_linear_remap(pixDist, -0.5f, 0.5f, 0.0f, 1.0f);
    alpha = clamp(alpha, 0.0f, 1.0f);
    return float4(pixel.color.x, pixel.color.y, pixel.color.z, alpha * pixel.color.w);
}