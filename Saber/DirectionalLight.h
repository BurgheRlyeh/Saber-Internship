#pragma once

#include "Headers.h"

#include "Camera.h"
#include "LightSource.h"

class DirectionalLight : public LightSource, public IDirectionalLight {
public:
	DirectionalLight();

	LightType GetType() const override { return LightType::Directional; }

	DirectX::XMFLOAT3 GetDirection() const override { return m_direction; }
	void SetDirection(const DirectX::XMFLOAT3& direction) override;

	// TODO: cascades?
	size_t GetShadowViewCount() const override { return 1; }

	// Orthographic, looking along the direction at the shadow's box
	StaticCamera GetShadowCamera(const LightShadow& shadow, size_t viewIndex) const override;

protected:
	void FillLightDesc(LightDesc& light) const override;
	std::shared_ptr<LightShadow> MakeShadow() const override;

private:
	DirectX::XMFLOAT3 m_direction{};
};

// UI

bool DrawSettings(DirectionalLight& light);
