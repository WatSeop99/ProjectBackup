#pragma once

enum eRenderThreadEventType
{
	// RenderThreadEventType_Process,
	RenderThreadEventType_Shadow,
	RenderThreadEventType_Object,
	RenderThreadEventType_Mirror,
	RenderThreadEventType_Collider,
	RenderThreadEventType_Post,
	RenderThreadEventType_Desctroy,
	RenderThreadEventType_Count
};
struct RenderThreadDesc
{
	Renderer* pRenderer;
	ResourceManager* pResourceManager;
	UINT ThreadIndex;
	HANDLE hThread;
	HANDLE hEventList[RenderThreadEventType_Count];
};

UINT WINAPI RenderThread(void* pArg);
