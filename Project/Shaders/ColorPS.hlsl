#include "Common.hlsli"

struct PixelShaderOutput
{
	float4 PixelColor : SV_TARGET0;
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.PixelColor.rgba = 1.0f;
    
	return output;
}