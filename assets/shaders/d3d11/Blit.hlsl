struct VertexData
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
};

struct PixelData
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

Texture2D InputTexture : register(t0);
SamplerState InputSampler : register(s0);

PixelData Vertex(VertexData vertex)
{
    PixelData output;
    output.position = float4(vertex.position, 1.0f);
    output.texcoord = vertex.texcoord;
    return output;
}

float4 Pixel(PixelData pixel) : SV_Target
{
    return InputTexture.Sample(InputSampler, pixel.texcoord);
}