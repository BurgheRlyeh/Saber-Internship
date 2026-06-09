#include "LightSource.h"

#include "imgui.h"

//template <>
void DrawSettings(LightSource::Settings& settings) {
	ImGui::SeparatorText("Emission");
	ImGui::ColorEdit3("Color", &settings.color.x);
	ImGui::SliderFloat("Intensity", &settings.intensity, 0.f, 10.f);
}
