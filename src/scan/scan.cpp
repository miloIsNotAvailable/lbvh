#include <scan.hpp>

Buffer& BlellochScan::operator()(  Buffer &input, uint32_t size, uint32_t wgSize ) {

    size_t wgSumSize = size;

    Buffer *curr = &input; 

    size_t n = size;
    int levelsNeeded = 0;

    while (n > 1) {
        n = (n + wgSize - 1) / wgSize;
        levelsNeeded ++;
    }

    std::vector<uint32_t> sizes(levelsNeeded);
    for( int i = 0; i < levelsNeeded; i ++ ) {

        // Buffer *flagsSSBO = &levels[i].scan;
        // Buffer *wgSumSSBO = &levels[i].sum;

        // Buffer flagsSSBO(
        //     GL_SHADER_STORAGE_BUFFER,
        //     wgSumSize * sizeof(uint32_t),
        //     nullptr,
        //     GL_DYNAMIC_COPY
        // );
        
        sizeCount.update( &wgSumSize, sizeof(uint32_t) );

        sizes[i] = wgSumSize;
        wgSumSize = (wgSumSize + wgSize - 1) / wgSize;

        // Buffer wgSumSSBO(
        //     GL_SHADER_STORAGE_BUFFER,
        //     wgSumSize * sizeof(uint32_t),
        //     nullptr,
        //     GL_DYNAMIC_COPY
        // );

        curr->toGPU(0);
        levels[i].scan.toGPU(1);
        levels[i].sum.toGPU(2);
        sizeCount.toGPU(3);

        scanSums( wgSumSize, 1, 1 );
        barrier( GL_SHADER_STORAGE_BARRIER_BIT );

        // scanData = wgSumSSBO.toCPU<uint32_t>();

        // Scan scan_( std::move(flagsSSBO), std::move(wgSumSSBO), wgSumSize );
        // levels.push_back( std::move( scan_ ) );       

        curr = &levels[i].sum;
    }

    for( int i = levelsNeeded - 1; i > 0; i-- ) {
        // Scan &l = levels[i];
        // std::vector<uint32_t> t = l.sum.toCPU<uint32_t>();
        
        Scan& parent = levels[i];
        Scan& child  = levels[i - 1];

        // std::vector<uint32_t> p = parent.scan.toCPU<uint32_t>();
        // std::vector<uint32_t> c = child.scan.toCPU<uint32_t>();

        parent.scan.toGPU( 0 );
        child.scan.toGPU( 1 );
        
        // printf( "%d, %d\n", sizes[i], child.sumSize );
        // GLuint groups = (child.sumSize + wgSize - 1) / wgSize;
        addSums( sizes[i], 1, 1 );
        barrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // printf( "%d\n", i );
        // for( int i = 0; i < p.size(); i++ ) {
        //     printf( "%d,", p[i] );
        // }
        
        // std::cout << "\n";

        // for( int i = 0; i < c.size(); i++ ) {
        //     printf( "%d,", c[i] );
        // }
        
        // std::cout << "\n\n";
    }

    return levels[0].scan;
}

// Buffer blellochScan( Buffer &input, uint32_t size, uint32_t wgSize ) {
//     std::vector<Scan> levels;
//     size_t wgSumSize = size;

//     // std::vector<uint32_t> scanData = flags;

//     Buffer *curr = &input; 

//     Program scanSums( scanScanSrc );
//     Program addSums( addScansSrc );

//     while( wgSumSize > 1 ) {

//         Buffer flagsSSBO(
//             GL_SHADER_STORAGE_BUFFER,
//             wgSumSize * sizeof(uint32_t),
//             nullptr,
//             GL_DYNAMIC_COPY
//         );

//         wgSumSize = (wgSumSize + wgSize - 1) / wgSize;

//         Buffer wgSumSSBO(
//             GL_SHADER_STORAGE_BUFFER,
//             wgSumSize * sizeof(uint32_t),
//             nullptr,
//             GL_DYNAMIC_COPY
//         );

//         curr->toGPU(0);
//         flagsSSBO.toGPU(1);
//         wgSumSSBO.toGPU(2);

//         scanSums( wgSumSize, 1, 1 );
//         barrier( GL_ALL_BARRIER_BITS );

//         // scanData = wgSumSSBO.toCPU<uint32_t>();

//         Scan scan_( std::move(flagsSSBO), std::move(wgSumSSBO), wgSumSize );
//         levels.push_back( std::move( scan_ ) );       

//         curr = &levels.back().sum;
//     }

//     for( int i = levels.size() - 1; i > 0; i-- ) {
//         // Scan &l = levels[i];
//         // std::vector<uint32_t> t = l.sum.toCPU<uint32_t>();
        
//         Scan& parent = levels[i];
//         Scan& child  = levels[i - 1];

//         std::vector<uint32_t> p = parent.scan.toCPU<uint32_t>();
//         std::vector<uint32_t> c = child.scan.toCPU<uint32_t>();

//         parent.scan.toGPU( 0 );
//         child.scan.toGPU( 1 );
        
//         // GLuint groups = (child.sumSize + wgSize - 1) / wgSize;
//         addSums( child.sumSize, 1, 1 );
//         barrier(GL_SHADER_STORAGE_BARRIER_BIT);

//         // printf( "%d\n", i );
//         // for( int i = 0; i < p.size(); i++ ) {
//         //     printf( "%d,", p[i] );
//         // }
        
//         // std::cout << "\n";

//         // for( int i = 0; i < c.size(); i++ ) {
//         //     printf( "%d,", c[i] );
//         // }
        
//         // std::cout << "\n\n";
//     }

//     return std::move( levels[0].scan );
// }