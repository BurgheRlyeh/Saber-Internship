#pragma once

#include "Headers.h"

#include "Camera.h"
#include "LightSource.h"

class DirectionalLight : public LightSource {
	StaticCamera m_camera{};

public:
	struct Settings : LightSource::Settings {};

	DirectionalLight();

	Settings& GetSettings() override { return m_settings; }
	const Settings& GetSettings() const override { return m_settings; }

	size_t GetShadowViewCount() const override { return 1; }
	Camera&       GetShadowCamera(size_t /*viewIndex*/ = 0) override       { return m_camera; }
	const Camera& GetShadowCamera(size_t /*viewIndex*/ = 0) const override { return m_camera; }

	// Derived views over the camera's placement — no backing state.
	DirectX::XMFLOAT3 GetDirection() const;  // normalize(poi - pos)
	DirectX::XMFLOAT3 GetTarget()    const;  // m_camera.poi
	float             GetDistance()  const;  // length(poi - pos)

	void SetDirection(const DirectX::XMFLOAT3& dir);  // updates pos, preserves target & distance
	void SetTarget   (const DirectX::XMFLOAT3& tgt);  // translates pos by the same delta
	void SetDistance (float d);                       // moves pos along -direction

private:
	Settings m_settings{};
};

void DrawSettings(DirectionalLight& light);
