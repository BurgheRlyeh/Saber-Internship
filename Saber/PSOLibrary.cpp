#include "PSOLibrary.h"

#include "Device.h"

PSOLibrary::PSOLibrary(
	std::shared_ptr<Device> pDevice,
	const std::wstring& filename
) {
	m_file.Init(filename);

	HRESULT hr{ pDevice->GetD3D12Device()->CreatePipelineLibrary(
		m_file.GetData(),
		m_file.GetSize(),
		IID_PPV_ARGS(&m_pPipelineLibrary)
	)};

	if (hr == E_INVALIDARG
		|| hr == D3D12_ERROR_ADAPTER_NOT_FOUND
		|| hr == D3D12_ERROR_DRIVER_VERSION_MISMATCH
	) {
		m_file.Destroy(true);
		m_file.Init(filename);
		ThrowIfFailed(pDevice->GetD3D12Device()->CreatePipelineLibrary(
			m_file.GetData(),
			m_file.GetSize(),
			IID_PPV_ARGS(&m_pPipelineLibrary)
		));
	}
}

PSOLibrary::~PSOLibrary() {
	Destroy(false);
}

void PSOLibrary::Destroy(bool ClearPsoCache) {
	if (!ClearPsoCache) {
		FlushCacheToFile();
	}

	m_file.Destroy(ClearPsoCache);
}

void PSOLibrary::FlushCacheToFile() {
	if (!m_isRenewed.load())
		return;
	m_isRenewed.store(false);

	std::scoped_lock lock(m_pipelineLibraryMutex);

	auto librarySize = m_pPipelineLibrary->GetSerializedSize();
	const size_t neededSize = sizeof(UINT) + librarySize;
	auto currentFileSize = m_file.GetSize();
	if (neededSize > currentFileSize)
	{
		void* pTempData = new BYTE[librarySize];
		if (pTempData)
		{
			ThrowIfFailed(m_pPipelineLibrary->Serialize(pTempData, librarySize));
			m_file.GrowMapping(static_cast<UINT>(librarySize));
			memcpy(m_file.GetData(), pTempData, librarySize);
			m_file.SetSize(static_cast<UINT>(librarySize));

			delete[] pTempData;
		}
	}
	else {
		m_pPipelineLibrary->Serialize(m_file.GetData(), librarySize);
	}
	m_file.Flush();

}

Microsoft::WRL::ComPtr<ID3D12PipelineState> PSOLibrary::Find(
	const std::wstring& filename,
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
) {
	std::scoped_lock lock(m_pipelineLibraryMutex);

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pPSO{};
	HRESULT hr{ m_pPipelineLibrary->LoadGraphicsPipeline(
		filename.c_str(),
		pPSODesc,
		IID_PPV_ARGS(&pPSO)
	) };
	return SUCCEEDED(hr) ? pPSO : nullptr;
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> PSOLibrary::Find(
	const std::wstring& filename,
	const D3D12_COMPUTE_PIPELINE_STATE_DESC* pPSODesc
) {
	std::scoped_lock lock(m_pipelineLibraryMutex);

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pPSO{};
	HRESULT hr{ m_pPipelineLibrary->LoadComputePipeline(
		filename.c_str(),
		pPSODesc,
		IID_PPV_ARGS(&pPSO)
	) };
	return SUCCEEDED(hr) ? pPSO : nullptr;
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> PSOLibrary::Assign(
	std::shared_ptr<Device> pDevice,
	const std::wstring& filename,
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
) {
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pPSO{ Find(filename, pPSODesc) };
	if (pPSO) {
		return pPSO;
	}

	std::scoped_lock lock(m_pipelineLibraryMutex);

	ThrowIfFailed(pDevice->GetD3D12Device()->CreateGraphicsPipelineState(pPSODesc, IID_PPV_ARGS(&pPSO)));
	ThrowIfFailed(m_pPipelineLibrary->StorePipeline(filename.c_str(), pPSO.Get()));

	m_isRenewed.store(true);
	return pPSO;
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> PSOLibrary::Assign(
	std::shared_ptr<Device> pDevice,
	const std::wstring& filename,
	const D3D12_COMPUTE_PIPELINE_STATE_DESC* pPSODesc
) {
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pPSO{ Find(filename, pPSODesc) };
	if (pPSO) {
		return pPSO;
	}

	std::scoped_lock lock(m_pipelineLibraryMutex);

	ThrowIfFailed(pDevice->GetD3D12Device()->CreateComputePipelineState(pPSODesc, IID_PPV_ARGS(&pPSO)));
	ThrowIfFailed(m_pPipelineLibrary->StorePipeline(filename.c_str(), pPSO.Get()));

	m_isRenewed.store(true);
	return pPSO;
}
