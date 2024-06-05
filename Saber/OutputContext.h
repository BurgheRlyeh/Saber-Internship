#pragma once

#include "Headers.h"

class OutputContext {
    HINSTANCE m_hInst;

    // Window handle
    HWND m_hWnd{};

    // Window rectangle (used to toggle fullscreen state)
    RECT m_windowRect{};

    // By default, use windowed mode.
    // Can be toggled with the Alt+Enter or F11
    bool m_fullscreen{};

    const wchar_t* m_windowClassName{};
    const wchar_t* m_windowTitle{};

public:
    OutputContext(OutputContext&&) = delete;
    OutputContext(const OutputContext&) = delete;
    OutputContext(
        HINSTANCE hInst,
        const wchar_t* windowClassName = L"SaberInternshipWindowClass",
        const wchar_t* windowTitle = L"Saber Internship"
    );

    void RegisterWindowClass(
        LRESULT (*WndProc)(HWND, UINT, WPARAM, LPARAM)
    );

    void CreateAppWindow(uint32_t width = 1280, uint32_t height = 720);

    HWND getHWND() {
        return m_hWnd;
    }

    void initWindowRect();

    void showWindow();

    void switchFullscreen();
};