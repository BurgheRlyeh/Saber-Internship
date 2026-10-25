#include "PointLight.h"

#include <algorithm>
#include <array>

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

PointLight::PointLight(const DirectX::XMFLOAT3& position, float range)
	: m_position(position)
{
	SetRange(range);
}

void PointLight::SetRange(float range) {
	m_range = std::max(range, MinRange);
}

StaticCamera PointLight::GetShadowCamera(const LightShadow& shadow, size_t viewIndex) const {
	assert(viewIndex < FaceCount);

	StaticCamera camera{};
	StaticCamera::Settings& settings{ camera.GetSettings() };
	settings.projectionType = ProjectionType::Perspective;
	settings.fov = 90.f;	// exactly one cube face
	settings.aspectRatio = 1.f;
	settings.nearPlane = shadow.GetSettings().nearPlane;
	settings.farPlane = m_range;

	settings.pos = m_position;
	DirectX::XMStoreFloat3(&settings.poi, DirectX::XMVectorAdd(
		DirectX::XMLoadFloat3(&m_position),
		DirectX::XMLoadFloat3(&FaceDirections[viewIndex])
	));
	settings.up = FaceUps[viewIndex];

	return camera;
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
