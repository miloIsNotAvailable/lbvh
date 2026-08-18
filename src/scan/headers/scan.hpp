#pragma once

#include <string>
#include <utils.hpp>

struct Scan {
    Buffer scan;
    Buffer sum;
    uint32_t sumSize;

    Scan(Buffer&& scan_, Buffer&& sum_, uint32_t sSize)
        : scan(std::move(scan_)),
          sum(std::move(sum_)), sumSize( sSize )
    {}
};

Buffer blellochScan( std::vector<uint32_t> flags, uint32_t wgSize );

inline const std::string scanScanSrc = R"(
#version 430

layout(local_size_x = 2) in;

layout(std430, binding = 0) buffer Scans
{
    uint scan[];
};

layout(std430, binding = 1) buffer WgScans
{
    uint wgSum[];
};

const uint THREADS = 2u;
shared uint temp[ THREADS ];

void main()
{
    uint gid = gl_GlobalInvocationID.x;
    uint lid = gl_LocalInvocationID.x;
    uint wid = gl_WorkGroupID.x;

    temp[lid] = gid < scan.length() ? scan[gid] : 0u;
    barrier();

    for( uint i = 1; i < THREADS; i *= 2 ) {

        uint id1 = lid + 1;
        uint idx1 = id1 * i * 2;
        
        uint idx0 = idx1 - 1;
        
        if (idx0 < THREADS)
        {
            temp[idx0] += temp[idx0-i];
        }

        barrier();
    }

    if (lid == 0) {
        wgSum[ wid ] = temp[THREADS - 1u];
        temp[THREADS - 1u] = 0;
    }

    barrier();

    for( uint i = THREADS / 2; i > 0; i /= 2 ) {

        uint id1 = lid + 1;
        uint idx1 = id1 * i * 2;
        
        uint idx0 = idx1 - 1;

        uint idxRight0 = idx0;
        uint idxLeft0 = idx0 - i;

        
        if (idx0 < THREADS)
        {
            uint t = temp[idxLeft0];
            temp[idxLeft0] = temp[idxRight0];
            temp[idxRight0] += t;
        }

        barrier();
    }

    if (gid < scan.length()) scan[gid] = temp[lid];
}
)";

inline const std::string addScansSrc = R"(
#version 430

layout(local_size_x = 2) in;

layout(std430, binding = 0) buffer PScans
{
    uint parent[];
};

layout(std430, binding = 1) buffer CScans
{
    uint child[];
};

void main()
{
    uint gid = gl_GlobalInvocationID.x;
    uint lid = gl_LocalInvocationID.x;
    uint wid = gl_WorkGroupID.x;


    if (gid >= child.length())
        return;

    child[gid] += parent[wid];
}
)";