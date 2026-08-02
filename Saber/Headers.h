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

// Used DXGI and D3D12 types versions

// DXGI
using DXGIFactory = IDXGIFactory6;
using DXGIAdapter = IDXGIAdapter4;
using DXGISwapChain = IDXGISwapChain4;

// Device
using D3D12Device = ID3D12Device2;

// Command submission
using D3D12CommandQueue = ID3D12CommandQueue;
using D3D12CommandAllocator = ID3D12CommandAllocator;
using D3D12CommandList = ID3D12CommandList;
using D3D12GraphicsCommandList = ID3D12GraphicsCommandList2;
using D3D12CommandSignature = ID3D12CommandSignature;

// Pipeline
using D3D12RootSignature = ID3D12RootSignature;
using D3D12PipelineState = ID3D12PipelineState;
using D3D12PipelineLibrary = ID3D12PipelineLibrary1;
using D3DBlob = ID3DBlob;

// Resources
using D3D12Resource = ID3D12Resource;
using D3D12DescriptorHeap = ID3D12DescriptorHeap;

// Synchronization
using D3D12Fence = ID3D12Fence;

// Debug & diagnostics
using D3D12Debug = ID3D12Debug1;
using D3D12InfoQueue = ID3D12InfoQueue;
using D3D12DeviceRemovedExtendedData = ID3D12DeviceRemovedExtendedData;
using D3D12DeviceRemovedExtendedDataSettings = ID3D12DeviceRemovedExtendedDataSettings;

// Helper functions

inline void ThrowIfFailed(HRESULT hr, Microsoft::WRL::ComPtr<ID3D12Device> pDevice = nullptr) {
    if (SUCCEEDED(hr)) {
        return;
    }

    // todo
    if (Microsoft::WRL::ComPtr<D3D12DeviceRemovedExtendedData> pDred;
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
