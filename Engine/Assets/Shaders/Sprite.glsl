#type vertex
#version 450 core

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec4  a_Color;
layout(location = 2) in vec2  a_TexCoord;
layout(location = 3) in float a_TexIndex;

uniform mat4 u_ViewProjection;

out vec4  v_Color;
out vec2  v_TexCoord;
out float v_TexIndex;

void main()
{
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
    v_Color     = a_Color;
    v_TexCoord  = a_TexCoord;
    v_TexIndex  = a_TexIndex;
}

#type fragment
#version 450 core

in vec4  v_Color;
in vec2  v_TexCoord;
in float v_TexIndex;

uniform sampler2D u_Textures[16];

out vec4 FragColor;

void main()
{
    int   lIdx    = int(v_TexIndex);
    vec4  lSample = texture(u_Textures[lIdx], v_TexCoord);
    FragColor     = lSample * v_Color;
}
