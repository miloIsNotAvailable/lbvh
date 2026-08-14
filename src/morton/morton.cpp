#include <morton.hpp>

inline constexpr const char * header= R"(#version 430

    layout(local_size_x = 64) in;

    struct AABB {
        vec4 bmin; 
        vec4 bmax;
    };

    struct Triangle {
        vec4 u, v, w;
        uvec4 quantized;
        vec4 c;
        AABB aabb;
        int matId;
    };
    
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

GLuint createQuantizeShader() {
    std::string source = std::string(header) + R"(
    
    unsigned int expandBits(unsigned int v)
    {
        v = (v * 0x00010001u) & 0xFF0000FFu;
        v = (v * 0x00000101u) & 0x0F00F00Fu;
        v = (v * 0x00000011u) & 0xC30C30C3u;
        v = (v * 0x00000005u) & 0x49249249u;
        return v;
    }

    // Calculates a 30-bit Morton code for the
    // given 3D point located within the unit cube [0,1].
    unsigned int morton3D(float x, float y, float z)
    {
        x = min(max(x * 1024.0f, 0.0f), 1023.0f);
        y = min(max(y * 1024.0f, 0.0f), 1023.0f);
        z = min(max(z * 1024.0f, 0.0f), 1023.0f);
        unsigned int xx = expandBits(uint(x));
        unsigned int yy = expandBits(uint(y));
        unsigned int zz = expandBits(uint(z));
        return xx * 4 + yy * 2 + zz;
    }
    
    void main()
    {
        uint id = gl_GlobalInvocationID.x;

        if (id >= triangles.length())
            return;
        
        // input[ id ] = 
        vec4 norm = (triangles[id].c - scene.bmin) / (scene.bmax - scene.bmin);
        triangles[id].quantized = uvec4( norm * 1023.f );
    }
    )";

    GLuint shader = compileShader( source );
    GLuint computeProgram = linkProgram( shader );
    return computeProgram;
}

GLuint createMortonShader() {
    std::string source = std::string(header) + R"(
    
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

    GLuint shader = compileShader( source );
    GLuint computeProgram = linkProgram( shader );
    return computeProgram;
}