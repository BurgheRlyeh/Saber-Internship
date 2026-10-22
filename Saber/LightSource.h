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

	// Turns the light in place
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

	Settings& GetSettings() { return m_settings; }
	const Settings& GetSettings() const { return m_settings; }

	LightDesc GetLightDesc() const;

protected:
	virtual void FillLightDesc(LightDesc& light) const = 0;

private:
	Settings m_settings{};
};

// UI
bool DrawSettings(LightSource::Settings& settings);

// Position and direction fields for whatever the light supports
bool DrawPlacement(LightSource& light);

bool DrawSettings(LightSource& light);
