#pragma once

#include "Headers.h"

#include "MemoryMappedFile.h"

class Device;

class PSOLibrary {
private:
	MemoryMappedFile m_file;

	Microsoft::WRL::ComPtr<ID3D12PipelineLibrary1> m_pPipelineLibrary{};
	bool m_renewed{};

public:
	PSOLibrary(
		std::shared_ptr<Device> pDevice,
		const std::wstring& filename
	);

	~PSOLibrary();

	void Destroy(bool ClearPsoCache);
	void FlushCacheToFile();

	Microsoft::WRL::ComPtr<ID3D12PipelineState> Find(
		const std::wstring& filename,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
	);

	Microsoft::WRL::ComPtr<ID3D12PipelineState> Find(
		const std::wstring& filename,
		const D3D12_COMPUTE_PIPELINE_STATE_DESC* pPSODesc
	);

	bool Add(
		std::shared_ptr<Device> pDevice,
		const std::wstring& filename,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
	);

	bool Add(
		std::shared_ptr<Device> pDevice,
		const std::wstring& filename,
		const D3D12_COMPUTE_PIPELINE_STATE_DESC* pPSODesc
	);

	Microsoft::WRL::ComPtr<ID3D12PipelineState> Assign(
		std::shared_ptr<Device> pDevice,
		const std::wstring& filename,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
	);

	Microsoft::WRL::ComPtr<ID3D12PipelineState> Assign(
		std::shared_ptr<Device> pDevice,
		const std::wstring& filename,
		const D3D12_COMPUTE_PIPELINE_STATE_DESC* pPSODesc
	);
};