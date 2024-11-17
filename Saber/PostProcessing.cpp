#include "PostProcessing.h"

void PostProcessing::DrawCall(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList
) const {
    pCommandList->DrawInstanced(3, 1, 0, 0);
}
