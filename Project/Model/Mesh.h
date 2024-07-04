#pragma once

#include "../pch.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/Texture.h"

// Vertex and Index Info
struct BufferInfo
{
	ID3D12Resource* pBuffer = nullptr;
	union
	{
		D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
		D3D12_INDEX_BUFFER_VIEW IndexBufferView;
	};
	UINT Count;
};
struct Material
{
	Texture Albedo;
	Texture Emissive;
	Texture Normal;
	Texture Height;
	Texture AmbientOcclusion;
	Texture Metallic;
	Texture Roughness;
};
class Mesh
{
public:
	Mesh() = default;
	~Mesh()
	{
		SAFE_RELEASE(Vertex.pBuffer);
		SAFE_RELEASE(Index.pBuffer);
		MeshConstant.Clear();
		MaterialConstant.Clear();
	}

public:
	BufferInfo Vertex;
	BufferInfo Index;
	Material Material;

	ConstantBuffer MeshConstant;
	ConstantBuffer MaterialConstant;

	bool bSkinnedMesh = false;
};
