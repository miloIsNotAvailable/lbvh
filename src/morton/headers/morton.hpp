#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

#include <utils.hpp>

GLuint createQuantizeShader();
GLuint createMortonShader();

const std::string mortonLayout = R"(#version 430

    layout(local_size_x = 64) in;
)";

inline const std::string mortonHeader = mortonLayout + structs + R"(
    
    layout(std430, binding = 0) buffer Morton
    {
        uvec2 morton[];
    };
    
    layout(std430, binding = 1) buffer Data
    {
        Triangle triangles[];
    };

    layout(std140, binding = 0) uniform Params
    {
        AABB scene;
    };
)";


inline const std::string mortonSrc = mortonHeader + R"(
    
    uint expandBits(uint v)
    {
        v = (v * 0x00010001u) & 0xFF0000FFu;
        v = (v * 0x00000101u) & 0x0F00F00Fu;
        v = (v * 0x00000011u) & 0xC30C30C3u;
        v = (v * 0x00000005u) & 0x49249249u;
        return v;
    }

    // Calculates a 30-bit Morton code for the
    // given 3D point located within the unit cube [0,1].
    uint morton3D(float x, float y, float z)
    {
        x = min(max(x * 1024.0f, 0.0f), 1023.0f);
        y = min(max(y * 1024.0f, 0.0f), 1023.0f);
        z = min(max(z * 1024.0f, 0.0f), 1023.0f);
        uint xx = expandBits(uint(x));
        uint yy = expandBits(uint(y));
        uint zz = expandBits(uint(z));
        return xx * 4u + yy * 2u + zz;
    }
    
    void main()
    {
        uint id = gl_GlobalInvocationID.x;

        if (id >= triangles.length())
            return;
    
        // uvec3 c = triangles[ id ].quantized;
        
        vec4 norm = (triangles[id].c - scene.bmin) / (scene.bmax - scene.bmin);
        uint m = morton3D( norm.x, norm.y, norm.z );

        // uint idx = id << 30;

        morton[ id ] = uvec2( m, id );
    }
)";