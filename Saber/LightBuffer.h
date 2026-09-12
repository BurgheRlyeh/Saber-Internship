#ifndef LIGHT_BUFFER_H
#define LIGHT_BUFFER_H

#include "HlslTypesDef.h"

#define LIGHTS_MAX_COUNT 10

enum class LightType : uint {
    Point,
    Directional,
    Spot,

    Count
};

struct Light {
    float4 position;    // point, spot
    float4 direction;   // directional, spot

    float4 diffuseColorAndPower;
    float4 specularColorAndPower;

    float4 cone;        // x - range, y - cos(inner angle), z - cos(outer angle)

    uint4 type;         // x - LightType
};

struct LightBuffer {
    float4 ambientColorAndPower;
    uint4 lightsCount;
    Light lights[LIGHTS_MAX_COUNT];

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
