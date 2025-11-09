cbuffer ConstantBuffer : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
}

struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    float4 pos = float4(input.position, 1.0f);
    pos = mul(pos, World);
    pos = mul(pos, View);
    pos = mul(pos, Projection);
    output.position = pos;
    output.color = input.color;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_Target
{
    return input.color;
}

float4 main( float4 pos : POSITION ) : SV_POSITION
{
	return pos;
}