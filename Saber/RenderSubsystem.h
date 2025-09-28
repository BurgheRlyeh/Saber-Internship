#pragma once

#include "Headers.h"

#include "IndirectCommand.h"
#include "IndirectCommandBuffer.h"
#include "RenderObject.h"
#include "MeshRenderObject.h"
#include "SeparateChainingMap.h"

template <typename IndirectCommand>
class RenderSubsystem {
	std::wstring m_name{};

	Microsoft::WRL::ComPtr<ID3D12Device2> m_pDevice{};

	size_t m_capacity{};

	std::vector<std::shared_ptr<RenderObject>> m_objects{};
	std::mutex m_objectsMutex{};

	std::shared_ptr<IndirectCommandBuffer<IndirectCommand>> m_pIndirectCommandBuffer{};

	std::shared_ptr<DescHeapRange> m_pModelBuffersCbvRange{};

public:
	RenderSubsystem(
		const std::wstring& name,
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		size_t capacity = 128
	) : m_name(name),
		m_pDevice(pDevice),
		m_capacity(capacity)
	{
		IndirectCommandBase<IndirectCommand>::Assert();

		m_objects.reserve(m_capacity);
	}

	bool Add(std::shared_ptr<RenderObject> pObject) {
		std::scoped_lock<std::mutex> lock(m_objectsMutex);
		if (m_objects.size() == m_capacity) {
			return false;
		}
		if (!m_objects.empty()) {
			assert(pObject->GetPipelineState() == m_objects.front()->GetPipelineState());
		}
		m_objects.push_back(pObject);
		if (m_pIndirectCommandBuffer) {
			IndirectCommand indirectCommand;
			pObject->FillIndirectCommand(indirectCommand);
			m_pIndirectCommandBuffer->SetUpdateAt(m_objects.size() - 1, indirectCommand);
		}
		if (m_pModelBuffersCbvRange) {
			// TODO fix ModelBuffer struct specification
			std::shared_ptr<MeshRenderObject<ModelBuffer>> pMeshObject{
				std::dynamic_pointer_cast<MeshRenderObject<ModelBuffer>>(pObject)
			};
			pMeshObject->AllocateModelBufferCbv(m_pDevice, m_pModelBuffersCbvRange);
		}
	}

	void Render(
		std::shared_ptr<CommandList> pCommandList,
		const std::function<void()>& commandListPrepare,
		bool offset = false
	) {
		std::scoped_lock<std::mutex> lock(m_objectsMutex);
		if (m_objects.empty()) {
			return;
		}
		m_objects.front()->SetPipelineStateAndRootSignature(pCommandList);
		commandListPrepare();
		pCommandList->GetD3D12CommandList()->SetGraphicsRootDescriptorTable(3 + offset, m_pModelBuffersCbvRange->GetGpuHandle());
		m_pIndirectCommandBuffer->Execute(pCommandList);
	}

	void CreateModelBuffersRange(
		std::shared_ptr<DescriptorHeapManager> pResDescHeapManager
	) {
		m_pModelBuffersCbvRange = pResDescHeapManager->AllocateRange(
			m_name + L"/ModelBuffersCbvRange",
			m_capacity
		);
	}

	bool InitializeIndirectCommandBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
		std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap,
		std::shared_ptr<ComputeObject> pIndirectUpdater = nullptr
	) {
		std::scoped_lock<std::mutex> lock(m_objectsMutex);
		if (m_objects.empty()) {
			return false;
		}

		if (pIndirectUpdater) {
			m_pIndirectCommandBuffer = std::make_shared<
				DynamicIndirectCommandBuffer<IndirectCommand>
			>(
				pDevice,
				pAllocator,
				IndirectCommand::GetCommandSignatureDesc(),
				m_objects.front()->GetRootSignature(),
				pDescHeapManagerCbvSrvUav,
				m_name + L"IndirectBuffer",
				m_objects.size(),
				pDynamicUploadHeap,
				pIndirectUpdater
			);
		}
		else {
			m_pIndirectCommandBuffer = std::make_shared<
				StaticIndirectCommandBuffer<IndirectCommand>
			>(
				pDevice,
				pAllocator,
				IndirectCommand::GetCommandSignatureDesc(),
				m_objects.front()->GetRootSignature(),
				pDescHeapManagerCbvSrvUav,
				m_name + L"IndirectBuffer",
				m_objects.size(),
				pDynamicUploadHeap
			);
		}

		for (size_t i{}; i < m_objects.size(); ++i) {
			IndirectCommand indirectCommand;
			m_objects[i]->FillIndirectCommand(indirectCommand);
			m_pIndirectCommandBuffer->SetUpdateAt(i, indirectCommand);
		}

		return true;
	}

	bool PerformIndirectBufferUpdate(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandQueue> pCommandQueueCopy,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	) {
		if (!m_pIndirectCommandBuffer) {
			return false;
		}
		m_pIndirectCommandBuffer->PerformUpdate(
			pDevice,
			pAllocator,
			pCommandQueueCopy,
			pCommandQueueDirect
		);
		return true;
	}
};
