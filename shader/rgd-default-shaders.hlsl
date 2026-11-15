/*
** rgd-default-shaders.hlsl
** Copyright (C) 2026 The mkxp-z authors
** Licensed under CC0 (https://creativecommons.org/publicdomain/zero/1.0/)
*/

struct PS_INPUT
{
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
};

struct Attributes
{
    float4 position : POSITION0;
    PS_INPUT ps_input;
};

Attributes vs_main(Attributes attributes)
{
    return attributes;
}

PS_OUTPUT ps_main(PS_INPUT ps_input)
{
    return GetOutput(tex2D(spriteSampler, ps_input.texcoord));
}
