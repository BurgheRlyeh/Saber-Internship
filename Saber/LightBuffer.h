#ifndef LIGHT_BUFFER_H
#define LIGHT_BUFFER_H

#include "HlslTypesDef.h"

#define LIGHTS_MAX_COUNT 10

// Per scene, across all lights
#define SHADOW_VIEWS_MAX_COUNT 16

enum class LightType : uint {
    Point,
    Directional,
    Spot,

    Count
};

struct LightDesc {
    float4 position;    // w = 1: xyz - position; w = 0: xyz - direction to the light
    float4 direction;   // xyz - where the light points, normalized

    float4 diffuseColorAndPower;
    float4 specularColorAndPower;

    float4 params;      // x - LightType, y - 1 / range, z - spot scale, w - spot offset
};

struct LightBuffer {
    float4 ambientColorAndPower;
    uint4 lightsCount;
    LightDesc lights[LIGHTS_MAX_COUNT];

#ifdef __cplusplus
    void SetAmbientLight(
        const DirectX::XMFLOAT3& color,
        const float& power
    ) {
        ambientColorAndPower = {
	        color.x,
	        color.y,
	        color.z,
            power
        };
    }
#endif
};

#include "HlslTypesUndef.h"

#ifdef __cplusplus
const char* LightTypeName(LightType type);

// UI
bool DrawSettings(LightBuffer& lightBuffer);
#endif  // __cplusplus

#endif  // LIGHT_BUFFER_H
