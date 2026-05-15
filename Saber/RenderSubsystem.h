/**
 * @file RenderSubsystem.h
 * @brief Batched GPU-driven render subsystem using @c ExecuteIndirect.
 *
 * @ref RenderSubsystem<IndirectCommand> owns a list of @ref RenderObject instances
 * that share the same PSO, a @ref Buffer<ModelBuffer> for per-object constant data,
 * and an @ref IndirectCommandBuffer<IndirectCommand> for the GPU-driven draw arguments.
 *
 * Objects are registered with @ref Add; @ref InitializeModelBuffer and
 * @ref InitializeIndirectCommandBuffer prepare the GPU buffers; @ref PerformUpdate
 * flushes any pending CPU-side writes; @ref Render issues the @c ExecuteIndirect call.
 */
#pragma once

#include "Headers.h"

#include "IndirectCommand.h"
#include "IndirectCommandBuffer.h"
#include "MeshRenderObject.h"

/**
 * @brief Manages a batch of same-PSO objects rendered with a single @c ExecuteIndirect call.
 *
 * @tparam IndirectCommand Indirect-command struct; must satisfy @ref IndirectCommandConcept.
 */
template <IndirectCommandConcept IndirectCommand>
class RenderSubsystem {
	std::wstring m_name{};

	size_t m_capacity{}; /**< @brief Maximum number of objects. */

	std::vector<std::shared_ptr<RenderObject>> m_objects{};
	std::mutex m_objectsMutex{};

	std::shared_ptr<Buffer<ModelBuffer>> m_pModelBuffers{};
	std::shared_ptr<IndirectCommandBuffer<IndirectCommand>> m_pIndirectCommandBuffer{};

public:
	/**
	 * @brief Constructs the subsystem and reserves space for @p capacity objects.
	 * @param name     Debug name.
	 * @param capacity Maximum object count (default 128).
	 */
	RenderSubsystem(
		const std::wstring& name,
		size_t capacity = 128
	) : m_name(name),
		m_capacity(capacity)
	{
		IndirectCommandBase<IndirectCommand>::Assert();

		m_objects.reserve(m_capacity);
	}

	/**
	 * @brief Returns @c true if any GPU buffer has a pending CPU-to-GPU upload.
	 */
	bool IsUpdatePending() const {
		return (m_pModelBuffers && m_pModelBuffers->IsUpdatePending())
			|| (m_pIndirectCommandBuffer && m_pIndirectCommandBuffer->IsUpdatePending());
	}

	/**
	 * @brief Registers a @ref RenderObject with the subsystem.
	 *
	 * All objects must share the same PSO.  Stages model-buffer and indirect-command
	 * updates immediately if the GPU buffers are already initialised.
	 *
	 * @param pObject Object to register; must be a @ref MeshRenderObject<ModelBuffer>.
	 * @return @c true if added; @c false if the subsystem is at capacity.
	 */
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
			m_pIndirectCommandBuffer->UpdateAt(id, indirectCommand);
		}
		if (m_pModelBuffers) {
			ModelBuffer modelBuffer{ pMeshObject->GetModelBuffer() };
			m_pModelBuffers->UpdateAt(id, modelBuffer);
		}
	}

	/**
	 * @brief Binds state shared by all objects and issues the @c ExecuteIndirect call.
	 * @param pCommandList      Direct command list.
	 * @param commandListPrepare Callback invoked after PSO / root signature binding
	 *                          and before the draw; use it to bind scene-level descriptors.
	 * @param offset            Unused placeholder for future draw-offset support.
	 */
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
		if (m_pModelBuffers->GetResource()->GetState() != D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) {
			m_pModelBuffers->GetResource()->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		}
		pCommandList->GetD3D12CommandList()->SetGraphicsRootShaderResourceView(
			2,
			m_pModelBuffers->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
		);
		m_pIndirectCommandBuffer->Execute(pCommandList);
	}

	/**
	 * @brief Creates the per-object model-buffer GPU resource and stages initial data.
	 * @param pDeviceContext   Device context.
	 * @param pIndirectUpdater Unused; reserved for future GPU-driven model-buffer updates.
	 * @return Always @c true.
	 */
	bool InitializeModelBuffer(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<ComputeObject> pIndirectUpdater = nullptr
	) {
		m_pModelBuffers = std::make_shared<Buffer<ModelBuffer>>(
			m_name + L"/ModelBuffers",
			pDeviceContext,
			m_capacity,
			GPUResource::AllocationDesc{ D3D12_HEAP_TYPE_DEFAULT },
			GPUResource::ResourceDesc{ CD3DX12_RESOURCE_DESC::Buffer(0) }
		);
		m_pModelBuffers->CreateStorage<VectorBufferStorage<ModelBuffer>>();
		m_pModelBuffers->CreateUpdater<RangeBufferUpdater<ModelBuffer>>();

		for (size_t i{}; i < m_objects.size(); ++i) {
			auto pMeshObject = std::dynamic_pointer_cast<MeshRenderObject<ModelBuffer>>(m_objects[i]);
			ModelBuffer modelBuffer{ pMeshObject->GetModelBuffer() };
			m_pModelBuffers->UpdateAt(i, modelBuffer);
		}

		return true;
	}

	/**
	 * @brief Creates the indirect-command buffer and stages initial draw arguments.
	 *
	 * When @p pIndirectUpdater is provided the buffer is configured with a
	 * @ref DynamicBufferUpdater (GPU scatter-write); otherwise a
	 * @ref RangeBufferUpdater is used with CPU-side storage.
	 *
	 * @param pDeviceContext   Device context.
	 * @param pIndirectUpdater Optional compute object for GPU-side updates.
	 * @return @c false if the object list is empty; @c true otherwise.
	 */
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
			m_pIndirectCommandBuffer->CreateStorage<VectorBufferStorage<IndirectCommand>>();
			m_pIndirectCommandBuffer->CreateUpdater<RangeBufferUpdater<IndirectCommand>>();
		}

		for (size_t i{}; i < m_objects.size(); ++i) {
			IndirectCommand indirectCommand;
			m_objects[i]->FillIndirectCommand(indirectCommand);
			m_pIndirectCommandBuffer->UpdateAt(i, indirectCommand);
		}

		return true;
	}

	/**
	 * @brief Flushes pending model-buffer and indirect-command-buffer updates to the GPU.
	 * @param pDeviceContext Device context.
	 * @param pCommandList  Command list for copy/barrier commands.
	 */
	void PerformUpdate(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandList
	) {
		if (m_pModelBuffers) {
			m_pModelBuffers->PerformUpdate(
				pDeviceContext,
				pCommandList
			);
		}
		if (m_pIndirectCommandBuffer) {
			m_pIndirectCommandBuffer->PerformUpdate(
				pDeviceContext,
				pCommandList
			);
		}
	}
};
