/**
 * @file OutputContext.h
 * @brief Win32 window creation, registration, and fullscreen toggle.
 *
 * @ref OutputContext wraps the Win32 window class registration, window
 * creation, and Alt+Enter / F11 fullscreen switching into a single
 * non-copyable object.
 */
#pragma once

#include "Headers.h"

/**
 * @brief Manages the Win32 application window lifecycle.
 *
 * The window starts in windowed mode.  Fullscreen state can be toggled at
 * runtime via @ref SwitchFullscreen.  The stored @ref m_windowRect is used
 * to restore the windowed position and size when leaving fullscreen.
 */
class OutputContext {
    HINSTANCE m_hInst; /**< @brief Application instance handle. */

    HWND m_hWnd{};      /**< @brief Win32 window handle. */

    RECT m_windowRect{}; /**< @brief Saved window rect for restoring from fullscreen. */

    bool m_fullscreen{}; /**< @brief Current fullscreen state. */

    const wchar_t* m_windowClassName{}; /**< @brief Registered window class name. */
    const wchar_t* m_windowTitle{};     /**< @brief Window title bar text. */

public:
    OutputContext(OutputContext&&) = delete;
    OutputContext(const OutputContext&) = delete;

    /**
     * @brief Stores the instance handle and default window class / title strings.
     * @param hInst           Application instance handle.
     * @param windowClassName Window class name (default @c "SaberInternshipWindowClass").
     * @param windowTitle     Window title (default @c "Saber Internship").
     */
    OutputContext(
        HINSTANCE hInst,
        const wchar_t* windowClassName = L"SaberInternshipWindowClass",
        const wchar_t* windowTitle = L"Saber Internship"
    );

    /**
     * @brief Registers the Win32 window class with the provided window procedure.
     * @param WndProc Pointer to the window procedure callback.
     */
    void RegisterWindowClass(
        LRESULT (*WndProc)(HWND, UINT, WPARAM, LPARAM)
    );

    /**
     * @brief Creates the application window at the given client area resolution.
     * @param width  Client area width in pixels (default 1280).
     * @param height Client area height in pixels (default 720).
     */
    void CreateAppWindow(uint32_t width = 1280, uint32_t height = 720);

    /** @brief Returns the Win32 window handle. */
    HWND getHWND() {
        return m_hWnd;
    }

    /** @brief Saves the current window rectangle for fullscreen restoration. */
    void InitWindowRect();

    /** @brief Makes the window visible (@c ShowWindow / @c UpdateWindow). */
    void ShowWindow();

    /**
     * @brief Toggles between windowed and borderless-fullscreen modes.
     *
     * Saves the window rect before entering fullscreen and restores it on exit.
     */
    void SwitchFullscreen();
};
