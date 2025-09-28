#include "PostProcessing.h"

void PostProcessing::DrawCall(
    std::shared_ptr<CommandList> pCommandList
) const {
    pCommandList->GetD3D12CommandList()->DrawInstanced(3, 1, 0, 0);
}
