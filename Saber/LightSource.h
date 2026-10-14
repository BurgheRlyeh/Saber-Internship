#pragma once

#include "Headers.h"

#include "Camera.h"
#include "LightBuffer.h"

// Has a point in space it shines from
class IPositionableLight {
public:
	virtual DirectX::XMFLOAT3 GetPosition() const = 0;
	virtual void SetPosition(const DirectX::XMFLOAT3& position) = 0;

protected:
	~IPositionableLight() = default;	// Never deleted through the interface
};

// Shines along a direction
class IDirectionalLight {
public:
	virtual DirectX::XMFLOAT3 GetDirection() const = 0;	// Normalized

	// Spot rotates around its position, directional rotates around its target
	virtual void SetDirection(const DirectX::XMFLOAT3& direction) = 0;

protected:
	~IDirectionalLight() = default;

	// Up vector for the camera look-at that is never parallel to the direction
	static DirectX::XMVECTOR StableUpFor(DirectX::XMVECTOR normalizedDirection);

	// Normalized direction, straight down when it is zero
	static DirectX::XMVECTOR LoadDirectionOrFallback(const DirectX::XMFLOAT3& direction);
};

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
	virtual const Camera& GetShadowCamera(size_t viewIndex = 0) const = 0;

	DirectX::XMMATRIX GetViewProjectionMatrix(size_t viewIndex = 0) const {
		return GetShadowCamera(viewIndex).GetViewProjectionMatrix();
	}

	LightDesc GetLightDesc() const;

protected:
	virtual void FillLightDesc(LightDesc& light) const = 0;
};

// UI
bool DrawSettings(LightSource::Settings& settings);

// Position and direction fields for whatever the light supports
bool DrawPlacement(LightSource& light);

bool DrawSettings(LightSource& light);
