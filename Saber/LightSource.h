#pragma once

#include "Headers.h"

#include "Camera.h"

class LightSource {
public:
	struct Settings {
		DirectX::XMFLOAT3 color{ 1.f, 1.f, 1.f };
		float intensity{ 1.f };
	};

	virtual ~LightSource() = default;

	virtual Settings& GetSettings() = 0;
	virtual const Settings& GetSettings() const = 0;

	const DirectX::XMFLOAT3& GetColor() const { return GetSettings().color; }
	float GetIntensity() const { return GetSettings().intensity; }

	virtual size_t GetShadowViewCount() const = 0;

	virtual Camera& GetShadowCamera(size_t viewIndex = 0) = 0;
	virtual const Camera& GetShadowCamera(size_t viewIndex = 0) const = 0;

	DirectX::XMMATRIX GetViewProjectionMatrix(size_t viewIndex = 0) const {
		return GetShadowCamera(viewIndex).GetViewProjectionMatrix();
	}
};

//template <>
void DrawSettings(LightSource::Settings& settings);
