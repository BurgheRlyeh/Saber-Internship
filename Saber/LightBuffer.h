/**
 * @file LightBuffer.h
 * @brief Defines the GPU-visible light constant buffer structures shared between
 *        C++ and HLSL shader code.
 */
#ifndef LIGHT_BUFFER_H
#define LIGHT_BUFFER_H

#include "HlslTypesDef.h"

/** @brief Maximum number of dynamic point lights supported per scene. */
#define LIGHTS_MAX_COUNT 10

/**
 * @brief Per-light data stored in the GPU constant buffer.
 */
struct Light {
    float4 position;               /**< @brief World-space light position (xyz) and unused w. */
    float4 diffuseColorAndPower;   /**< @brief Diffuse light color (xyz) and intensity (w). */
    float4 specularColorAndPower;  /**< @brief Specular light color (xyz) and intensity (w). */
};

/**
 * @brief Scene-wide lighting constant buffer holding ambient and point light data.
 *
 * Uploaded to the GPU once per frame and bound as a constant buffer in shaders.
 */
struct LightBuffer {
    float4 ambientColorAndPower;        /**< @brief Ambient color (xyz) and intensity (w). */
    uint4  lightsCount;                 /**< @brief Number of active lights in x; yzw unused. */
    Light  lights[LIGHTS_MAX_COUNT];    /**< @brief Array of active point lights. */

#ifdef __cplusplus
    /**
     * @brief Sets the ambient light color and intensity.
     * @param color RGB ambient color.
     * @param power Ambient light intensity.
     */
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

    /**
     * @brief Adds a point light to the buffer.
     * @param position      World-space position (xyzw).
     * @param diffuseColor  Diffuse RGB color.
     * @param diffusePower  Diffuse intensity.
     * @param specularColor Specular RGB color.
     * @param specularPower Specular intensity.
     * @return @c true if the light was added; @c false if the buffer is full.
     */
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

#include "HlslTypesUndef.h"

#endif  // LIGHT_BUFFER_H
