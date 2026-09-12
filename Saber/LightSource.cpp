#include "LightSource.h"

#include "imgui.h"

#include "DirectionalLight.h"
#include "PointLight.h"
#include "SpotLight.h"

Light LightSource::GetLight() const {
	const Settings& settings{ GetSettings() };

	Light light{
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
		.cone{},
		.type{ static_cast<uint32_t>(GetType()), 0, 0, 0 }
	};

	FillLight(light);

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

bool DrawSettings(LightSource& light) {
	switch (light.GetType()) {
	case LightType::Point:			return DrawSettings(static_cast<PointLight&>(light));
	case LightType::Directional:	return DrawSettings(static_cast<DirectionalLight&>(light));
	case LightType::Spot:			return DrawSettings(static_cast<SpotLight&>(light));
	}

	return DrawSettings(light.GetSettings());
}
