struct SceneBuffer
{
    matrix vpMatrix;
    matrix invViewProjMatrix;
    matrix invViewMatrix;
    matrix invProjMatrix;
    float4 cameraPosition;
    float4 nearFar;
};

ConstantBuffer<SceneBuffer> SceneCB : register(b0);

struct ModelBuffer
{
    matrix modelMatrix;
    matrix normalMatrix;
    uint4 materialId;
};

ConstantBuffer<ModelBuffer> ModelCB : register(b1);

struct VSInput
{
    float3 position : POSITION;
    float3 norm : NORMAL;
    float4 tang : TANGENT;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float3 worldPos : POSITION;
    float3 norm : NORMAL;
    float4 tang : TANGENT;
    float2 uv : TEXCOORD;
    float4 position : SV_Position;
};

VSOutput main(VSInput vtxIn)
{
    VSOutput vtxOut;
    
    float4 position = float4(vtxIn.position.xyz, 1.f);
    vtxOut.worldPos = mul(ModelCB.modelMatrix, position).xyz;
    
    vtxOut.norm = mul(ModelCB.normalMatrix, float4(vtxIn.norm.xyz, 0.f)).xyz;
    vtxOut.tang.xyz = mul(ModelCB.normalMatrix, float4(vtxIn.tang.xyz, 0.f)).xyz;
    vtxOut.tang.w = vtxIn.tang.w;
    vtxOut.uv = vtxIn.uv;
    
    position = float4(vtxOut.worldPos, 1.f);
    vtxOut.position = mul(SceneCB.vpMatrix, position);
    
    return vtxOut;
}