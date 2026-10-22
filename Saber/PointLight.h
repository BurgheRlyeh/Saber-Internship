#pragma once

#include "Headers.h"

#include "LightSource.h"

class PointLight : public LightSource, public IPositionableLight {
public:
	explicit PointLight(
		const DirectX::XMFLOAT3& position = { 0.f, 0.f, 0.f },
		float range = 25.f
	);

	LightType GetType() const override { return LightType::Point; }

	DirectX::XMFLOAT3 GetPosition() const override { return m_position; }
	void SetPosition(const DirectX::XMFLOAT3& position) override { m_position = position; }

	float GetRange() const { return m_range; }
	void SetRange(float range);

protected:
	void FillLightDesc(LightDesc& light) const override;

private:
	DirectX::XMFLOAT3 m_position{};
	float m_range{};
};

// UI

bool DrawSettings(PointLight& light);
