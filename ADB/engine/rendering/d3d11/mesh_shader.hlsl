// [Inputs/Outputs]

cbuffer Constants : register(b0)
{
    float3x3 Transform;
};

cbuffer Material : register(b1)
{
    float Opacity;
};

struct CPUToVertex
{
    float3 Position : POS;
    float2 Texture  : TEX;
    float3 Normal   : NORM;
};

struct VertexToPixel
{
    float4 Position : SV_POSITION;
    float4 Color    : TINT;
};


// [Vertex Shader]

VertexToPixel VertexMain(CPUToVertex Input)
{
    VertexToPixel Output;
    Output.Position = float4(0.f, 0.f, 0.f, 0.f);
    Output.Color    = float4(1.f, 0.f, 0.f, 1.f);
    return Output;
}

// [Pixel Shader]


float4 PixelMain(VertexToPixel Input) : SV_TARGET
{
    float4 Color = Input.Color;
    return Color;
}