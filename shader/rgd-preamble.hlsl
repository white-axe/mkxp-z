/*
** rgd-preamble.hlsl
** Copyright (C) 2026 The mkxp-z authors
** Licensed under CC0 (https://creativecommons.org/publicdomain/zero/1.0/)
*/

// This is the type that RGD pixel shader functions are supposed to return.
// Use the `GetOutput()` function to construct it. Don't construct it manually.
struct PS_OUTPUT
{
    float4 __SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED_color0 : COLOR0;
};
PS_OUTPUT GetOutput(float4 output_color)
{
    PS_OUTPUT output = {
        output_color,
    };
    return output;
}

// This constant is defined to 9 significant figures by RGD, so we also have it here for compatibility.
static const float PI = 3.14159265;

///////////////////////////////////////////////////////////////////////////////
// Textures
///////////////////////////////////////////////////////////////////////////////

// The RGSS sprite you bound this shader to via `Sprite#effect_name`.
// The width and height of this texture are equal to the width and height of the sprite.
sampler2D spriteSampler;

// The transition texture currently being shown on screen via `Graphics.transition`.
sampler2D transitionSampler;

///////////////////////////////////////////////////////////////////////////////
// Uniforms
///////////////////////////////////////////////////////////////////////////////

// The reciprocal of the width of the sprite you bound to this shader via `Sprite#effect_name`.
float sampWidth;
// The reciprocal of the height of the sprite you bound to this shader via `Sprite#effect_name`.
float sampHeight;

// The `color` property of the sprite you bound to this shader via `Sprite#effect_name`, with the components normalized to the 0-1 range instead of the 0-255 range used by RGSS.
float4 blendColor;
// The `tone` property of the sprite you bound to this shader via `Sprite#effect_name`, with the components normalized to the 0-1 range instead of the 0-255 range used by RGSS.
float4 tone;

// These two uniforms are, based on the documentation, presumably used internally by RGD to implement the RGSS `Plane` class.
// We don't use them. They'll always be set them to `float2(1.0, 1.0)` and `float2(0.0, 0.0)`, respectively.
// They only exist here for compatibility reasons, since they're documented to exist.
float2 texcoordScale;
float2 texcoordOffset;

// RGD internally uses some uniforms with undocumented names to control the wave effect in `samp()`.
// These are obviously not the original names of the uniforms. They're just named like this to discourage shaders from using them directly.
float __SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED_wave_amp; // `sprite.wave_amp`
float __SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED_wave_length; // `sprite.wave_length`
float __SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED_wave_phase; // `sprite.wave_phase`

// RGD internally uses some uniforms with undocumented names to control the bush effect in `BushMap()`.
// These are obviously not the original names of the uniforms. They're just named like this to discourage shaders from using them directly.
float __SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED_bush_depth; // `sprite.bush_depth`
float __SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED_bush_opacity; // `sprite.bush_opacity / 255.0`

///////////////////////////////////////////////////////////////////////////////
// Library functions
///////////////////////////////////////////////////////////////////////////////

// Uses `tex2D()` to sample the sprite and applies the wave effect.
float4 samp(float2 texcoord)
{
    float y = texcoord.y / sampHeight; // y-coordinate in pixels
    float wavePos = __SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED_wave_phase * (PI / 180.0) + (y / __SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED_wave_length) * (2.0 * PI);
    float x = sin(wavePos) * __SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED_wave_amp;
    texcoord.x += x * sampWidth;

    return tex2D(spriteSampler, texcoord);
}

// Overlays (using alpha blending) the color given by `blendColor` on top of the color given by `origin`.
float4 ColorMap(float4 origin)
{
    return float4(lerp(origin.rgb, blendColor.rgb, blendColor.a), origin.a);
}

// Applies the bush effect to the color given by `origin` and the coordinates on the sprite texture given by `texcoord` (x=0, y=0 is the top-left corner of the sprite, x=1, y=1 is the bottom-right).
float4 BushMap(float4 origin, float2 texcoord)
{
    if (1.0 - texcoord.y < __SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED_bush_depth * sampHeight) {
        origin.a *= __SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED_bush_opacity;
    }
    return origin;
}

// Applies the tone given by `tone` to the color given by `origin` using the tone algorithm used by RGSS.
float4 ToneMap(float4 origin)
{
    float3 luminance_coefficients = float3(0.299, 0.587, 0.114);
    return float4(lerp(origin.rgb, float3(dot(origin.rgb, luminance_coefficients)), tone.a) + tone.rgb, origin.a);
}

///////////////////////////////////////////////////////////////////////////////
// Preset shaders
///////////////////////////////////////////////////////////////////////////////

// This pixel shader is essentially a recreation of the normal rendering process for sprites.
// Copied verbatim from the documentation at <https://cirno.blog/archives/290>.
PS_OUTPUT PS(float4 color : COLOR0, float2 texcoord : TEXCOORD0)
{
    float4 res = samp(texcoord);
    res = ColorMap(res);
    res = BushMap(res, texcoord);
    res *= color;
    res = ToneMap(res);
    res.rgb *= res.a;
    return GetOutput(res);
}

///////////////////////////////////////////////////////////////////////////////
// End of preamble
///////////////////////////////////////////////////////////////////////////////

