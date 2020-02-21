#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 m_trans;
uniform mat4 m_view;
uniform mat4 m_proj;

void main()
{
    gl_Position = m_proj * m_view * m_trans * vec4(aPos, 1.0);
}