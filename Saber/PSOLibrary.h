#pragma once

#include "Headers.h"

#include <mutex>

#include "MemoryMappedFile.h"

class Device;

class PSOLibrary {
private:
	MemoryMappedFile m_file;

	Microsoft::WRL::ComPtr<D3D12PipelineLibrary> m_pPipelineLibrary{};
	std::mutex m_pipelineLibraryMutex;

	std::atomic<bool> m_isRenewed{};

public:
	PSOLibrary(
		std::shared_ptr<Device> pDevice,
		const std::wstring& filename
	);

	~PSOLibrary();

	void Destroy(bool ClearPsoCache);
	void FlushCacheToFile();

	Microsoft::WRL::ComPtr<D3D12PipelineState> Find(
		const std::wstring& filename,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
	);

	Microsoft::WRL::ComPtr<D3D12PipelineState> Find(
		const std::wstring& filename,
		const D3D12_COMPUTE_PIPELINE_STATE_DESC* pPSODesc
	);

	Microsoft::WRL::ComPtr<D3D12PipelineState> Assign(
		std::shared_ptr<Device> pDevice,
		const std::wstring& filename,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
	);

	Microsoft::WRL::ComPtr<D3D12PipelineState> Assign(
		std::shared_ptr<Device> pDevice,
		const std::wstring& filename,
		const D3D12_COMPUTE_PIPELINE_STATE_DESC* pPSODesc
	);
};