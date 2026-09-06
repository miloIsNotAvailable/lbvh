#include <ray.hpp>

void Integrator::generateCameraRays() {

    // pos.toGPU(0);
    // dir.toGPU(1);
    
    oX.toGPU(0);
    oY.toGPU(1);
    oZ.toGPU(2);

    dX.toGPU(3);
    dY.toGPU(4);
    dZ.toGPU(5);

    L.toGPU(6);
    beta.toGPU(7);
    states.toGPU(8);
    
    camera.toGPU(0);
    dims.toGPU(1);
    iteration.toGPU(2);

    measureGPU( "gen rays", [&]() {
        uint32_t g = (width * height + 64 - 1) / 64;
        cameraRays( g, 1, 1 );

        L.update( &zeros, zeros.size() * sizeof( glm::vec4 ) );
        beta.update( &ones, ones.size() * sizeof( glm::vec4 ));
    } );

    barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    // std::vector<glm::vec4> p = pos.toCPU<glm::vec4>();
    // std::vector<glm::vec4> dirs = dir.toCPU<glm::vec4>();

    // for(int i = 0; i < 10; i ++) {
    //     glm::vec4 r = dirs[ i ];
    //     glm::vec4 r2 = p[ i ];
    //     printf( "o: %f, %f, %f\n"
    //             "dir: %f, %f, %f\n",
    //         r2.x,r2.y, r2.z,
    //         r.x,r.y, r.z );
    // }
}

void Integrator::traverse( Buffer &lbvh, Buffer &triangles, Buffer &tIds ) {
    oX.toGPU(0);
    oY.toGPU(1);
    oZ.toGPU(2);

    dX.toGPU(3);
    dY.toGPU(4);
    dZ.toGPU(5);
    
    dead.toGPU(6);
    hits.toGPU(7);
    lbvh.toGPU(8);
    triangles.toGPU(9);
    tIds.toGPU(10);
    triIds.toGPU(11);

    uint32_t g = (width * height + 64 - 1) / 64;
    measureGPU( "traverse time", [&] {
        traverse_( g, 1, 1 );
        barrier( GL_SHADER_STORAGE_BARRIER_BIT );
    } );

    std::vector<float> hitVals = hits.toCPU<float>();
    std::vector<uint32_t> deadVals = dead.toCPU<uint32_t>();

    size_t hugeHits = 0;
    size_t deadRays = 0;

    for (size_t i = 0; i < hitVals.size(); ++i)
    {
        if (hitVals[i] > 1e10f)
            ++hugeHits;

        if (deadVals[i] == 1u)
            ++deadRays;
    }

    printf("hits > 1e10: %zu\n", hugeHits);
    printf("dead rays:     %zu\n", deadRays);
}

void Integrator::sampleNewDir( Buffer &normals ) {
    
    dX.toGPU(0);
    dY.toGPU(1);
    dZ.toGPU(2);

    states.toGPU(3);
    triIds.toGPU(4);
    normals.toGPU(5);

    uint32_t g = (width * height + 64 - 1) / 64;
    sampleDirection( g, 1, 1 );
    barrier( GL_SHADER_STORAGE_BARRIER_BIT );
}

void Integrator::radixSortPosAndDir() {


    std::vector<uint32_t> deadCPU = dead.toCPU<uint32_t>();

    std::vector<float> ox = oX.toCPU<float>();
    std::vector<float> oy = oY.toCPU<float>();
    std::vector<float> oz = oZ.toCPU<float>();

    std::vector<float> dx = dX.toCPU<float>();
    std::vector<float> dy = dY.toCPU<float>();
    std::vector<float> dz = dZ.toCPU<float>();

    size_t write = 0;

    for (size_t read = 0; read < deadCPU.size(); ++read)
    {
        if (deadCPU[read] != 0u)
            continue;

        ox[write] = ox[read];
        oy[write] = oy[read];
        oz[write] = oz[read];

        dx[write] = dx[read];
        dy[write] = dy[read];
        dz[write] = dz[read];

        deadCPU[write] = 0u;

        ++write;
    }

    uint32_t activeRays = static_cast<uint32_t>(write);

    size_t rayBytes = activeRays * sizeof(float);

    oX.update(ox.data(), rayBytes);
    oY.update(oy.data(), rayBytes);
    oZ.update(oz.data(), rayBytes);

    dX.update(dx.data(), rayBytes);
    dY.update(dy.data(), rayBytes);
    dZ.update(dz.data(), rayBytes);

    dead.update(
        deadCPU.data(),
        activeRays * sizeof(uint32_t)
    );

    // pos.toGPU(0);
    // dir.toGPU(1);

    oX.toGPU(0);
    oY.toGPU(1);
    oZ.toGPU(2);

    dX.toGPU(3);
    dY.toGPU(4);
    dZ.toGPU(5);

    inpA.toGPU(6);
    scene.toGPU(0);

    uint32_t g = (width * height + 64 - 1) / 64;
    // 1ms with ox, oy, oz
    Buffer *out;
    measureGPU( "encode time", [&] {
        encodeRays( g, 1, 1 );
        barrier( GL_SHADER_STORAGE_BARRIER_BIT );

        inpA.toGPU(0);
        count.toGPU(1);
        countBuckets( g, 1, 1 );
        barrier( GL_SHADER_STORAGE_BARRIER_BIT );        

        out = &scan( count, 0, 64 );
        barrier( GL_SHADER_STORAGE_BARRIER_BIT );
    
        inpA.toGPU(0);
        out->toGPU(1);
        inpB.toGPU(2);
        inpC.toGPU(3);
    
        scatterBuckets( g, 1, 1 );
        barrier( GL_SHADER_STORAGE_BARRIER_BIT );
    } );


    std::vector<uint32_t> buckets = inpA.toCPU<uint32_t>();
    std::vector<uint32_t> counts = count.toCPU<uint32_t>();
    std::vector<uint32_t> sortedIds = inpC.toCPU<uint32_t>();

    const size_t N = sortedIds.size();

    std::vector<float> oxSorted(N);
    std::vector<float> oySorted(N);
    std::vector<float> ozSorted(N);

    std::vector<float> dxSorted(N);
    std::vector<float> dySorted(N);
    std::vector<float> dzSorted(N);

    for (size_t i = 0; i < N; ++i)
    {
        uint32_t src = sortedIds[i];

        oxSorted[i] = ox[src];
        oySorted[i] = oy[src];
        ozSorted[i] = oz[src];

        dxSorted[i] = dx[src];
        dySorted[i] = dy[src];
        dzSorted[i] = dz[src];
    }

    oX.update(oxSorted.data(), oxSorted.size() * sizeof(float));
    oY.update(oySorted.data(), oySorted.size() * sizeof(float));
    oZ.update(ozSorted.data(), ozSorted.size() * sizeof(float));

    dX.update(dxSorted.data(), dxSorted.size() * sizeof(float));
    dY.update(dySorted.data(), dySorted.size() * sizeof(float));
    dZ.update(dzSorted.data(), dzSorted.size() * sizeof(float));

    uint32_t wantedBucket = 2334;
    int printed = 0;

    for (size_t i = 0; i < buckets.size() && printed < 10; ++i)
    {
        if (buckets[i] != wantedBucket)
            continue;

        printf(
            "ray %zu | o=(%.5f %.5f %.5f) d=(%.5f %.5f %.5f)\n",
            i,
            ox[i], oy[i], oz[i],
            dx[i], dy[i], dz[i]
        );

        ++printed;
    }

    // for( int i = 0; i < counts.size(); i ++ ) {
    //     if( counts[i] != 0 )
    //         printf( "bucket: %d, count: %d\n", i, counts[i] );
    // }

    // std::vector<uint32_t> sortedIds = inpC.toCPU<uint32_t>();
    // std::vector<uint32_t> offsets   = out->toCPU<uint32_t>();

    // uint32_t start = offsets[wantedBucket];

    // for (int i = 0; i < 10; ++i)
    // {
    //     printf("%u\n", sortedIds[start + i]);
    // }

}