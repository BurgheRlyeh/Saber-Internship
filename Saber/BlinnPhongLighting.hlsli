struct Light
{
    float4 position;
    float4 diffuseColorAndPower;
    float4 specularColorAndPower;
};

struct Lighting
{
    float3 diffuse;
    float3 specular;
};

Lighting GetPointLight(
    Light light,
    float3 position,
    float3 cameraPosition,
    float3 normal,
    float shininess
)
{
    Lighting lighting;
    lighting.diffuse = 0.f;
    lighting.specular = 0.f;
    
    if (light.diffuseColorAndPower.w <= 0.f)
    {
        return lighting;
    }
    
    float3 lightDir = light.position.xyz - position;
    float lightDist = length(lightDir);
    lightDir = lightDir / lightDist;
    
    float attenuation = saturate(1.f / (lightDist * lightDist));
    
    // diffuse part
    {
        float NdotL = dot(normal, lightDir);
        float diffuseIntensity = saturate(NdotL);
        
        lighting.diffuse = diffuseIntensity * attenuation * light.diffuseColorAndPower.xyz * light.diffuseColorAndPower.w;
    }
    
    if (shininess <= 0.f || light.specularColorAndPower.w <= 0.f)
    {
        return lighting;
    }

    
    float3 viewDir = normalize(position - cameraPosition);
    float specularIntensity;
    
    // specular blinn
    {
        float3 H = normalize(lightDir + viewDir);
        float NdotH = dot(normal, H);
        specularIntensity = pow(saturate(NdotH), shininess);
    }
    
    lighting.specular = specularIntensity * attenuation * light.specularColorAndPower.xyz * light.specularColorAndPower.w;

    return lighting;
}
