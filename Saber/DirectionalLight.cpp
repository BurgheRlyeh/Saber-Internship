#include "DirectionalLight.h"

#include <algorithm>
#include <cmath>

#include "imgui.h"

namespace {
	constexpr float MinDistance{ 1e-3f };

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

DirectionalLight::DirectionalLight() {
	StaticCamera::Settings& camera{ m_camera.GetSettings() };
	camera.projectionType = ProjectionType::Orthographic;
	camera.nearPlane = 0.1f;
	camera.farPlane = 100.f;
	camera.orthographicViewWidth = 30.f;
	camera.orthographicViewHeight = 30.f;

	camera.poi = { 0.f, 0.f, 0.f };
	camera.pos = { 0.f, 50.f, 0.f };
	camera.up = { 0.f, 0.f, 1.f };
}

DirectX::XMFLOAT3 DirectionalLight::GetPosition() const {
	return m_camera.GetSettings().pos;
}

DirectX::XMFLOAT3 DirectionalLight::GetDirection() const {
	const StaticCamera::Settings& camera{ m_camera.GetSettings() };

	DirectX::XMFLOAT3 direction{};
	DirectX::XMStoreFloat3(&direction, DirectX::XMVector3Normalize(
		DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&camera.poi), DirectX::XMLoadFloat3(&camera.pos))
	));
	return direction;
}

DirectX::XMFLOAT3 DirectionalLight::GetTarget() const {
	return m_camera.GetSettings().poi;
}

float DirectionalLight::GetDistance() const {
	const StaticCamera::Settings& camera{ m_camera.GetSettings() };
	return DirectX::XMVectorGetX(DirectX::XMVector3Length(
		DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&camera.poi), DirectX::XMLoadFloat3(&camera.pos))
	));
}

void DirectionalLight::SetDirection(const DirectX::XMFLOAT3& direction) {
	StaticCamera::Settings& camera{ m_camera.GetSettings() };

	const DirectX::XMVECTOR normalizedDir{ LoadDirectionOrFallback(direction) };
	const float distance{ std::max(GetDistance(), MinDistance) };

	DirectX::XMStoreFloat3(&camera.pos, DirectX::XMVectorSubtract(
		DirectX::XMLoadFloat3(&camera.poi),
		DirectX::XMVectorScale(normalizedDir, distance)
	));
	DirectX::XMStoreFloat3(&camera.up, StableUpFor(normalizedDir));
}

void DirectionalLight::SetTarget(const DirectX::XMFLOAT3& target) {
	StaticCamera::Settings& camera{ m_camera.GetSettings() };

	const DirectX::XMVECTOR newTarget{ DirectX::XMLoadFloat3(&target) };
	const DirectX::XMVECTOR delta{ DirectX::XMVectorSubtract(newTarget, DirectX::XMLoadFloat3(&camera.poi)) };

	DirectX::XMStoreFloat3(&camera.pos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&camera.pos), delta));
	DirectX::XMStoreFloat3(&camera.poi, newTarget);
}

void DirectionalLight::SetDistance(float distance) {
	StaticCamera::Settings& camera{ m_camera.GetSettings() };

	const DirectX::XMFLOAT3 direction{ GetDirection() };
	DirectX::XMStoreFloat3(&camera.pos, DirectX::XMVectorSubtract(
		DirectX::XMLoadFloat3(&camera.poi),
		DirectX::XMVectorScale(DirectX::XMLoadFloat3(&direction), std::max(distance, MinDistance))
	));
}

void DirectionalLight::SetPlacement(
	const DirectX::XMFLOAT3& position,
	const DirectX::XMFLOAT3& direction
) {
	SetDirection(direction);

	const DirectX::XMVECTOR dir{ LoadDirectionOrFallback(direction) };
	DirectX::XMFLOAT3 target{};
	DirectX::XMStoreFloat3(&target, DirectX::XMVectorAdd(
		DirectX::XMLoadFloat3(&position),
		DirectX::XMVectorScale(dir, GetDistance())
	));
	SetTarget(target);
}

void DirectionalLight::FillLight(Light& light) const {
	const DirectX::XMFLOAT3 direction{ GetDirection() };
	light.direction = { direction.x, direction.y, direction.z, 0.f };
}

bool DrawSettings(DirectionalLight& light) {
	bool isChanged{ DrawSettings(light.GetSettings()) };

	ImGui::SeparatorText("Placement");

	DirectX::XMFLOAT3 direction{ light.GetDirection() };
	if (ImGui::DragFloat3("Direction", &direction.x, 0.01f, -1.f, 1.f)) {
		light.SetDirection(direction);
		isChanged = true;
	}

	DirectX::XMFLOAT3 target{ light.GetTarget() };
	if (ImGui::DragFloat3("Target", &target.x, 0.1f, -100.f, 100.f)) {
		light.SetTarget(target);
		isChanged = true;
	}

	float distance{ light.GetDistance() };
	if (ImGui::SliderFloat("Distance", &distance, 1.f, 200.f)) {
		light.SetDistance(distance);
		isChanged = true;
	}

	Camera::Settings& camera{ light.GetShadowCamera().GetSettings() };
	if (ImGui::SliderFloat("Shadow extent", &camera.orthographicViewWidth, 1.f, 200.f)) {
		camera.orthographicViewHeight = camera.orthographicViewWidth;
		isChanged = true;
	}

	return isChanged;
}
