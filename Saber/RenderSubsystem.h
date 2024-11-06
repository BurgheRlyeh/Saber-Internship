#pragma once

#include "Headers.h"
#include "IndirectCommandBuffer.h"

#include "RenderObject.h"
#include "MeshRenderObject.h"
#include "SeparateChainingMap.h"

class RenderSubsystem {
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

private:
	size_t PsoToMapKey(Microsoft::WRL::ComPtr<ID3D12PipelineState> pPso);
};

//template <size_t ObjectsMaxCount>
//class IndirectRenderSubsystem : public RenderSubsystem {
//	std::wstring m_name{};
//
//	struct IndirectCommand {
//		D3D12_DRAW_INDEXED_ARGUMENTS drawArguments{};
//		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
//		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
//		D3D12_GPU_VIRTUAL_ADDRESS cbv{};
//		UINT alignment{};
//	};
//	std::shared_ptr<IndirectCommandBuffer<IndirectCommand, ObjectsMaxCount>> m_pIndirectCommandBuffer{};
//
//	static inline D3D12_INDIRECT_ARGUMENT_DESC m_indirectArgumentDescs[4]{
//		{.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED } },
//		{.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW }, .VertexBuffer{ .Slot{ 0 } } },
//		{.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW }, },
//		{.Type{ D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW }, .ConstantBufferView{ 1 } }
//	};
//	static inline D3D12_COMMAND_SIGNATURE_DESC m_commandSignatureDesc{
//		.ByteStride{ sizeof(IndirectCommand) },
//		.NumArgumentDescs{ _countof(m_indirectArgumentDescs) },
//		.pArgumentDescs{ m_indirectArgumentDescs }
//	};
//
//public:
//	IndirectRenderSubsystem(
//		const std::wstring& name
//	) : m_name(name) {}
//
//	void InitializeIndirectCommandBuffer(
//		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
//		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
//		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
//		const std::wstring& objectName,
//		std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap,
//		std::shared_ptr<ComputeObject> pIndirectUpdater
//	) {
//		m_pIndirectCommandBuffer = std::make_shared<
//			DynamicIndirectCommandBuffer<IndirectCommand, ObjectsMaxCount>
//		>(
//			pDevice,
//			pAllocator,
//			m_commandSignatureDesc,
//			m_pRootSignatureResource->pRootSignature,
//			pDescHeapManagerCbvSrvUav,
//			objectName,
//			pDynamicUploadHeap,
//			pIndirectUpdater
//		);
//	}
//};
