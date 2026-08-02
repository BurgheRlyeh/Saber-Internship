#pragma once

#include "Headers.h"

#include "D3D12MemAlloc.h"

class Device : public std::enable_shared_from_this<Device> {
protected:
	Microsoft::WRL::ComPtr<D3D12Device> m_pDevice{};
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> m_pAllocator{};

public:
	Device(
		const std::wstring& name,
		Microsoft::WRL::ComPtr<DXGIAdapter> pAdapter
	);
	~Device();

	Microsoft::WRL::ComPtr<D3D12Device> GetD3D12Device() const;
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> GetD3D12Allocator() const;

private:
	static Microsoft::WRL::ComPtr<D3D12Device> CreateDevice(
		Microsoft::WRL::ComPtr<DXGIAdapter> pAdapter,
		const D3D_FEATURE_LEVEL& featureLevel = D3D_FEATURE_LEVEL_11_0
	);
	static Microsoft::WRL::ComPtr<D3D12MA::Allocator> CreateAllocator(
		Microsoft::WRL::ComPtr<D3D12Device> pDevice,
		Microsoft::WRL::ComPtr<DXGIAdapter> pAdapter,
		const D3D12MA::ALLOCATOR_FLAGS& allocatorFlags = D3D12MA::ALLOCATOR_FLAG_NONE
	);
};
