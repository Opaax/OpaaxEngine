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

// The pass's input: the casters' coverage (the occlusion map), then the previous pass.
layout(binding = 0) uniform sampler2D u_Source;

layout(std140, binding = 5) uniform AmbientOcclusionUBO
{
    // x: 0 shrinks the occlusion map (AO_DOWNSAMPLE_2D each way), 1 blurs across, 2 blurs down;
    // y: the blur's sigma, texels
    vec4 u_AOPass;
};

layout(location = 0) out vec4 FragColor;

const int DOWNSAMPLE = 4;    // AO_DOWNSAMPLE_2D (Lighting2D.h)
const int MAX_REACH  = 32;   // blur taps each side, at most

float Fetch(ivec2 InTexel, ivec2 InSize)
{
    return texelFetch(u_Source, clamp(InTexel, ivec2(0), InSize - 1), 0).r;
}

void main()
{
    ivec2 lTexel = ivec2(gl_FragCoord.xy);
    ivec2 lSize  = textureSize(u_Source, 0);
    int   lMode  = int(u_AOPass.x + 0.5);

    // The average coverage of the block of occlusion texels under this one.
    if (lMode == 0)
    {
        float lSum = 0.0;
        for (int y = 0; y < DOWNSAMPLE; ++y)
        {
            for (int x = 0; x < DOWNSAMPLE; ++x)
            {
                lSum += Fetch(lTexel * DOWNSAMPLE + ivec2(x, y), lSize);
            }
        }
        FragColor = vec4(lSum / float(DOWNSAMPLE * DOWNSAMPLE), 0.0, 0.0, 1.0);
        return;
    }

    // A Gaussian along one axis: how much is covered around this point.
    ivec2 lAxis  = (lMode == 1) ? ivec2(1, 0) : ivec2(0, 1);
    float lSigma = max(u_AOPass.y, 0.5);
    int   lReach = int(min(ceil(lSigma * 2.5), float(MAX_REACH)));

    float lSum     = 0.0;
    float lWeights = 0.0;
    for (int i = -MAX_REACH; i <= MAX_REACH; ++i)
    {
        if (abs(i) > lReach)
        {
            continue;
        }

        float lWeight = exp(-0.5 * float(i * i) / (lSigma * lSigma));
        lSum     += lWeight * Fetch(lTexel + lAxis * i, lSize);
        lWeights += lWeight;
    }

    FragColor = vec4(lSum / lWeights, 0.0, 0.0, 1.0);
}
