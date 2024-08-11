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
    float4 color : COLOR;
};

struct VSOutput
{
    float4 color : COLOR;
    float4 position : SV_Position;
};

VSOutput main(VSInput vtxIn)
{
    VSOutput vtxOut;
    
    vtxOut.color = vtxIn.color;
    
    matrix mvpMatrix = mul(SceneCB.vpMatrix, ModelCB.modelMatrix);
    float4 position = float4(vtxIn.position.x, vtxIn.position.y, vtxIn.position.z, 1.f);
    vtxOut.position = mul(mvpMatrix, position);
    
    return vtxOut;
}