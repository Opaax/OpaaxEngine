#type vertex
#version 450 core

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec4  a_Color;
layout(location = 2) in vec2  a_TexCoord;
layout(location = 3) in float a_TexIndex;
layout(location = 4) in vec2  a_InnerHalf;
layout(location = 5) in vec2  a_MaskUV;
layout(location = 6) in float a_MaskIndex;

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

void main()
{
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
    v_Color     = a_Color;
    v_TexCoord  = a_TexCoord;
    v_TexIndex  = a_TexIndex;
    v_InnerHalf = a_InnerHalf;
    v_MaskUV    = a_MaskUV;
    v_MaskIndex = a_MaskIndex;
}

#type fragment
#version 450 core

layout(location = 0) in vec4  v_Color;
layout(location = 1) in vec2  v_TexCoord;
layout(location = 2) in float v_TexIndex;
layout(location = 3) in vec2  v_InnerHalf;
layout(location = 4) in vec2  v_MaskUV;
layout(location = 5) in float v_MaskIndex;

// Explicit binding — the array spans texture units 0..15 (replaces the SetIntArray call).
layout(binding = 0) uniform sampler2D u_Textures[16];

layout(std140, binding = 1) uniform CameraUBO
{
    mat4 u_ViewProjection;
    vec4 u_PassParams;
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

void main()
{
    // A HOLLOW quad carves its middle out here. v_InnerHalf is {0,0} for every ordinary draw, so
    // the guard costs one comparison and the branch is never taken.
    //
    // v_TexCoord IS the local position for this path: DrawQuadOutline is untextured and spans the
    // full 0..1 UVs. That is why no textured outline entry point exists — an atlas sub-rect would
    // put the hole somewhere else entirely.
    if (v_InnerHalf.x > 0.0 && all(lessThan(abs(v_TexCoord - vec2(0.5)), v_InnerHalf)))
    {
        discard;
    }

    // THE MASK (**UI16**). v_MaskIndex is -1 for every draw that named no mask, so this whole
    // path is skipped and nothing that existed before this feature changed.
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

    int   lIdx    = int(v_TexIndex);
    vec4  lSample = SampleSlot(lIdx, v_TexCoord);
    FragColor     = lSample * v_Color * vec4(1.0, 1.0, 1.0, lMask);

    // An HDR pass works in linear colour; textures and tints are authored in screen colour.
    if (u_PassParams.x > 0.5)
    {
        FragColor.rgb = pow(FragColor.rgb, vec3(2.2));
    }
}
