#include "DirectionalLight.h"

#include "imgui.h"

DirectionalLight::DirectionalLight() {
	SetDirection({ 0.f, -1.f, 0.f });

	// The sun is the one light that shadows by default
	EnableShadow(true);
}

void DirectionalLight::SetDirection(const DirectX::XMFLOAT3& direction) {
	DirectX::XMStoreFloat3(&m_direction, LoadDirectionOrFallback(direction));
}

std::shared_ptr<LightShadow> DirectionalLight::MakeShadow() const {
	return std::make_shared<DirectionalShadow>();
}

StaticCamera DirectionalLight::GetShadowCamera(const LightShadow& shadow, size_t viewIndex) const {
	assert(viewIndex == 0);

	// Its own MakeShadow is the only way a directional light gets a shadow
	const DirectionalShadow::Box& box{ static_cast<const DirectionalShadow&>(shadow).GetBox() };

	StaticCamera camera{};
	StaticCamera::Settings& settings{ camera.GetSettings() };
	settings.projectionType = ProjectionType::Orthographic;
	settings.orthographicViewWidth = box.extent;
	settings.orthographicViewHeight = box.extent;
	settings.nearPlane = shadow.GetSettings().nearPlane;
	settings.farPlane = box.depth;

	const DirectX::XMVECTOR direction{ DirectX::XMLoadFloat3(&m_direction) };
	settings.poi = box.poi;
	DirectX::XMStoreFloat3(&settings.pos, DirectX::XMVectorSubtract(
		DirectX::XMLoadFloat3(&box.poi),
		DirectX::XMVectorScale(direction, box.distance)
	));
	DirectX::XMStoreFloat3(&settings.up, StableUpFor(direction));

	return camera;
}

void DirectionalLight::FillLightDesc(LightDesc& light) const {
	light.position = { -m_direction.x, -m_direction.y, -m_direction.z, 0.f };
	light.direction = { m_direction.x, m_direction.y, m_direction.z, 0.f };
}

bool DrawSettings(DirectionalLight& light) {
	bool isChanged{ DrawSettings(light.GetSettings()) };

	isChanged |= DrawPlacement(light);

	return isChanged;
}
