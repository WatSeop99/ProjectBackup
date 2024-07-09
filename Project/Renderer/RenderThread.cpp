#include "../pch.h"
#include "Renderer.h"
#include "RenderThread.h"

class Renderer;

UINT WINAPI RenderThread(void* pArg)
{
	RenderThreadDesc* pDesc = (RenderThreadDesc*)pArg;
	Renderer* pRenderer = pDesc->pRenderer;
	UINT threadIndex = pDesc->ThreadIndex;
	const HANDLE* phEventList = pDesc->hEventList;

	while (true)
	{
		UINT eventIndex = WaitForMultipleObjects(RenderThreadEventType_Count, phEventList, FALSE, INFINITE);

		switch (eventIndex)
		{
			case RenderThreadEventType_Process:
				pRenderer->ProcessByThread(threadIndex);
				break;

			case RenderThreadEventType_Desctroy:
				goto LB_EXIT;

			default:
				// __debugbreak();
				break;
		}
	}

LB_EXIT:
	_endthreadex(0);
	return 0;
}
