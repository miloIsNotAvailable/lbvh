#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <string>

// #include "../../utils/headers/utils.hpp"
#include <utils.hpp>

// template<size_t N>
// struct RadixData {
//     std::vector<uint32_t, N> input, extractData, scanData, scatterData, outputData;
//     RadixData( const std::vector<uint32_t> &data, size_t size ) 
//     : input( data ) {} 

// };

// class Buffer {
    
//     private: 
//     GLuint buffer;
//     public: 
//     Buffer() {
//         glGenBuffers( 1, &buffer );
//     }
// };

GLuint createExtractShader();
GLuint createScanShader();
GLuint createScatterShader();

GLuint generateBuffer( GLenum target, GLsizeiptr size, const void * data, GLenum usage );
void modifyBufferData(
    GLuint buffer,
    GLenum target,
    GLintptr offset,
    GLsizeiptr size,
    const void* data);

// void dispatchProgram( GLuint u, GLuint v, GLuint w, GLuint computeProgram );

void extract( std::vector<uint32_t> data );
void scan( std::vector<uint32_t> data );
void scatter( std::vector<uint32_t> data );

// void bindBuffer( GLuint &ssbo, GLsizeiptr size, const void * data, GLenum usage );

class Radix {

    private: 
    size_t size;
    GLuint inputSSBO, extractSSBO, outputSSBO, uniformUBO;
    GLuint extractProgram, scanProgram, scatterProgram;

    public: 
    Radix( GLuint inputSSBO, size_t size );
    GLuint operator()( uint32_t bit, GLuint gX, GLuint gY, GLuint t );
};

inline constexpr const char * header = R"(#version 430

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

    layout(std430, binding = 0) buffer Data
    {
        uvec2 input[];
    };

    layout(std430, binding = 1) buffer TriIn
    {
        Triangle triangles[];
    };

    layout(std430, binding = 2) buffer Extract
    {
        uvec2 extract[];
    };

    layout(std430, binding = 3) buffer Output
    {
        uvec2 outputData[];
    };

    layout(std430, binding = 6) buffer TriOut
    {
        Triangle trianglesOut[];
    };

    layout(std140, binding = 0) uniform Params
    {
        uint bit;
    };
)";


inline const std::string radixExtractSrc = std::string(header) + R"(
void main()
{
    uint id = gl_GlobalInvocationID.x;
    extract[id].x = 1u - ((input[id].x >> bit) & 1u);
    extract[id].y = input[id].y;
}
)";

inline const std::string radixScanSrc = std::string(header) + R"(
void main()
{
    uint id = gl_GlobalInvocationID.x;

    for( uint i = 1; i < extract.length(); i *= 2 ) {

        uint id1 = id + 1;
        uint idx1 = id1 * i * 2;
        
        uint idx0 = idx1 - 1;
        
        if (idx0 < extract.length())
        {
            extract[idx0].x += extract[idx0 - i].x;
        }
        // scan[ idx0 ] = extract[ idx0 - i ] + extract[ idx0 ];
        barrier();
    }

    if (id == 0)
        extract[extract.length() - 1].x = 0;

    barrier();

    for( uint i = extract.length() / 2; i > 0; i /= 2 ) {

        uint id1 = id + 1;
        uint idx1 = id1 * i * 2;
        
        uint idx0 = idx1 - 1;

        uint idxRight0 = idx0;
        uint idxLeft0 = idx0 - i;

        
        if (idx0 < extract.length())
        {
            uint temp = extract[idxLeft0].x;
            extract[idxLeft0].x = extract[idxRight0].x;
            extract[idxRight0].x += temp;
        }
        // scan[ idx0 ] = extract[ idx0 - i ] + extract[ idx0 ];
        barrier();
    }
}
)";

inline const std::string radixScatterSrc = std::string(header) + R"(
void main()
{
    uint id = gl_GlobalInvocationID.x;

    if (id >= input.length())
        return;

    uint b = (input[id].x >> bit) & 1u;

    uint totalZeros = extract[extract.length() - 1].x + 
                    (1u - ((input[extract.length() - 1].x >> bit) & 1u));

    uint ind;

    if (b == 0u)
    {
        ind = extract[id].x;
    }
    else
    {
        uint onesBefore = id - extract[id].x;
        ind = totalZeros + onesBefore;
    }

    outputData[ind] = input[id];
    // trianglesOut[ind] = triangles[id];
    }
)";

inline const std::string radixReorderTrianglesSrc = std::string(header) + R"(
void main()
{
    uint id = gl_GlobalInvocationID.x;

    if (id >= triangles.length())
        return;

    uint triangleIndex = input[id].y;

    trianglesOut[id] = triangles[triangleIndex];
    }
)";
