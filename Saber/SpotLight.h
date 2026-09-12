#pragma once

#include "Headers.h"

#include "Camera.h"
#include "LightSource.h"

class SpotLight : public LightSource {
public:
	struct Settings : LightSource::Settings {
		// Full cone half-angles in degrees
		float innerAngle{ 15.0f };
		float outerAngle{ 25.0f };
	};

	SpotLight(
		const DirectX::XMFLOAT3& position = { 0.0f, 10.0f, 0.0f },
		const DirectX::XMFLOAT3& direction = { 0.0f, -1.0f, 0.0f },
		float range = 40.0f
	);

	LightType GetType() const override { return LightType::Spot; }

	Settings& GetSettings() override { return m_settings; }
	const Settings& GetSettings() const override { return m_settings; }

	size_t GetShadowViewCount() const override { return 1; }
	Camera& GetShadowCamera(size_t viewIndex = 0) override {
		assert(viewIndex == 0);
		return m_camera;
	}
	const Camera& GetShadowCamera(size_t viewIndex = 0) const override {
		assert(viewIndex == 0);
		return m_camera;
	}

	DirectX::XMFLOAT3 GetPosition() const override;
	DirectX::XMFLOAT3 GetDirection() const override;
	float GetRange() const;

	void SetPosition(const DirectX::XMFLOAT3& position);
	void SetDirection(const DirectX::XMFLOAT3& direction);
	void SetRange(float range);

	void SetPlacement(
		const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& direction
	) override;

	// Keeps the shadow camera's fov in step with the outer angle
	void SetCone(float innerAngle, float outerAngle);

protected:
	void FillLight(Light& light) const override;

private:
	Settings m_settings{};
	StaticCamera m_camera{};

	void PlaceCamera(
		const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& direction
	);
};

// UI
bool DrawSettings(SpotLight& light);
