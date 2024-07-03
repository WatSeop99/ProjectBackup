#include "Common.hlsli"

Texture2D g_HeightTex : register(t6);

PixelShaderInput main(VertexShaderInput input)
{
    // �� ��ǥ��� NDC�̱� ������ ���� ��ǥ�� �̿��ؼ� ���� ���.
    PixelShaderInput output;
    
#ifdef SKINNED
    float weights[8] = 
    {
        input.BoneWeights0.x,
        input.BoneWeights0.y,
        input.BoneWeights0.z,
        input.BoneWeights0.w,
        input.BoneWeights1.x,
        input.BoneWeights1.y,
        input.BoneWeights1.z,
        input.BoneWeights1.w,
    };
    uint indices[8] = 
    {
        input.BoneIndices0.x,
        input.BoneIndices0.y,
        input.BoneIndices0.z,
        input.BoneIndices0.w,
        input.BoneIndices1.x,
        input.BoneIndices1.y,
        input.BoneIndices1.z,
        input.BoneIndices1.w,
    };

    float3 modelPos = float3(0.0f, 0.0f, 0.0f);
    float3 modelNormal = float3(0.0f, 0.0f, 0.0f);
    float3 modelTangent = float3(0.0f, 0.0f, 0.0f);
    
    // Uniform Scaling ����.
    // (float3x3)boneTransforms ĳ�������� Translation ����.
    for (int i = 0; i < 8; ++i)
    {
        // weight�� ���� 1��.
        modelPos += weights[i] * mul(float4(input.ModelPosition, 1.0f), g_BoneTransforms[indices[i]]).xyz;
        modelNormal += weights[i] * mul(input.ModelNormal, (float3x3)g_BoneTransforms[indices[i]]);
        modelTangent += weights[i] * mul(input.ModelTangent, (float3x3)g_BoneTransforms[indices[i]]);
    }

    input.ModelPosition = modelPos;
    input.ModelNormal = modelNormal;
    input.ModelTangent = modelTangent;
#endif
    
    output.ModelPosition = input.ModelPosition;
    output.WorldNormal = mul(float4(input.ModelNormal, 0.0f), g_WorldInverseTranspose).xyz;
    output.WorldNormal = normalize(output.WorldNormal);
    output.WorldPosition = mul(float4(input.ModelPosition, 1.0f), g_World).xyz;
    
    if (bUseHeightMap)
    {
        float height = g_HeightTex.SampleLevel(g_LinearClampSampler, input.Texcoord, 0.0f).r;
        height = height * 2.0f - 1.0f;
        output.WorldPosition += output.WorldNormal * height * g_HeightScale;
    }
    
    output.ProjectedPosition = mul(float4(output.WorldPosition, 1.0f), g_ViewProjection);
    output.Texcoord = input.Texcoord;
    output.WorldTangent = mul(float4(input.ModelTangent, 0.0f), g_World).xyz;

    return output;
}
