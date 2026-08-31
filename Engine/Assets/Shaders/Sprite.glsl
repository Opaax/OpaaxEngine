#type vertex
#version 450 core

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec4  a_Color;
layout(location = 2) in vec2  a_TexCoord;
layout(location = 3) in float a_TexIndex;
layout(location = 4) in vec2  a_InnerHalf;

// SPIR-V forbids default-block uniforms — view-projection rides a UBO. Binding 1 so it shares
// one Vulkan descriptor set with the sampler array (binding 0) without colliding; on OpenGL the
// UBO and sampler binding namespaces are separate, so the slot move is invisible there.
layout(std140, binding = 1) uniform CameraUBO
{
    mat4 u_ViewProjection;
};

layout(location = 0) out vec4  v_Color;
layout(location = 1) out vec2  v_TexCoord;
layout(location = 2) out float v_TexIndex;
layout(location = 3) out vec2  v_InnerHalf;

void main()
{
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
    v_Color     = a_Color;
    v_TexCoord  = a_TexCoord;
    v_TexIndex  = a_TexIndex;
    v_InnerHalf = a_InnerHalf;
}

#type fragment
#version 450 core

layout(location = 0) in vec4  v_Color;
layout(location = 1) in vec2  v_TexCoord;
layout(location = 2) in float v_TexIndex;
layout(location = 3) in vec2  v_InnerHalf;

// Explicit binding — the array spans texture units 0..15 (replaces the SetIntArray call).
layout(binding = 0) uniform sampler2D u_Textures[16];

layout(location = 0) out vec4 FragColor;

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

    int   lIdx    = int(v_TexIndex);
    vec4  lSample = texture(u_Textures[lIdx], v_TexCoord);
    FragColor     = lSample * v_Color;
}
