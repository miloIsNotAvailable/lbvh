#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <string>

// #include "../../utils/headers/utils.hpp"
#include <utils.hpp>
#include <scan.hpp>

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

inline const std::string swapperSrc = R"(
#version 430

layout(local_size_x = 64) in;

layout(std430, binding = 0) buffer TriIn
{
    uint nums[];
};

layout(std430, binding = 1) buffer TriOut
{
    uint numsOut[];
};

layout(std430, binding = 2) buffer IdsIn
{
    uint offsets[];
};

void main() {
    uint id = gl_GlobalInvocationID.x;

    if (id >= nums.length())
        return;

    numsOut[ offsets[id] ] = nums[id];
}
)";

const std::string radixLayout = R"(#version 430

    layout(local_size_x = 64) in;)";

inline const std::string header = radixLayout + structs + R"(
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


inline const std::string radixExtractSrc = radixLayout + structs + R"(

layout(std430, binding = 0) buffer Data
{
    uint input[];
};

layout(std430, binding = 1) buffer Extract
{
    uint extract[];
};

layout(std140, binding = 0) uniform Params
{
    uint bit;
};

void main()
{
    uint id = gl_GlobalInvocationID.x;

    if( id >= extract.length() ) return;

    extract[id] = 1u - ((input[id] >> bit) & 1u);
    // extract[id].y = input[id].y;
}
)";

inline const std::string radixScanSrc = header + R"(
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

inline const std::string radixScatterSrc = radixLayout + structs + R"(

layout(std430, binding = 0) buffer Data
{
    uint morton[];
};

layout(std430, binding = 1) buffer Scan
{
    uint scanned[];
};

// layout(std430, binding = 2) buffer trIds
// {
//     uint triIds[];
// };

layout(std430, binding = 2) buffer OutMorton
{
    uint outData[];
};

// layout(std430, binding = 4) buffer OuttrIds
// {
//     uint outTriIds[];
// };

layout(std140, binding = 0) uniform Params
{
    uint bit;
};

void main()
{
    uint id = gl_GlobalInvocationID.x;

    if (id >= morton.length())
        return;

    uint b = (morton[id] >> bit) & 1u;

    uint totalZeros = scanned[morton.length() - 1] +  (1u - ((morton[morton.length() - 1] >> bit) & 1u));

    uint ind;

    if (b == 0u)
    {
        ind = scanned[id];
    }
    else
    {
        uint onesBefore = id - scanned[id];
        ind = totalZeros + onesBefore;
    }

    outData[id] = ind;
    // outTriIds[ind] = triIds[id];
    }
)";

inline const std::string radixReorderTrianglesSrc = header + R"(
void main()
{
    uint id = gl_GlobalInvocationID.x;

    if (id >= triangles.length())
        return;

    uint triangleIndex = input[id].y;

    trianglesOut[id] = triangles[triangleIndex];
    }
)";

/**
 * @brief Radix sort usage example.
 *
 * Pass the unpadded data size to the Radix constructor.
 *
 * @example
 * std::vector<uint32_t> data = { 4, 2, 3, 1, 5 };
 *
 * data.resize(64, 0);
 *
 * Buffer inpA(
 *     GL_SHADER_STORAGE_BUFFER,
 *     data.size() * sizeof(uint32_t),
 *     data.data(),
 *     GL_DYNAMIC_COPY
 * );
 *
 * Buffer inpB(
 *     GL_SHADER_STORAGE_BUFFER,
 *     data.size() * sizeof(uint32_t),
 *     data.data(),
 *     GL_DYNAMIC_COPY
 * );
 *
 * Program swapper(swapperSrc);
 *
 * Radix sort(data.size(), 64);
 *
 * for (int i = 0; i < 32; i++) {
 *     Buffer& out = sort(inpA, i);
 *
 *     inpA.toGPU(0);
 *     inpB.toGPU(1);
 *     out.toGPU(2);
 *
 *     swapper((data.size() + 63) / 64, 1, 1);
 *     barrier(GL_SHADER_STORAGE_BARRIER_BIT);
 *
 *     std::swap(inpA, inpB);
 * }
 *
 * std::vector<uint32_t> a = inpB.toCPU<uint32_t>();
 *
 * for (auto& d : a) {
 *     printf("%d\n", d);
 * }
 *
 * std::cout << "\n\n";
 */
class Radix {

    private: 
    size_t size, groups;

    Buffer input, uniformUBO, extractSSBO, outputSSBO;

    Program extract;
    Program scan;
    Program scatter;

    BlellochScan scanner;

    public: 
    Radix() = default;
    Radix( size_t size, uint32_t THREADS ) 
    : extract( radixExtractSrc ),
    scanner(  size, THREADS ),
    scatter( radixScatterSrc ),

    extractSSBO( 
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY
    ),

    outputSSBO( 
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY
    )
    {

        Buffer uUBO(
            GL_UNIFORM_BUFFER,
            sizeof(uint32_t),
            nullptr,
            GL_DYNAMIC_DRAW
        );

        uniformUBO = std::move(uUBO);

        groups = (size + THREADS - 1) / THREADS;
    }
    Buffer& operator()( Buffer& input, uint32_t bit );
};
