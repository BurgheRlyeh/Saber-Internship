#include "LightBuffer.h"

struct Lighting
{
    float3 diffuse;
    float3 specular;
};

Lighting GetLight(
    Light light,
    float3 position,
    float3 cameraPosition,
    float3 normal,
    float shininess
) {
    Lighting lighting;
    lighting.diffuse = 0.f;
    lighting.specular = 0.f;

    if (light.diffuseColorAndPower.w <= 0.f && light.specularColorAndPower.w <= 0.f)
    {
        return lighting;
    }

    float3 lightDir;
    float attenuation;

    if (light.type.x == (uint)LightType::Directional)
    {
        lightDir = normalize(-light.direction.xyz);
        attenuation = 1.f;
    }
    else
    {
        float3 toLight = light.position.xyz - position;
        float lightDist = length(toLight);
        lightDir = toLight / lightDist;

        float range = light.cone.x;
        if (lightDist >= range)
        {
            return lighting;
        }

        float window = saturate(1.f - pow(lightDist / range, 4.f));
        attenuation = saturate(1.f / (lightDist * lightDist)) * window * window;

        if (light.type.x == (uint)LightType::Spot)
        {
            // Angle between the cone axis and the direction back to the surface
            float cosAngle = dot(normalize(light.direction.xyz), -lightDir);
            attenuation *= smoothstep(light.cone.z, light.cone.y, cosAngle);
        }
    }

    if (attenuation <= 0.f)
    {
        return lighting;
    }

    float NdotL = dot(normal, lightDir);

    // diffuse part
    {
        float diffuseIntensity = max(NdotL, 0.f);
        diffuseIntensity *= attenuation * light.diffuseColorAndPower.w;
        lighting.diffuse = diffuseIntensity * light.diffuseColorAndPower.xyz;
    }

    if (shininess <= 0.f || light.specularColorAndPower.w <= 0.f || NdotL <= 0.f)
    {
        return lighting;
    }

    float3 viewDir = normalize(cameraPosition - position);
    float specularIntensity;

    // specular blinn
    {
        float3 H = normalize(lightDir + viewDir);
        float NdotH = dot(normal, H);
        specularIntensity = pow(max(NdotH, 0.f), shininess);
    }

    specularIntensity *= attenuation * light.specularColorAndPower.w;
    lighting.specular = specularIntensity * light.specularColorAndPower.xyz;

    return lighting;
}
