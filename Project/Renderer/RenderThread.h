#pragma once

enum eRenderThreadEventType
{
	RenderThreadEventType_Process,
	RenderThreadEventType_Desctroy,
	RenderThreadEventType_Count
};
struct RenderThreadDesc
{
	Renderer* pRenderer;
	UINT ThreadIndex;
	HANDLE hThread;
	HANDLE hEventList[RenderThreadEventType_Count];
};

UINT WINAPI RenderThread(void* pArg);
