#pragma once

#include "Headers.h"

#include "Camera.h"
#include "LightSource.h"

class DirectionalLight : public LightSource, public IDirectionalLight {
public:
	struct Settings : LightSource::Settings {};

	DirectionalLight();

	LightType GetType() const override { return LightType::Directional; }

	Settings& GetSettings() override { return m_settings; }
	const Settings& GetSettings() const override { return m_settings; }

	// TODO: cascades?
	size_t GetShadowViewCount() const override { return 1; }
	const Camera& GetShadowCamera(size_t viewIndex = 0) const override {
		assert(viewIndex == 0);
		return m_camera;
	}

	DirectX::XMFLOAT3 GetDirection() const override;	// normalize(poi - pos)
	DirectX::XMFLOAT3 GetTarget() const;				// the camera's poi
	float GetDistance() const;							// length(poi - pos)
	float GetShadowExtent() const;						// side of the square shadow box

	void SetDirection(const DirectX::XMFLOAT3& direction) override;	// moves pos, keeps target and distance
	void SetTarget(const DirectX::XMFLOAT3& target);		// moves pos by the same delta
	void SetDistance(float distance);						// moves pos along -direction
	void SetShadowExtent(float extent);

protected:
	void FillLightDesc(LightDesc& light) const override;

private:
	Settings m_settings{};
	StaticCamera m_camera{};
};

// UI

bool DrawSettings(DirectionalLight& light);
