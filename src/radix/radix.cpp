#include <radix.hpp>


Buffer& Radix::operator()( Buffer& input, uint32_t bit ) {
    uniformUBO.update(
        &bit,
        sizeof(bit)
    );

    input.toGPU(0);
    extractSSBO.toGPU(1);
    uniformUBO.toGPU(0);

    extract( groups, 1, 1 );
    barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    // std::vector<uint32_t> e = extractSSBO.toCPU<uint32_t>();
    // printf( "extract: " );
    // for( auto &d : e ) {
    //     printf( "%d,", d );
    // }
    // printf( "\n" );

    // scan( groups, 1, 1 );
    // barrier( GL_SHADER_STORAGE_BARRIER_BIT );
    // Buffer scanned = blellochScan( extractSSBO, triIds.size(), 64 );
    // scanner.counter().update( &groups, sizeof(uint32_t) );
    Buffer &scanned = scanner( extractSSBO, 0, 64 );
    barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    // e = scanned.toCPU<uint32_t>();
    // printf( "scan: " );
    // for( auto &d : e ) {
    //     printf( "%d,", d );
    // }
    // printf( "\n" );

    input.toGPU(0);
    scanned.toGPU(1);
    outputSSBO.toGPU(2);

    scatter( groups, 1, 1 );
    barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    // e = outputSSBO.toCPU<uint32_t>();
    // printf( "out: " );
    // for( auto &d : e ) {
    //     printf( "%d,", d );
    // }
    // printf( "\n" );

    return outputSSBO;
}