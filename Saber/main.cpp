#include "Headers.h"

#include <shellapi.h> // For CommandLineToArgvW
#include <Shlwapi.h>
#include "windowsx.h"

#include "OutputContext.h"
#include "JobSystem.h"
#include "Renderer.h"

// Use WARP adapter
bool g_useWarp{};

// Window client area size
uint32_t g_clientWidth{ 1280 };
uint32_t g_clientHeight{ 720 };

std::unique_ptr<OutputContext> g_pOutputContext{};
std::shared_ptr<JobSystem<>> g_pJobSystem{};
std::unique_ptr<Renderer> g_pRenderer{};
bool g_isInitialized{};

bool g_pressedKeys[255]{};
bool g_isRButtonDown{};
int g_mouseX{}, g_mouseY{};
float g_speed{ 1.f };

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
        case '0':
        case '1':
        case '2':
        case '3':
            g_pRenderer->SetSceneId(wParam - '0');
            break;
        case 'C':
            g_pRenderer->SwitchToNextCamera();
            break;
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

        case VK_SHIFT: {
            if (!g_pressedKeys[wParam]) {
                g_speed *= 2.f;
                g_pressedKeys[wParam] = true;
            }
            break;
        }

        case 'W':
        case 'w':
            if (!g_pressedKeys[wParam]) {
                g_pRenderer->MoveCamera(-g_speed, 0.f);
                g_pressedKeys[wParam] = true;
            }
            break;
        case 'S':
        case 's':
            if (!g_pressedKeys[wParam]) {
                g_pRenderer->MoveCamera(g_speed, 0.f);
                g_pressedKeys[wParam] = true;
            }
            break;

        case 'A':
        case 'a':
            if (!g_pressedKeys[wParam]) {
                g_pRenderer->MoveCamera(0.f, -g_speed);
                g_pressedKeys[wParam] = true;
            }
            break;
        case 'D':
        case 'd':
            if (!g_pressedKeys[wParam]) {
                g_pRenderer->MoveCamera(0.f, g_speed);
                g_pressedKeys[wParam] = true;
            }
            break;
        }
        break;
    }
    case WM_KEYUP: {
        switch (wParam) {
        case VK_SHIFT: {
            if (g_pressedKeys[wParam]) {
                g_speed *= 0.5f;
            }
            g_pressedKeys[wParam] = false;
            break;
        }

        case 'W':
        case 'w':
            if (g_pressedKeys[wParam]) {
                g_pRenderer->MoveCamera(g_speed, 0.f);
            }
            g_pressedKeys[wParam] = false;
            break;
        case 'S':
        case 's':
            if (g_pressedKeys[wParam]) {
                g_pRenderer->MoveCamera(-g_speed, 0.f);
            }
            g_pressedKeys[wParam] = false;
            break;

        case 'A':
        case 'a':
            if (g_pressedKeys[wParam]) {
                g_pRenderer->MoveCamera(0.f, g_speed);
            }
            g_pressedKeys[wParam] = false;
            break;
        case 'D':
        case 'd':
            if (g_pressedKeys[wParam]) {
                g_pRenderer->MoveCamera(0.f, -g_speed);
            }
            g_pressedKeys[wParam] = false;
            break;
        }
        break;
    }
    case WM_RBUTTONDOWN: {
        g_isRButtonDown = true;
        g_mouseX = GET_X_LPARAM(lParam);
        g_mouseY = GET_Y_LPARAM(lParam);
        break;
    }
    case WM_RBUTTONUP: {
        g_isRButtonDown = false;
        break;
    }
    case WM_MOUSEMOVE: {
        if (g_isRButtonDown) {
            int oldX{ g_mouseX }, oldY{ g_mouseY };
            g_mouseX = GET_X_LPARAM(lParam);
            g_mouseY = GET_Y_LPARAM(lParam);

            g_pRenderer->RotateCamera(
                static_cast<float>(oldX - g_mouseX), 
                static_cast<float>(g_mouseY - oldY)
            );
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
    // Set the working directory to the path of the executable.
    WCHAR path[MAX_PATH];
    HMODULE hModule = GetModuleHandleW(NULL);
    if (GetModuleFileNameW(hModule, path, MAX_PATH) > 0) {
        PathRemoveFileSpecW(path);
        SetCurrentDirectoryW(path);
    }

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

    g_pJobSystem = std::make_shared<JobSystem<>>();

    g_pRenderer = std::make_unique<Renderer>(g_pJobSystem, 3, g_useWarp, g_clientWidth, g_clientHeight, true);
    g_pRenderer->Initialize(g_pOutputContext->getHWND());

    g_isInitialized = true;

    g_pOutputContext->ShowWindow();

    bool isStarted{ g_pRenderer->StartRenderThread() };
    assert(isStarted);

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        //g_pRenderer->PerformResize();
        //g_pRenderer->Update();
        //g_pRenderer->Render();
    }

    g_pRenderer->StopRenderThread();

    return 0;
}
