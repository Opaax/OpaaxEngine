#type vertex
#version 450 core

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec4  a_Color;
layout(location = 2) in vec2  a_TexCoord;
layout(location = 3) in float a_TexIndex;
layout(location = 4) in vec2  a_InnerHalf;
layout(location = 5) in vec2  a_MaskUV;
layout(location = 6) in float a_MaskIndex;
layout(location = 7) in float a_NormalIndex;
layout(location = 8) in vec4  a_Emissive;     // rgb: linear glow, w: 1 when lit
layout(location = 9) in vec2  a_RotationCS;   // the quad's rotation (cos, sin)

// SPIR-V forbids default-block uniforms — view-projection rides a UBO. Binding 1 so it shares
// one Vulkan descriptor set with the sampler array (binding 0) without colliding; on OpenGL the
// UBO and sampler binding namespaces are separate, so the slot move is invisible there.
layout(std140, binding = 1) uniform CameraUBO
{
    mat4 u_ViewProjection;
    vec4 u_PassParams;   // x: 1 when the pass draws linear colour (an HDR target)
};

layout(location = 0) out vec4  v_Color;
layout(location = 1) out vec2  v_TexCoord;
layout(location = 2) out float v_TexIndex;
layout(location = 3) out vec2  v_InnerHalf;
layout(location = 4) out vec2  v_MaskUV;
layout(location = 5) out float v_MaskIndex;
layout(location = 6) out float v_NormalIndex;
layout(location = 7) out vec4  v_Emissive;
layout(location = 8) out vec2  v_RotationCS;
layout(location = 9) out vec2  v_WorldPosition;

void main()
{
    gl_Position     = u_ViewProjection * vec4(a_Position, 1.0);
    v_Color         = a_Color;
    v_TexCoord      = a_TexCoord;
    v_TexIndex      = a_TexIndex;
    v_InnerHalf     = a_InnerHalf;
    v_MaskUV        = a_MaskUV;
    v_MaskIndex     = a_MaskIndex;
    v_NormalIndex   = a_NormalIndex;
    v_Emissive      = a_Emissive;
    v_RotationCS    = a_RotationCS;
    v_WorldPosition = a_Position.xy;
}

#type fragment
#version 450 core

layout(location = 0) in vec4  v_Color;
layout(location = 1) in vec2  v_TexCoord;
layout(location = 2) in float v_TexIndex;
layout(location = 3) in vec2  v_InnerHalf;
layout(location = 4) in vec2  v_MaskUV;
layout(location = 5) in float v_MaskIndex;
layout(location = 6) in float v_NormalIndex;
layout(location = 7) in vec4  v_Emissive;
layout(location = 8) in vec2  v_RotationCS;
layout(location = 9) in vec2  v_WorldPosition;

// Explicit binding — the array spans texture units 0..15 (replaces the SetIntArray call).
layout(binding = 0) uniform sampler2D u_Textures[16];

layout(std140, binding = 1) uniform CameraUBO
{
    mat4 u_ViewProjection;
    vec4 u_PassParams;
};

// The lights of an HDR pass (Lighting2D.h, LightsBlock2D). Colours are linear, times intensity.
const int MAX_LIGHTS = 32;
layout(std140, binding = 3) uniform LightsUBO
{
    vec4 u_Ambient;                      // rgb: ambient light, w: light count
    vec4 u_LightPosition[MAX_LIGHTS];    // xy: position, z: radius, w: 0 point, 1 spot, 2 global
    vec4 u_LightColor[MAX_LIGHTS];       // rgb: colour, w: falloff exponent
    vec4 u_LightDirection[MAX_LIGHTS];   // xy: where it points, z: cos(outer half-angle), w: cos(inner)
    vec4 u_LightParams[MAX_LIGHTS];      // x: height (point, spot) or sin(elevation) (global)
};

layout(location = 0) out vec4 FragColor;

// One constant index per case: GLSL leaves a sampler index that varies within a draw undefined
// (some drivers show it), and the sprites of one batch use different textures.
vec4 SampleSlot(int InSlot, vec2 InUV)
{
    switch (InSlot)
    {
        case 0:  return texture(u_Textures[0],  InUV);
        case 1:  return texture(u_Textures[1],  InUV);
        case 2:  return texture(u_Textures[2],  InUV);
        case 3:  return texture(u_Textures[3],  InUV);
        case 4:  return texture(u_Textures[4],  InUV);
        case 5:  return texture(u_Textures[5],  InUV);
        case 6:  return texture(u_Textures[6],  InUV);
        case 7:  return texture(u_Textures[7],  InUV);
        case 8:  return texture(u_Textures[8],  InUV);
        case 9:  return texture(u_Textures[9],  InUV);
        case 10: return texture(u_Textures[10], InUV);
        case 11: return texture(u_Textures[11], InUV);
        case 12: return texture(u_Textures[12], InUV);
        case 13: return texture(u_Textures[13], InUV);
        case 14: return texture(u_Textures[14], InUV);
        case 15: return texture(u_Textures[15], InUV);
    }
    return vec4(1.0, 0.0, 1.0, 1.0);
}

// The light reaching a point: ambient, plus every light. Without a normal map the surface faces the
// viewer and every light reaches it fully; with one, a light counts by the angle it arrives at.
vec3 LightAt(vec2 InPosition, vec3 InNormal, bool bInHasNormal)
{
    vec3 lLight = u_Ambient.rgb;
    int  lCount = int(u_Ambient.w + 0.5);

    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        if (i >= lCount)
        {
            break;
        }

        vec4  lPosition = u_LightPosition[i];
        vec4  lColor    = u_LightColor[i];
        vec4  lCone     = u_LightDirection[i];
        float lHeight   = u_LightParams[i].x;
        int   lType     = int(lPosition.w + 0.5);

        float lReach = 1.0;
        vec3  lToLight;

        if (lType == 2)
        {
            // A global light comes from its direction, lHeight being the sine of its elevation.
            float lFlat = sqrt(max(1.0 - lHeight * lHeight, 0.0));
            lToLight    = normalize(vec3(-lCone.xy * lFlat, lHeight));
        }
        else
        {
            vec2  lDelta    = lPosition.xy - InPosition;
            float lDistance = length(lDelta);
            if (lDistance >= lPosition.z)
            {
                continue;
            }

            lReach = pow(1.0 - lDistance / lPosition.z, lColor.w);

            if (lType == 1)
            {
                // Full light inside the inner cone, none outside the outer one.
                float lCos = (lDistance > 0.0) ? dot(-lDelta / lDistance, lCone.xy) : 1.0;
                lReach *= clamp((lCos - lCone.z) / max(lCone.w - lCone.z, 1e-4), 0.0, 1.0);
            }

            lToLight = normalize(vec3(lDelta, lHeight));
        }

        float lFacing = bInHasNormal ? max(dot(InNormal, lToLight), 0.0) : 1.0;
        lLight += lColor.rgb * lReach * lFacing;
    }

    return lLight;
}

void main()
{
    // A hollow quad carves its middle out here. v_InnerHalf is {0,0} for every ordinary draw, so
    // the guard costs one comparison and the branch is never taken.
    //
    // v_TexCoord is the local position for this path: DrawQuadOutline is untextured and spans the
    // full 0..1 UVs. That is why no textured outline entry point exists — an atlas sub-rect would
    // put the hole somewhere else entirely.
    if (v_InnerHalf.x > 0.0 && all(lessThan(abs(v_TexCoord - vec2(0.5)), v_InnerHalf)))
    {
        discard;
    }

    // The mask. v_MaskIndex is -1 for every draw that named no mask, so this whole path is
    // skipped then.
    //
    // white shows, black hides: the visibility is mask.r * mask.a. A black-and-white png with no
    // alpha gives r * 1 = its luminance; a white shape on transparency gives 1 * a = its
    // silhouette. One expression, both intuitions, no mode flag.
    float lMask = 1.0;
    if (v_MaskIndex >= 0.0)
    {
        // Outside the mask widget's own rect there is no mask texture to sample, and a masked
        // child that escapes its mask must not draw.
        if (any(lessThan(v_MaskUV, vec2(0.0))) || any(greaterThan(v_MaskUV, vec2(1.0))))
        {
            discard;
        }

        vec4 lMaskSample = SampleSlot(int(v_MaskIndex), v_MaskUV);
        lMask            = lMaskSample.r * lMaskSample.a;
    }

    vec4 lSample = SampleSlot(int(v_TexIndex), v_TexCoord);
    FragColor    = lSample * v_Color * vec4(1.0, 1.0, 1.0, lMask);

    // An HDR pass works in linear colour (textures and tints are authored in screen colour), and
    // lights its lit quads.
    if (u_PassParams.x > 0.5)
    {
        vec3 lAlbedo = pow(FragColor.rgb, vec3(2.2));
        vec3 lColor  = lAlbedo;

        if (v_Emissive.w > 0.5)
        {
            vec3 lNormal     = vec3(0.0, 0.0, 1.0);
            bool bHasNormal  = v_NormalIndex >= 0.0;
            if (bHasNormal)
            {
                // The map is in the sprite's own space: it turns with the sprite.
                vec3 lMapped = SampleSlot(int(v_NormalIndex), v_TexCoord).xyz * 2.0 - 1.0;
                lNormal = normalize(vec3(v_RotationCS.x * lMapped.x - v_RotationCS.y * lMapped.y,
                                         v_RotationCS.y * lMapped.x + v_RotationCS.x * lMapped.y,
                                         lMapped.z));
            }

            lColor *= LightAt(v_WorldPosition, lNormal, bHasNormal);
        }

        // The glow is the sprite's own colour, unaffected by the lights.
        FragColor.rgb = lColor + lAlbedo * v_Emissive.rgb;
    }
}
