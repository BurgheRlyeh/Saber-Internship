#define LIGHTS_MAX_COUNT 10

struct SceneBuffer
{
    matrix vpMatrix;
    float4 cameraPosition;
};

ConstantBuffer<SceneBuffer> SceneCB : register(b0);

struct Light
{
    float4 position;
    float4 diffuseColorAndPower;
    float4 specularColorAndPower;
};

struct LightBuffer
{
    float4 ambientColorAndPower;
    uint4 lightCount;
    Light lights[LIGHTS_MAX_COUNT];
};

ConstantBuffer<LightBuffer> LightCB : register(b1);

struct ModelBuffer
{
    matrix modelMatrix;
    matrix normalMatrix;
};

ConstantBuffer<ModelBuffer> ModelCB : register(b2);

Texture2D t1 : register(t0);
SamplerState s1 : register(s0);

struct PSInput
{
    float3 worldPos : POSITION;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
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
    
    if (light.diffuseColorAndPower.w <= 0.f) {
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
    
    // specular phong
    //{
    //    float3 R = reflect(-lightDir, normal);
    //    float VdotR = dot(viewDir, R);
    //    specularIntensity = pow(saturate(VdotR), shininess);
    //}
    
    // specular blinn
    {
        float3 H = normalize(lightDir + viewDir);
        float NdotH = dot(normal, H);
        specularIntensity = pow(saturate(NdotH), shininess);
    }
    
    lighting.specular = specularIntensity * attenuation * light.specularColorAndPower.xyz * light.specularColorAndPower.w;

    return lighting;
}

float4 main(PSInput input) : SV_TARGET
{
    float3 lightColor = LightCB.ambientColorAndPower.xyz * LightCB.ambientColorAndPower.w;
    
    for (uint i = 0; i < LightCB.lightCount.x; ++i)
    {
        Lighting lighting = GetPointLight(
            LightCB.lights[i],
            input.worldPos,
            input.worldPos - SceneCB.cameraPosition.xyz,
            input.norm,
            10.f
        );

        lightColor += lighting.diffuse;
        lightColor += lighting.specular;
    }
    
    float3 finalColor = t1.Sample(s1, input.uv).xyz * lightColor;
    return float4(finalColor.xyz, 0.f);
}
