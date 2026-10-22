#include "DirectionalLight.h"

#include "imgui.h"

DirectionalLight::DirectionalLight() {
	SetDirection({ 0.f, -1.f, 0.f });
}

void DirectionalLight::SetDirection(const DirectX::XMFLOAT3& direction) {
	DirectX::XMStoreFloat3(&m_direction, LoadDirectionOrFallback(direction));
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
