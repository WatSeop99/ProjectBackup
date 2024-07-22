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
		if (Vertex.pBuffer)
		{
			Vertex.pBuffer->Release();
			Vertex.pBuffer = nullptr;
		}
		if (Index.pBuffer)
		{
			Index.pBuffer->Release();
			Index.pBuffer = nullptr;
		}

		MeshConstant.Cleanup();
		MaterialConstant.Cleanup();
	}

public:
	BufferInfo Vertex;
	BufferInfo Index;
	Material Material;

	ConstantBuffer MeshConstant;
	ConstantBuffer MaterialConstant;

	bool bSkinnedMesh = false;
};
