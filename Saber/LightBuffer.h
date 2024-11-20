#ifndef LIGHT_BUFFER_H
#define LIGHT_BUFFER_H

#include "HlslCppTypesRedefine.h"

#define LIGHTS_MAX_COUNT 10

struct Light {
    float4 position;
    float4 diffuseColorAndPower;
    float4 specularColorAndPower;
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

    bool Add(
        const DirectX::XMFLOAT4& position,
        const DirectX::XMFLOAT3& diffuseColor,
        const float& diffusePower,
        const DirectX::XMFLOAT3& specularColor,
        const float& specularPower
    ) {
        if (lightsCount.x == LIGHTS_MAX_COUNT) {
            return false;
        }

        lights[lightsCount.x++] = {
            position,
            {
                diffuseColor.x,
                diffuseColor.y,
                diffuseColor.z,
                diffusePower
            },
            {
                specularColor.x,
                specularColor.y,
                specularColor.z,
                specularPower
            }
        };
        return true;
    }
#endif
};

#endif  // LIGHT_BUFFER_H
