#include <denoise.hpp>

void AtrousDenoiser::saveGBuffers(
        uint32_t size,
        Buffer &dead,
        Buffer &nor,
        Buffer &triIds,
        Buffer &triangles,
        Buffer &materials,
        Buffer &rays,
        Buffer &hitT,
        Buffer &scene
) {

    GLuint idx = 0;
    depth.bindGPU( idx );
    normals.bindGPU( idx );
    valid.bindGPU( idx );
    dead.bindGPU( idx );
    nor.bindGPU( idx );
    triIds.bindGPU( idx );
    triangles.bindGPU( idx );
    materials.bindGPU( idx );
    rays.bindGPU( idx );
    hitT.bindGPU( idx );

    idx = 0;
    scene.bindGPU( idx );

    saveBufferInfo((size + 63) / 64, 1, 1);
    barrier(GL_SHADER_STORAGE_BARRIER_BIT);    
}

Buffer& AtrousDenoiser::operator()(
        uint32_t size,
        Buffer &colors,
        Buffer &camera
) {

    for( int i = 0; i < 4; i ++ ) {

        int step = 1 << i;

        sw.update( &step, sizeof(int) );
        
        GLuint idx = 0;
        colors.bindGPU( idx );
        normals.bindGPU( idx );
        depth.bindGPU( idx );
        valid.bindGPU( idx );
        offsets.bindGPU( idx );
        kernel.bindGPU( idx );
        outPixel.bindGPU( idx );
        
        idx = 0;
        sw.bindGPU( idx );
        camera.bindGPU( idx );

        denoise((size + 63) / 64, 1, 1);
        barrier(GL_SHADER_STORAGE_BARRIER_BIT); 

        std::swap( colors, outPixel );
    }

    return colors;
}