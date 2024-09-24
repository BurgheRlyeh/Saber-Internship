#ifndef MATERIAL_CB_H
#define MATERIAL_CB_H

#include "HlslCppTypesRedefine.h"

#define MaterialCB_SIZE 1024

struct MaterialCB
{
    // x - albedoId, y - normalId
    uint4 materials[MaterialCB_SIZE];
};
#endif  // MATERIAL_CB_H
