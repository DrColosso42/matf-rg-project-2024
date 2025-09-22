//#shader vertex
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;
out vec3 Normal;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    // Convert UDIM coordinates to single texture
    vec2 udimCoords = aTexCoords;
    udimCoords.x = udimCoords.x * 0.5; // Scale X to fit in half width
    if (aTexCoords.x >= 1.0) {
        udimCoords.x = (aTexCoords.x - 1.0) * 0.5 + 0.5; // Right tile: shift to right half
    }
    TexCoords = vec2(udimCoords.x, 1.0 - udimCoords.y);

    gl_Position = projection * view * vec4(FragPos, 1.0);
}

//#shader fragment
#version 330 core

out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D texture_diffuse1;


void main()
{

    vec2 wrappedCoords = fract(TexCoords);
    FragColor = vec4(texture(texture_diffuse1, wrappedCoords).rgb, 1.0);
}