#include "LightShadow.h"

#include "imgui.h"

#include "CommandList.h"
#include "DepthBuffer.h"
#include "DeviceContext.h"
#include "TextureResource.h"

UINT ShadowMapRegion::GetSize() const {
	assert(pMap);
	return size ? size : static_cast<UINT>(pMap->GetDesc().Width);
}

LightShadow::LightShadow(UINT16 viewCount)
	: m_viewCount(viewCount)
{
	assert(viewCount > 0);
}

void LightShadow::CreateMap(std::shared_ptr<DeviceContext> pDeviceContext, const std::wstring& name) {
	m_map = ShadowMapRegion{
		.pMap{ std::make_shared<DepthBuffer>(
			name,
			pDeviceContext,
			m_settings.resolution,
			m_settings.resolution,
			DXGI_FORMAT_D32_FLOAT,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
			m_viewCount
		) }
	};
	m_ownsMap = true;
}

void LightShadow::SetMap(const ShadowMapRegion& region) {
	assert(!region.IsValid() || region.firstSlice + m_viewCount <= region.pMap->GetArraySize());
	m_map = region;
	m_ownsMap = false;
}

void LightShadow::ResetMap() {
	m_map = {};
	m_ownsMap = false;
}

void LightShadow::Update(
	std::shared_ptr<DeviceContext> pDeviceContext,
	std::shared_ptr<CommandList> pCommandList
) {
	if (!m_ownsMap || m_map.pMap->GetDesc().Width == m_settings.resolution) {
		return;
	}

	// Frames in flight may still sample the old texture
	pCommandList->KeepAlive(m_map.pMap->GetTexture());
	m_map.pMap->Resize(pDeviceContext->GetDevice(), m_settings.resolution, m_settings.resolution);
}

bool DrawSettings(LightShadow& shadow) {
	bool isChanged{};
	LightShadow::Settings& settings{ shadow.GetSettings() };

	// Only a map of its own follows the setting, a shared one is sized by its owner
	ImGui::BeginDisabled(shadow.GetMap().IsValid() && !shadow.OwnsMap());
	int resolution{ static_cast<int>(settings.resolution) };
	if (ImGui::SliderInt("Resolution", &resolution, 128, 4096)) {
		settings.resolution = static_cast<UINT>(resolution);
		isChanged = true;
	}
	ImGui::EndDisabled();

	isChanged |= ImGui::SliderFloat("Near plane", &settings.nearPlane, .01f, 10.f, "%.2f");

	if (auto* pDirectional{ dynamic_cast<DirectionalShadow*>(&shadow) }) {
		DirectionalShadow::Box& box{ pDirectional->GetBox() };
		isChanged |= ImGui::DragFloat3("Box poi", &box.poi.x, 0.1f, -100.f, 100.f);
		isChanged |= ImGui::SliderFloat("Box distance", &box.distance, 1.f, 200.f);
		isChanged |= ImGui::SliderFloat("Box extent", &box.extent, 1.f, 200.f);
		isChanged |= ImGui::SliderFloat("Box depth", &box.depth, 1.f, 400.f);
	}

	isChanged |= ImGui::SliderFloat("Depth bias, texels", &settings.depthBias, 0.f, 8.f, "%.2f");
	isChanged |= ImGui::SliderFloat("Normal offset, texels", &settings.normalOffset, 0.f, 4.f, "%.2f");

	// Each step costs (2r + 1)^2 comparisons, and the sampler filters 2x2 per tap
	isChanged |= ImGui::SliderInt("PCF radius", &settings.pcfRadius, 0, 4);

	const ShadowMapRegion& region{ shadow.GetMap() };
	if (!region.IsValid()) {
		ImGui::TextUnformatted("No map yet");
		return isChanged;
	}

	const D3D12_RESOURCE_DESC mapDesc{ region.pMap->GetDesc() };
	ImGui::Text("Map %llu x %u, %u slices", mapDesc.Width, mapDesc.Height, mapDesc.DepthOrArraySize);
	// The UI samples a Texture2D, which only a single-slice map has
	if (mapDesc.DepthOrArraySize == 1) {
		// Reversed depth in one channel: red, brightest closest to the light
		constexpr float PreviewSize{ 256.f };
		ImGui::Image(
			static_cast<ImTextureID>(region.pMap->GetSrvGpuDescHandle().ptr),
			ImVec2{ PreviewSize, PreviewSize }
		);
	}

	return isChanged;
}
