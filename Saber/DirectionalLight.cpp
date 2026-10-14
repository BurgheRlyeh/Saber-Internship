#include "DirectionalLight.h"

#include <algorithm>
#include <cmath>

#include "imgui.h"

namespace {
	constexpr float MinDistance{ 1e-3f };
	constexpr float MinShadowExtent{ 1e-2f };
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

DirectX::XMFLOAT3 DirectionalLight::GetDirection() const {
	const StaticCamera::Settings& camera{ m_camera.GetSettings() };

	DirectX::XMFLOAT3 direction{};
	DirectX::XMStoreFloat3(&direction,
		DirectX::XMVector3Normalize(
			DirectX::XMVectorSubtract(
				DirectX::XMLoadFloat3(&camera.poi),
				DirectX::XMLoadFloat3(&camera.pos)
			)
		)
	);
	return direction;
}

DirectX::XMFLOAT3 DirectionalLight::GetTarget() const {
	return m_camera.GetSettings().poi;
}

float DirectionalLight::GetDistance() const {
	const StaticCamera::Settings& camera{ m_camera.GetSettings() };
	return DirectX::XMVectorGetX(DirectX::XMVector3Length(
		DirectX::XMVectorSubtract(
			DirectX::XMLoadFloat3(&camera.poi),
			DirectX::XMLoadFloat3(&camera.pos)
		)
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
	const DirectX::XMVECTOR delta{
		DirectX::XMVectorSubtract(newTarget, DirectX::XMLoadFloat3(&camera.poi))
	};

	DirectX::XMStoreFloat3(&camera.pos,
		DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&camera.pos), delta)
	);
	DirectX::XMStoreFloat3(&camera.poi, newTarget);
}

void DirectionalLight::SetDistance(float distance) {
	StaticCamera::Settings& camera{ m_camera.GetSettings() };

	const DirectX::XMFLOAT3 direction{ GetDirection() };
	DirectX::XMStoreFloat3(&camera.pos, DirectX::XMVectorSubtract(
		DirectX::XMLoadFloat3(&camera.poi),
		DirectX::XMVectorScale(
			DirectX::XMLoadFloat3(&direction),
			std::max(distance, MinDistance)
		)
	));
}

float DirectionalLight::GetShadowExtent() const {
	return m_camera.GetSettings().orthographicViewWidth;
}

void DirectionalLight::SetShadowExtent(float extent) {
	StaticCamera::Settings& camera{ m_camera.GetSettings() };
	camera.orthographicViewWidth = std::max(extent, MinShadowExtent);
	camera.orthographicViewHeight = camera.orthographicViewWidth;
}

void DirectionalLight::FillLightDesc(LightDesc& light) const {
	const DirectX::XMFLOAT3 direction{ GetDirection() };
	light.position = { -direction.x, -direction.y, -direction.z, 0.f };
	light.direction = { direction.x, direction.y, direction.z, 0.f };
}

bool DrawSettings(DirectionalLight& light) {
	bool isChanged{ DrawSettings(light.GetSettings()) };

	isChanged |= DrawPlacement(light);

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

	float shadowExtent{ light.GetShadowExtent() };
	if (ImGui::SliderFloat("Shadow extent", &shadowExtent, 1.f, 200.f)) {
		light.SetShadowExtent(shadowExtent);
		isChanged = true;
	}

	return isChanged;
}
