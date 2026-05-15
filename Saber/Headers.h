/**
 * @file Headers.h
 * @brief Central include file for DirectX 12, WRL, and common STL headers used throughout the project.
 *
 * Also defines the @c ThrowIfFailed helper that checks HRESULT values and
 * optionally queries DRED data on device removal.
 */
#pragma once

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN

// Windows Header Files
#define NOMINMAX
#include <Windows.h> // For HRESULT

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
#include <comdef.h>

// DirectX
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib")

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// D3D12 extension library.
#include <d3dx12.h>

// STL Headers
#include <cassert>
#include <exception>

/**
 * @brief Throws an exception if the given HRESULT indicates failure.
 *
 * On debug builds, if a device-removal error is detected, the function also
 * queries DRED auto-breadcrumbs and page-fault allocation data for diagnostics.
 *
 * @param hr      The HRESULT value to check.
 * @param pDevice Optional D3D12 device used for DRED queries on device removal.
 * @throws std::exception if @p hr is a failure code.
 */
inline void ThrowIfFailed(HRESULT hr, Microsoft::WRL::ComPtr<ID3D12Device> pDevice = nullptr) {
    if (SUCCEEDED(hr)) {
        return;
    }

    // todo
    if (Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData> pDred;
        pDevice && SUCCEEDED(pDevice.As(&pDred))
        ) {
        if (D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT DredAutoBreadcrumbsOutput;
            SUCCEEDED(pDred->GetAutoBreadcrumbsOutput(&DredAutoBreadcrumbsOutput))
            ) {
            OutputDebugString(L"AutoBreadcrumbs are gotten\n");
        }

        if (D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;
            SUCCEEDED(pDred->GetPageFaultAllocationOutput(&DredPageFaultOutput))
            ) {
            OutputDebugString(L"PageFaultAllocation is gotten\n");
        }
    }

    _com_error err(hr);
    OutputDebugString(L"HRESULT: ");
    OutputDebugString(err.ErrorMessage());
    OutputDebugString(L"\n");

    throw std::exception();
}
