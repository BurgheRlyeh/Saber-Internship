#include "LightBuffer.h"

#include "imgui.h"

const char* LightTypeName(LightType type) {
	switch (type) {
	case LightType::Point:			return "Point";
	case LightType::Directional:	return "Directional";
	case LightType::Spot:			return "Spot";
	}

	return "Unknown";
}

bool DrawSettings(LightBuffer& lightBuffer) {
	bool isChanged{};

	ImGui::SeparatorText("Ambient");
	isChanged |= ImGui::ColorEdit3("Color##ambient", &lightBuffer.ambientColorAndPower.x);
	isChanged |= ImGui::SliderFloat("Power##ambient", &lightBuffer.ambientColorAndPower.w, 0.0f, 10.0f);

	return isChanged;
}
