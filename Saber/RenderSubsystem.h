#pragma once

#include "Headers.h"
#include "IndirectCommandBuffer.h"

#include "RenderObject.h"
#include "RenderModel.h"
#include "ModelBuffers.h"
#include "SeparateChainingMap.h"

class RenderSubsystem {
protected:
	UnorderedSeparateChainingMap<size_t, std::shared_ptr<RenderObject>> m_objects{};
	
public:
	void Add(std::shared_ptr<RenderObject> pObject);

	void Render(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
		D3D12_VIEWPORT viewport,
		D3D12_RECT rect,
		D3D12_CPU_DESCRIPTOR_HANDLE* pRTVs,
		size_t rtvsCount,
		D3D12_CPU_DESCRIPTOR_HANDLE* pDSV,
		std::function<void(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>, UINT&)> outerRootParametersSetter
	);

protected:
	size_t PsoToMapKey(Microsoft::WRL::ComPtr<ID3D12PipelineState> pPso);
};

class IndirectRenderSubsystem {
	std::wstring m_name{};
	std::vector<std::shared_ptr<MeshRenderObject>> m_objects{};
	std::mutex m_objectsMutex{};

	std::shared_ptr<IndirectCommandBuffer<MeshRenderObject::IndirectCommand>> m_pIndirectCommandBuffer{};

public:
	IndirectRenderSubsystem(const std::wstring& name) : m_name(name) {}

	void Add(std::shared_ptr<MeshRenderObject> pObject) {
		std::scoped_lock<std::mutex> lock(m_objectsMutex);
		if (!m_objects.empty()) {
			assert(pObject->GetPipelineState() == m_objects.front()->GetPipelineState());
		}
		m_objects.push_back(pObject);
		if (m_pIndirectCommandBuffer) {
			m_pIndirectCommandBuffer->SetUpdateAt(m_objects.size() - 1, pObject->GetIndirectCommand());
		}
	}

	void Render(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
		D3D12_VIEWPORT viewport,
		D3D12_RECT rect,
		D3D12_CPU_DESCRIPTOR_HANDLE* pRTVs,
		size_t rtvsCount,
		D3D12_CPU_DESCRIPTOR_HANDLE* pDSV,
		std::function<void(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>, UINT&)> outerRootParametersSetter
	) {
		if (m_objects.empty()) {
			return;
		}

		UINT rootParameterIndex{};
		std::function forPso{ [&](std::shared_ptr<RenderObject> pObject) {
			pObject->SetPipelineStateAndRootSignature(pCommandList);

			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			pCommandList->RSSetViewports(1, &viewport);
			pCommandList->RSSetScissorRects(1, &rect);

			pCommandList->OMSetRenderTargets(static_cast<UINT>(rtvsCount), pRTVs, FALSE, pDSV);

			outerRootParametersSetter(pCommandList, rootParameterIndex);
		} };
		std::function forObj{ [&](std::shared_ptr<RenderObject> pObject) {
			pObject->Render(pCommandList, rootParameterIndex);
		} };

		//m_objects.ForEachValue(forObj, forPso);
		std::scoped_lock<std::mutex> lock(m_objectsMutex);
		forPso(m_objects.front());
		m_pIndirectCommandBuffer->Execute(pCommandList);

		//for (const auto& pObject : m_objects) {
		//	forObj(pObject);
		//}
	}

	bool InitializeIndirectCommandBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
		std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap,
		std::shared_ptr<ComputeObject> pIndirectUpdater
	) {
		std::scoped_lock<std::mutex> lock(m_objectsMutex);
		if (m_objects.empty()) {
			return false;
		}
		m_pIndirectCommandBuffer = std::make_shared<
			DynamicIndirectCommandBuffer<MeshRenderObject::IndirectCommand>
		>(
			pDevice,
			pAllocator,
			MeshRenderObject::IndirectCommand::GetCommandSignatureDesc(),
			m_objects.front()->GetRootSignature(),
			pDescHeapManagerCbvSrvUav,
			m_name + L"IndirectBuffer",
			m_objects.size(),
			pDynamicUploadHeap,
			pIndirectUpdater
		);

		for (size_t i{}; i < m_objects.size(); ++i) {
			m_pIndirectCommandBuffer->SetUpdateAt(i, m_objects[i]->GetIndirectCommand());
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
