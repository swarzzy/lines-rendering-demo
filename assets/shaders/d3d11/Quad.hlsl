cbuffer Constants : register(b0)
{
    row_major float4x4 transform;
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

float4 Pixel(PixelData pixel) : SV_Target
{
    float4 sample = InputTexture.Sample(InputSampler, pixel.texcoord);
    return float4(sample.xyz * pixel.color.xyz, 1.0f);
}