#include <bounce.hpp>

void Contribution::emissive(
        uint32_t size,
        Buffer &raysIn, 
        Buffer &hitT, 
        Buffer &dead, 
        Buffer &hitEm, 
        Buffer &L, 
        Buffer &beta,
        Buffer &light,
        uint32_t b
) {

    bounce.update( &b, sizeof( uint32_t ) );

    GLuint idx = 0;
    raysIn.bindGPU( idx );
    hitT.bindGPU( idx );
    dead.bindGPU( idx );
    hitEm.bindGPU( idx );
    L.bindGPU( idx );
    beta.bindGPU( idx );
    light.bindGPU( idx );
    pdf_bsdf.bindGPU( idx );
    bounce.bindGPU( idx );

    addEmissive( (size + 63) / 64, 1, 1 );
    barrier(GL_SHADER_STORAGE_BARRIER_BIT);
}


void Contribution::contribute(
        uint32_t size,
        Buffer &raysIn,
        Buffer &shadowRays,
        Buffer &hitT,
        Buffer &dead, 
        Buffer &triangles,
        Buffer &triIds, 
        Buffer &L,
        Buffer &beta,
        Buffer &light,
        Buffer &materials,
        Buffer &normals,
        Buffer &occluded,
        Buffer &dist2
    ) 
{
    GLuint idx = 0;
    raysIn.bindGPU( idx );
    shadowRays.bindGPU( idx );
    hitT.bindGPU( idx );
    dead.bindGPU( idx );
    triangles.bindGPU( idx );
    triIds.bindGPU( idx );
    L.bindGPU( idx );
    beta.bindGPU( idx );
    light.bindGPU( idx );
    materials.bindGPU( idx );
    normals.bindGPU( idx );
    occluded.bindGPU( idx );
    dist2.bindGPU( idx );

    addDirect( (size + 63) / 64, 1, 1 );
    barrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void Contribution::sampleBSDF(
        uint32_t size,
        Buffer &raysIn,
        Buffer &dead, 
        Random2D rand2d,
        Buffer &rand1d,
        Buffer &triIds, 
        Buffer &normals,
        Buffer &triangles,
        Buffer &materials,
        Buffer &hitT,
        Buffer &beta
    )
{
    GLuint idx = 0;
    raysIn.bindGPU( idx );
    dead.bindGPU( idx );
    rand2d.toGPU( idx );
    rand1d.bindGPU( idx );
    pdf_bsdf.bindGPU( idx );
    triIds.bindGPU( idx );
    normals.bindGPU( idx );
    triangles.bindGPU( idx );
    materials.bindGPU( idx );
    bounce.bindGPU( idx );
    hitT.bindGPU( idx );
    beta.bindGPU( idx );

    sampleDir( (size + 63) / 64, 1, 1 );
    barrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void Contribution::addColor( uint32_t size, Buffer &L, Buffer &outCol, Buffer &iter ) {

    GLuint idx = 0;
    L.bindGPU( idx );
    outCol.bindGPU( idx );
    
    idx = 0;
    iter.bindGPU( idx );

    addCol( (size + 63) / 64, 1, 1 );
    barrier(GL_SHADER_STORAGE_BARRIER_BIT);
}