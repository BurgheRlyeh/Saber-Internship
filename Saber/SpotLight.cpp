#include "SpotLight.h"

#include <algorithm>
#include <cmath>

#include "imgui.h"

namespace {
	constexpr float MinRange{ 1e-2f };
	constexpr float MinAngle{ 1.f };
	constexpr float MaxAngle{ 89.f };

	DirectX::XMVECTOR StableUpFor(DirectX::XMVECTOR normalizedDirection) {
		DirectX::XMVECTOR up{ DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f) };
		if (std::abs(DirectX::XMVectorGetX(DirectX::XMVector3Dot(normalizedDirection, up))) > 0.99f) {
			up = DirectX::XMVectorSet(0.f, 0.f, 1.f, 0.f);
		}
		return up;
	}

	DirectX::XMVECTOR LoadDirectionOrFallback(const DirectX::XMFLOAT3& direction) {
		DirectX::XMVECTOR loaded{ DirectX::XMLoadFloat3(&direction) };
		if (DirectX::XMVector3Equal(loaded, DirectX::XMVectorZero())) {
			loaded = DirectX::XMVectorSet(0.f, -1.f, 0.f, 0.f);
		}
		return DirectX::XMVector3Normalize(loaded);
	}
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
	return m_camera.GetViewDirection();
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

void SpotLight::SetPlacement(
	const DirectX::XMFLOAT3& position,
	const DirectX::XMFLOAT3& direction
) {
	PlaceCamera(position, direction);
}

void SpotLight::SetCone(float innerAngle, float outerAngle) {
	m_settings.outerAngle = std::clamp(outerAngle, MinAngle, MaxAngle);
	m_settings.innerAngle = std::clamp(innerAngle, MinAngle, m_settings.outerAngle);

	// fov is the full angle, the cone is stored as a half-angle
	m_camera.GetSettings().fov = 2.f * m_settings.outerAngle;
}

void SpotLight::FillLight(Light& light) const {
	const DirectX::XMFLOAT3 position{ GetPosition() };
	const DirectX::XMFLOAT3 direction{ GetDirection() };

	light.position = { position.x, position.y, position.z, 0.f };
	light.direction = { direction.x, direction.y, direction.z, 0.f };

	light.cone = {
		GetRange(),
		std::cosf(DirectX::XMConvertToRadians(m_settings.innerAngle)),
		std::cosf(DirectX::XMConvertToRadians(m_settings.outerAngle)),
		0.f
	};
}

bool DrawSettings(SpotLight& light) {
	bool isChanged{ DrawSettings(light.GetSettings()) };

	ImGui::SeparatorText("Placement");

	DirectX::XMFLOAT3 position{ light.GetPosition() };
	if (ImGui::DragFloat3("Position", &position.x, 0.1f, -100.f, 100.f)) {
		light.SetPosition(position);
		isChanged = true;
	}

	DirectX::XMFLOAT3 direction{ light.GetDirection() };
	if (ImGui::DragFloat3("Direction", &direction.x, 0.01f, -1.f, 1.f)) {
		light.SetDirection(direction);
		isChanged = true;
	}

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
