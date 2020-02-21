#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec3 DebugColor;
in mat3 TBN;

uniform vec3 c_diff;
uniform vec3 c_spec;
uniform float c_metal;
uniform float c_rough;
uniform vec3 c_emission;
uniform float c_env_intesity;

#define MAX_LIGHT_NUM 16
struct Light
{
    vec3 color;
    vec4 pos;
	float intensity;
};
uniform Light l[MAX_LIGHT_NUM];
uniform int l_num;
uniform vec3 l_ambient;

uniform vec3 v_pos;

uniform vec2 uv_offset;
uniform vec2 uv_scale;

uniform int use_difftex;
uniform int use_spectex;
uniform int use_normaltex;
uniform int use_metaltex;
uniform int use_roughtex;
uniform int use_emissiontex;
uniform int use_aotex;
uniform sampler2D t_diff;
uniform sampler2D t_spec;
uniform sampler2D t_normal;
uniform sampler2D t_metal;
uniform sampler2D t_rough;
uniform sampler2D t_emission;
uniform sampler2D t_ao;
uniform sampler2D t_lut;
uniform samplerCube t_env_diff;
uniform samplerCube t_env_spec;
uniform samplerCube t_env;

const float PI = 3.14159265359;

float D_GGX(float NdotH, float rough)
{
	float a = rough * rough;
    float a2 = a*a;
    float NdotH2 = NdotH*NdotH;

    float nom = a2;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float G_GGX(float NdotV, float NdotL, float rough)
{
	float r=(rough+1);
	float k=(r*r)/8.0;

	float g1=NdotV/(NdotV*(1-k)+k);
	float g2=NdotL/(NdotL*(1-k)+k);
	return g1*g2;
}

vec3 F_SCH(float NdotV, vec3 f0)
{
	return f0+(1-f0)*pow(1.0 - NdotV, 5.0);
}

vec3 F_SCH_R(float NdotV, vec3 f0, float roughness)
{
	return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - NdotV, 5.0);
}

float desaturate(vec3 color)
{
	return 0.299*color.r + 0.587*color.g + 0.114*color.b;
}

vec3 PBRLighting(vec3 N, vec3 V, vec3 R)
{
	vec3 Lo = vec3(0.0);
	vec3 albedo = use_difftex==1?pow(texture(t_diff, TexCoord * uv_scale - uv_offset).rgb,vec3(2.2)):c_diff;
	vec3 specular = use_spectex==1?pow(texture(t_spec, TexCoord * uv_scale - uv_offset).rgb,vec3(2.2)):c_spec;
	float metal = use_metaltex==1?desaturate(texture(t_metal, TexCoord * uv_scale - uv_offset).rgb):c_metal;
	float rough = use_roughtex==1?desaturate(texture(t_rough, TexCoord * uv_scale - uv_offset).rgb):c_rough;
	vec3 emission = use_emissiontex==1?pow(texture(t_emission, TexCoord * uv_scale - uv_offset).rgb,vec3(2.2)):c_emission;
	vec3 ao = use_aotex==1?vec3(desaturate(texture(t_ao, TexCoord * uv_scale - uv_offset).rgb)):vec3(1);
	vec3 f0 = vec3(0.04); 
	f0 = mix(f0, albedo, metal);
	float roughness = clamp(rough,0.01,0.99);
	
	for(int i=0;i<l_num;i++)
	{
		vec3 L;
		vec3 H;
		vec3 radiance;
		if(l[i].pos.w>0.5)
			L=normalize(l[i].pos.xyz-FragPos);
		else
			L=normalize(l[i].pos.xyz);
		H=normalize(V+L);

		radiance = l[i].intensity * l[i].color;

		float NdotV = max(dot(N, V), 0.0);
		float NdotH = max(dot(N, H), 0.0);
		float NdotL = max(dot(N, L), 0.0);
		float HdotV = max(dot(H, V), 0.0);
		vec3 F = F_SCH(HdotV, f0);
		float D = D_GGX(NdotH, roughness);
		float G = G_GGX(NdotV, NdotL, roughness);

		vec3 nominator = D * G * F * specular;
		float denominator = 4.0 * NdotV * NdotL + 0.001; 
		vec3 spec = nominator / denominator;
		vec3 kD = vec3(1) - F;
		kD *= 1.0 - metal;
		Lo += (kD * albedo / PI + spec) * radiance * NdotL;
	}
	float NdotV = max(dot(N, V), 0.0);
	vec3 kS = F_SCH_R(NdotV, f0, roughness);
	vec3 kD = vec3(1) - kS;
	kD *= 1.0 - metal;     
	vec3 irradiance = texture(t_env_diff, N).rgb * exp2(c_env_intesity);
	vec3 diffuse    = irradiance * albedo;
	
	const float MAX_REFLECTION_LOD = 4.0;
	vec3 prefilteredColor = textureLod(t_env_spec, R,  roughness * MAX_REFLECTION_LOD).rgb * exp2(c_env_intesity);   
	vec2 envBRDF  = texture(t_lut, vec2(NdotV, roughness)).rg;
	vec3 reflection = prefilteredColor * (kS * envBRDF.x + envBRDF.y) * specular;

	vec3 ambient = (kD * diffuse + reflection); 
	return (Lo + ambient) * ao + vec3(emission);
}

vec3 ACESToneMapping(vec3 color)
{
    float A = 2.51f;
    float B = 0.03f;
    float C = 2.43f;
    float D = 0.59f;
    float E = 0.14f;
	color*=exp2(0);
	//return pow(color,vec3(1/2.2));
    return pow((color * (A * color + B)) / (color * (C * color + D) + E),vec3(1/2.2));
}

float LinearizeDepth(float depth) 
{
	float near = 1; 
	float far  = 100000.0; 
    float z = depth * 2.0 - 1.0; // back to NDC 
    return (2.0 * near * far) / (far + near - z * (far - near));    
}

void main()
{
	vec3 N = normalize(Normal);
	if(use_normaltex==1)
	{
		N = texture(t_normal, TexCoord * uv_scale - uv_offset).rgb;
		N = normalize(N * 2.0 - 1.0);   
		N = normalize(TBN * N);
	}
	vec3 V = normalize(v_pos-FragPos);
	vec3 R = reflect(-V, N);
	vec3 color = PBRLighting(N, V, R);
	FragColor = vec4(ACESToneMapping(color),1);
    //FragColor = vec4(TexCoord,0,1);
    //FragColor = vec4(N/2+vec3(0.5),1);
    //FragColor = vec4(DebugColor/2+vec3(0.5),1);
	//FragColor = texture(t_normal, TexCoord * uv_scale + uv_offset);
	//FragColor = texture(t_spec, TexCoord * uv_scale + uv_offset);
	//FragColor = texture(t_rough, TexCoord);
	//FragColor = textureLod(t_env_diff, N,0);
}