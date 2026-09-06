#include <primaryRays.hpp>


Buffer& ThinLens::operator()( uint32_t size, Random2D &random, Film &film ) {

    GLuint idx = 0;
    outRays.bindGPU( idx );
    random.toGPU( idx );

    idx = 0;
    film.camera.bindGPU(idx);
    film.dims.bindGPU(idx);
    film.iteration.bindGPU(idx);

    genRays( (size + 63) / 64, 1, 1 );
    barrier(GL_SHADER_STORAGE_BARRIER_BIT);

    return outRays;
}