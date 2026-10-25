#pragma once

#include "Headers.h"

#include "Camera.h"
#include "LightBuffer.h"
#include "LightShadow.h"

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

	// 1 view for directional and spot, 6 for a point light
	virtual size_t GetShadowViewCount() const = 0;

	// Built from the light's placement and the shadow's settings
	virtual StaticCamera GetShadowCamera(const LightShadow& shadow, size_t viewIndex) const = 0;

	std::shared_ptr<LightShadow> GetShadow() { return m_pShadow; }
	std::shared_ptr<const LightShadow> GetShadow() const { return m_pShadow; }

	void EnableShadow(bool enable);

	LightDesc GetLightDesc() const;

protected:
	virtual void FillLightDesc(LightDesc& light) const = 0;

	virtual std::shared_ptr<LightShadow> MakeShadow() const;

private:
	Settings m_settings{};
	std::shared_ptr<LightShadow> m_pShadow{};
};

// UI
bool DrawSettings(LightSource::Settings& settings);

// Position and direction fields for whatever the light supports
bool DrawPlacement(LightSource& light);

// Shadow switch, then the shadow's own settings when it is on
bool DrawShadowSettings(LightSource& light);

bool DrawSettings(LightSource& light);
