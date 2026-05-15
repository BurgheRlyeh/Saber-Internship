/**
 * @file Device.h
 * @brief Wraps the D3D12 logical device and its D3D12MA memory allocator.
 */
#pragma once

#include "Headers.h"

#include "D3D12MemAlloc.h"

/**
 * @brief Owns the @c ID3D12Device2 and the associated @c D3D12MA::Allocator.
 *
 * Created once at startup and shared throughout the engine via @c shared_ptr.
 * Inherits @c enable_shared_from_this so subsystems can safely obtain a
 * shared pointer to the device from member functions.
 */
class Device : public std::enable_shared_from_this<Device> {
protected:
    Microsoft::WRL::ComPtr<ID3D12Device2> m_pDevice{};        /**< @brief Underlying D3D12 device. */
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> m_pAllocator{}; /**< @brief D3D12 Memory Allocator instance. */

public:
    /**
     * @brief Creates the D3D12 device and memory allocator for the given adapter.
     * @param name     Debug name applied to the device.
     * @param pAdapter DXGI adapter used to create the device.
     */
    Device(
        const std::wstring& name,
        Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter
    );

    ~Device();

    /** @brief Returns the underlying @c ID3D12Device2 COM pointer. */
    Microsoft::WRL::ComPtr<ID3D12Device2> GetD3D12Device() const;

    /** @brief Returns the D3D12 Memory Allocator used for resource suballocation. */
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> GetD3D12Allocator() const;

private:
    /**
     * @brief Creates an @c ID3D12Device2 for the given adapter and feature level.
     * @param pAdapter     Adapter to create the device on.
     * @param featureLevel Minimum required feature level (default D3D_FEATURE_LEVEL_11_0).
     * @return Newly created device.
     */
    static Microsoft::WRL::ComPtr<ID3D12Device2> CreateDevice(
        Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter,
        const D3D_FEATURE_LEVEL& featureLevel = D3D_FEATURE_LEVEL_11_0
    );

    /**
     * @brief Creates a @c D3D12MA::Allocator for the given device and adapter.
     * @param pDevice        D3D12 device.
     * @param pAdapter       DXGI adapter.
     * @param allocatorFlags Allocator creation flags (default none).
     * @return Newly created allocator.
     */
    static Microsoft::WRL::ComPtr<D3D12MA::Allocator> CreateAllocator(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter,
        const D3D12MA::ALLOCATOR_FLAGS& allocatorFlags = D3D12MA::ALLOCATOR_FLAG_NONE
    );
};
