#include "UIContext.h"

#include "CommandList.h"
#include "CommandQueue.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "DeviceContext.h"
#include "Device.h"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

// ImGui DX12 backend wants C-style callbacks to allocate/free SRVs.
namespace {
    DescRange* g_pImGuiSrvRange{};

    void ImGuiSrvAlloc(
        ImGui_ImplDX12_InitInfo*,
        D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
        D3D12_GPU_DESCRIPTOR_HANDLE* outGpu
    ) {
        assert(g_pImGuiSrvRange);
        const size_t id{ g_pImGuiSrvRange->Allocate() };
        *outCpu = g_pImGuiSrvRange->GetCpuHandle(id);
        *outGpu = g_pImGuiSrvRange->GetGpuHandle(id);
    }

    void ImGuiSrvFree(
        ImGui_ImplDX12_InitInfo*,
        D3D12_CPU_DESCRIPTOR_HANDLE cpu,
        D3D12_GPU_DESCRIPTOR_HANDLE /*gpu*/
    ) {
        assert(g_pImGuiSrvRange);
        g_pImGuiSrvRange->Free(cpu);
    }
}

UIContext::UIContext(
    HWND hWnd,
    std::shared_ptr<DeviceContext> pDeviceContext,
    DXGI_FORMAT rtvFormat,
    uint32_t numFramesInFlight,
    size_t srvPoolSize
) {
    pDeviceContext = pDeviceContext;

    m_pSrvRange = pDeviceContext->AllocateDescRange<PoolDescRange>(
        L"ImGui", DescRangeType::Srv, srvPoolSize
    );
    g_pImGuiSrvRange = m_pSrvRange.get();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hWnd);

    ImGui_ImplDX12_InitInfo init{};
    init.Device = pDeviceContext->GetDevice()->GetD3D12Device().Get();
    init.CommandQueue = pDeviceContext->GetCommandQueue()->GetD3D12CommandQueue().Get();
    init.NumFramesInFlight = static_cast<int>(numFramesInFlight);
    init.RTVFormat = rtvFormat;
    init.DSVFormat = DXGI_FORMAT_UNKNOWN;
    init.SrvDescriptorHeap = pDeviceContext
        ->GetDescriptorHeap(DescRangeType::Srv)
        ->GetD3D12DescriptorHeap().Get();
    init.SrvDescriptorAllocFn = &ImGuiSrvAlloc;
    init.SrvDescriptorFreeFn = &ImGuiSrvFree;
    ImGui_ImplDX12_Init(&init);
}

UIContext::~UIContext() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    m_panels.clear();
    g_pImGuiSrvRange = nullptr;
    m_pSrvRange.reset();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT UIContext::WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!ImGui::GetCurrentContext())
        return 0;
    return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}

bool UIContext::WantCaptureMouse() {
    return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse;
}
bool UIContext::WantCaptureKeyboard() {
    return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard;
}

void UIContext::RegisterPanel(PanelFn fn) {
    m_panels.emplace_back(std::move(fn));
}

void UIContext::Render(
    std::shared_ptr<CommandList> pCommandList,
    std::shared_ptr<DeviceContext> pDeviceContext
) {
    if (!m_isVisible)
        return;

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    for (auto& fn : m_panels) {
        fn();
    }

    ImGui::Render();

    pCommandList->PixBeginEvent(L"ImGui");
    auto pD3D12CommandList{ pCommandList->GetD3D12CommandList() };

    const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pSrvHeap{
        pDeviceContext->GetDescriptorHeap(DescRangeType::Srv)->GetD3D12DescriptorHeap()
    };
    pD3D12CommandList->SetDescriptorHeaps(1, pSrvHeap.GetAddressOf());

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pD3D12CommandList.Get());

    pCommandList->PixEndEvent();
}
