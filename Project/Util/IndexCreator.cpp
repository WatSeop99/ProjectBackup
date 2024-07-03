#include "../pch.h"
#include "IndexCreator.h"

IndexCreator::~IndexCreator()
{
	Check();
	Clear();
}

void IndexCreator::Initialize(ULONG num)
{
	m_pIndexTable = new ULONG[num];
	memset(m_pIndexTable, 0, sizeof(ULONG) * num);
	m_MaxNum = num;

	for (ULONG i = 0; i < m_MaxNum; ++i)
	{
		m_pIndexTable[i] = i;
	}
}

ULONG IndexCreator::Alloc()
{
	// 1. m_lAllocatedCount���� 1�� ����.
	// 2. m_lAllocatedCount-1��ġ�� dwIndex�� ��ִ´�.
	// �� �ΰ��� �׼��� �ʿ��ѵ� 1�� 2���̿� �ٸ� �����尡 Alloc�� ȣ���ϸ� �̹� �Ҵ�� �ε����� ���� ���� �߻��Ѵ�.
	// ���� Alloc�� Free���� �� ���ɶ����� ���ƾ��Ѵ�.

	ULONG result = 0xffff;

	if (m_AllocatedCount >= m_MaxNum)
	{
		goto LB_RET;
	}

	result = m_pIndexTable[m_AllocatedCount];
	++m_AllocatedCount;

LB_RET:
	return result;
}

void IndexCreator::Free(ULONG index)
{
	if (!m_AllocatedCount)
	{
		__debugbreak();
	}
	--m_AllocatedCount;
	m_pIndexTable[m_AllocatedCount] = index;
}

void IndexCreator::Clear()
{
	m_MaxNum = 0;
	m_AllocatedCount = 0;

	if (m_pIndexTable)
	{
		delete[] m_pIndexTable;
		m_pIndexTable = nullptr;
	}
}

void IndexCreator::Check()
{
	if (m_AllocatedCount)
	{
		__debugbreak();
	}
}
