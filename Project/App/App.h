#pragma once

#include "../Graphics/Light.h"
#include "../Util/LinkedList.h"
#include "../Model/Model.h"
#include "../Renderer/Renderer.h"
#include "../Util/Utility.h"
#include "../Renderer/Timer.h"

class App final : public Renderer
{
public:
	App() = default;
	~App() { Clear(); }

	void Initialize();

	int Run();

	void Update(const float DELTA_TIME);

	void Clear();

protected:
	void initExternalData(UINT64* pTotalRenderObjectCount);

	void updateAnimation(const float DELTA_TIME);

private:
	Timer m_Timer;

	// data
	std::vector<Model*> m_RenderObjects;
	std::vector<Light> m_Lights;
	std::vector<Model*> m_LightSpheres;

	Texture m_EnvTexture;
	Texture m_IrradianceTexture;
	Texture m_SpecularTexture;
	Texture m_BRDFTexture;
	
	Model* m_pMirror = nullptr;
	Model* m_pPickedModel = nullptr;
	SkinnedMeshModel* m_pCharacter = nullptr;
	DirectX::SimpleMath::Plane m_MirrorPlane;
};

