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
) {
	StaticCamera::Settings& camera{ m_camera.GetSettings() };
	camera.nearPlane = 0.1f;
	camera.farPlane = std::max(range, MinRange);
	camera.projectionType = ProjectionType::Perspective;
	camera.aspectRatio = 1.f;

	PlaceCamera(position, direction);
	SetCone(m_settings.innerAngle, m_settings.outerAngle);
}

void SpotLight::PlaceCamera(
	const DirectX::XMFLOAT3& position,
	const DirectX::XMFLOAT3& direction
) {
	StaticCamera::Settings& camera{ m_camera.GetSettings() };

	const auto normalizedDir{ LoadDirectionOrFallback(direction) };

	camera.pos = position;
	DirectX::XMStoreFloat3(
		&camera.poi,
		DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&position), normalizedDir)
	);
	DirectX::XMStoreFloat3(&camera.up, StableUpFor(normalizedDir));
}

DirectX::XMFLOAT3 SpotLight::GetPosition() const {
	return m_camera.GetSettings().pos;
}

DirectX::XMFLOAT3 SpotLight::GetDirection() const {
	DirectX::XMFLOAT3 direction{ m_camera.GetViewDirection() };
	DirectX::XMStoreFloat3(&direction, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&direction)));
	return direction;
}

float SpotLight::GetRange() const {
	return m_camera.GetSettings().farPlane;
}

void SpotLight::SetPosition(const DirectX::XMFLOAT3& position) {
	PlaceCamera(position, GetDirection());
}

void SpotLight::SetDirection(const DirectX::XMFLOAT3& direction) {
	PlaceCamera(GetPosition(), direction);
}

void SpotLight::SetRange(float range) {
	m_camera.GetSettings().farPlane = std::max(range, MinRange);
}

void SpotLight::SetCone(float innerAngle, float outerAngle) {
	m_settings.outerAngle = std::clamp(outerAngle, MinAngle, MaxAngle);
	m_settings.innerAngle = std::clamp(innerAngle, MinAngle, m_settings.outerAngle);

	// fov is the full angle, the cone is stored as a half-angle
	m_camera.GetSettings().fov = 2.f * m_settings.outerAngle;
}

void SpotLight::FillLightDesc(LightDesc& light) const {
	const DirectX::XMFLOAT3 position{ GetPosition() };
	const DirectX::XMFLOAT3 direction{ GetDirection() };

	light.position = { position.x, position.y, position.z, 1.f };
	light.direction = { direction.x, direction.y, direction.z, 0.f };

	// spot = saturate(cos * scale + offset): 0 at the outer angle, 1 at the inner one
	const float cosInner{ std::cosf(DirectX::XMConvertToRadians(m_settings.innerAngle)) };
	const float cosOuter{ std::cosf(DirectX::XMConvertToRadians(m_settings.outerAngle)) };
	const float scale{ 1.f / std::max(cosInner - cosOuter, 1e-4f) };

	light.params.y = 1.f / GetRange();
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

	float innerAngle{ light.GetSettings().innerAngle };
	float outerAngle{ light.GetSettings().outerAngle };
	if (ImGui::SliderFloat("Inner angle", &innerAngle, MinAngle, MaxAngle)
		|| ImGui::SliderFloat("Outer angle", &outerAngle, MinAngle, MaxAngle)
	) {
		light.SetCone(innerAngle, outerAngle);
		isChanged = true;
	}

	return isChanged;
}
