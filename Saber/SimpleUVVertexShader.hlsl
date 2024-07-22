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
    float4 position : POSITION;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float2 uv : TEXCOORD;
    float4 position : SV_Position;
};

VSOutput main(VSInput vtxIn)
{
    VSOutput vtxOut;
    
    //matrix mvpMatrix = mul(ModelCB.modelMatrix, SceneCB.vpMatrix);
    matrix mvpMatrix = mul(SceneCB.vpMatrix, ModelCB.modelMatrix);
    
    vtxOut.uv = vtxIn.uv;
    vtxOut.position = mul(mvpMatrix, vtxIn.position);
    
    return vtxOut;
}