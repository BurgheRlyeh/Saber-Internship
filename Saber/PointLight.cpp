#include "PointLight.h"

#include <algorithm>

#include "imgui.h"

namespace {
	constexpr float MinRange{ 1e-2f };

	// +X, -X, +Y, -Y, +Z, -Z. Up is picked so it is never parallel to the axis
	constexpr std::array<DirectX::XMFLOAT3, PointLight::FaceCount> FaceDirections{ {
		{  1.f,  0.f,  0.f },
		{ -1.f,  0.f,  0.f },
		{  0.f,  1.f,  0.f },
		{  0.f, -1.f,  0.f },
		{  0.f,  0.f,  1.f },
		{  0.f,  0.f, -1.f }
	} };

	constexpr std::array<DirectX::XMFLOAT3, PointLight::FaceCount> FaceUps{ {
		{ 0.f, 1.f, 0.f },
		{ 0.f, 1.f, 0.f },
		{ 0.f, 0.f, 1.f },
		{ 0.f, 0.f, 1.f },
		{ 0.f, 1.f, 0.f },
		{ 0.f, 1.f, 0.f }
	} };
}

PointLight::PointLight(const DirectX::XMFLOAT3& position, float range) {
	for (StaticCamera& camera : m_cameras) {
		StaticCamera::Settings& settings{ camera.GetSettings() };
		settings.projectionType = ProjectionType::Perspective;
		settings.fov = 90.f;	// exactly one cube face
		settings.aspectRatio = 1.0f;
		settings.nearPlane = 0.1f;
		settings.farPlane = std::max(range, MinRange);
	}

	SetPosition(position);
}

DirectX::XMFLOAT3 PointLight::GetPosition() const {
	return m_cameras[0].GetSettings().pos;
}

void PointLight::SetPosition(const DirectX::XMFLOAT3& position) {
	const DirectX::XMVECTOR origin{ DirectX::XMLoadFloat3(&position) };

	for (size_t face{}; face < FaceCount; ++face) {
		DirectX::XMVECTOR poiVector{ DirectX::XMVectorAdd(
			origin,
			DirectX::XMLoadFloat3(&FaceDirections[face])
		) };

		StaticCamera::Settings& settings{ m_cameras[face].GetSettings() };
		settings.pos = position;
		DirectX::XMStoreFloat3(&settings.poi, poiVector);
		settings.up = FaceUps[face];
	}
}

float PointLight::GetRange() const {
	return m_cameras[0].GetSettings().farPlane;
}

void PointLight::SetRange(float range) {
	for (StaticCamera& camera : m_cameras) {
		camera.GetSettings().farPlane = std::max(range, MinRange);
	}
}

void PointLight::FillLightDesc(LightDesc& light) const {
	const DirectX::XMFLOAT3 position{ GetPosition() };
	light.position = { position.x, position.y, position.z, 1.f };
	light.params.y = 1.f / GetRange();
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
