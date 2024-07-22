#include "PSOLibrary.h"

PSOLibrary::PSOLibrary(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, std::wstring filename) {
	m_file.Init(filename);

	HRESULT hr{ pDevice->CreatePipelineLibrary(
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
		ThrowIfFailed(pDevice->CreatePipelineLibrary(
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
	if (!m_renewed)
		return;
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
	m_renewed = false;

}

Microsoft::WRL::ComPtr<ID3D12PipelineState> PSOLibrary::Find(
	LPCWSTR filename,
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
) {
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pPSO{};
	HRESULT hr{ m_pPipelineLibrary->LoadGraphicsPipeline(
		filename,
		pPSODesc,
		IID_PPV_ARGS(&pPSO)
	) };
	return SUCCEEDED(hr) ? pPSO : nullptr;
}

bool PSOLibrary::Add(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	LPCWSTR filename,
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
) {
	if (Find(filename, pPSODesc)) {
		return false;
	}

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pPSO{};
	pDevice->CreateGraphicsPipelineState(pPSODesc, IID_PPV_ARGS(&pPSO));
	if (SUCCEEDED(m_pPipelineLibrary->StorePipeline(filename, pPSO.Get()))) {
		m_renewed = true;
		return true;
	}
	return false;
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> PSOLibrary::Assign(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	LPCWSTR filename,
	const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pPSODesc
) {
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pPSO{ Find(filename, pPSODesc) };
	if (pPSO) {
		return pPSO;
	}

	ThrowIfFailed(pDevice->CreateGraphicsPipelineState(pPSODesc, IID_PPV_ARGS(&pPSO)));
	ThrowIfFailed(m_pPipelineLibrary->StorePipeline(filename, pPSO.Get()));
	m_renewed = true;

	return pPSO;
}
