#include "DirectionalLight.h"

#include <cmath>

#include "imgui.h"

using namespace DirectX;

namespace {
	constexpr float kMinDistance{ 1e-3f };

	// A look-at with `up` parallel to view direction is degenerate. Pick a stable
	// up when the light points (almost) straight along the world Y axis.
	XMVECTOR StableUpFor(XMVECTOR dirNormalized) {
		XMVECTOR up{ XMVectorSet(0.f, 1.f, 0.f, 0.f) };
		if (std::abs(XMVectorGetX(XMVector3Dot(dirNormalized, up))) > 0.99f) {
			up = XMVectorSet(0.f, 0.f, 1.f, 0.f);
		}
		return up;
	}

	XMVECTOR LoadDirOrFallback(const XMFLOAT3& dir) {
		XMVECTOR v{ XMLoadFloat3(&dir) };
		if (XMVector3Equal(v, XMVectorZero())) {
			v = XMVectorSet(0.f, -1.f, 0.f, 0.f);
		}
		return XMVector3Normalize(v);
	}
}

DirectionalLight::DirectionalLight() {
	StaticCamera::Settings& s{ m_camera.GetSettings() };
	s.projectionType = ProjectionType::Orthographic;
	s.nearPlane = 0.1f;
	s.farPlane = 100.f;
	s.orthographicViewWidth = 30.f;
	s.orthographicViewHeight = 30.f;

	// Default placement: light at +Y, looking down at the origin from `distance`.
	s.poi = { 0.f, 0.f, 0.f };
	s.pos = { 0.f, 50.f, 0.f };
	s.up  = { 0.f, 0.f, 1.f };  // Y-axis case requires non-Y up
}

XMFLOAT3 DirectionalLight::GetDirection() const {
	const StaticCamera::Settings& s{ m_camera.GetSettings() };
	XMVECTOR dir{ XMVectorSubtract(XMLoadFloat3(&s.poi), XMLoadFloat3(&s.pos)) };
	XMFLOAT3 out{};
	XMStoreFloat3(&out, XMVector3Normalize(dir));
	return out;
}

XMFLOAT3 DirectionalLight::GetTarget() const {
	return m_camera.GetSettings().poi;
}

float DirectionalLight::GetDistance() const {
	const StaticCamera::Settings& s{ m_camera.GetSettings() };
	XMVECTOR dir{ XMVectorSubtract(XMLoadFloat3(&s.poi), XMLoadFloat3(&s.pos)) };
	return XMVectorGetX(XMVector3Length(dir));
}

void DirectionalLight::SetDirection(const XMFLOAT3& dir) {
	StaticCamera::Settings& s{ m_camera.GetSettings() };

	XMVECTOR dirN{ LoadDirOrFallback(dir) };
	const float dist{ std::max(GetDistance(), kMinDistance) };

	XMVECTOR poi{ XMLoadFloat3(&s.poi) };
	XMVECTOR pos{ XMVectorSubtract(poi, XMVectorScale(dirN, dist)) };
	XMStoreFloat3(&s.pos, pos);
	XMStoreFloat3(&s.up, StableUpFor(dirN));
}

void DirectionalLight::SetTarget(const XMFLOAT3& tgt) {
	StaticCamera::Settings& s{ m_camera.GetSettings() };

	XMVECTOR newPoi{ XMLoadFloat3(&tgt) };
	XMVECTOR oldPoi{ XMLoadFloat3(&s.poi) };
	XMVECTOR delta{ XMVectorSubtract(newPoi, oldPoi) };

	XMVECTOR newPos{ XMVectorAdd(XMLoadFloat3(&s.pos), delta) };
	XMStoreFloat3(&s.pos, newPos);
	XMStoreFloat3(&s.poi, newPoi);
}

void DirectionalLight::SetDistance(float d) {
	d = std::max(d, kMinDistance);

	StaticCamera::Settings& s{ m_camera.GetSettings() };
	XMFLOAT3 dir{ GetDirection() };
	XMVECTOR dirN{ XMLoadFloat3(&dir) };

	XMVECTOR poi{ XMLoadFloat3(&s.poi) };
	XMVECTOR pos{ XMVectorSubtract(poi, XMVectorScale(dirN, d)) };
	XMStoreFloat3(&s.pos, pos);
}

// UI

void DrawSettings(DirectionalLight& light) {
	DrawSettings(static_cast<LightSource::Settings&>(light.GetSettings()));

	ImGui::SeparatorText("Placement");

	XMFLOAT3 dir{ light.GetDirection() };
	if (ImGui::DragFloat3("Direction", &dir.x, 0.01f, -1.f, 1.f)) {
		light.SetDirection(dir);
	}

	XMFLOAT3 tgt{ light.GetTarget() };
	if (ImGui::DragFloat3("Target", &tgt.x, 0.1f, -100.f, 100.f)) {
		light.SetTarget(tgt);
	}

	float dist{ light.GetDistance() };
	if (ImGui::SliderFloat("Distance", &dist, 1.f, 200.f)) {
		light.SetDistance(dist);
	}

	if (ImGui::CollapsingHeader("Shadow Camera")) {
		ImGui::PushID("ShadowCamera");
		DrawSettings(static_cast<StaticCamera::Settings&>(light.GetShadowCamera().GetSettings()));
		ImGui::PopID();
	}
}
