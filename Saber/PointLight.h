#pragma once

#include "Headers.h"

#include "Camera.h"
#include "LightSource.h"

class PointLight : public LightSource, public IPositionableLight {
public:
	static constexpr size_t FaceCount{ 6 };

	explicit PointLight(
		const DirectX::XMFLOAT3& position = { 0.f, 0.f, 0.f },
		float range = 25.f
	);

	LightType GetType() const override { return LightType::Point; }

	DirectX::XMFLOAT3 GetPosition() const override { return m_position; }
	void SetPosition(const DirectX::XMFLOAT3& position) override { m_position = position; }

	float GetRange() const { return m_range; }
	void SetRange(float range);

	size_t GetShadowViewCount() const override { return FaceCount; }

	// Cube face viewIndex: +X, -X, +Y, -Y, +Z, -Z
	StaticCamera GetShadowCamera(const LightShadow& shadow, size_t viewIndex) const override;

protected:
	void FillLightDesc(LightDesc& light) const override;

private:
	DirectX::XMFLOAT3 m_position{};
	float m_range{};
};

// UI

bool DrawSettings(PointLight& light);
