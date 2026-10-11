#type vertex
#version 450 core

// One triangle that covers the target, made from the vertex index: no vertex buffer.
void main()
{
    vec2 lCorner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position  = vec4(lCorner * 2.0 - 1.0, 0.0, 1.0);
}

#type fragment
#version 450 core

// Where the shadow casters are: their coverage, in .r.
layout(binding = 0) uniform sampler2D u_Occlusion;

// Lighting2D.h, ShadowBlock2D.
const int MAX_SHADOWED = 16;
layout(std140, binding = 4) uniform ShadowUBO
{
    vec4 u_OcclusionRect;               // xy: the occlusion map's world min, zw: its world size
    vec4 u_ShadowInfo;                  // x: rows in use, y: directions per row (the map's width)
    vec4 u_ShadowLight[MAX_SHADOWED];   // xy: position, z: radius
};

layout(location = 0) out vec4 FragColor;

const int   MAX_STEPS = 768;
const float PI        = 3.14159265;

// Whether a caster covers a world position. Outside the map nothing does.
bool Blocked(vec2 InWorld)
{
    vec2 lUV = (InWorld - u_OcclusionRect.xy) / u_OcclusionRect.zw;
    if (any(lessThan(lUV, vec2(0.0))) || any(greaterThan(lUV, vec2(1.0))))
    {
        return false;
    }
    return textureLod(u_Occlusion, lUV, 0.0).r > 0.5;
}

// One texel = one direction of one light: how far the light gets (0..1 of its radius) before a
// caster stops it. Sprite.glsl reads it back by the angle from the light to the point it shades.
void main()
{
    int lRow = int(gl_FragCoord.y);
    if (lRow >= int(u_ShadowInfo.x + 0.5))
    {
        FragColor = vec4(1.0);
        return;
    }

    vec2  lLight  = u_ShadowLight[lRow].xy;
    float lRadius = max(u_ShadowLight[lRow].z, 1e-3);

    // The texel's centre is its direction: u = angle / 2pi + 0.5, as Sprite.glsl computes it.
    float lAngle = (gl_FragCoord.x / u_ShadowInfo.y) * 2.0 * PI - PI;
    vec2  lDir   = vec2(cos(lAngle), sin(lAngle));

    // About one occlusion texel a step, fewer for a light that reaches far.
    vec2  lTexel = u_OcclusionRect.zw / vec2(textureSize(u_Occlusion, 0));
    float lStep  = max(min(lTexel.x, lTexel.y), lRadius / float(MAX_STEPS));
    int   lSteps = int(ceil(lRadius / lStep));

    // A light inside a caster (a torch in a hand) is not stopped by it: hits count once the ray is out.
    bool  bLeaving = Blocked(lLight);
    float lReach   = 1.0;
    for (int i = 1; i <= MAX_STEPS; ++i)
    {
        if (i > lSteps)
        {
            break;
        }

        float lT       = float(i) * lStep;
        bool  bBlocked = Blocked(lLight + lDir * lT);
        if (bLeaving)
        {
            bLeaving = bBlocked;
            continue;
        }

        if (bBlocked)
        {
            lReach = min(lT / lRadius, 1.0);
            break;
        }
    }

    FragColor = vec4(lReach, 0.0, 0.0, 1.0);
}
