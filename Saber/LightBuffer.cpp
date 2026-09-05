#include "LightBuffer.h"

#include "imgui.h"

bool DrawSettings(Light& light) {
	bool isChanged{};

	isChanged |= ImGui::DragFloat3("Position", &light.position.x, 0.1f, -100.0f, 100.0f);

	isChanged |= ImGui::ColorEdit3("Diffuse", &light.diffuseColorAndPower.x);
	isChanged |= ImGui::SliderFloat("Diffuse power", &light.diffuseColorAndPower.w, 0.0f, 10.0f);

	isChanged |= ImGui::ColorEdit3("Specular", &light.specularColorAndPower.x);
	isChanged |= ImGui::SliderFloat("Specular power", &light.specularColorAndPower.w, 0.0f, 10.0f);

	return isChanged;
}

bool DrawSettings(LightBuffer& lightBuffer) {
	bool isChanged{};

	ImGui::SeparatorText("Ambient");
	isChanged |= ImGui::ColorEdit3("Color##ambient", &lightBuffer.ambientColorAndPower.x);
	isChanged |= ImGui::SliderFloat("Power##ambient", &lightBuffer.ambientColorAndPower.w, 0.0f, 10.0f);

	ImGui::SeparatorText("Sources");

	uint32_t& count{ lightBuffer.lightsCount.x };
	ImGui::Text("Count: %u / %d", count, LIGHTS_MAX_COUNT);

	if (ImGui::Button("+")) {
		isChanged |= lightBuffer.Add(
			DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f },
			DirectX::XMFLOAT3{ 1.0f, 1.0f, 1.0f }, 1.0f,
			DirectX::XMFLOAT3{ 1.0f, 1.0f, 1.0f }, 1.0f
		);
	}
	ImGui::SameLine();
	if (ImGui::Button("-") && count > 0) {
		--count;
		isChanged = true;
	}

	for (uint32_t i{}; i < count; ++i) {
		ImGui::PushID(static_cast<int>(i));
		if (ImGui::TreeNode("source", "Light %u", i)) {
			isChanged |= DrawSettings(lightBuffer.lights[i]);
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	return isChanged;
}
