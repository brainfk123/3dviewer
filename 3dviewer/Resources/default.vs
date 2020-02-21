#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBiTangent;

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out vec3 DebugColor;
out mat3 TBN;

uniform vec3 v_pos;

uniform mat4 m_trans;
uniform mat4 m_invtrans;
uniform mat4 m_view;
uniform mat4 m_proj;

void main()
{
    gl_Position = m_proj * m_view * m_trans * vec4(aPos, 1.0);
    //gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord;
	Normal = mat3(m_invtrans) * aNormal;
	vec3 T = normalize(mat3(m_invtrans) * aTangent);
	vec3 B = normalize(mat3(m_invtrans) * aBiTangent);
	vec3 N = normalize(Normal);
	TBN = mat3(T, B, N);
	DebugColor = B;
	FragPos = vec3(m_trans * vec4(aPos, 1.0));
}