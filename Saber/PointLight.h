#pragma once

#include "Headers.h"

#include <array>

#include "Camera.h"
#include "LightSource.h"

class PointLight : public LightSource {
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
	Camera& GetShadowCamera(size_t viewIndex = 0) override {
		assert(viewIndex < FaceCount);
		return m_cameras[viewIndex];
	}
	const Camera& GetShadowCamera(size_t viewIndex = 0) const override {
		assert(viewIndex < FaceCount);
		return m_cameras[viewIndex];
	}

	DirectX::XMFLOAT3 GetPosition() const override;
	void SetPosition(const DirectX::XMFLOAT3& position);

	DirectX::XMFLOAT3 GetDirection() const override;
	void SetPlacement(
		const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& direction
	) override;

	float GetRange() const;
	void SetRange(float range);

protected:
	void FillLight(Light& light) const override;

private:
	Settings m_settings{};
	std::array<StaticCamera, FaceCount> m_cameras{};
};

// UI

bool DrawSettings(PointLight& light);
