
struct VS_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
    float3 Normal   : NORMAL;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float3 WorldPos : TEXCOORD2;
};

cbuffer TransformBuffer : register(b0)
{
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
};

Texture2D    ColorTexture   : register(t0);
SamplerState TextureSampler : register(s0);


PS_INPUT VS(VS_INPUT Input)
{
    PS_INPUT Output;
    
    float4 WorldPos = mul(World, float4(Input.Position, 1.0));
    float4 ViewPos  = mul(View, WorldPos);
    Output.Position = mul(Projection, ViewPos);
    
    Output.WorldPos = WorldPos.xyz;
    Output.TexCoord = Input.TexCoord;
    Output.Normal   = mul(Input.Normal, (float3x3) World);
    
    return Output;
}


float4 PS(PS_INPUT Input) : SV_TARGET
{
    float3 Albedo = ColorTexture.Sample(TextureSampler, Input.TexCoord).rgb;
    
    return float4(Albedo, 1.f);
}