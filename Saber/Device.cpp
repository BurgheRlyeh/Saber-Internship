#include "Device.h"

Device::Device(
	const std::wstring& name,
	Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter
) {
	m_pDevice = CreateDevice(pAdapter);
	m_pDevice->SetName(name.c_str());

	m_pAllocator = CreateAllocator(
		m_pDevice,
		pAdapter,
		D3D12MA::ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED
	);
}

Device::~Device() {
	m_pAllocator.Reset();
	m_pDevice.Reset();
}

Microsoft::WRL::ComPtr<ID3D12Device2> Device::GetD3D12Device() const {
	return m_pDevice;
}

Microsoft::WRL::ComPtr<D3D12MA::Allocator> Device::GetD3D12Allocator() const {
	return m_pAllocator;
}

Microsoft::WRL::ComPtr<ID3D12Device2> Device::CreateDevice(
	Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter,
	const D3D_FEATURE_LEVEL& featureLevel
) {
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice;
	ThrowIfFailed(D3D12CreateDevice(
		pAdapter.Get(),
		featureLevel,
		IID_PPV_ARGS(&pDevice)
	));
	return pDevice;
}

Microsoft::WRL::ComPtr<D3D12MA::Allocator> Device::CreateAllocator(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter,
	const D3D12MA::ALLOCATOR_FLAGS& allocatorFlags
) {
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator{};

	D3D12MA::ALLOCATOR_DESC desc{
		.Flags{ allocatorFlags },
		.pDevice{ pDevice.Get() },
		.pAdapter{ pAdapter.Get() }
	};
	ThrowIfFailed(D3D12MA::CreateAllocator(&desc, pAllocator.GetAddressOf()));

	return pAllocator;
}
