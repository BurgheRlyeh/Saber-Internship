#include "OutputContext.h"

OutputContext::OutputContext(HINSTANCE hInst, const wchar_t* windowClassName, const wchar_t* windowTitle) {
    m_hInst = hInst;
    m_windowClassName = windowClassName;
    m_windowTitle = windowTitle;
}

void OutputContext::RegisterWindowClass(LRESULT(*WndProc)(HWND, UINT, WPARAM, LPARAM)) {
    // Register a window class for creating our render window with.
    WNDCLASSEXW windowClass{
        .cbSize{ sizeof(WNDCLASSEX) },
        .style{ CS_HREDRAW | CS_VREDRAW },
        .lpfnWndProc{ WndProc },
        .cbClsExtra{},
        .cbWndExtra{},
        .hInstance{ m_hInst },
        .hIcon{ ::LoadIcon(m_hInst, NULL) },
        .hCursor{ ::LoadCursor(NULL, IDC_ARROW) },
        .hbrBackground{ (HBRUSH)(COLOR_WINDOW + 1) },
        .lpszMenuName{},
        .lpszClassName{ m_windowClassName },
        .hIconSm{ ::LoadIcon(m_hInst, NULL) }
    };

    assert(::RegisterClassExW(&windowClass) > 0);
}

void OutputContext::CreateAppWindow(uint32_t width, uint32_t height) {
    int screenWidth{ ::GetSystemMetrics(SM_CXSCREEN) };     // The width of the screen of the primary display monitor, in pixels
    int screenHeight{ ::GetSystemMetrics(SM_CYSCREEN) };    // The height of the screen of the primary display monitor, in pixels

    RECT windowRect{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    ::AdjustWindowRect(
        &windowRect,
        WS_OVERLAPPEDWINDOW,    // The window style of the window whose required size is to be calculated
        FALSE                   // Indicates whether the window has a menu
    );

    int windowWidth{ windowRect.right - windowRect.left };
    int windowHeight{ windowRect.bottom - windowRect.top };

    // Center the window within the screen. Clamp to 0, 0 for the top-left corner.
    int windowX{ std::max<int>(0, (screenWidth - windowWidth) / 2) };
    int windowY{ std::max<int>(0, (screenHeight - windowHeight) / 2) };

    m_hWnd = ::CreateWindowExW(
        NULL,                   // The extended window style of the window being created
        m_windowClassName,      // Class name got with RegisterClass(Ex)
        m_windowTitle,          // The window name
        WS_OVERLAPPEDWINDOW,    // The style of the window being created
        windowX,                // The initial horizontal position of the window
        windowY,                // The initial vertical position of the window
        windowWidth,            // The width, in device units, of the window
        windowHeight,           // The height, in device units, of the window
        NULL,                   // A handle to the parent or owner window of the window being created
        NULL,                   // A handle to a menu
        m_hInst,                // A handle to the instance of the module to be associated with the window
        nullptr
    );

    assert(m_hWnd && "Failed to create window");
}

void OutputContext::initWindowRect() {
    // Initialize the global window rect variable.
    ::GetWindowRect(m_hWnd, &m_windowRect);
}

void OutputContext::showWindow() {
    ::ShowWindow(m_hWnd, SW_SHOW);
}

void OutputContext::switchFullscreen() {
    m_fullscreen = !m_fullscreen;

    // Switching to fullscreen.
    if (m_fullscreen) {
        // Store the current window dimensions so they can be restored 
        // when switching out of fullscreen state.
        ::GetWindowRect(m_hWnd, &m_windowRect);

        // Set the window style to a borderless window so the client area fills
        // the entire screen.
        UINT windowStyle{ WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX) };

        ::SetWindowLongW(m_hWnd, GWL_STYLE, windowStyle);

        // Query the name of the nearest display device for the window.
        // This is required to set the fullscreen dimensions of the window
        // when using a multi-monitor setup.
        HMONITOR hMonitor{ ::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST) };
        MONITORINFOEX monitorInfo{};
        monitorInfo.cbSize = sizeof(MONITORINFOEX);

        ::GetMonitorInfo(hMonitor, &monitorInfo);

        ::SetWindowPos(
            m_hWnd,
            HWND_TOP,
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE
        );

        ::ShowWindow(m_hWnd, SW_MAXIMIZE);
    }
    else {
        // Restore all the window decorators.
        ::SetWindowLong(m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

        ::SetWindowPos(m_hWnd, HWND_NOTOPMOST,
            m_windowRect.left,
            m_windowRect.top,
            m_windowRect.right - m_windowRect.left,
            m_windowRect.bottom - m_windowRect.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ::ShowWindow(m_hWnd, SW_NORMAL);
    }
}
