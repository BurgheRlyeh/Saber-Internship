#pragma once

#include "Headers.h"

#include "CullingParams.h"
#include "IndirectCommand.h"
#include "IndirectCommandBuffer.h"
#include "MeshRenderObject.h"
#include "RenderSubsystemTypes.h"

template <IndirectCommandConcept IndirectCommand>
class RenderSubsystem {
	std::wstring m_name{};

	size_t m_capacity{};

	std::vector<std::shared_ptr<RenderObject>> m_objects{};
	std::mutex m_objectsMutex{};

	std::shared_ptr<Buffer<ModelBuffer>> m_pModelBuffers{};
	std::shared_ptr<IndirectCommandBuffer<IndirectCommand>> m_pIndirectCommandBuffer{};

	// One uint per object: was it visible last frame. The first pass reads it, the
	// second one writes it for the frame after
	std::shared_ptr<Buffer<uint32_t>> m_pVisibility{};

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

	size_t GetObjectsCount() {
		std::scoped_lock<std::mutex> lock(m_objectsMutex);
		return m_objects.size();
	}

	bool IsUpdatePending() const {
		return (m_pModelBuffers && m_pModelBuffers->IsUpdatePending())
			|| (m_pIndirectCommandBuffer && m_pIndirectCommandBuffer->IsUpdatePending())
			|| (m_pVisibility && m_pVisibility->IsUpdatePending());
	}

	size_t Add(std::shared_ptr<RenderObject> pObject) {
		std::unique_lock<std::mutex> lock(m_objectsMutex);
		assert(m_objects.empty() || pObject->GetPipelineState() == m_objects.front()->GetPipelineState());
		if (m_objects.size() == m_capacity) {
			return InvalidRenderObjectId;
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
		return id;
	}

	void UpdateModelMatrix(size_t id, const DirectX::XMMATRIX& modelMatrix) {
		if (!m_pModelBuffers || id >= m_objects.size()) {
			assert(false);
			return;
		}

		ModelBuffer modelBuffer{ m_pModelBuffers->GetStorageData()[id] };
		modelBuffer.UpdateMatrices(modelMatrix);
		m_pModelBuffers->UpdateAt(id, modelBuffer);
	}

	void Render(
		std::shared_ptr<CommandList> pCommandList,
		const std::function<void()>& commandListPrepare,
		bool drawCulledCommands = false
	) {
		std::scoped_lock<std::mutex> lock(m_objectsMutex);
		if (m_objects.empty()) {
			return;
		}
		m_objects.front()->SetPipelineStateAndRootSignature(pCommandList);
		commandListPrepare();
		m_pModelBuffers->GetResource()->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		pCommandList->GetD3D12CommandList()->SetGraphicsRootShaderResourceView(
			2,
			m_pModelBuffers->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
		);

		if (drawCulledCommands) {
			m_pIndirectCommandBuffer->ExecuteCulled(pCommandList);
		}
		else {
			m_pIndirectCommandBuffer->Execute(pCommandList, m_objects.size());
		}
	}

	// Fills the culled command buffer for one of the two passes. Which pass it is,
	// what is switched on and which bounding volume to test all come in as params
	void Cull(
		std::shared_ptr<CommandList> pCommandList,
		const std::shared_ptr<ComputeObject>& pOcclusionCulling,
		Microsoft::WRL::ComPtr<D3D12DescriptorHeap> pResDescHeap,
		D3D12_GPU_DESCRIPTOR_HANDLE hzbSrvHandle,
		D3D12_GPU_VIRTUAL_ADDRESS cullingCameraAddress,
		D3D12_GPU_VIRTUAL_ADDRESS statsAddress,
		CullingParams params
	) {
		std::scoped_lock<std::mutex> lock(m_objectsMutex);
		if (m_objects.empty() || !m_pIndirectCommandBuffer || !m_pVisibility) {
			return;
		}

		params.objectCount = static_cast<uint32_t>(m_objects.size());

		m_pIndirectCommandBuffer->PrepareForCulling(pCommandList);
		m_pModelBuffers->GetResource()->ResourceTransition(
			pCommandList,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
		);
		m_pVisibility->GetResource()->ResourceTransition(
			pCommandList,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		);

		const UINT groupCount{
			(params.objectCount + CULLING_GROUP_SIZE - 1) / CULLING_GROUP_SIZE
		};
		pOcclusionCulling->Dispatch(
			pCommandList,
			{ groupCount, 1, 1 },
			[&](std::shared_ptr<CommandList> pDispatchList, UINT& rootParamId) {
				auto pD3D12CommandList{ pDispatchList->GetD3D12CommandList() };
				pD3D12CommandList->SetDescriptorHeaps(1, pResDescHeap.GetAddressOf());

				pD3D12CommandList->SetComputeRoot32BitConstants(
					0,
					sizeof(CullingParams) / sizeof(uint32_t),
					&params,
					0
				);
				pD3D12CommandList->SetComputeRootConstantBufferView(1, cullingCameraAddress);
				pD3D12CommandList->SetComputeRootShaderResourceView(
					2,
					m_pModelBuffers->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
				);
				pD3D12CommandList->SetComputeRootShaderResourceView(
					3,
					m_pIndirectCommandBuffer->GetSourceCommandsAddress()
				);
				pD3D12CommandList->SetComputeRootUnorderedAccessView(
					4,
					m_pIndirectCommandBuffer->GetCulledCommandsAddress()
				);
				pD3D12CommandList->SetComputeRootUnorderedAccessView(
					5,
					m_pIndirectCommandBuffer->GetCulledCommandsCountAddress()
				);
				pD3D12CommandList->SetComputeRootUnorderedAccessView(
					6,
					m_pVisibility->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
				);
				pD3D12CommandList->SetComputeRootUnorderedAccessView(7, statsAddress);
				pD3D12CommandList->SetComputeRootDescriptorTable(8, hzbSrvHandle);
			}
		);

		// The command buffers are ordered by their transition to indirect argument,
		// the visibility buffer stays a uav across both passes and is not
		pCommandList->UavBarrier(m_pVisibility->GetResource());
	}

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

		// Indexed the same way as the commands. Bound as a root descriptor, so no
		// view of its own
		const size_t capacity{ m_pIndirectCommandBuffer->GetCapacity() };
		m_pVisibility = std::make_shared<Buffer<uint32_t>>(
			m_name + L"/Visibility",
			pDeviceContext,
			capacity,
			GPUResource::AllocationDesc{ D3D12_HEAP_TYPE_DEFAULT },
			GPUResource::ResourceDesc{
				.resDesc{ CD3DX12_RESOURCE_DESC::Buffer(
					capacity * sizeof(uint32_t),
					D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
				) }
			},
			EnumFlags<ResourceView>{ ResourceView::None }
		);
		m_pVisibility->CreateStorage<VectorBufferStorage<uint32_t>>();
		m_pVisibility->CreateUpdater<RangeBufferUpdater<uint32_t>>();

		// Nothing is known to be visible before the first frame, so the first pass
		// draws nothing and the second one draws everything
		const std::vector<uint32_t> notVisible(capacity, 0u);
		m_pVisibility->UpdateAll(notVisible.data(), notVisible.size());

		return true;
	}


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
		if (m_pVisibility) {
			m_pVisibility->PerformUpdate(
				pDeviceContext,
				pCommandList
			);
		}
	}
};
