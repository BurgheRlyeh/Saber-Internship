#pragma once

#include "Headers.h"

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
