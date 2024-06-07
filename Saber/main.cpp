#include "Headers.h"

#include <shellapi.h> // For CommandLineToArgvW

#include "OutputContext.h"
#include "Renderer.h"

// Use WARP adapter
bool g_useWarp{};

// Window client area size
uint32_t g_clientWidth{ 1280 };
uint32_t g_clientHeight{ 720 };

std::unique_ptr<OutputContext> g_pOutputContext{};
std::unique_ptr<Renderer> g_pRenderer{};
bool g_isInitialized{};

void ParseCommandLineArguments() {
    int argc{};
    wchar_t** argv{ ::CommandLineToArgvW(::GetCommandLineW(), &argc) };

    for (size_t i{}; i < argc; ++i) {
        // Width
        if (::wcscmp(argv[i], L"-w") == 0 || ::wcscmp(argv[i], L"--width") == 0)
            g_clientWidth = ::wcstol(argv[++i], nullptr, 10);

        // Height
        if (::wcscmp(argv[i], L"-h") == 0 || ::wcscmp(argv[i], L"--height") == 0)
            g_clientHeight = ::wcstol(argv[++i], nullptr, 10);

        // Is using warp renderer
        g_useWarp = ::wcscmp(argv[i], L"-warp") == 0 || ::wcscmp(argv[i], L"--warp") == 0;
    }

    // Free memory allocated by CommandLineToArgvW
    ::LocalFree(argv);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (!g_isInitialized) {
        return ::DefWindowProcW(hwnd, message, wParam, lParam);
    }

    switch (message) {
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN: {
        bool alt{ (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0 };

        switch (wParam) {
        case 'V':
            g_pRenderer->SwitchVSync();
            break;
        case VK_ESCAPE:
            ::PostQuitMessage(0);
            break;
        case VK_RETURN:
            if (alt) {
        case VK_F11:
            g_pOutputContext->SwitchFullscreen();
            }
            break;
        }
        break;
    }
    // The default window procedure will play a system notification sound 
    // when pressing the Alt+Enter keyboard combination if this message is 
    // not handled.
    case WM_SYSCHAR:
        break;
    case WM_SIZE: {
        RECT clientRect = {};
        ::GetClientRect(g_pOutputContext->getHWND(), &clientRect);

        int width = clientRect.right - clientRect.left;
        int height = clientRect.bottom - clientRect.top;

        g_pRenderer->Resize(width, height);
    }
    break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        break;
    default:
        return ::DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return 0;
}

// Main Win32 apps entry point
int CALLBACK wWinMain(
    HINSTANCE hInst,        // Handle to instance/module
    HINSTANCE hPrevInst,    // Has no meaning (used in 16-bit window, now always zero)
    PWSTR lpCmdLine,        // Cmd-line args as Unicode string
    int nCmdShow            // is minimized / maximized / shown normally
) {
    // Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
    // Using this awareness context allows the client area of the window 
    // to achieve 100% scaling while still allowing non-client window content to 
    // be rendered in a DPI sensitive fashion.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Window class name. Used for registering / creating the window.
    const wchar_t* windowClassName = L"SaberInternshipWindowClass";
    ParseCommandLineArguments();

    g_pOutputContext = std::make_unique<OutputContext>(
        hInst,
        L"SaberInternshipWindowClass",
        L"Saber Internship"
    );
    g_pOutputContext->RegisterWindowClass(WndProc);
    g_pOutputContext->CreateAppWindow(g_clientWidth, g_clientHeight);
    g_pOutputContext->InitWindowRect();

    g_pRenderer = std::make_unique<Renderer>(3, g_useWarp, g_clientWidth, g_clientHeight, true);
    g_pRenderer->Initialize(g_pOutputContext->getHWND());

    g_isInitialized = true;

    g_pOutputContext->ShowWindow();

    assert(g_pRenderer->StartRenderThread());

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
    }

    g_pRenderer->StopRenderThread();

    return 0;
}
