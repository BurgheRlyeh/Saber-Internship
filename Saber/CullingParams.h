#ifndef CULLING_PARAMS_H
#define CULLING_PARAMS_H

#include "HlslTypesDef.h"

#define CULLING_GROUP_SIZE 128

#define CULLING_FLAG_FRUSTUM     (1 << 0)
#define CULLING_FLAG_OCCLUSION   (1 << 1)
// The second pass tests everything, draws what the first pass has not and leaves
// the visibility for the next frame behind
#define CULLING_FLAG_SECOND_PASS (1 << 2)

// Kept in step with Scene::BoundingVolumeType
#define BOUNDING_VOLUME_AABB   0
#define BOUNDING_VOLUME_SPHERE 1

// Filled by both passes, one block per render subsystem. The second pass tests
// every object, so its numbers describe the whole frame
struct CullingStats {
    uint drawnFirstPass;
    uint frustumCulledFirstPass;

    uint drawnSecondPass;
    uint frustumCulledSecondPass;
    uint occlusionCulledSecondPass;
    uint visible;

    uint padding0;
    uint padding1;
};

struct CullingParams {
    uint objectCount;
    uint flags;
    uint boundingVolume;
    uint hzbSize;
};

#include "HlslTypesUndef.h"

#endif  // CULLING_PARAMS_H
