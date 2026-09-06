#include <shadowRays.hpp>

Buffer& ShadowRays::generate( 
    uint32_t size, 
    Buffer& rays, 
    Buffer& hitT, 
    Buffer &triIds, 
    Buffer &normals, 
    Buffer &dead, 
    Buffer &light, 
    Random2D random2d ) {

    GLuint idx = 0;

    rays.bindGPU( idx );
    outRays.bindGPU( idx );
    hitT.bindGPU( idx );
    light.bindGPU( idx );
    dead.bindGPU( idx );
    triIds.bindGPU( idx );
    normals.bindGPU( idx );
    tmax.bindGPU( idx );
    random2d.toGPU( idx );
    // nor.toGPU( idx );

    genShadowRays( (size + 63) / 64, 1, 1 );
    barrier(GL_SHADER_STORAGE_BARRIER_BIT);

    return outRays;
}

Buffer& ShadowRays::traverse( 
        uint32_t size, 
        Buffer &lbvh, 
        Buffer &triangles, 
        Buffer &dead, 
        Buffer &tIds, 
        Buffer &light,
        Buffer &normals ) 
{

    GLuint idx = 0;
    outRays.bindGPU( idx );
    occluded.bindGPU( idx );
    dead.bindGPU( idx );
    lbvh.bindGPU( idx );
    triangles.bindGPU( idx );
    tIds.bindGPU( idx );
    tmax.bindGPU( idx );
    light.bindGPU( idx );
    nor.bindGPU( idx );

    traverseShadowRays( (size + 63) / 64, 1, 1 );
    barrier(GL_SHADER_STORAGE_BARRIER_BIT);

    return occluded;
}