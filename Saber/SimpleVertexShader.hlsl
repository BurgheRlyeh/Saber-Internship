struct MVP
{
    matrix mvp;
};

ConstantBuffer<MVP> MVPCB : register(b0);

struct VSInput
{
    float4 position : POSITION;
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
    vtxOut.position = mul(MVPCB.mvp, vtxIn.position);
    
    return vtxOut;
}