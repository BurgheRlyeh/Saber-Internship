#include "PostProcessing.h"

#include "CommandList.h"
#include "DeviceContext.h"
#include "Resources.h"

CopyPostProcessing::CopyPostProcessing(
    std::shared_ptr<DeviceContext> pDeviceContext
) : FullscreenDrawPass() {
    InitMaterial(
        pDeviceContext,
        RootSignatureData{
            CreateRootSignatureBlob(),
            L"CopyPostProcessingRootSignature"
        },
        ShaderData{
            L"CopyPostProcessingVS.cso",
            L"CopyPostProcessingPS.cso"
        },
        PipelineStateData{
            CreatePipelineStateDesc()
        }
    );
}

Microsoft::WRL::ComPtr<ID3DBlob> CopyPostProcessing::CreateRootSignatureBlob() {
    // Allow input layout and deny unnecessary access to certain pipeline stages.
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags{
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
    };

    CD3DX12_DESCRIPTOR_RANGE1 rangeDescs[1]{};
    rangeDescs[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER1 rootParameters[1]{};
    rootParameters[0].InitAsDescriptorTable(_countof(rangeDescs), rangeDescs, D3D12_SHADER_VISIBILITY_PIXEL);
    
    D3D12_STATIC_SAMPLER_DESC sampler{
        .Filter{ D3D12_FILTER_MIN_MAG_MIP_POINT },
        .AddressU{ D3D12_TEXTURE_ADDRESS_MODE_BORDER },
        .AddressV{ D3D12_TEXTURE_ADDRESS_MODE_BORDER },
        .AddressW{ D3D12_TEXTURE_ADDRESS_MODE_BORDER },
        .MipLODBias{},
        .MaxAnisotropy{},
        .ComparisonFunc{ D3D12_COMPARISON_FUNC_NEVER },
        .BorderColor{ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK },
        .MinLOD{},
        .MaxLOD{ D3D12_FLOAT32_MAX },
        .ShaderRegister{},
        .RegisterSpace{},
        .ShaderVisibility{ D3D12_SHADER_VISIBILITY_PIXEL }
    };
    
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
    rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

    // Serialize the root signature.
    Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob, errorBlob;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
        &rootSignatureDescription,
        D3D_ROOT_SIGNATURE_VERSION_1_1,
        &rootSignatureBlob,
        &errorBlob
    ));

    return rootSignatureBlob;
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC CopyPostProcessing::CreatePipelineStateDesc() {
    CD3DX12_RASTERIZER_DESC rasterizerDesc{ D3D12_DEFAULT };
    rasterizerDesc.FrontCounterClockwise = true;

    D3D12_RT_FORMAT_ARRAY rtvFormats{};
    rtvFormats.NumRenderTargets = 1;
    rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    CD3DX12_DEPTH_STENCIL_DESC1 depthStencilDesc{};
    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.StencilEnable = FALSE;

    CD3DX12_PIPELINE_STATE_STREAM pipelineStateStream{};
    pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineStateStream.RasterizerState = rasterizerDesc;
    pipelineStateStream.DepthStencilState = depthStencilDesc;
    pipelineStateStream.RTVFormats = rtvFormats;

    return pipelineStateStream.GraphicsDescV0();
}

void FullscreenDrawPass::DrawCall(
    std::shared_ptr<CommandList> pCommandList
) const {
    pCommandList->GetD3D12CommandList()->DrawInstanced(3, 1, 0, 0);
}

TestPostProcessing::TestPostProcessing(
	std::shared_ptr<DeviceContext> pDeviceContext
) : FullscreenDrawPass() {
	InitMaterial(
		pDeviceContext,
		RootSignatureData{
			CreateRootSignatureBlob(),
			L"TestPostProcessingRootSignature"
		},
		ShaderData{
			L"CopyPostProcessingVS.cso",
			L"TestPostProcessingPS.cso"
		},
		PipelineStateData{
			CreatePipelineStateDesc()
		}
	);
}

Microsoft::WRL::ComPtr<ID3DBlob> TestPostProcessing::CreateRootSignatureBlob() {
	// Allow input layout and deny unnecessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags{
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
	};

	CD3DX12_DESCRIPTOR_RANGE1 rangeDescs[1]{};
	rangeDescs[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER1 rootParameters[2]{};
	rootParameters[0].InitAsDescriptorTable(_countof(rangeDescs), rangeDescs, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[1].InitAsConstants(4, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

	D3D12_STATIC_SAMPLER_DESC sampler{
		.Filter{ D3D12_FILTER_MIN_MAG_MIP_POINT },
		.AddressU{ D3D12_TEXTURE_ADDRESS_MODE_BORDER },
		.AddressV{ D3D12_TEXTURE_ADDRESS_MODE_BORDER },
		.AddressW{ D3D12_TEXTURE_ADDRESS_MODE_BORDER },
		.MipLODBias{},
		.MaxAnisotropy{},
		.ComparisonFunc{ D3D12_COMPARISON_FUNC_NEVER },
		.BorderColor{ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK },
		.MinLOD{},
		.MaxLOD{ D3D12_FLOAT32_MAX },
		.ShaderRegister{},
		.RegisterSpace{},
		.ShaderVisibility{ D3D12_SHADER_VISIBILITY_PIXEL }
	};

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

	// Serialize the root signature.
	Microsoft::WRL::ComPtr<ID3DBlob> rootSignatureBlob, errorBlob;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(
		&rootSignatureDescription,
		D3D_ROOT_SIGNATURE_VERSION_1_1,
		&rootSignatureBlob,
		&errorBlob
	));

	return rootSignatureBlob;
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC TestPostProcessing::CreatePipelineStateDesc() {
	CD3DX12_RASTERIZER_DESC rasterizerDesc{ D3D12_DEFAULT };
	rasterizerDesc.FrontCounterClockwise = true;

	D3D12_RT_FORMAT_ARRAY rtvFormats{};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	CD3DX12_DEPTH_STENCIL_DESC1 depthStencilDesc{};
	depthStencilDesc.DepthEnable = FALSE;
	depthStencilDesc.StencilEnable = FALSE;

	CD3DX12_BLEND_DESC blendDesc{ D3D12_DEFAULT };
	auto& rt = blendDesc.RenderTarget[0];
	rt.BlendEnable = TRUE;
	rt.SrcBlend = D3D12_BLEND_ZERO;
	rt.DestBlend = D3D12_BLEND_SRC_COLOR;
	rt.BlendOp = D3D12_BLEND_OP_ADD;
	rt.SrcBlendAlpha = D3D12_BLEND_ZERO;
	rt.DestBlendAlpha = D3D12_BLEND_ONE;
	rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	CD3DX12_PIPELINE_STATE_STREAM pipelineStateStream{};
	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.RasterizerState = rasterizerDesc;
	pipelineStateStream.DepthStencilState = depthStencilDesc;
	pipelineStateStream.RTVFormats = rtvFormats;
	pipelineStateStream.BlendState = blendDesc;

	return pipelineStateStream.GraphicsDescV0();
}
