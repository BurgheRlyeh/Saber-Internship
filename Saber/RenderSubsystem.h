#pragma once

#include "Headers.h"

#include "IndirectCommand.h"
#include "IndirectCommandBuffer.h"
#include "RenderObject.h"
#include "MeshRenderObject.h"
#include "SeparateChainingMap.h"

template <IndirectCommandConcept IndirectCommand>
class RenderSubsystem {
	std::wstring m_name{};

	size_t m_capacity{};

	std::vector<std::shared_ptr<RenderObject>> m_objects{};
	std::mutex m_objectsMutex{};

	std::shared_ptr<IndirectCommandBuffer<IndirectCommand>> m_pIndirectCommandBuffer{};

	std::shared_ptr<Buffer<ModelBuffer>> m_pModelBuffers{};

public:
	RenderSubsystem(
		const std::wstring& name,
		size_t capacity = 128
	) : m_name(name),
		m_capacity(capacity)
	{
		IndirectCommandBase<IndirectCommand>::Assert();

		m_objects.reserve(m_capacity);
	}

	bool Add(std::shared_ptr<RenderObject> pObject) {
		std::unique_lock<std::mutex> lock(m_objectsMutex);
		assert(m_objects.empty() || pObject->GetPipelineState() == m_objects.front()->GetPipelineState());
		if (m_objects.size() == m_capacity) {
			return false;
		}
		m_objects.push_back(pObject);
		size_t id{ m_objects.size() - 1 };
		lock.unlock();

		auto pMeshObject{ std::dynamic_pointer_cast<MeshRenderObject<ModelBuffer>>(pObject) };
		pMeshObject->SetModelBufferId(id);

		if (m_pIndirectCommandBuffer) {
			IndirectCommand indirectCommand;
			pMeshObject->FillIndirectCommand(indirectCommand);
			m_pIndirectCommandBuffer->SetUpdateAt(id, indirectCommand);
		}
		if (m_pModelBuffers) {
			ModelBuffer modelBuffer{ pMeshObject->GetModelBuffer() };
			m_pModelBuffers->SetUpdateAt(id, modelBuffer);
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
		pCommandList->GetD3D12CommandList()->SetGraphicsRootShaderResourceView(
			2,
			m_pModelBuffers->GetResource()->GetResource()->GetGPUVirtualAddress()
		);
		m_pIndirectCommandBuffer->Execute(pCommandList);
	}

	bool InitializeModelBuffer(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<ComputeObject> pIndirectUpdater = nullptr
	) {
		m_pModelBuffers = std::make_shared<Buffer<ModelBuffer>>(
			m_name + L"/ModelBuffers",
			pDeviceContext,
			m_capacity,
			GPUResource::HeapData{ D3D12_HEAP_TYPE_UPLOAD },
			GPUResource::ResourceData{ CD3DX12_RESOURCE_DESC::Buffer(0), D3D12_RESOURCE_STATE_GENERIC_READ }
		);
		m_pModelBuffers->CreateUpdater<InstUploadBufferUpdater<ModelBuffer>>();

		for (size_t i{}; i < m_objects.size(); ++i) {
			auto pMeshObject = std::dynamic_pointer_cast<MeshRenderObject<ModelBuffer>>(m_objects[i]);
			ModelBuffer modelBuffer{ pMeshObject->GetModelBuffer() };
			m_pModelBuffers->SetUpdateAt(i, modelBuffer);
		}

		return true;
	}

	bool InitializeIndirectCommandBuffer(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<ComputeObject> pIndirectUpdater = nullptr
	) {
		std::scoped_lock<std::mutex> lock(m_objectsMutex);
		if (m_objects.empty()) {
			return false;
		}

		m_pIndirectCommandBuffer = std::make_shared<
			IndirectCommandBuffer<IndirectCommand>
		>(
			m_name + L"/IndirectCommandBuffer",
			pDeviceContext,
			IndirectCommand::GetCommandSignatureDesc(),
			m_objects.front()->GetRootSignature(),
			m_objects.size()
		);
		if (pIndirectUpdater) {
			m_pIndirectCommandBuffer->CreateUpdater<DynamicBufferUpdater<IndirectCommand>>(pIndirectUpdater);
		}
		else {
			m_pIndirectCommandBuffer->CreateUpdater<StaticBufferUpdater<IndirectCommand>>();
		}

		for (size_t i{}; i < m_objects.size(); ++i) {
			IndirectCommand indirectCommand;
			m_objects[i]->FillIndirectCommand(indirectCommand);
			m_pIndirectCommandBuffer->SetUpdateAt(i, indirectCommand);
		}

		return true;
	}

	bool PerformIndirectBufferUpdate(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	) {
		if (!m_pIndirectCommandBuffer) {
			return false;
		}
		m_pIndirectCommandBuffer->PerformUpdate(
			pDeviceContext,
			pCommandQueueDirect
		);
		return true;
	}

	bool PerformModelBufferUpdate(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandQueue> pCommandQueueDirect
	) {
		if (!m_pModelBuffers) {
			return false;
		}
		m_pModelBuffers->PerformUpdate(
			pDeviceContext,
			pCommandQueueDirect
		);
		return true;
	}
};
