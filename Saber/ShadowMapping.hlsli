#ifndef SHADOW_MAPPING_HLSLI
#define SHADOW_MAPPING_HLSLI

// Depth is reversed throughout the renderer: the projection swaps near and far,
// so 1 sits at the near plane, the map is cleared to 0, and the comparison sampler
// has to be GREATER_EQUAL. A bias moves the reference towards the light, which
// means adding to it rather than subtracting

// Places a point in the map. Returns false when it falls outside, which leaves the
// caller to treat it as lit: nothing was recorded there to occlude it
bool ProjectIntoShadowMap(
    matrix shadowViewProj,
    float normalOffset,
    float3 worldPos,
    float3 normal,
    out float3 shadowUvz
)
{
    shadowUvz = float3(0.f, 0.f, 0.f);

    // Offsetting along the normal is what actually lifts a surface off its own
    // shadow. A depth bias alone either leaves acne on slopes or, once big enough
    // to clear them, detaches contact shadows. The offset comes in already scaled
    // to the map's texel size
    float4 lightClip = mul(shadowViewProj, float4(worldPos + normal * normalOffset, 1.f));
    if (lightClip.w <= 0.f)
    {
        return false;
    }

    float3 lightNdc = lightClip.xyz / lightClip.w;
    shadowUvz = float3(lightNdc.xy * float2(.5f, -.5f) + .5f, lightNdc.z);

    // z above 1 is allowed through: that is a caster nearer to the light than the
    // near plane, and the comparison below reports it lit, which is right
    return all(saturate(shadowUvz.xy) == shadowUvz.xy) && shadowUvz.z > 0.f;
}

float SampleShadowPCF(
    Texture2D<float> shadowMap,
    SamplerComparisonState shadowSampler,
    float3 shadowUvz,
    float depthBias,
    float texelSize,
    int pcfRadius
)
{
    const float reference = shadowUvz.z + depthBias;

    // The comparison sampler filters 2x2 per tap and neighbouring taps overlap, so
    // (2r + 1) taps per axis reach (2r + 2) texels: radius 1 covers 4x4
    float visibility = 0.f;
    for (int y = -pcfRadius; y <= pcfRadius; ++y)
    {
        for (int x = -pcfRadius; x <= pcfRadius; ++x)
        {
            visibility += shadowMap.SampleCmpLevelZero(
                shadowSampler,
                shadowUvz.xy + float2(x, y) * texelSize,
                reference
            );
        }
    }

    const float tapCount = (2 * pcfRadius + 1) * (2 * pcfRadius + 1);
    return visibility / tapCount;
}

#endif  // SHADOW_MAPPING_HLSLI
