#include "RenderSubsystem.h"

void RenderSubsystem::Add(std::shared_ptr<MeshRenderObject> pObject) {
	m_objects.Add(PsoToMapKey(pObject->GetPipelineState()), pObject);
}

void RenderSubsystem::Render(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect, D3D12_VIEWPORT viewport, D3D12_RECT rect, D3D12_CPU_DESCRIPTOR_HANDLE* pRTVs, size_t rtvsCount, D3D12_CPU_DESCRIPTOR_HANDLE* pDSV, std::function<void(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>, UINT&)> outerRootParametersSetter) {
	m_objects.ForEachValue([&](std::shared_ptr<MeshRenderObject> pObject) {
		pObject->Render(
			pCommandListDirect,
			viewport,
			rect,
			pRTVs,
			rtvsCount,
			pDSV,
			outerRootParametersSetter
		);
	});
}

size_t RenderSubsystem::PsoToMapKey(Microsoft::WRL::ComPtr<ID3D12PipelineState> pPso) {
	return reinterpret_cast<size_t>(pPso.Get());
}
