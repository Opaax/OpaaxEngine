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

// The scene in linear HDR colour.
layout(binding = 0) uniform sampler2D u_Scene;

layout(std140, binding = 2) uniform PostUBO
{
    // x: exposure multiplier, y: tonemapper (0 none, 1 Reinhard, 2 ACES)
    vec4 u_Post;
};

layout(location = 0) out vec4 FragColor;

// Narkowicz's fit of the ACES filmic curve.
vec3 ACESFilm(vec3 InColor)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((InColor * (a * InColor + b)) / (InColor * (c * InColor + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 lColor = texture(u_Scene, v_UV).rgb * u_Post.x;

    int lTonemapper = int(u_Post.y + 0.5);
    if (lTonemapper == 1)
    {
        lColor = lColor / (1.0 + lColor);
    }
    else if (lTonemapper == 2)
    {
        lColor = ACESFilm(lColor);
    }

    // Back to the screen's gamma: the inverse of the decode the scene pass did.
    FragColor = vec4(pow(clamp(lColor, 0.0, 1.0), vec3(1.0 / 2.2)), 1.0);
}
