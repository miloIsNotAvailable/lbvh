#include <lbvh.hpp>

Buffer& LBVH::operator()( Buffer &trianglesSSBO  ) {

    GLuint numElements = size;
    GLuint localSize   = 64;

    GLuint groups = (numElements + localSize - 1) / localSize;

    mortonSSBO.toGPU(0);
    trianglesSSBO.toGPU(1);
    sceneUBO.toGPU(0);
    morton( groups, 1, 1 );

    barrier(GL_SHADER_STORAGE_BARRIER_BIT);

    for( int i = 0; i < 30; i ++ ) {
        Buffer &out = sort( mortonSSBO, i );

        barrier(GL_SHADER_STORAGE_BARRIER_BIT);

        triIdASSBO.toGPU( 0 );
        triIdBSSBO.toGPU( 1 );

        mortonSSBO.toGPU( 2 );
        mortonBSSBO.toGPU( 3 );

        out.toGPU( 4 );

        trSwapper( (size + 63) / 64, 1, 1 );

        barrier(GL_SHADER_STORAGE_BARRIER_BIT);

        std::swap( mortonSSBO, mortonBSSBO );
        std::swap( triIdASSBO, triIdBSSBO );
    }


    mortonSSBO.toGPU(0);
    bvhSSBO.toGPU(1);
    triangleSizeUBO.toGPU(0);

    lbvh( groups, 1, 1 );
    barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    // literally what the fuck it wont work here
    // hence aabbs computed outside
    // std::vector<Triangle> tr  = trianglesSSBO.toCPU<Triangle>();
    // for( auto& t : tr ) {
    //     printf( "%f, %f, %f\n", t.aabb.bmax.x, t.aabb.bmax.y, t.aabb.bmax.z );
    // }


    return bvhSSBO;
}