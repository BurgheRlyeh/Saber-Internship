struct SceneBuffer
{
    matrix vpMatrix;
};

ConstantBuffer<SceneBuffer> SceneCB : register(b0);

struct ModelBuffer
{
    matrix modelMatrix;
};

ConstantBuffer<ModelBuffer> ModelCB : register(b1);

struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float2 uv : TEXCOORD;
    float4 position : SV_Position;
};

VSOutput main(float3 position : POSITION, float2 uv : TEXCOORD)
{
    VSOutput vtxOut;
    
    vtxOut.uv = uv;
    
    matrix mvpMatrix = mul(SceneCB.vpMatrix, ModelCB.modelMatrix);
    float4 pos = float4(position.x, position.y, position.z, 1.f);
    vtxOut.position = mul(mvpMatrix, pos);
    
    return vtxOut;
}