#version 330 core
out vec4 FragColor;
in vec3 localPos;

uniform float exposure;
uniform sampler2D equirectangularMap;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

vec3 ACESToneMapping(vec3 color)
{
    float A = 2.51f;
    float B = 0.03f;
    float C = 2.43f;
    float D = 0.59f;
    float E = 0.14f;
    return (color * (A * color + B)) / (color * (C * color + D) + E);
}

void main()
{       
    vec2 uv = SampleSphericalMap(normalize(localPos)); // make sure to normalize localPos
    //vec3 color = ACESToneMapping(texture(equirectangularMap, uv).rgb * exp2(exposure));
    vec3 color = texture(equirectangularMap, uv).rgb * exp2(exposure);

    FragColor = vec4(color, 1.0);
}