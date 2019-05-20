//=================================================================================================
#if defined(VS_0) || defined(PS_0)

#define k_rsi "RootFlags(0)"

//-------------------------------------------------------------------------------------------------
#if defined(VS_0)

[RootSignature(k_rsi)]
float4 vertex_main(uint vertex_id : SV_VertexID) : SV_Position
{
    float2 positions[3] = { float2(-1.0f, -1.0f), float2(0.0f, 1.0f), float2(1.0f, -1.0f) };
    return float4(0.7f * positions[vertex_id], 0.0f, 1.0f);
}
//-------------------------------------------------------------------------------------------------
#elif defined(PS_0)

[RootSignature(k_rsi)]
float4 pixel_main(float4 position : SV_Position) : SV_Target0
{
    return float4(0.0f, 0.5f, 0.0f, 0.0f);
}
//=================================================================================================
#endif
#elif defined(VS_1) || defined(PS_1)

//=================================================================================================
#endif
