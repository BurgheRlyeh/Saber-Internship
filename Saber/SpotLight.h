#pragma once

#include "Headers.h"

#include "LightSource.h"

class SpotLight : public LightSource, public IPositionableLight, public IDirectionalLight {
public:
	SpotLight(
		const DirectX::XMFLOAT3& position = { 0.0f, 10.0f, 0.0f },
		const DirectX::XMFLOAT3& direction = { 0.0f, -1.0f, 0.0f },
		float range = 40.0f
	);

	LightType GetType() const override { return LightType::Spot; }

	DirectX::XMFLOAT3 GetPosition() const override { return m_position; }
	void SetPosition(const DirectX::XMFLOAT3& position) override { m_position = position; }

	DirectX::XMFLOAT3 GetDirection() const override { return m_direction; }
	void SetDirection(const DirectX::XMFLOAT3& direction) override;

	float GetRange() const { return m_range; }
	void SetRange(float range);

	// Half-angles in degrees
	float GetInnerAngle() const { return m_innerAngle; }
	float GetOuterAngle() const { return m_outerAngle; }
	void SetCone(float innerAngle, float outerAngle);

protected:
	void FillLightDesc(LightDesc& light) const override;

private:
	DirectX::XMFLOAT3 m_position{};
	DirectX::XMFLOAT3 m_direction{};
	float m_range{};
	float m_innerAngle{ 15.f };
	float m_outerAngle{ 25.f };
};

// UI
bool DrawSettings(SpotLight& light);
