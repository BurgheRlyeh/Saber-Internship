#include "RenderSubsystem.h"

void RenderSubsystem::Add(std::shared_ptr<RenderObject> pObject) {
	m_objects.Add(PsoToMapKey(pObject->GetPipelineState()), pObject);
}

void RenderSubsystem::Render(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
	D3D12_VIEWPORT viewport,
	D3D12_RECT rect,
	D3D12_CPU_DESCRIPTOR_HANDLE* pRTVs,
	size_t rtvsCount,
	D3D12_CPU_DESCRIPTOR_HANDLE* pDSV,
	std::function<void(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>, UINT&)> outerRootParametersSetter
) {
	UINT rootParameterIndex{};
	m_objects.ForEachValue(
		[&](std::shared_ptr<RenderObject> pObject) {
			pObject->Render(pCommandList, rootParameterIndex);
		},
		[&](std::shared_ptr<RenderObject> pObject) {
			pObject->SetPipelineStateAndRootSignature(pCommandList);

			pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			pCommandList->RSSetViewports(1, &viewport);
			pCommandList->RSSetScissorRects(1, &rect);

			pCommandList->OMSetRenderTargets(static_cast<UINT>(rtvsCount), pRTVs, FALSE, pDSV);

			outerRootParametersSetter(pCommandList, rootParameterIndex);
		}
	);
}

size_t RenderSubsystem::PsoToMapKey(Microsoft::WRL::ComPtr<ID3D12PipelineState> pPso) {
	return reinterpret_cast<size_t>(pPso.Get());
}
