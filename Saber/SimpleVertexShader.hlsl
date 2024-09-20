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

ConstantBuffer<ModelBuffer> ModelCB : register(b1);

struct VSInput
{
    float3 position : POSITION;
    float3 norm : NORMAL;
    float4 color : COLOR;
};

struct VSOutput
{
    float3 norm : NORMAL;
    float4 color : COLOR;
    float4 position : SV_Position;
};

VSOutput main(VSInput vtxIn)
{
    VSOutput vtxOut;
    
    vtxOut.norm = mul(ModelCB.normalMatrix, float4(vtxIn.norm.xyz, 0.f)).xyz;
    
    vtxOut.color = vtxIn.color;
    
    matrix mvpMatrix = mul(SceneCB.vpMatrix, ModelCB.modelMatrix);
    float4 position = float4(vtxIn.position.xyz, 1.f);
    vtxOut.position = mul(mvpMatrix, position);
    
    return vtxOut;
}