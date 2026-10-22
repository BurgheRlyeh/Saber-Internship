#include "SpotLight.h"

#include <algorithm>
#include <cmath>

#include "imgui.h"

namespace {
	constexpr float MinRange{ 1e-2f };
	constexpr float MinAngle{ 1.f };
	constexpr float MaxAngle{ 89.f };
}

SpotLight::SpotLight(
	const DirectX::XMFLOAT3& position,
	const DirectX::XMFLOAT3& direction,
	float range
) : m_position(position)
{
	SetDirection(direction);
	SetRange(range);
	SetCone(m_innerAngle, m_outerAngle);
}

void SpotLight::SetDirection(const DirectX::XMFLOAT3& direction) {
	DirectX::XMStoreFloat3(&m_direction, LoadDirectionOrFallback(direction));
}

void SpotLight::SetRange(float range) {
	m_range = std::max(range, MinRange);
}

void SpotLight::SetCone(float innerAngle, float outerAngle) {
	m_outerAngle = std::clamp(outerAngle, MinAngle, MaxAngle);
	m_innerAngle = std::clamp(innerAngle, MinAngle, m_outerAngle);
}

void SpotLight::FillLightDesc(LightDesc& light) const {
	light.position = { m_position.x, m_position.y, m_position.z, 1.f };
	light.direction = { m_direction.x, m_direction.y, m_direction.z, 0.f };

	// spot = saturate(cos * scale + offset): 0 at the outer angle, 1 at the inner one
	const float cosInner{ std::cosf(DirectX::XMConvertToRadians(m_innerAngle)) };
	const float cosOuter{ std::cosf(DirectX::XMConvertToRadians(m_outerAngle)) };
	const float scale{ 1.f / std::max(cosInner - cosOuter, 1e-4f) };

	light.params.y = 1.f / m_range;
	light.params.z = scale;
	light.params.w = -cosOuter * scale;
}

bool DrawSettings(SpotLight& light) {
	bool isChanged{ DrawSettings(light.GetSettings()) };

	isChanged |= DrawPlacement(light);

	float range{ light.GetRange() };
	if (ImGui::SliderFloat("Range", &range, 1.f, 200.f)) {
		light.SetRange(range);
		isChanged = true;
	}

	ImGui::SeparatorText("Cone");

	float innerAngle{ light.GetInnerAngle() };
	float outerAngle{ light.GetOuterAngle() };
	if (ImGui::SliderFloat("Inner angle", &innerAngle, MinAngle, MaxAngle)
		|| ImGui::SliderFloat("Outer angle", &outerAngle, MinAngle, MaxAngle)
	) {
		light.SetCone(innerAngle, outerAngle);
		isChanged = true;
	}

	return isChanged;
}
