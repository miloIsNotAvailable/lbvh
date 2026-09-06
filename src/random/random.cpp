#include <randoms.hpp>

void WhiteNoise::seed( uint32_t size, Buffer &iter) {

    GLuint idx=0;
    states.bindGPU( idx );

    idx = 0;
    iter.bindGPU( idx );

    seedProgram( (size + 63) / 64, 1, 1 );
    barrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void WhiteNoise::Rand01(Buffer& out, uint32_t size)
{
    GLuint idx = 0;

    states.bindGPU(idx);
    out.bindGPU(idx);

    rand01((size + 63) / 64, 1, 1);
    barrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

Buffer& WhiteNoise::random1D( uint32_t size ) {

    Rand01( rX, size );

    return rX;
}

Random2D WhiteNoise::random2D( uint32_t size ) {

    Random2D outRand(size);

    Rand01(outRand.X(), size);
    Rand01(outRand.Y(), size);

    return outRand;
}

void Sobol::rand( Buffer &inp, uint32_t size, uint32_t d, uint32_t seed_, uint32_t i ) {

    dim.update( &d, sizeof(uint32_t) );
    seed.update( &seed_, sizeof(uint32_t) );
    iter.update( &i, sizeof(uint32_t) );

    GLuint id = 0;
    inp.bindGPU( id );
    v_k.bindGPU( id );
    iter.bindGPU( id );
    id = 0;
    dim.bindGPU( id );
    seed.bindGPU( id );

    sample1D((size + 63) / 64, 1, 1);
    barrier(GL_SHADER_STORAGE_BARRIER_BIT);

}

Buffer& Sobol::random1D( uint32_t size, uint32_t& d, uint32_t seed_, uint32_t i ) {

    rand( data, size, d, seed_, i );
    
    d+=1;

    return data;
}

Random2D Sobol::random2D( uint32_t size, uint32_t& d, uint32_t seed_, uint32_t i ) {

    Random2D rand2D( size );

    rand( rand2D.X(), size, d, seed_ ^ d, i );
    rand( rand2D.Y(), size, d + 1, seed_ ^ (d+1), i );

    d+=2;

    return rand2D;
}