#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <triangle.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <radix.hpp>
#include <morton.hpp>
#include <lbvh.hpp>
#include <ray.hpp>
#include <loadObj.hpp>
#include <lens.hpp>
#include "stb_image_write.h"
#include <scan.hpp>
#include <numeric>

// std::vector<Triangle> triangles = {
//     Triangle(
//         glm::vec3(10.5, 10.5, 10.5),
//         glm::vec3(11.5, 10.5, 10.5),
//         glm::vec3(10.5, 11.5, 11.5)
//     ),

//     Triangle(
//         glm::vec3(14.5, 14.5, 14.5),
//         glm::vec3(15.5, 14.5, 14.5),
//         glm::vec3(14.5, 15.5, 15.5)
//     ),

//     Triangle(
//         glm::vec3(11.5, 11.5, 11.5),
//         glm::vec3(12.5, 11.5, 11.5),
//         glm::vec3(11.5, 12.5, 12.5)
//     ),

//     Triangle(
//         glm::vec3(12.5, 12.5, 12.5),
//         glm::vec3(13.5, 12.5, 12.5),
//         glm::vec3(12.5, 13.5, 13.5)
//     )
// };

// std::vector<uint32_t> data = {2, 1, 4, 6, 3, 7};

const GLuint WIDTH = 600, HEIGHT = 600;

glm::vec3 e(0., 0., -700.f);
glm::vec3 c(0., 0., 0.f);

float angle = 60.f / 180.f * PI;
float FOV = .5f / tan( angle / 2.f );

Camera camera( FOV, 200.f, 0.f, e, c );

std::vector<Triangle> triangles = {
    Triangle(
        glm::vec3(20, 20, 20),
        glm::vec3(22, 20, 20),
        glm::vec3(20, 22, 22)
    ),
    
    Triangle(
        glm::vec3(40, 40, 40),
        glm::vec3(42, 40, 40),
        glm::vec3(40, 42, 42)
    ),
    
    Triangle(
        glm::vec3(10, 10, 10),
        glm::vec3(12, 10, 10),
        glm::vec3(10, 12, 12)
    ),

    Triangle(
        glm::vec3(30, 30, 30),
        glm::vec3(32, 30, 30),
        glm::vec3(30, 32, 32)
    )
};

int main()
{
    camera.WIDTH = WIDTH;
    camera.HEIGHT = HEIGHT;
    Mesh obj = LoadObj();
    // obj.triangles = triangles;

    // std::vector<Triangle> triangles = obj.triangles;

    // for (size_t i = 0; i < obj.triangles.size(); ++i) {
    //     const Triangle& t = obj.triangles[i];

    //     std::cout << "Triangle " << i << '\n';

    //     std::cout
    //         << "  u: "
    //         << t.u.x << ", "
    //         << t.u.y << ", "
    //         << t.u.z << '\n';

    //     std::cout
    //         << "  v: "
    //         << t.v.x << ", "
    //         << t.v.y << ", "
    //         << t.v.z << '\n';

    //     std::cout
    //         << "  w: "
    //         << t.w.x << ", "
    //         << t.w.y << ", "
    //         << t.w.z << '\n';
    // }

    AABB aabb;

    for( auto& t : obj.triangles ) {
        aabb.grow( t.u );
        aabb.grow( t.v );
        aabb.grow( t.w );
    }

    glm::vec4 center = (aabb.bmin + aabb.bmax) * 0.5f;

    for (auto& t : obj.triangles) {
        t.u -= center;
        t.v -= center;
        t.w -= center;

        t.aabb.bmin -= center;
        t.aabb.bmax -= center;
        t.c         -= center;
    }

    aabb.bmin -= center;
    aabb.bmax -= center;

    // size_t p = 1;
    // while (p < obj.triangles.size())
    //     p <<= 1;

    Triangle dummy(
        glm::vec3(1e10f, 1e10f, 1e10f),
        glm::vec3(1e10f, 1e10f, 1e10f),
        glm::vec3(1e10f, 1e10f, 1e10f)
    );

    // dummy.matId = -1;

    // obj.triangles.resize( p, dummy );
    
    // for (size_t i = 0; i < obj.triangles.size(); ++i) {
    //     const Triangle& t = obj.triangles[i];
    //     glm::vec4 c = (t.u+t.v+t.w) / 3.f;

    //     std::cout << "Triangle " << i << '\n';

    //     std::cout
    //         << "  u: "
    //         << t.u.x << ", "
    //         << t.u.y << ", "
    //         << t.u.z << '\n';

    //     std::cout
    //         << "  v: "
    //         << t.v.x << ", "
    //         << t.v.y << ", "
    //         << t.v.z << '\n';

    //     std::cout
    //         << "  w: "
    //         << t.w.x << ", "
    //         << t.w.y << ", "
    //         << t.w.z << '\n';

    //     std::cout
    //         << "  c: "
    //         << c.x << ", "
    //         << c.y << ", "
    //         << c.z << '\n';
    // }


    std::vector<Ray> rays;
    std::vector<Pixel> pixels;

    for (int i = 0; i < WIDTH * HEIGHT; i ++) {
        
        int x = int(i % WIDTH);
        int y = int(i / WIDTH);

        glm::vec2 fragCoord = glm::vec2(float(x), float(y));
        glm::vec2 st = fragCoord / glm::vec2(WIDTH, HEIGHT) - 0.5f;

        st.x *= float(WIDTH)/float(HEIGHT);

        Lens l = camera.thinLensRay( st );
        // Ray r{ glm::vec4(l.point, 0.), 
        //        glm::vec4(l.dir, 0.),
        //         glm::vec4( 0.f ),
        //         0.f, 1e30f };

        Ray r( l.point, l.dir, 0 );
        rays.push_back( r );

        // Pixel p( i );
        pixels.emplace_back( i );
    }
    
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(
        800,
        600,
        "peepoo",
        nullptr,
        nullptr
    );

    if (!window)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGL(glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    std::cout << "OpenGL version: "
              << glGetString(GL_VERSION)
              << "\n";


    // //   0011 2223 3444 5555
    // std::vector<uint32_t> flags = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1 };
    
    // std::vector<uint32_t> bigFlags( 600 * 600 );
    // std::mt19937 rng(12345);
    // std::bernoulli_distribution dist(0.5);

    // for( auto &d : bigFlags ) {
    //     d = dist( rng ) ? 1u : 0u;
    // }

    // flags.resize( 64, 0 );
    // bigFlags.resize( (( bigFlags.size() + 63 ) / 64) * 64, 0 );

    // uint32_t wgSize = 64;
   
    // Buffer e(
    //     GL_SHADER_STORAGE_BUFFER,
    //     bigFlags.size() * sizeof(uint32_t),
    //     bigFlags.data(),
    //     GL_DYNAMIC_COPY  
    // );
    // BlellochScan scanner( bigFlags.size(), wgSize );
    // // Buffer out = blellochScan( e, flags.size(), wgSize );
    // GLuint query;
    // glGenQueries(1, &query);

    // glBeginQuery(GL_TIME_ELAPSED, query);

    // Buffer out = scanner(e, bigFlags.size(), wgSize);

    // glEndQuery(GL_TIME_ELAPSED);

    // GLuint64 timeNs;
    // glGetQueryObjectui64v(
    //     query,
    //     GL_QUERY_RESULT,
    //     &timeNs
    // );

    // printf("Blelloch scan: %.3f ms\n",
    //     double(timeNs) / 1e6);

    // glDeleteQueries(1, &query);

    // std::vector<uint32_t> c = out.toCPU<uint32_t>();
    // for( int i = 0; i < c.size(); i++ ) {
    //     printf( "%d,", c[i] );
    // }
    
    std::cout << "\n\n";
    
    int THREADS = 64;
    size_t N = obj.triangles.size();
    obj.triangles.resize( ((obj.triangles.size() + THREADS - 1) / THREADS) * THREADS, dummy );

    Buffer trianglesSSBO(
        GL_SHADER_STORAGE_BUFFER,
        obj.triangles.size() * sizeof(Triangle),
        obj.triangles.data(),
        GL_DYNAMIC_COPY
    );

    Buffer mortonSSBO(
        GL_SHADER_STORAGE_BUFFER,
        obj.triangles.size() * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY
    );

    Buffer sceneUBO(
        GL_UNIFORM_BUFFER,
        sizeof(AABB),
        &aabb,
        GL_DYNAMIC_COPY
    );


    GLuint numElements = static_cast<GLuint>(rays.size());
    GLuint localSize   = 64;

    GLuint groups = (numElements + localSize - 1) / localSize;

    Program morton( mortonSrc );

    mortonSSBO.toGPU(0);
    trianglesSSBO.toGPU(1);
    sceneUBO.toGPU(0);
    morton( groups, 1, 1 );

    Buffer extractSSBO(
        GL_SHADER_STORAGE_BUFFER,
        obj.triangles.size() * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY
    );

    Buffer outputSSBO(
        GL_SHADER_STORAGE_BUFFER,
        obj.triangles.size() * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY
    );

    Buffer uniformUBO(
        GL_UNIFORM_BUFFER,
        sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_DRAW
    );

    std::vector<uint32_t> triIds(obj.triangles.size());
    std::iota(triIds.begin(), triIds.end(), 0u);

    // triIds.resize( 64, 100 );

    Buffer triIdsA(
        GL_SHADER_STORAGE_BUFFER,
        triIds.size() * sizeof(uint32_t),
        triIds.data(),
        GL_DYNAMIC_COPY
    );
    
    Buffer triIdsB(
        GL_SHADER_STORAGE_BUFFER,
        triIds.size() * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY
    );

    // trianglesSSBO.toGPU(1);
    // extractSSBO.toGPU(2);
    // outputSSBO.toGPU(3);
    // trianglesSortedSSBO.toGPU(6);
    // uniformUBO.toGPU(0);

    // GLuint extractProgram = createExtractShader();
    // GLuint scanProgram    = createScanShader();
    // GLuint scatterProgram = createScatterShader();

    Program extract( radixExtractSrc );
    Program scan( radixScanSrc );
    Program scatter( radixScatterSrc );

    BlellochScan radixScan( triIds.size(), THREADS );

    for (int i = 0; i < 30; ++i) {

        uniformUBO.update(
            &i,
            sizeof(i)
        );

        mortonSSBO.toGPU(0);
        extractSSBO.toGPU(1);
        uniformUBO.toGPU(0);

        extract( groups, 1, 1 );
        barrier( GL_SHADER_STORAGE_BARRIER_BIT );
        
        // scan( groups, 1, 1 );
        // barrier( GL_SHADER_STORAGE_BARRIER_BIT );
        // Buffer scanned = blellochScan( extractSSBO, triIds.size(), 64 );
        Buffer &scanned = radixScan( extractSSBO, triIds.size(), 64 );
        barrier( GL_SHADER_STORAGE_BARRIER_BIT );

        mortonSSBO.toGPU(0);
        scanned.toGPU(1);
        triIdsA.toGPU(2);
        outputSSBO.toGPU(3);
        triIdsB.toGPU(4);

        scatter( groups, 1, 1 );
        barrier( GL_SHADER_STORAGE_BARRIER_BIT );

        swap(mortonSSBO, outputSSBO);
        swap(triIdsA, triIdsB);

        // mortonSSBO.toGPU(0);
        // outputSSBO.toGPU(3);

        // swap(trianglesSSBO, trianglesSortedSSBO);

        // trianglesSSBO.toGPU(1);
        // trianglesSortedSSBO.toGPU(6);
    }

    // std::vector<uint32_t> triOut = triIdsA.toCPU<uint32_t>();
    // for( auto& i: triOut ) {
    //     printf( "%d ", i );
    // }
    // printf( "\n" );

    // Program reorder( radixReorderTrianglesSrc );
    // reorder( groups, 1, 1 );
    // barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    // // trianglesSortedSSBO.toCPU();

    // // Triangle* sortedTriangles = trianglesSortedSSBO.get<Triangle>();

    // // std::cout << '\n';

    std::vector<Node> nodes(2 * N - 1);

    Buffer bvhSSBO(
        GL_SHADER_STORAGE_BUFFER,
        nodes.size() * sizeof(Node),
        nodes.data(),
        GL_DYNAMIC_COPY
    );
    
    Buffer matSSBO(
        GL_SHADER_STORAGE_BUFFER,
        obj.materials.size() * sizeof(Material),
        obj.materials.data(),
        GL_DYNAMIC_COPY
    );

    Buffer normalsSSBO(
        GL_SHADER_STORAGE_BUFFER,
        obj.normals.size() * sizeof(glm::vec4),
        obj.normals.data(),
        GL_DYNAMIC_COPY
    );

    Buffer pixelSSBO(
        GL_SHADER_STORAGE_BUFFER,
        rays.size() * sizeof(Pixel),
        pixels.data(),
        GL_DYNAMIC_COPY
    );

    Buffer cameraUBO(
        GL_UNIFORM_BUFFER,
        sizeof( Camera ),
        &camera,
        GL_DYNAMIC_COPY
    );

    Buffer triangleSizeUBO(
        GL_UNIFORM_BUFFER,
        sizeof( uint32_t ),
        &N,
        GL_DYNAMIC_COPY
    );

    // bvhSSBO.toGPU(4);
    // matSSBO.toGPU(6);
    // pixelSSBO.toGPU(7);

    size_t raysSizePadded = ((pixels.size() + THREADS - 1) / THREADS) * THREADS;

    Buffer traverseSSBO(
        GL_SHADER_STORAGE_BUFFER,
        rays.size() * sizeof(Ray),
        // rays.data(),
        nullptr,
        GL_DYNAMIC_COPY
    );

    // traverseSSBO.toGPU(5);

    Program lbvh( lbvhSrc );
    Program aabbs( aabbSrc );

    mortonSSBO.toGPU(0);
    bvhSSBO.toGPU(1);
    triangleSizeUBO.toGPU(0);

    lbvh( groups, 1, 1 );
    barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    bvhSSBO.toGPU(0);
    trianglesSSBO.toGPU(1);
    triIdsA.toGPU(2);

    aabbs( groups, 1, 1 );
    barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    // GLuint bvhProgram = createLBVHShader();
    // dispatchProgram(groups, 1, 1, bvhProgram);

    // GLuint aabbProgram = createAABBShader();
    // dispatchProgram(groups, 1, 1, aabbProgram);

    // std::vector<Node> ef = bvhSSBO.toCPU<Node>();
    // for( auto& d : ef ) {
    //     glm::vec4 e = (d.aabb.bmax + d.aabb.bmin) * .5f;
    //     printf( "%d, %d, %d, %f, %f, %f\n", d.parent, d.left, d.right, e.x, e.y, e.z );
    // }

    mortonSSBO.destroy();

    // GLuint traverseProgram = createTraversalShader();
    // dispatchProgram(groups, 1, 1, traverseProgram);
    
    Program generatePrimaryRays( generatePrimaryRaySrc );
    Program traverse( traverseSrc );
    Program bounce( computeBounceSrc );
    Program evalBounce( evalBouncesSrc );

    Buffer shadowRaySSBO(
        GL_SHADER_STORAGE_BUFFER,
        rays.size() * sizeof( ShadowRay ),
        nullptr,
        GL_DYNAMIC_COPY
    );

    Buffer rayActiveSSBO(
        GL_SHADER_STORAGE_BUFFER,
        raysSizePadded * sizeof( uint32_t ),
        nullptr,
        GL_DYNAMIC_COPY
    );

    Buffer rayCompactedSSBO(
        GL_SHADER_STORAGE_BUFFER,
        rays.size() * sizeof( Ray ),
        nullptr,
        GL_DYNAMIC_COPY
    );

    uint32_t rSize = rays.size();
    Buffer rayCountSSBO(
        GL_SHADER_STORAGE_BUFFER,
        sizeof( uint32_t ),
        &rSize,
        GL_DYNAMIC_COPY
    );
    
    DispatchArgs argsInit { groups, 1, 1 };
    Buffer indirectDispatchBuffer(
        GL_DISPATCH_INDIRECT_BUFFER,
        sizeof( DispatchArgs ),
        &argsInit,
        GL_DYNAMIC_COPY
    );

    Program generateShadowRays( generateShadowRaySrc );
    Program traceShadowRays( traverseShadowRaySrc );
    Program compactRaysScatter( compactRaysScatterSrc );
    Program updateRayCount( updateRayCountSrc );

    BlellochScan compactScan( rSize, THREADS );

    GLuint query;
    glGenQueries(1, &query);

    glBeginQuery(GL_TIME_ELAPSED, query);

    const int MAX_ITER = 64;
    for( int i = 0; i < MAX_ITER; i ++ ) {

        traverseSSBO.toGPU( 0 );
        pixelSSBO.toGPU( 1 );
        cameraUBO.toGPU( 1 );
    
        generatePrimaryRays( groups, 1, 1 );
        barrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        rayCountSSBO.update( &rSize, sizeof(uint32_t) );
        barrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        
        indirectDispatchBuffer.update( &argsInit, sizeof( DispatchArgs ) );
        barrier(GL_BUFFER_UPDATE_BARRIER_BIT);

        for( int bounces = 0; bounces < 7; bounces ++ ) {
    
            trianglesSSBO.toGPU( 0 );
            triIdsA.toGPU( 1 );
            bvhSSBO.toGPU( 2 );
            traverseSSBO.toGPU( 3 );
            rayActiveSSBO.toGPU( 4 );
            rayCountSSBO.toGPU( 5 );
            triangleSizeUBO.toGPU( 0 );
            
            traverse( groups, 1, 1 );
            // traverse.indirect( indirectDispatchBuffer );
            barrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
            
            if( bounces > 3 ) {
                uint32_t activeCount = rayCountSSBO.toCPU<uint32_t>()[0];

                // Buffer scanned = blellochScan( rayActiveSSBO, ((activeCount + THREADS - 1) / THREADS) * THREADS, THREADS );
                Buffer &scanned = compactScan( rayActiveSSBO, ((activeCount + THREADS - 1) / THREADS) * THREADS, THREADS );
                
                traverseSSBO.toGPU( 0 );
                scanned.toGPU( 1 );
                rayCompactedSSBO.toGPU( 2 );
                rayCountSSBO.toGPU( 3 );
                
                compactRaysScatter( groups, 1, 1 );
                barrier(GL_SHADER_STORAGE_BARRIER_BIT);
                
                indirectDispatchBuffer.updateTarget( GL_SHADER_STORAGE_BUFFER );

                traverseSSBO.toGPU( 0 );
                scanned.toGPU( 1 );
                rayCountSSBO.toGPU( 2 );
                indirectDispatchBuffer.toGPU( 3 );
                
                updateRayCount( 1, 1, 1 );
                barrier( GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT );
                
                indirectDispatchBuffer.updateTarget( GL_DISPATCH_INDIRECT_BUFFER );

                std::swap( traverseSSBO, rayCompactedSSBO );
            }
        
            trianglesSSBO.toGPU( 0 );
            traverseSSBO.toGPU( 1 );
            pixelSSBO.toGPU( 2 );
            shadowRaySSBO.toGPU( 3 );
            normalsSSBO.toGPU( 4 );
            rayCountSSBO.toGPU( 5 );
        
            generateShadowRays( groups, 1, 1 );
            // generateShadowRays.indirect( indirectDispatchBuffer );

            barrier( GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT );
        
            trianglesSSBO.toGPU( 0 );
            triIdsA.toGPU( 1 );
            bvhSSBO.toGPU( 2 );
            shadowRaySSBO.toGPU( 3 );
            rayCountSSBO.toGPU( 4 );
        
            traceShadowRays( groups, 1, 1 );
            // traceShadowRays.indirect( indirectDispatchBuffer );
        
            barrier( GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT );
        
            // shadowRaySSBO.toCPU();
        
            // ShadowRay* shadows = shadowRaySSBO.get<ShadowRay>();
            // for( int i = 0; i < 10; i ++ ) {
            //     glm::vec4 dir = shadows[i].dir;
            //     printf( "%f, %f, %f, %d\n", dir.x, dir.y, dir.z, shadows[i].occluded );
            // }
        
            traverseSSBO.toGPU( 0 );
            matSSBO.toGPU( 1 );
            pixelSSBO.toGPU( 2 );
            shadowRaySSBO.toGPU( 3 );
            normalsSSBO.toGPU( 4 );
            rayActiveSSBO.toGPU( 5 );
            rayCountSSBO.toGPU( 6 );

            bounce( groups, 1, 1 );
            // bounce.indirect( indirectDispatchBuffer );
            barrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
        }
        
        pixelSSBO.toGPU( 0 );
        evalBounce( groups, 1, 1 );
        barrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glFinish();
        printf("FINISHED ITER %d\n", i);
    }

    glEndQuery(GL_TIME_ELAPSED);

    GLuint64 elapsedNs;
    glGetQueryObjectui64v(
        query,
        GL_QUERY_RESULT,
        &elapsedNs
    );

    double elapsedMs = elapsedNs / 1'000'000.0;

    // with compaction 2.377s
    printf("GPU time: %.3f s\n", elapsedMs * .001);

    glDeleteQueries(1, &query);

    // hitSSBO.toCPU();
    std::vector<Ray> res = traverseSSBO.toCPU<Ray>();
    // Hit* res = hitSSBO.get<Hit>();
    // Ray* res = traverseSSBO.get<Ray>();

    std::vector<Pixel> pixelRes = pixelSSBO.toCPU<Pixel>();
    // Pixel* pixelRes = pixelSSBO.get<Pixel>();

    for (size_t i = 0; i < 10; ++i) {
        
        Ray r = res[i];
        Pixel p = pixelRes[i];

        glm::vec4 color, hit;
        color = p.col / float(MAX_ITER);
        if( r.matId >= 0 ) {
            Material mat = obj.materials[ r.matId ];
            hit = r.o + r.t * r.dir;
        } else {
            // color = glm::vec4(0.f);
            hit = glm::vec4(0.f);
        }
        
        printf(
            "hit: %f, %f, %f\t" 
            "color: %f, %f, %f\n",
            hit.x,
            hit.y,
            hit.z,
            color.x,
            color.y,
            color.z
        );
    }

    std::vector<unsigned char> img(WIDTH * HEIGHT * 3);

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {

            size_t src = y * WIDTH + x;
            size_t dst = (HEIGHT - 1 - y) * WIDTH + x;

            // Ray r = res[src];
            Pixel p = pixelRes[ src ];
            glm::vec4 color = p.col / float(MAX_ITER);

            // glm::vec4 color;
            // if( r.matId >= 0 ) {
            //     Material mat = obj.Materials[ r.matId ];
            //     color = mat.diffuse;
            // } else {
            //     color = glm::vec4(0.f);
            // }

            glm::vec3 c = glm::clamp(
                glm::vec3(color),
                glm::vec3(0.0f),
                glm::vec3(1.0f)
            );

            img[dst * 3 + 0] = static_cast<unsigned char>(c.r * 255.0f);
            img[dst * 3 + 1] = static_cast<unsigned char>(c.g * 255.0f);
            img[dst * 3 + 2] = static_cast<unsigned char>(c.b * 255.0f);
        }
    }

    stbi_write_jpg(
        "output3.jpg",
        WIDTH,
        HEIGHT,
        3,
        img.data(),
        100
    );

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}