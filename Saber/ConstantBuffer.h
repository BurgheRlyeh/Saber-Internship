#pragma once

#include "Headers.h"

#include "GPUResource.h"

class Device;

class ConstantBuffer : public GPUResource {
public:
	ConstantBuffer(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		const UINT64& size,
		void* initData = nullptr,
		const HeapData& heapData = HeapData{ D3D12_HEAP_TYPE_UPLOAD },
		const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
	);

	void CreateConstantBufferView(
		std::shared_ptr<Device> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle
	);

	void Update(void* newData, size_t offset = 0, size_t size = 0);
};
