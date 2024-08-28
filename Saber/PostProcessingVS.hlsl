struct VSOutput
{
    float2 uv : TEXCOORD;
    float4 pos : SV_Position;
};

VSOutput main(unsigned int vertexId : SV_VertexID)
{
    float4 pos;
    switch (vertexId)
    {
        case 0:
            pos = float4(-1.f, 1.f, 0.f, 1.f);
            break;
        case 1:
            pos = float4(3.f, 1.f, 0.f, 1.f);
            break;
        case 2:
            pos = float4(-1.f, -3.f, 0.f, 1.f);
            break;
    }
    
    VSOutput result;
    result.uv = float2(.5f * pos.x + .5f, .5f - .5f * pos.y);
    result.pos = pos;
    
    return result;
}