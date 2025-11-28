//#include "SceneBuffer.h"

////ConstantBuffer<SceneBuffer> SceneCB : register(b0);

//RWByteAddressBuffer VisibilityBuffer : register(u0);

//#define BITS_PER_WORD 32

//bool ReadBit(RWByteAddressBuffer buffer, uint bitIndex)
//{
//    uint byteOffset = bitIndex / BITS_PER_WORD;
//    uint bitOffset = bitIndex % BITS_PER_WORD;
    
//    uint word = buffer.Load(byteOffset * 4);
//    return (word & (1u << bitOffset));
//}
//void SetBit(RWByteAddressBuffer buffer, uint bitIndex, bool value)
//{
//    uint byteOffset = bitIndex / BITS_PER_WORD;
//    uint bitOffset = bitIndex % BITS_PER_WORD;

//    uint mask = 1u << bitOffset;
//    uint original_value;
//    if (value)
//    {
//        buffer.InterlockedOr(byteOffset * 4, mask, original_value);
//    }
//    else
//    {
//        buffer.InterlockedAnd(byteOffset * 4, ~mask, original_value);
//    }
//}

//bool IsAABBInFrustum(float4 bbmin, float4 bbmax) {
//    for (uint i = 0; i < 6; ++i) {
//        float4 p = float4(
//			SceneCB.viewFrustumPlanes[i].x < 0.f ? bbmin.x : bbmax.x,
//			SceneCB.viewFrustumPlanes[i].y < 0.f ? bbmin.y : bbmax.y,
//			SceneCB.viewFrustumPlanes[i].z < 0.f ? bbmin.z : bbmax.z,
//			1.f
//		);
//        if (dot(p, SceneCB.viewFrustumPlanes[i]) < 0.f) {
//            return false;
//        }
//    }
//    return true;
//}

struct ComputeShaderInput
{
    uint3 GroupID : SV_GroupID; // 3D index of the thread group in the dispatch.
    uint3 GroupThreadID : SV_GroupThreadID; // 3D index of local thread ID in a thread group.
    uint3 DispatchThreadID : SV_DispatchThreadID; // 3D index of global thread ID in the dispatch.
    uint GroupIndex : SV_GroupIndex; // Flattened local index of the thread within a thread group.
};
#define BLOCK_SIZE 128
[numthreads(BLOCK_SIZE, 1, 1)]
void main(ComputeShaderInput IN)
{

}