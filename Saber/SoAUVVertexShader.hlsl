struct SceneBuffer
{
    matrix vpMatrix;
    float4 cameraPosition;
};

ConstantBuffer<SceneBuffer> SceneCB : register(b0);

struct ModelBuffer
{
    matrix modelMatrix;
    matrix normalMatrix;
};

ConstantBuffer<ModelBuffer> ModelCB : register(b2);

struct VSOutput
{
    float3 worldPos : POSITION;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD;
    float4 position : SV_Position;
};

VSOutput main(
    float3 position : POSITION,
    float3 norm : NORMAL,
    float2 uv : TEXCOORD
) {
    VSOutput vtxOut;
    
    float4 pos = float4(position.xyz, 1.f);
    vtxOut.worldPos = mul(ModelCB.modelMatrix, pos);
    
    vtxOut.norm = mul(ModelCB.normalMatrix, float4(norm.xyz, 0.f)).xyz;
    
    vtxOut.uv = uv;
    
    pos = float4(vtxOut.worldPos, 1.f);
    vtxOut.position = mul(SceneCB.vpMatrix, pos);
    
    return vtxOut;
}
