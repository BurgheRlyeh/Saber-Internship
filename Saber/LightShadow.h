#pragma once

#include "Headers.h"

#include <DirectXMath.h>

class CommandList;
class DepthBuffer;
class DeviceContext;

// The part of a depth buffer a light renders its shadow into
struct ShadowMapRegion {
	std::shared_ptr<DepthBuffer> pMap{};
	UINT firstSlice{};

	UINT x{};
	UINT y{};
	UINT size{};

	bool IsValid() const { return pMap != nullptr; }
	UINT GetSize() const;
};

class LightShadow {
public:
	struct Settings {
		UINT resolution{ 1024 };
		float nearPlane{ .1f };

		// In texels of the map
		float depthBias{ 2.f };
		float normalOffset{ 1.5f };
		int pcfRadius{ 1 };
	};

	explicit LightShadow(UINT16 viewCount);
	virtual ~LightShadow() = default;

	Settings& GetSettings() { return m_settings; }
	const Settings& GetSettings() const { return m_settings; }

	UINT16 GetViewCount() const { return m_viewCount; }

	// A map of its own with a slice per view
	void CreateMap(std::shared_ptr<DeviceContext> pDeviceContext, const std::wstring& name);

	// Renders into a region of a map someone else owns: another map or an atlas
	void SetMap(const ShadowMapRegion& region);

	void ResetMap();

	const ShadowMapRegion& GetMap() const { return m_map; }
	bool OwnsMap() const { return m_ownsMap; }

	// Once per frame: a map of its own follows the resolution setting
	void Update(
		std::shared_ptr<DeviceContext> pDeviceContext,
		std::shared_ptr<CommandList> pCommandList
	);

private:
	UINT16 m_viewCount{};
	Settings m_settings{};

	ShadowMapRegion m_map{};
	bool m_ownsMap{};
};

// The sun's shadow box: which part of the world its map covers
class DirectionalShadow : public LightShadow {
public:
	struct Box {
		DirectX::XMFLOAT3 poi{};
		float distance{ 50.f };		// from the poi back towards the light
		float extent{ 30.f };		// side of the square, world units
		float depth{ 100.f };		// far plane
	};

	DirectionalShadow() : LightShadow(1) {}

	Box& GetBox() { return m_box; }
	const Box& GetBox() const { return m_box; }

private:
	Box m_box{};
};

// UI: settings, the sun's box and a preview of the map
bool DrawSettings(LightShadow& shadow);
