#ifndef MATERIAL_CB_H
#define MATERIAL_CB_H

#include "HlslTypesDef.h"

#define MaterialCB_SIZE 1024

struct Material
{
    uint4 textureIds;   // x - albedoId, y - normalId
    float4 phong;       // x - ambient, y - diffuse, z - specular, w - shininess
};

struct MaterialCB
{
    Material materials[MaterialCB_SIZE];
};

#include "HlslTypesUndef.h"

#ifdef __cplusplus
struct PhongParams {
    float ambient{ 1.f };
    float diffuse{ 1.f };
    float specular{ 0.35f };

    float shininess{ 16.f };

    bool operator==(const PhongParams& other) const = default;
};
#endif  // __cplusplus

#endif  // MATERIAL_CB_H
