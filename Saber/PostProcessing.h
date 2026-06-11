#pragma once

#include "Headers.h"

#include "RenderObject.h"

class DeviceContext;

class CopyPostProcessing : public FullscreenDrawPass {
    using FullscreenDrawPass::FullscreenDrawPass;

public:
    CopyPostProcessing(
        std::shared_ptr<DeviceContext> pDeviceContext
    );

private:
    static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob();
    static D3D12_GRAPHICS_PIPELINE_STATE_DESC CreatePipelineStateDesc();
};

class TestPostProcessing : public FullscreenDrawPass {
	using FullscreenDrawPass::FullscreenDrawPass;

public:
	TestPostProcessing(
		std::shared_ptr<DeviceContext> pDeviceContext
	);

private:
	static Microsoft::WRL::ComPtr<ID3DBlob> CreateRootSignatureBlob();
	static D3D12_GRAPHICS_PIPELINE_STATE_DESC CreatePipelineStateDesc();
};
