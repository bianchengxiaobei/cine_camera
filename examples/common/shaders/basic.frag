#version 330 core
in vec3 vNormal;
in vec3 vColor;

uniform vec3 uTint;

out vec4 FragColor;

void main()
{
    vec3 normal = normalize(vNormal);
    vec3 lightDirection = normalize(vec3(0.35, 0.85, 0.35));
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float light = 0.35 + diffuse * 0.65;
    FragColor = vec4(vColor * uTint * light, 1.0);
}
