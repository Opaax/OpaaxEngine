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

layout(location = 0) in vec2 v_UV;

// The pass's input: the HDR scene, then each level of the chain.
layout(binding = 0) uniform sampler2D u_Source;

layout(std140, binding = 6) uniform BloomUBO
{
    // x: 0 first downsample (keeps what passes the threshold), 1 downsample, 2 upsample (added);
    // y: threshold, z: soft knee (0 hard .. 1 soft)
    vec4 u_BloomPass;
};

layout(location = 0) out vec4 FragColor;

float Luminance(vec3 InColor)
{
    return dot(InColor, vec3(0.2126, 0.7152, 0.0722));
}

// Keeps the light above the threshold, fading in over the knee below it.
vec3 Prefilter(vec3 InColor)
{
    float lThreshold  = u_BloomPass.y;
    float lKnee       = lThreshold * u_BloomPass.z + 1e-4;
    float lBrightness = max(InColor.r, max(InColor.g, InColor.b));

    float lSoft = clamp(lBrightness - lThreshold + lKnee, 0.0, 2.0 * lKnee);
    lSoft       = lSoft * lSoft / (4.0 * lKnee);

    return InColor * (max(lSoft, lBrightness - lThreshold) / max(lBrightness, 1e-4));
}

// Halves the source with 13 taps (Jimenez, "Next Generation Post Processing in Call of Duty:
// Advanced Warfare"): a centre box and four corner boxes. The first level weighs each box by its
// brightness (Karis), so a lone bright texel cannot flicker.
vec3 Downsample(bool bInFirst)
{
    vec2 lTexel = 1.0 / vec2(textureSize(u_Source, 0));

    vec3 a = texture(u_Source, v_UV + lTexel * vec2(-2.0,  2.0)).rgb;
    vec3 b = texture(u_Source, v_UV + lTexel * vec2( 0.0,  2.0)).rgb;
    vec3 c = texture(u_Source, v_UV + lTexel * vec2( 2.0,  2.0)).rgb;
    vec3 d = texture(u_Source, v_UV + lTexel * vec2(-2.0,  0.0)).rgb;
    vec3 e = texture(u_Source, v_UV).rgb;
    vec3 f = texture(u_Source, v_UV + lTexel * vec2( 2.0,  0.0)).rgb;
    vec3 g = texture(u_Source, v_UV + lTexel * vec2(-2.0, -2.0)).rgb;
    vec3 h = texture(u_Source, v_UV + lTexel * vec2( 0.0, -2.0)).rgb;
    vec3 i = texture(u_Source, v_UV + lTexel * vec2( 2.0, -2.0)).rgb;
    vec3 j = texture(u_Source, v_UV + lTexel * vec2(-1.0,  1.0)).rgb;
    vec3 k = texture(u_Source, v_UV + lTexel * vec2( 1.0,  1.0)).rgb;
    vec3 l = texture(u_Source, v_UV + lTexel * vec2(-1.0, -1.0)).rgb;
    vec3 m = texture(u_Source, v_UV + lTexel * vec2( 1.0, -1.0)).rgb;

    if (!bInFirst)
    {
        return e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
    }

    vec3  lBoxes[5];
    float lWeights[5];
    lBoxes[0] = (j + k + l + m) * 0.25;
    lBoxes[1] = (a + b + d + e) * 0.25;
    lBoxes[2] = (b + c + e + f) * 0.25;
    lBoxes[3] = (d + e + g + h) * 0.25;
    lBoxes[4] = (e + f + h + i) * 0.25;

    vec3  lSum    = vec3(0.0);
    float lWeight = 0.0;
    for (int n = 0; n < 5; ++n)
    {
        float lShare = (n == 0) ? 0.5 : 0.125;
        lWeights[n]  = lShare / (1.0 + Luminance(lBoxes[n]));
        lSum        += lBoxes[n] * lWeights[n];
        lWeight     += lWeights[n];
    }
    return lSum / lWeight;
}

// Doubles the source with a 3x3 tent.
vec3 Upsample()
{
    vec2 lTexel = 1.0 / vec2(textureSize(u_Source, 0));

    vec3 lSum = texture(u_Source, v_UV).rgb * 4.0;
    lSum += (texture(u_Source, v_UV + lTexel * vec2(-1.0,  0.0)).rgb
           + texture(u_Source, v_UV + lTexel * vec2( 1.0,  0.0)).rgb
           + texture(u_Source, v_UV + lTexel * vec2( 0.0, -1.0)).rgb
           + texture(u_Source, v_UV + lTexel * vec2( 0.0,  1.0)).rgb) * 2.0;
    lSum += texture(u_Source, v_UV + lTexel * vec2(-1.0, -1.0)).rgb
          + texture(u_Source, v_UV + lTexel * vec2( 1.0, -1.0)).rgb
          + texture(u_Source, v_UV + lTexel * vec2(-1.0,  1.0)).rgb
          + texture(u_Source, v_UV + lTexel * vec2( 1.0,  1.0)).rgb;
    return lSum / 16.0;
}

void main()
{
    int lMode = int(u_BloomPass.x + 0.5);

    if (lMode == 0)
    {
        FragColor = vec4(Prefilter(Downsample(true)), 1.0);
    }
    else if (lMode == 1)
    {
        FragColor = vec4(Downsample(false), 1.0);
    }
    else
    {
        FragColor = vec4(Upsample(), 1.0);
    }
}
