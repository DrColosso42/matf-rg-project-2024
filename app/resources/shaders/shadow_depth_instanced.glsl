//#shader vertex
#version 330 core
layout (location = 0) in vec3 aPos;
// Instance attributes (matrix takes 4 attribute locations, starting after mesh attributes)
layout (location = 5) in vec4 instanceMatrix0;
layout (location = 6) in vec4 instanceMatrix1;
layout (location = 7) in vec4 instanceMatrix2;
layout (location = 8) in vec4 instanceMatrix3;

void main()
{
    // Reconstruct model matrix from instance attributes
    mat4 model = mat4(instanceMatrix0, instanceMatrix1, instanceMatrix2, instanceMatrix3);
    gl_Position = model * vec4(aPos, 1.0);
}

//#shader geometry
#version 330 core
layout (triangles) in;
layout (triangle_strip, max_vertices=18) out;

uniform mat4 shadowMatrices[6];

out vec4 FragPos; // FragPos from GS (output per emitvertex)

void main()
{
    for(int face = 0; face < 6; ++face)
    {
        gl_Layer = face; // built-in variable that specifies to which face we render.
        for(int i = 0; i < 3; ++i) // for each triangle vertex
        {
            FragPos = gl_in[i].gl_Position;
            gl_Position = shadowMatrices[face] * FragPos;
            EmitVertex();
        }
        EndPrimitive();
    }
}

//#shader fragment
#version 330 core
in vec4 FragPos;

uniform vec3 lightPos;
uniform float far_plane;

void main()
{
    // Calculate distance from light to fragment
    float lightDistance = length(FragPos.xyz - lightPos);

    // Map to [0;1] range by dividing by far_plane
    lightDistance = lightDistance / far_plane;

    // Write this as modified depth
    gl_FragDepth = lightDistance;
}