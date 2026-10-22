#pragma once

#include "Headers.h"

#include "LightSource.h"

class DirectionalLight : public LightSource, public IDirectionalLight {
public:
	DirectionalLight();

	LightType GetType() const override { return LightType::Directional; }

	DirectX::XMFLOAT3 GetDirection() const override { return m_direction; }
	void SetDirection(const DirectX::XMFLOAT3& direction) override;

protected:
	void FillLightDesc(LightDesc& light) const override;

private:
	DirectX::XMFLOAT3 m_direction{};
};

// UI

bool DrawSettings(DirectionalLight& light);
