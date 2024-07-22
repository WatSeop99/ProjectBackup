#include "../pch.h"
#include "PhysicsManager.h"

using namespace physx;

#define PVD_HOST "127.0.0.1"

void PhysicsManager::Initialize(UINT numThreads)
{
	_ASSERT(numThreads >= 1);

	m_pFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_Allocator, m_ErrorCallback);
	if (!m_pFoundation)
	{
		__debugbreak();
	}
	
	m_pPVD = PxCreatePvd(*m_pFoundation);
	if (!m_pPVD)
	{
		__debugbreak();
	}

	PxPvdTransport* pTransport = PxDefaultPvdSocketTransportCreate(PVD_HOST, 5425, 10);
	m_pPVD->connect(*pTransport, PxPvdInstrumentationFlag::ePROFILE);

	m_pPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_pFoundation, PxTolerancesScale(), true, m_pPVD);
	if (!m_pPhysics)
	{
		__debugbreak();
	}

	PxSceneDesc sceneDesc(m_pPhysics->getTolerancesScale());
	sceneDesc.gravity = m_GRAVITY;

	m_pDispatcher = PxDefaultCpuDispatcherCreate(numThreads);
	if (!m_pDispatcher)
	{
		__debugbreak();
	}
	sceneDesc.cpuDispatcher = m_pDispatcher;
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;

	m_pScene = m_pPhysics->createScene(sceneDesc);
	if (!m_pScene)
	{
		__debugbreak();
	}

	PxPvdSceneClient* pPVDClient = m_pScene->getScenePvdClient();
	if (pPVDClient)
	{
		pPVDClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, false);
		pPVDClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_CONTACTS, false);
		pPVDClient->setScenePvdFlag(PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, false);
	}
	else
	{
		__debugbreak();
	}

	m_pTaskManager = PxTaskManager::createTaskManager(m_pFoundation->getErrorCallback(), m_pDispatcher);
	if (!m_pTaskManager)
	{
		__debugbreak();
	}

	// pCommonMaterial = m_pPhysics->createMaterial(0.5f, 0.5f, 0.6f);
	pCommonMaterial = m_pPhysics->createMaterial(0.5f, 0.5f, 0.6f); // (¸¶Âû·Â, dynamic ¸¶Âû·Â, Åº¼º·Â)
}

void PhysicsManager::Update(const float DELTA_TIME)
{
	_ASSERT(m_pScene);

	m_pScene->simulate(DELTA_TIME);
	m_pScene->fetchResults(true);
}

void PhysicsManager::AddActor(physx::PxRigidActor* pActor)
{
	_ASSERT(m_pScene);
	_ASSERT(pActor);

	m_pScene->addActor(*pActor);
}

void PhysicsManager::Cleanup()
{
	PX_RELEASE(m_pTaskManager);
	PX_RELEASE(m_pScene);
	PX_RELEASE(m_pDispatcher);
	PX_RELEASE(m_pPhysics);
	if (m_pPVD)
	{
		PxPvdTransport* pTransport = m_pPVD->getTransport();
		PX_RELEASE(m_pPVD);
		PX_RELEASE(pTransport);
	}
	PX_RELEASE(m_pFoundation);
}
