#pragma once

#include "Headers.h"

#include "Camera.h"
#include "LightBuffer.h"

class LightSource {
public:
	struct Settings {
		DirectX::XMFLOAT3 diffuseColor{ 1.f, 1.f, 1.f };
		float diffusePower{ 1.f };

		DirectX::XMFLOAT3 specularColor{ 1.f, 1.f, 1.f };
		float specularPower{ 1.f };
	};

	virtual ~LightSource() = default;

	virtual LightType GetType() const = 0;

	virtual Settings& GetSettings() = 0;
	virtual const Settings& GetSettings() const = 0;

	// 1 view for directional and spot, 6 for a point light
	virtual size_t GetShadowViewCount() const = 0;
	virtual Camera& GetShadowCamera(size_t viewIndex = 0) = 0;
	virtual const Camera& GetShadowCamera(size_t viewIndex = 0) const = 0;

	DirectX::XMMATRIX GetViewProjectionMatrix(size_t viewIndex = 0) const {
		return GetShadowCamera(viewIndex).GetViewProjectionMatrix();
	}

	virtual DirectX::XMFLOAT3 GetPosition() const = 0;
	virtual DirectX::XMFLOAT3 GetDirection() const = 0;
	virtual void SetPlacement(
		const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& direction
	) = 0;

	Light GetLight() const;

protected:
	virtual void FillLight(Light& light) const = 0;
};

// UI
bool DrawSettings(LightSource::Settings& settings);

bool DrawSettings(LightSource& light);
