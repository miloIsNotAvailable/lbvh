#pragma once

#include <string>
#include <utils.hpp>


Buffer blellochScan( Buffer &input, uint32_t size, uint32_t wgSize );

inline const std::string scanScanSrc = R"(
#version 430

layout(local_size_x = 64) in;

layout(std430, binding = 0) buffer Input
{
    uint input[];
};

layout(std430, binding = 1) buffer Scans
{
    uint scan[];
};

layout(std430, binding = 2) buffer WgScans
{
    uint wgSum[];
};

layout(std430, binding = 3) buffer Dispatch
{
    uint gX;
    uint gY;
    uint gZ;
    uint sizeCount;
};

const uint THREADS = 64u;
shared uint temp[ THREADS ];

void main()
{
    uint gid = gl_GlobalInvocationID.x;
    uint lid = gl_LocalInvocationID.x;
    uint wid = gl_WorkGroupID.x;

    temp[lid] = gid < sizeCount ? input[gid] : 0u;
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

    if (gid < sizeCount) scan[gid] = temp[lid];
}
)";

inline const std::string addScansSrc = R"(
#version 430

layout(local_size_x = 64) in;

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

inline const std::string updateDispatchSrc = R"(
#version 430

layout(local_size_x = 1) in;

layout(std430, binding = 0) buffer Counter
{
    uint sizeCount;
};

const uint THREADS = 64u;

layout(std430, binding = 1) buffer Dispatch
{
    uint gX;
    uint gY;
    uint gZ;
    uint counter;
};

void main()
{

    counter = sizeCount;    

    if( sizeCount <= 1 ) {
        gX = 0;
        gY = 1;
        gZ = 1;

        return;
    }

    gX = (sizeCount + THREADS - 1) / THREADS;
    gY = 1;
    gZ = 1;

    // sizeCount = gX;
}
)";

inline const std::string updateCounterSrc = R"(
#version 430

layout(local_size_x = 1) in;

layout(std430, binding = 0) buffer Counter
{
    uint sizeCount;
};

layout(std430, binding = 1) buffer Dispatch
{
    uint gX;
    uint gY;
    uint gZ;
};

void main()
{
    sizeCount = gX;
}
)";

struct Scan {
    Buffer scan;
    Buffer sum;
    Buffer dispatch;

    Scan(Buffer&& scan_, Buffer&& sum_, Buffer&& sSize)
        : scan(std::move(scan_)),
          sum(std::move(sum_)), dispatch( sSize )
    {}
};

struct WgDispatch {
    uint32_t groupX;
    uint32_t groupY;
    uint32_t groupZ;
    uint32_t count;

    WgDispatch( uint32_t gX, uint32_t gY, uint32_t gZ, uint32_t c ) 
    : groupX( gX ), groupY( gY ), groupZ( gZ ), count( c )
    {};
    
    WgDispatch() = default;
};

class BlellochScan {

    private:
    std::vector<Scan> levels;
    size_t maxSize;
    uint32_t wgSize;

    Program scanSums, addSums, updateCounter, updateDispatch;
    Buffer sizeCount;

    public:
    BlellochScan( size_t size, uint32_t THREADS ) 
    : maxSize(size),
      wgSize(THREADS),
      scanSums(scanScanSrc),
      addSums(addScansSrc),
      updateCounter( updateCounterSrc ),
      updateDispatch( updateDispatchSrc) {

        Buffer sc( 
            GL_SHADER_STORAGE_BUFFER,
            sizeof(uint32_t),
            &size,
            GL_DYNAMIC_COPY
        );
        sizeCount = std::move( sc );

        // std::vector<uint32_t> e = sizeCount.toCPU<uint32_t>();
        // printf( "size count: %d\n", e[0] );

        size_t n = size;

        while (n > 1) {
            // n = (n + THREADS - 1) / THREADS;

            Buffer flagsSSBO(
                GL_SHADER_STORAGE_BUFFER,
                n * sizeof(uint32_t),
                nullptr,
                GL_DYNAMIC_COPY
            );

            n = (n + wgSize - 1) / wgSize;

            Buffer wgSumSSBO(
                GL_SHADER_STORAGE_BUFFER,
                n * sizeof(uint32_t),
                nullptr,
                GL_DYNAMIC_COPY
            );

            WgDispatch initDispVals( n, 1, 1, n );

            Buffer wgDispatch(
                GL_SHADER_STORAGE_BUFFER,
                sizeof(WgDispatch),
                nullptr,
                GL_DYNAMIC_COPY
            );

            Scan s( std::move(flagsSSBO), std::move(wgSumSSBO), std::move(wgDispatch) );

            levels.push_back( std::move(s) );
        }

    }

    Buffer &counter() {
        return sizeCount;
    }

    Buffer& operator ()( Buffer &input, uint32_t size, uint32_t wgSize );
};