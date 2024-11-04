#include "PostProcessing.h"

void PostProcessing::Render(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
	UINT rootParameterIndex
) {
    RenderJob(pCommandListDirect);
    InnerRootParametersSetter(pCommandListDirect, rootParameterIndex);
    pCommandListDirect->DrawInstanced(
        GetIndexCountPerInstance(),
        GetInstanceCount(),
        0, 0
    );
}

UINT PostProcessing::GetIndexCountPerInstance() const {
    return 3;
}

UINT PostProcessing::GetInstanceCount() const {
    return 1;
}
