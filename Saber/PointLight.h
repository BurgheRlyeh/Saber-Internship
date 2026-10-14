#pragma once

#include "Headers.h"

#include <array>

#include "Camera.h"
#include "LightSource.h"

class PointLight : public LightSource, public IPositionableLight {
public:
	static constexpr size_t FaceCount{ 6 };

	struct Settings : LightSource::Settings {};

	explicit PointLight(
		const DirectX::XMFLOAT3& position = { 0.f, 0.f, 0.f },
		float range = 25.f
	);

	LightType GetType() const override { return LightType::Point; }

	Settings& GetSettings() override { return m_settings; }
	const Settings& GetSettings() const override { return m_settings; }

	size_t GetShadowViewCount() const override { return FaceCount; }
	const Camera& GetShadowCamera(size_t viewIndex = 0) const override {
		assert(viewIndex < FaceCount);
		return m_cameras[viewIndex];
	}

	DirectX::XMFLOAT3 GetPosition() const override;
	void SetPosition(const DirectX::XMFLOAT3& position) override;

	float GetRange() const;
	void SetRange(float range);

protected:
	void FillLightDesc(LightDesc& light) const override;

private:
	Settings m_settings{};
	std::array<StaticCamera, FaceCount> m_cameras{};
};

// UI

bool DrawSettings(PointLight& light);
