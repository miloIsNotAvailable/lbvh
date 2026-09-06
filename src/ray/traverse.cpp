#include <traverse.hpp>

Buffer& Traverse::operator()( 
    uint32_t size, 
    Buffer &rays, 
    Buffer &lbvh, 
    Buffer &triangles, 
    Buffer &tIds,
    Buffer &light ) {

    GLuint id = 0;
    rays.bindGPU( id );

    t.bindGPU( id );

    dead.bindGPU( id );
    lbvh.bindGPU( id );
    triangles.bindGPU( id );
    tIds.bindGPU( id );
    triIds.bindGPU( id );
    light.bindGPU( id );
    hitEmissive.bindGPU( id );

    traverse( (size + 63) / 64, 1, 1 );
    barrier(GL_SHADER_STORAGE_BARRIER_BIT);

    return t;
}