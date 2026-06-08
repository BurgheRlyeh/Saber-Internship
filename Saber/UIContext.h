#pragma once

#include "Headers.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class CommandList;
class DescRange;
class DeviceContext;

class UIContext {
public:
    using PanelFn = std::function<void()>;

    static constexpr size_t DefaultSrvPoolSize{ 64 };

private:
    std::shared_ptr<DescRange> m_pSrvRange{};

    std::vector<PanelFn> m_panels{};

    bool m_isVisible{ true };

public:
    UIContext(
        HWND hWnd,
        std::shared_ptr<DeviceContext> pDeviceContext,
        DXGI_FORMAT rtvFormat,
        uint32_t numFramesInFlight,
        size_t srvPoolSize = DefaultSrvPoolSize
    );
    ~UIContext();

    UIContext(const UIContext&) = delete;
    UIContext& operator=(const UIContext&) = delete;

    UIContext(UIContext&&) = delete;
    UIContext& operator=(UIContext&&) = delete;

    static LRESULT WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    static bool WantCaptureMouse();
    static bool WantCaptureKeyboard();

    void RegisterPanel(PanelFn fn);

    void SetVisible(bool v) { m_isVisible = v; }
    void ToggleVisible()    { m_isVisible = !m_isVisible; }
    bool IsVisible() const  { return m_isVisible; }

    void Render(
        std::shared_ptr<CommandList> pCommandList,
        std::shared_ptr<DeviceContext> pDeviceContext
    );
};
