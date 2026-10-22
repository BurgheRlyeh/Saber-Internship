#include "PointLight.h"

#include <algorithm>

#include "imgui.h"

namespace {
	constexpr float MinRange{ 1e-2f };
}

PointLight::PointLight(const DirectX::XMFLOAT3& position, float range)
	: m_position(position)
{
	SetRange(range);
}

void PointLight::SetRange(float range) {
	m_range = std::max(range, MinRange);
}

void PointLight::FillLightDesc(LightDesc& light) const {
	light.position = { m_position.x, m_position.y, m_position.z, 1.f };
	light.params.y = 1.f / m_range;
}

bool DrawSettings(PointLight& light) {
	bool isChanged{ DrawSettings(light.GetSettings()) };

	isChanged |= DrawPlacement(light);

	float range{ light.GetRange() };
	if (ImGui::SliderFloat("Range", &range, 1.f, 200.f)) {
		light.SetRange(range);
		isChanged = true;
	}

	return isChanged;
}
