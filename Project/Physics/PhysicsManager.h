#pragma once

class PhysicsManager
{
public:
	PhysicsManager() = default;
	~PhysicsManager() { Cleanup(); };

	void Initialize(UINT numThreads = 1);

	void Update(const float DELTA_TIME);

	void AddActor(physx::PxRigidActor* pActor);

	void Cleanup();

public:
	physx::PxMaterial* pCommonMaterial = nullptr;

private:
	const physx::PxVec3 m_GRAVITY = physx::PxVec3(0.0f, -9.81f, 0.0f);

	physx::PxDefaultAllocator m_Allocator;
	physx::PxDefaultErrorCallback m_ErrorCallback;
	physx::PxFoundation* m_pFoundation = nullptr;
	physx::PxPhysics* m_pPhysics = nullptr;
	physx::PxScene* m_pScene = nullptr;
	physx::PxPvd* m_pPVD = nullptr;
	physx::PxTaskManager* m_pTaskManager = nullptr;
	physx::PxDefaultCpuDispatcher* m_pDispatcher = nullptr;

};
