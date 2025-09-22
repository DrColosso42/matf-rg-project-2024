//#shader vertex
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

layout (location = 5) in vec4 instanceMatrix0;
layout (location = 6) in vec4 instanceMatrix1;
layout (location = 7) in vec4 instanceMatrix2;
layout (location = 8) in vec4 instanceMatrix3;

out vec2 TexCoords;
out vec3 Normal;
out vec3 FragPos;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    
    mat4 model = mat4(instanceMatrix0, instanceMatrix1, instanceMatrix2, instanceMatrix3);

    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;

    
    vec2 adjustedCoords = aTexCoords;
    adjustedCoords.y = (1 - adjustedCoords.y);
    TexCoords = adjustedCoords;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}

//#shader fragment
#version 330 core

struct Material {
    sampler2D diffuse;
    float shininess;
};

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3 position;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;

uniform Material material;
uniform DirLight dirLight;
uniform PointLight pointLight;
uniform SpotLight spotLight;
uniform vec3 viewPos;
uniform sampler2D texture_diffuse1;


uniform samplerCube depthMap;
uniform vec3 lightPos;
uniform float far_plane;
uniform float shadowStrength;

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 textureColor);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 textureColor);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 textureColor);
float ShadowCalculation(vec3 fragPos);

void main()
{
    
    vec3 textureColor = texture(texture_diffuse1, TexCoords).rgb;

    
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    
    vec3 result = CalcDirLight(dirLight, norm, viewDir, textureColor);
    result += CalcPointLight(pointLight, norm, FragPos, viewDir, textureColor);
    result += CalcSpotLight(spotLight, norm, FragPos, viewDir, textureColor);

    FragColor = vec4(result, 1.0);
}

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 textureColor)
{
    vec3 lightDir = normalize(-light.direction);

    
    float diff = max(dot(normal, lightDir), 0.0);

    
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    
    vec3 ambient = light.ambient * textureColor;
    vec3 diffuse = light.diffuse * diff * textureColor;
    vec3 specular = light.specular * spec * vec3(0.1); 

    return (ambient + diffuse + specular);
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 textureColor)
{
    vec3 lightDir = normalize(light.position - fragPos);

    
    float diff = max(dot(normal, lightDir), 0.0);

    
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    
    float shadow = ShadowCalculation(fragPos);

    
    vec3 ambient = light.ambient * textureColor;
    vec3 diffuse = light.diffuse * diff * textureColor;
    vec3 specular = light.specular * spec * vec3(0.3); 

    
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    
    float finalShadow = shadow * shadowStrength;
    return ambient + (1.0 - finalShadow) * (diffuse + specular);
}

float ShadowCalculation(vec3 fragPos)
{
    
    vec3 fragToLight = fragPos - lightPos;

    
    float closestDepth = texture(depthMap, fragToLight).r;

    
    closestDepth *= far_plane;

    
    float currentDepth = length(fragToLight);

    
    float bias = 0.05;
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

    return shadow;
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 textureColor)
{
    vec3 lightDir = normalize(light.position - fragPos);

    
    float diff = max(dot(normal, lightDir), 0.0);

    
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    
    vec3 ambient = light.ambient * textureColor;
    vec3 diffuse = light.diffuse * diff * textureColor;
    vec3 specular = light.specular * spec * vec3(0.5); 

    
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;

    return (ambient + diffuse + specular);
}
