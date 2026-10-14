#include "LightBuffer.h"

struct Lighting {
    float3 diffuse;
    float3 specular;
};

Lighting GetLight(
    LightDesc light,
    float3 position,
    float3 cameraPosition,
    float3 normal,
    float shininess
) {
    Lighting lighting;
    lighting.diffuse = 0.f;
    lighting.specular = 0.f;

    if (light.diffuseColorAndPower.w <= 0.f && light.specularColorAndPower.w <= 0.f) {
        return lighting;
    }

    // Point, spot: w is 1, the usual vector from the surface to the light.
    // Directional: w is 0, the surface position drops out
    float3 toLight = light.position.xyz - position * light.position.w;
    float lightDist = length(toLight); // 1 for directional lights
    float3 lightDir = toLight / lightDist;

    // Point, spot: 1 / d^2 never reaches zero, the window makes the light 0 beyond its range
    //              (d / range)^4 keeps it near 1 for most of the range.
    // Directional: 1 / range is 0, the window stays 1
    float invRange = light.params.y;
    float rangeFraction = lightDist * invRange;
    rangeFraction *= rangeFraction; // rangeFraction^2
    rangeFraction *= rangeFraction; // rangeFraction^4
    float window = saturate(1.f - rangeFraction);

    // Point, spot: inverse square falloff, capped at 1 next to the light.
    // Directional: the distance is 1, no falloff
    float attenuation = saturate(1.f / (lightDist * lightDist)) * window * window;

    // Cos of the angle between the cone axis and the ray from the light to the point
    float cosToAxis = dot(light.direction.xyz, -lightDir);

    // Spot: scale and offset map it linearly, cos(outer angle) -> 0, cos(inner angle) -> 1
    // Point, directional: scale 0 and offset 1 give 1 for any angle
    float spotScale = light.params.z;
    float spotOffset = light.params.w;
    float spot = saturate(cosToAxis * spotScale + spotOffset);
    attenuation *= spot * spot; // squared for a smooth outer edge

    if (attenuation <= 0.f) {
        return lighting;
    }

    // diffuse part
    float NdotL = dot(normal, lightDir);
    float diffuseIntensity = max(NdotL, 0.f);
    diffuseIntensity *= attenuation * light.diffuseColorAndPower.w;
    lighting.diffuse = diffuseIntensity * light.diffuseColorAndPower.xyz;

    if (shininess <= 0.f || light.specularColorAndPower.w <= 0.f || NdotL <= 0.f) {
        return lighting;
    }

    // specular blinn
    float3 viewDir = normalize(cameraPosition - position);
    float3 H = normalize(lightDir + viewDir);
    float NdotH = dot(normal, H);
    float specularIntensity = pow(max(NdotH, 0.f), shininess);

    specularIntensity *= attenuation * light.specularColorAndPower.w;
    lighting.specular = specularIntensity * light.specularColorAndPower.xyz;

    return lighting;
}
