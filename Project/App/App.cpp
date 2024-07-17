#include "../pch.h"
#include "../Model/GeometryGenerator.h"
#include "App.h"

void App::Initialize()
{
	UINT64 totalRenderObjectCount = 0;

	initMainWidndow();
	initDirect3D();
	initExternalData(&totalRenderObjectCount);

	Renderer::InitialData initData =
	{
		&m_RenderObjects,
		&m_Lights,
		&m_LightSpheres,

		&m_EnvTexture, &m_IrradianceTexture, &m_SpecularTexture, &m_BRDFTexture,

		m_pMirror, &m_MirrorPlane,
	};
	Renderer::Initizlie(&initData);
}

int App::Run()
{
	MSG msg = { 0, };

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			m_Timer.Tick();

			static UINT s_FrameCount = 0;
			static UINT64 s_PrevUpdateTick = 0;
			static UINT64 s_PrevFrameCheckTick = 0;

			float frameTime = (float)m_Timer.GetElapsedSeconds();
			float frameChange = 2.0f * frameTime;
			UINT64 curTick = GetTickCount64();

			++s_FrameCount;

			Update(frameChange);
			s_PrevUpdateTick = curTick;
			Render();

			if (curTick - s_PrevFrameCheckTick > 1000)
			{
				s_PrevFrameCheckTick = curTick;

				WCHAR txt[64];
				swprintf_s(txt, L"DX12  %uFPS", s_FrameCount);
				SetWindowText(m_hMainWindow, txt);

				s_FrameCount = 0;
			}
		}
	}

	return (int)msg.wParam;
}

void App::Update(const float DELTA_TIME)
{
	Renderer::Update(DELTA_TIME);

	for (UINT64 i = 0, size = m_RenderObjects.size(); i < size; ++i)
	{
		Model* pModel = m_RenderObjects[i];

		switch (pModel->ModelType)
		{
			case DefaultModel:
			case MirrorModel:
				pModel->UpdateConstantBuffers();
				break;

			case SkinnedModel:
			{
				SkinnedMeshModel* pCharacter = (SkinnedMeshModel*)pModel;
				updateAnimationState(DELTA_TIME);
				pCharacter->UpdateConstantBuffers();
			}
			break;

			default:
				break;
		}
	}
}

void App::Clear()
{
	fence();
	for (UINT i = 0; i < SWAP_CHAIN_FRAME_COUNT; ++i)
	{
		waitForFenceValue(m_LastFenceValues[i]);
	}

	for (UINT64 i = 0, size = m_RenderObjects.size(); i < size; ++i)
	{
		Model* pModel = m_RenderObjects[i];
		delete pModel;
	}
	m_RenderObjects.clear();
	m_Lights.clear();
	m_LightSpheres.clear();

	m_pCharacter = nullptr;
	m_pMirror = nullptr;
}

void App::initExternalData(UINT64* pTotalRenderObjectCount)
{
	_ASSERT(pTotalRenderObjectCount);

	ResourceManager* pResourceManager = GetResourceManager();

	m_Lights.resize(MAX_LIGHTS);
	m_LightSpheres.resize(MAX_LIGHTS);

	// 환경맵 텍스쳐 로드.
	{
		m_EnvTexture.InitializeWithDDS(pResourceManager, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
		m_IrradianceTexture.InitializeWithDDS(pResourceManager, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
		m_SpecularTexture.InitializeWithDDS(pResourceManager, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
		m_BRDFTexture.InitializeWithDDS(pResourceManager, L"./Assets/Textures/Cubemaps/HDRI/clear_pureskyEnvHDR.dds");
	}

	// 환경 박스 초기화.
	{
		MeshInfo skyboxMeshInfo = INIT_MESH_INFO;
		MakeBox(&skyboxMeshInfo, 40.0f);

		std::reverse(skyboxMeshInfo.Indices.begin(), skyboxMeshInfo.Indices.end());
		Model* pSkybox = new Model(pResourceManager, { skyboxMeshInfo });
		pSkybox->Name = "SkyBox";
		pSkybox->ModelType = SkyboxModel;
		m_RenderObjects.push_back(pSkybox);
	}

	// 조명 설정.
	{
		// 조명 0.
		m_Lights[0].Property.Radiance = Vector3(3.0f);
		m_Lights[0].Property.FallOffEnd = 10.0f;
		m_Lights[0].Property.Position = Vector3(0.0f, 0.0f, 0.0f);
		m_Lights[0].Property.Direction = Vector3(0.0f, 0.0f, 1.0f);
		m_Lights[0].Property.SpotPower = 3.0f;
		m_Lights[0].Property.LightType = LIGHT_POINT | LIGHT_SHADOW;
		m_Lights[0].Property.Radius = 0.03f;
		m_Lights[0].Initialize(pResourceManager);

		// 조명 1.
		m_Lights[1].Property.Radiance = Vector3(3.0f);
		m_Lights[1].Property.FallOffEnd = 10.0f;
		m_Lights[1].Property.Position = Vector3(1.0f, 1.1f, 2.0f);
		m_Lights[1].Property.SpotPower = 2.0f;
		m_Lights[1].Property.Direction = Vector3(0.0f, -0.5f, 1.7f) - m_Lights[1].Property.Position;
		m_Lights[1].Property.Direction.Normalize();
		m_Lights[1].Property.LightType = LIGHT_SPOT;
		m_Lights[1].Property.Radius = 0.03f;
		m_Lights[1].Initialize(pResourceManager);

		// 조명 2.
		m_Lights[2].Property.Radiance = Vector3(5.0f);
		m_Lights[2].Property.Position = Vector3(5.0f, 5.0f, 5.0f);
		m_Lights[2].Property.Direction = Vector3(-1.0f, -1.0f, -1.0f);
		m_Lights[2].Property.Direction.Normalize();
		m_Lights[2].Property.LightType = LIGHT_DIRECTIONAL | LIGHT_SHADOW;
		m_Lights[2].Property.Radius = 0.05f;
		m_Lights[2].Initialize(pResourceManager);
	}

	// 조명 위치 표시.
	{
		for (int i = 0; i < MAX_LIGHTS; ++i)
		{
			MeshInfo sphere = INIT_MESH_INFO;
			MakeSphere(&sphere, 1.0f, 20, 20);

			m_LightSpheres[i] = new Model(pResourceManager, { sphere });
			m_LightSpheres[i]->UpdateWorld(Matrix::CreateTranslation(m_Lights[i].Property.Position));

			MaterialConstant* pSphereMaterialConst = (MaterialConstant*)m_LightSpheres[i]->Meshes[0]->MaterialConstant.pData;
			pSphereMaterialConst->AlbedoFactor = Vector3(0.0f);
			pSphereMaterialConst->EmissionFactor = Vector3(1.0f, 1.0f, 0.0f);
			m_LightSpheres[i]->bCastShadow = false; // 조명 표시 물체들은 그림자 X.
			for (UINT64 j = 0, size = m_LightSpheres[i]->Meshes.size(); j < size; ++j)
			{
				Mesh* pCurMesh = m_LightSpheres[i]->Meshes[j];
				MaterialConstant* pMeshMaterialConst = (MaterialConstant*)pCurMesh->MaterialConstant.pData;
				pMeshMaterialConst->AlbedoFactor = Vector3(0.0f);
				pMeshMaterialConst->EmissionFactor = Vector3(1.0f, 1.0f, 0.0f);
			}

			m_LightSpheres[i]->bIsVisible = true;
			m_LightSpheres[i]->Name = "LightSphere" + std::to_string(i);
			m_LightSpheres[i]->bIsPickable = false;

			m_RenderObjects.push_back(m_LightSpheres[i]);
		}
	}

	// 바닥(거울).
	{
		Model* pGround = nullptr;
		MeshInfo mesh = INIT_MESH_INFO;
		MakeSquare(&mesh, 10.0f);

		std::wstring path = L"./Assets/Textures/PBR/stringy-marble-ue/";
		mesh.szAlbedoTextureFileName = path + L"stringy_marble_albedo.png";
		mesh.szEmissiveTextureFileName = L"";
		mesh.szAOTextureFileName = path + L"stringy_marble_ao.png";
		mesh.szMetallicTextureFileName = path + L"stringy_marble_Metallic.png";
		mesh.szNormalTextureFileName = path + L"stringy_marble_Normal-dx.png";
		mesh.szRoughnessTextureFileName = path + L"stringy_marble_Roughness.png";

		pGround = new Model(pResourceManager, { mesh });

		MaterialConstant* pGroundMaterialConst = (MaterialConstant*)pGround->Meshes[0]->MaterialConstant.pData;
		pGroundMaterialConst->AlbedoFactor = Vector3(0.7f);
		pGroundMaterialConst->EmissionFactor = Vector3(0.0f);
		pGroundMaterialConst->MetallicFactor = 0.5f;
		pGroundMaterialConst->RoughnessFactor = 0.3f;

		// Vector3 position = Vector3(0.0f, -1.0f, 0.0f);
		Vector3 position = Vector3(0.0f, -0.5f, 0.0f);
		pGround->UpdateWorld(Matrix::CreateRotationX(DirectX::XM_PI * 0.5f) * Matrix::CreateTranslation(position));
		pGround->bCastShadow = false; // 바닥은 그림자 만들기 생략.

		m_MirrorPlane = DirectX::SimpleMath::Plane(position, Vector3(0.0f, 1.0f, 0.0f));
		m_pMirror = pGround; // 바닥에 거울처럼 반사 구현.
		pGround->ModelType = MirrorModel;
		m_RenderObjects.push_back(pGround);
	}

	// Main Object.
	{
		std::wstring path = L"./Assets/";
		std::vector<std::wstring> clipNames =
		{
			L"CatwalkIdleTwistR.fbx", L"CatwalkIdleToWalkForward.fbx",
			L"CatwalkWalkForward.fbx", L"CatwalkWalkStop.fbx",
		};
		AnimationData animationData;

		std::wstring filename = L"Remy.fbx";
		std::vector<MeshInfo> characterMeshInfo;
		AnimationData characterDefaultAnimData;
		ReadAnimationFromFile(characterMeshInfo, characterDefaultAnimData, path, filename);

		// 애니메이션 클립들.
		for (UINT64 i = 0, size = clipNames.size(); i < size; ++i)
		{
			std::wstring& name = clipNames[i];
			std::vector<MeshInfo> animationMeshInfo;
			AnimationData animDataInClip;
			ReadAnimationFromFile(animationMeshInfo, animDataInClip, path, name);

			if (animationData.Clips.empty())
			{
				animationData = animDataInClip;
			}
			else
			{
				animationData.Clips.push_back(animDataInClip.Clips[0]);
			}
		}

		if (animationData.Clips.size() > 1)
		{
			m_pCharacter = new SkinnedMeshModel(pResourceManager, characterMeshInfo, animationData);
		}
		else
		{
			m_pCharacter = new SkinnedMeshModel(pResourceManager, characterMeshInfo, characterDefaultAnimData);
		}

		Vector3 center(0.0f, 0.0f, 2.0f);
		for (UINT64 i = 0, size = m_pCharacter->Meshes.size(); i < size; ++i)
		{
			Mesh* pCurMesh = m_pCharacter->Meshes[i];
			MaterialConstant* pMeshConst = (MaterialConstant*)pCurMesh->MaterialConstant.pData;

			pMeshConst->AlbedoFactor = Vector3(1.0f);
			pMeshConst->RoughnessFactor = 0.8f;
			pMeshConst->MetallicFactor = 0.0f;
		}
		m_pCharacter->Name = "MainCharacter";
		m_pCharacter->bIsPickable = true;
		m_pCharacter->UpdateWorld(Matrix::CreateScale(1.0f) * Matrix::CreateTranslation(center));

		m_RenderObjects.push_back((Model*)m_pCharacter);
	}
}

void App::updateAnimationState(const float DELTA_TIME)
{
	// States
	// 0: idle
	// 1: idle to walk
	// 2: walk forward
	// 3: walk to stop
	static int s_State = 0;
	static int s_FrameCount = 0;

	// 별도의 애니메이션 클립이 없을 경우.
	if (m_pCharacter->CharacterAnimationData.Clips.size() == 1)
	{
		/*if (!m_pPickedEndEffector)
		{
			goto LB_UPDATE;
		}

		m_pCharacter->UpdateCharacterIK(m_PickedTranslation, m_PickedEndEffectorType, s_State, s_FrameCount, DELTA_TIME);
		goto LB_UPDATE;*/

		goto LB_IK_PROCESS;
	}

	{
		const UINT64 ANIMATION_CLIP_SIZE = m_pCharacter->CharacterAnimationData.Clips[s_State].Keys[0].size();
		switch (s_State)
		{
			case 0:
				if (m_Keyboard.bPressed[VK_UP])
				{
					s_State = 1;
					s_FrameCount = 0;
				}
				else if (s_FrameCount == ANIMATION_CLIP_SIZE || m_Keyboard.bPressed[VK_UP]) // 재생이 다 끝난다면.
				{
					s_FrameCount = 0; // 상태 변화 없이 반복.
				}
				break;

			case 1:
				if (s_FrameCount == ANIMATION_CLIP_SIZE)
				{
					s_State = 2;
					s_FrameCount = 0;
				}
				break;

			case 2:
				if (m_Keyboard.bPressed[VK_RIGHT])
				{
					m_pCharacter->CharacterAnimationData.AccumulatedRootTransform =
						Matrix::CreateRotationY(DirectX::XM_PI * 60.0f / 180.0f * DELTA_TIME * 2.0f) *
						m_pCharacter->CharacterAnimationData.AccumulatedRootTransform;
				}
				if (m_Keyboard.bPressed[VK_LEFT])
				{
					m_pCharacter->CharacterAnimationData.AccumulatedRootTransform =
						Matrix::CreateRotationY(-DirectX::XM_PI * 60.0f / 180.0f * DELTA_TIME * 2.0f) *
						m_pCharacter->CharacterAnimationData.AccumulatedRootTransform;
				}
				if (s_FrameCount == ANIMATION_CLIP_SIZE)
				{
					// 방향키를 누르고 있지 않으면 정지. (누르고 있으면 계속 걷기)
					if (!m_Keyboard.bPressed[VK_UP])
					{
						s_State = 3;
					}
					s_FrameCount = 0;
				}
				break;

			case 3:
				if (s_FrameCount == ANIMATION_CLIP_SIZE)
				{
					s_State = 0;
					s_FrameCount = 0;
				}
				break;

			default:
				break;
		}
	}

LB_IK_PROCESS:
	if (!m_pPickedEndEffector)
	{
		goto LB_UPDATE;
	}
	m_pCharacter->UpdateCharacterIK(m_PickedTranslation, m_PickedEndEffectorType, s_State, s_FrameCount, DELTA_TIME);

LB_UPDATE:
	m_pCharacter->UpdateAnimation(s_State, s_FrameCount);
	++s_FrameCount;
}
