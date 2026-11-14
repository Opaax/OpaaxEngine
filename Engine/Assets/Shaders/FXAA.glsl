#type vertex
#version 450 core

// One triangle that covers the target, made from the vertex index: no vertex buffer.
layout(location = 0) out vec2 v_UV;

void main()
{
    vec2 lCorner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    v_UV         = lCorner;
    gl_Position  = vec4(lCorner * 2.0 - 1.0, 0.0, 1.0);
}

#type fragment
#version 450 core

// Fast approximate anti-aliasing after the tonemap, following Lottes' FXAA 3.11 (quality):
// find where the brightness changes sharply, which way the edge runs and how long it is, then
// read the picture a fraction of a pixel across the edge. It sees every edge in the finished
// picture: the inside of a texture, a cut-out, a colour brighter than white.

layout(location = 0) in vec2 v_UV;

// The finished picture in screen colour, filtered: a read between two texels blends them.
layout(binding = 0) uniform sampler2D u_Source;

layout(std140, binding = 7) uniform FXAAUBO
{
    // xy: one texel (1 / width, 1 / height), z: how much a lone bright or dark pixel is blended (0 to 1)
    vec4 u_FXAA;
};

layout(location = 0) out vec4 FragColor;

// A change of brightness is an edge above this share of the brightest neighbour...
const float EDGE_THRESHOLD = 0.125;
// ...and above this at least, so dark areas keep their noise.
const float EDGE_THRESHOLD_MIN = 0.0312;
// Steps along an edge looking for each of its ends.
const int SEARCH_STEPS = 10;

float Luma(vec3 InColor)
{
    return dot(InColor, vec3(0.299, 0.587, 0.114));
}

float LumaAt(vec2 InUV)
{
    return Luma(textureLod(u_Source, InUV, 0.0).rgb);
}

// The search walks one texel at a time near the pixel, then longer strides.
float SearchStride(int InStep)
{
    if (InStep < 4) { return 1.0; }
    if (InStep == 4) { return 1.5; }
    if (InStep < 8) { return 2.0; }
    if (InStep == 8) { return 4.0; }
    return 8.0;
}

void main()
{
    vec2 lTexel = u_FXAA.xy;

    vec4  lCentre = textureLod(u_Source, v_UV, 0.0);
    float lM      = Luma(lCentre.rgb);
    float lN      = LumaAt(v_UV + vec2(0.0,  lTexel.y));
    float lS      = LumaAt(v_UV + vec2(0.0, -lTexel.y));
    float lE      = LumaAt(v_UV + vec2( lTexel.x, 0.0));
    float lW      = LumaAt(v_UV + vec2(-lTexel.x, 0.0));

    float lMax   = max(lM, max(max(lN, lS), max(lE, lW)));
    float lMin   = min(lM, min(min(lN, lS), min(lE, lW)));
    float lRange = lMax - lMin;

    // Not an edge: the pixel as it is.
    if (lRange < max(EDGE_THRESHOLD_MIN, lMax * EDGE_THRESHOLD))
    {
        FragColor = lCentre;
        return;
    }

    float lNE = LumaAt(v_UV + vec2( lTexel.x,  lTexel.y));
    float lNW = LumaAt(v_UV + vec2(-lTexel.x,  lTexel.y));
    float lSE = LumaAt(v_UV + vec2( lTexel.x, -lTexel.y));
    float lSW = LumaAt(v_UV + vec2(-lTexel.x, -lTexel.y));

    float lNS       = lN + lS;
    float lEW       = lE + lW;
    float lCornersN = lNW + lNE;
    float lCornersS = lSW + lSE;
    float lCornersE = lNE + lSE;
    float lCornersW = lNW + lSW;

    // Which way the edge runs: where the brightness changes most, up and down or across.
    float lAcrossRows    = abs(lCornersW - 2.0 * lW) + 2.0 * abs(lNS - 2.0 * lM) + abs(lCornersE - 2.0 * lE);
    float lAcrossColumns = abs(lCornersS - 2.0 * lS) + 2.0 * abs(lEW - 2.0 * lM) + abs(lCornersN - 2.0 * lN);
    bool  bHorizontal    = lAcrossRows >= lAcrossColumns;

    // Which side of the pixel the edge is on: the neighbour that differs most.
    float lLumaBelow  = bHorizontal ? lS : lW;
    float lLumaAbove  = bHorizontal ? lN : lE;
    float lGradBelow  = lLumaBelow - lM;
    float lGradAbove  = lLumaAbove - lM;
    bool  bBelow      = abs(lGradBelow) >= abs(lGradAbove);
    float lGradScaled = 0.25 * max(abs(lGradBelow), abs(lGradAbove));

    float lStep     = bHorizontal ? lTexel.y : lTexel.x;
    float lLocalAvg = 0.5 * (lM + (bBelow ? lLumaBelow : lLumaAbove));
    if (bBelow)
    {
        lStep = -lStep;
    }

    // On the edge itself, half a texel towards that side.
    vec2 lOnEdge = v_UV;
    if (bHorizontal) { lOnEdge.y += 0.5 * lStep; } else { lOnEdge.x += 0.5 * lStep; }

    // Walk both ways along the edge until the brightness no longer matches it.
    vec2  lAlong = bHorizontal ? vec2(lTexel.x, 0.0) : vec2(0.0, lTexel.y);
    vec2  lUV1   = lOnEdge - lAlong;
    vec2  lUV2   = lOnEdge + lAlong;
    float lEnd1  = LumaAt(lUV1) - lLocalAvg;
    float lEnd2  = LumaAt(lUV2) - lLocalAvg;
    bool  bDone1 = abs(lEnd1) >= lGradScaled;
    bool  bDone2 = abs(lEnd2) >= lGradScaled;

    for (int lIndex = 0; lIndex < SEARCH_STEPS; ++lIndex)
    {
        if (bDone1 && bDone2)
        {
            break;
        }

        float lStride = SearchStride(lIndex);
        if (!bDone1)
        {
            lUV1  -= lAlong * lStride;
            lEnd1  = LumaAt(lUV1) - lLocalAvg;
            bDone1 = abs(lEnd1) >= lGradScaled;
        }
        if (!bDone2)
        {
            lUV2  += lAlong * lStride;
            lEnd2  = LumaAt(lUV2) - lLocalAvg;
            bDone2 = abs(lEnd2) >= lGradScaled;
        }
    }

    // How far the pixel is from the nearer end, against the edge's length: near an end, a larger shift.
    float lDist1      = bHorizontal ? (v_UV.x - lUV1.x) : (v_UV.y - lUV1.y);
    float lDist2      = bHorizontal ? (lUV2.x - v_UV.x) : (lUV2.y - v_UV.y);
    bool  bNearer1    = lDist1 < lDist2;
    float lEdgeLength = max(lDist1 + lDist2, 1e-6);
    float lEdgeShift  = 0.5 - min(lDist1, lDist2) / lEdgeLength;

    // Shifted only when the nearer end goes the other way from the pixel: else the pixel is outside the step.
    bool bCentreDarker = lM < lLocalAvg;
    bool bShift        = ((bNearer1 ? lEnd1 : lEnd2) < 0.0) != bCentreDarker;
    float lShift       = bShift ? lEdgeShift : 0.0;

    // A pixel unlike all its neighbours (a lone dot, a thin line) is blended by its contrast.
    float lAverage  = (2.0 * (lNS + lEW) + lCornersW + lCornersE) / 12.0;
    float lContrast = clamp(abs(lAverage - lM) / lRange, 0.0, 1.0);
    float lSmooth   = (3.0 - 2.0 * lContrast) * lContrast * lContrast;
    lShift          = max(lShift, lSmooth * lSmooth * u_FXAA.z);

    vec2 lUV = v_UV;
    if (bHorizontal) { lUV.y += lShift * lStep; } else { lUV.x += lShift * lStep; }

    // The colour across the edge, the pixel's own alpha.
    FragColor = vec4(textureLod(u_Source, lUV, 0.0).rgb, lCentre.a);
}
