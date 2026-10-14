#include "LightSource.h"

#include <cmath>

#include "imgui.h"

#include "DirectionalLight.h"
#include "PointLight.h"
#include "SpotLight.h"

DirectX::XMVECTOR IDirectionalLight::StableUpFor(DirectX::XMVECTOR normalizedDirection) {
	DirectX::XMVECTOR up{ DirectX::XMVectorSet(0.f, 1.f, 0.f, 0.f) };
	auto dot{ DirectX::XMVector3Dot(normalizedDirection, up) };
	if (std::abs(DirectX::XMVectorGetX(dot)) > 0.99f) {
		up = DirectX::XMVectorSet(0.f, 0.f, 1.f, 0.f);
	}
	return up;
}

DirectX::XMVECTOR IDirectionalLight::LoadDirectionOrFallback(const DirectX::XMFLOAT3& direction) {
	DirectX::XMVECTOR loaded{ DirectX::XMLoadFloat3(&direction) };
	if (DirectX::XMVector3Equal(loaded, DirectX::XMVectorZero())) {
		loaded = DirectX::XMVectorSet(0.f, -1.f, 0.f, 0.f);
	}
	return DirectX::XMVector3Normalize(loaded);
}

LightDesc LightSource::GetLightDesc() const {
	const Settings& settings{ GetSettings() };

	LightDesc light{
		.position{},
		.direction{},
		.diffuseColorAndPower{
			settings.diffuseColor.x,
			settings.diffuseColor.y,
			settings.diffuseColor.z,
			settings.diffusePower
		},
		.specularColorAndPower{
			settings.specularColor.x,
			settings.specularColor.y,
			settings.specularColor.z,
			settings.specularPower
		},
		.params{ static_cast<float>(GetType()), 0.f, 0.f, 1.f }
	};

	FillLightDesc(light);

	return light;
}

bool DrawSettings(LightSource::Settings& settings) {
	bool isChanged{};

	ImGui::SeparatorText("Emission");
	isChanged |= ImGui::ColorEdit3("Diffuse", &settings.diffuseColor.x);
	isChanged |= ImGui::SliderFloat("Diffuse power", &settings.diffusePower, 0.f, 100.f);
	isChanged |= ImGui::ColorEdit3("Specular", &settings.specularColor.x);
	isChanged |= ImGui::SliderFloat("Specular power", &settings.specularPower, 0.f, 100.f);

	return isChanged;
}

bool DrawPlacement(LightSource& light) {
	bool isChanged{};

	ImGui::SeparatorText("Placement");

	if (auto* pPositionable{ dynamic_cast<IPositionableLight*>(&light) }) {
		DirectX::XMFLOAT3 position{ pPositionable->GetPosition() };
		if (ImGui::DragFloat3("Position", &position.x, 0.1f, -100.f, 100.f)) {
			pPositionable->SetPosition(position);
			isChanged = true;
		}
	}

	if (auto* pDirectional{ dynamic_cast<IDirectionalLight*>(&light) }) {
		DirectX::XMFLOAT3 direction{ pDirectional->GetDirection() };
		if (ImGui::DragFloat3("Direction", &direction.x, 0.01f, -1.f, 1.f)) {
			pDirectional->SetDirection(direction);
			isChanged = true;
		}
	}

	return isChanged;
}

bool DrawSettings(LightSource& light) {
	switch (light.GetType()) {
	case LightType::Point:			return DrawSettings(static_cast<PointLight&>(light));
	case LightType::Directional:	return DrawSettings(static_cast<DirectionalLight&>(light));
	case LightType::Spot:			return DrawSettings(static_cast<SpotLight&>(light));
	}

	return DrawSettings(light.GetSettings());
}
