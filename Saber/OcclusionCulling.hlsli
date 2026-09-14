#ifndef OCCLUSION_CULLING_HLSLI
#define OCCLUSION_CULLING_HLSLI

#include "CameraBuffer.h"
#include "CullingParams.h"
#include "IndirectCommand.h"
#include "ModelBuffer.h"

ConstantBuffer<CullingParams> CullingCB : register(b0);
ConstantBuffer<CameraBuffer> CameraCB : register(b1);

StructuredBuffer<ModelBuffer> ModelBuffers : register(t0);
StructuredBuffer<CommandType> SourceCommands : register(t1);
Texture2D<float> HiZBuffer : register(t2);

RWStructuredBuffer<CommandType> CulledCommands : register(u0);
RWStructuredBuffer<uint> CulledCommandsCount : register(u1);
RWStructuredBuffer<uint> Visibility : register(u2);

// One block, bound at the offset of this subsystem
RWStructuredBuffer<CullingStats> Stats : register(u3);

struct ComputeShaderInput
{
    uint3 GroupID : SV_GroupID; // 3D index of the thread group in the dispatch.
    uint3 GroupThreadID : SV_GroupThreadID; // 3D index of local thread ID in a thread group.
    uint3 DispatchThreadID : SV_DispatchThreadID; // 3D index of global thread ID in the dispatch.
    uint GroupIndex : SV_GroupIndex; // Flattened local index of the thread within a thread group.
};

// The footprint of a bounding volume on screen, plus the depth of its closest
// point. Reverse-Z, so the closest point is the largest depth
struct ScreenBounds
{
    float2 uvMin;
    float2 uvMax;
    float nearestDepth;
};

// The projection is reverse-Z and right handed, so view space z is negative in
// front of the camera. The constant buffer holds the matrix transposed, which is
// why the third column is read as the third row
float ViewDepthToNdc(float viewZ)
{
    return (CameraCB.projMatrix[2][2] * viewZ + CameraCB.projMatrix[2][3]) / -viewZ;
}

float2 NdcToUv(float2 ndc)
{
    return float2(.5f + .5f * ndc.x, .5f - .5f * ndc.y);
}

bool IsAabbInFrustum(float3 bbMin, float3 bbMax)
{
    for (uint i = 0; i < 6; ++i)
    {
        float4 plane = CameraCB.viewFrustumPlanes[i];

        // The corner deepest into the negative half space decides for the whole box
        float3 corner = float3(
            plane.x < 0.f ? bbMin.x : bbMax.x,
            plane.y < 0.f ? bbMin.y : bbMax.y,
            plane.z < 0.f ? bbMin.z : bbMax.z
        );
        if (dot(float4(corner, 1.f), plane) < 0.f)
        {
            return false;
        }
    }

    return true;
}

bool IsSphereInFrustum(float3 center, float radius)
{
    for (uint i = 0; i < 6; ++i)
    {
        if (dot(float4(center, 1.f), CameraCB.viewFrustumPlanes[i]) + radius < 0.f)
        {
            return false;
        }
    }

    return true;
}

void GetWorldAabbCorners(ModelBuffer model, out float3 corners[8])
{
    for (uint i = 0; i < 8; ++i)
    {
        float3 corner = float3(
            (i & 1) ? model.bbmax.x : model.bbmin.x,
            (i & 2) ? model.bbmax.y : model.bbmin.y,
            (i & 4) ? model.bbmax.z : model.bbmin.z
        );
        corners[i] = mul(model.modelMatrix, float4(corner, 1.f)).xyz;
    }
}

// World space bounding sphere of a model. A non-uniform scale inflates it, which is
// what the AABB volume is there for
void GetWorldSphere(ModelBuffer model, out float3 center, out float radius)
{
    center = mul(model.modelMatrix, float4(model.boundingSphere.xyz, 1.f)).xyz;

    float3 scale = float3(
        length(mul(model.modelMatrix, float4(1.f, 0.f, 0.f, 0.f)).xyz),
        length(mul(model.modelMatrix, float4(0.f, 1.f, 0.f, 0.f)).xyz),
        length(mul(model.modelMatrix, float4(0.f, 0.f, 1.f, 0.f)).xyz)
    );
    radius = model.boundingSphere.w * max(scale.x, max(scale.y, scale.z));
}

// Fails when the box crosses the near plane: the projection of a corner behind the
// camera says nothing, and the caller has to treat the object as visible
bool TryGetAabbScreenBounds(float3 corners[8], out ScreenBounds bounds)
{
    bounds.uvMin = float2(1.f, 1.f);
    bounds.uvMax = float2(0.f, 0.f);
    bounds.nearestDepth = 0.f;

    for (uint i = 0; i < 8; ++i)
    {
        float4 clip = mul(CameraCB.viewProjMatrix, float4(corners[i], 1.f));
        if (clip.w <= 0.f)
        {
            return false;
        }

        float2 uv = NdcToUv(clip.xy / clip.w);
        bounds.uvMin = min(bounds.uvMin, uv);
        bounds.uvMax = max(bounds.uvMax, uv);

        // Monotonic in view space depth, so the closest corner gives the closest point
        bounds.nearestDepth = max(bounds.nearestDepth, clip.z / clip.w);
    }

    return true;
}

// 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere
// https://jcgt.org/published/0002/02/05/
bool TryGetSphereScreenBounds(float3 viewCenter, float radius, out ScreenBounds bounds)
{
    bounds.uvMin = float2(0.f, 0.f);
    bounds.uvMax = float2(1.f, 1.f);
    bounds.nearestDepth = 0.f;

    float nearPlane = CameraCB.nearFar.x;
    if (-viewCenter.z < radius + nearPlane)
    {
        return false;
    }

    float p00 = CameraCB.projMatrix[0][0];
    float p11 = CameraCB.projMatrix[1][1];

    // Both tangent lines from the camera to the circle, in the plane of that axis.
    // The center is negated so that the second component grows away from the camera
    float2 centerXZ = float2(-viewCenter.x, -viewCenter.z);
    float2 tangentX = float2(sqrt(dot(centerXZ, centerXZ) - radius * radius), radius);
    float2 minX = float2(
        tangentX.x * centerXZ.x - tangentX.y * centerXZ.y,
        tangentX.y * centerXZ.x + tangentX.x * centerXZ.y
    );
    float2 maxX = float2(
        tangentX.x * centerXZ.x + tangentX.y * centerXZ.y,
        -tangentX.y * centerXZ.x + tangentX.x * centerXZ.y
    );

    float2 centerYZ = float2(-viewCenter.y, -viewCenter.z);
    float2 tangentY = float2(sqrt(dot(centerYZ, centerYZ) - radius * radius), radius);
    float2 minY = float2(
        tangentY.x * centerYZ.x - tangentY.y * centerYZ.y,
        tangentY.y * centerYZ.x + tangentY.x * centerYZ.y
    );
    float2 maxY = float2(
        tangentY.x * centerYZ.x + tangentY.y * centerYZ.y,
        -tangentY.y * centerYZ.x + tangentY.x * centerYZ.y
    );

    // Negated back into clip space
    float2 ndcX = float2(-minX.x / minX.y, -maxX.x / maxX.y) * p00;
    float2 ndcY = float2(-minY.x / minY.y, -maxY.x / maxY.y) * p11;

    float2 uv0 = NdcToUv(float2(ndcX.x, ndcY.x));
    float2 uv1 = NdcToUv(float2(ndcX.y, ndcY.y));

    bounds.uvMin = min(uv0, uv1);
    bounds.uvMax = max(uv0, uv1);
    bounds.nearestDepth = ViewDepthToNdc(viewCenter.z + radius);

    return true;
}

// One sample would need a min reduction sampler, which is not guaranteed here, so
// the mip is picked to make the footprint fit into two texels per axis and its
// corners are read directly
bool IsOccluded(ScreenBounds bounds)
{
    float2 uvMin = saturate(bounds.uvMin);
    float2 uvMax = saturate(bounds.uvMax);

    float hzbSize = CullingCB.hzbSize;
    float2 sizeInTexels = (uvMax - uvMin) * hzbSize;

    int maxMip = (int)log2(hzbSize);
    int mip = clamp((int)floor(log2(max(max(sizeInTexels.x, sizeInTexels.y), 1.f))), 0, maxMip);

    int mipSize = max(1, (int)hzbSize >> mip);
    int2 texelMin = clamp((int2)(uvMin * mipSize), 0, mipSize - 1);
    int2 texelMax = clamp((int2)(uvMax * mipSize), 0, mipSize - 1);

    // The pyramid keeps the minimum, which under reverse-Z is the farthest occluder
    float occluderDepth = min(
        min(HiZBuffer.Load(int3(texelMin.x, texelMin.y, mip)),
            HiZBuffer.Load(int3(texelMax.x, texelMin.y, mip))),
        min(HiZBuffer.Load(int3(texelMin.x, texelMax.y, mip)),
            HiZBuffer.Load(int3(texelMax.x, texelMax.y, mip)))
    );

    return occluderDepth >= bounds.nearestDepth;
}

void CullObjects(ComputeShaderInput IN)
{
    uint index = IN.DispatchThreadID.x;
    if (index >= CullingCB.objectCount)
    {
        return;
    }

    bool isSecondPass = (CullingCB.flags & CULLING_FLAG_SECOND_PASS) != 0;
    bool wasVisible = Visibility[index] != 0;

    // The first pass redraws whatever the previous frame ended up with, and that is
    // what builds the depth the second pass tests against
    bool visible = isSecondPass || wasVisible;

    ModelBuffer model = ModelBuffers[SourceCommands[index].rootConstant.x];

    bool useAabb = CullingCB.boundingVolume == BOUNDING_VOLUME_AABB;

    float3 corners[8];
    float3 sphereCenter = float3(0.f, 0.f, 0.f);
    float sphereRadius = 0.f;

    float3 bbMin = float3(0.f, 0.f, 0.f);
    float3 bbMax = float3(0.f, 0.f, 0.f);

    if (useAabb)
    {
        GetWorldAabbCorners(model, corners);

        bbMin = corners[0];
        bbMax = corners[0];
        for (uint i = 1; i < 8; ++i)
        {
            bbMin = min(bbMin, corners[i]);
            bbMax = max(bbMax, corners[i]);
        }
    }
    else
    {
        GetWorldSphere(model, sphereCenter, sphereRadius);
    }

    if (visible && (CullingCB.flags & CULLING_FLAG_FRUSTUM))
    {
        visible = useAabb
            ? IsAabbInFrustum(bbMin, bbMax)
            : IsSphereInFrustum(sphereCenter, sphereRadius);

        if (!visible)
        {
            if (isSecondPass)
            {
                InterlockedAdd(Stats[0].frustumCulledSecondPass, 1);
            }
            else
            {
                InterlockedAdd(Stats[0].frustumCulledFirstPass, 1);
            }
        }
    }

    // Only the second pass has a pyramid to test against, the first one is what
    // fills it
    if (visible && isSecondPass && (CullingCB.flags & CULLING_FLAG_OCCLUSION))
    {
        ScreenBounds bounds;
        bool hasBounds = useAabb
            ? TryGetAabbScreenBounds(corners, bounds)
            : TryGetSphereScreenBounds(mul(CameraCB.viewMatrix, float4(sphereCenter, 1.f)).xyz, sphereRadius, bounds);

        if (hasBounds && IsOccluded(bounds))
        {
            visible = false;
            InterlockedAdd(Stats[0].occlusionCulledSecondPass, 1);
        }
    }

    // The second pass only adds what the first one has not drawn yet
    bool draw = isSecondPass ? (visible && !wasVisible) : visible;
    if (draw)
    {
        uint commandId;
        InterlockedAdd(CulledCommandsCount[0], 1, commandId);
        CulledCommands[commandId] = SourceCommands[index];

        if (isSecondPass)
        {
            InterlockedAdd(Stats[0].drawnSecondPass, 1);
        }
        else
        {
            InterlockedAdd(Stats[0].drawnFirstPass, 1);
        }
    }

    if (isSecondPass)
    {
        Visibility[index] = visible ? 1 : 0;

        if (visible)
        {
            InterlockedAdd(Stats[0].visible, 1);
        }
    }
}

#endif  // OCCLUSION_CULLING_HLSLI
