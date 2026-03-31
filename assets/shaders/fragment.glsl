#version 330 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 objectColor;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform float ambientStrength;

void main()
{
    // Ambient
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse
    vec3 norm      = normalize(Normal);
    vec3 lightDirN = normalize(-lightDir);
    float diff     = max(dot(norm, lightDirN), 0.0);
    vec3 diffuse   = diff * lightColor;

    vec3 result = (ambient + diffuse) * objectColor;
    FragColor   = vec4(result, 1.0);
}
