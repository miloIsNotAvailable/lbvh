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
// #include <ray.hpp>
#include <loadObj.hpp>
#include <lens.hpp>
#include "stb_image_write.h"
#include <scan.hpp>
#include <numeric>
#include <denoise.hpp>
#include <randoms.hpp>
#include <film.hpp>
#include <primaryRays.hpp>
#include <traverse.hpp>
#include <bounce.hpp>
#include <shadowRays.hpp>
#include <algorithm>

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

const GLuint WIDTH = 1600, HEIGHT = 1600;


// cornell box
glm::vec3 e(0., 0., -700.f);
glm::vec3 c(0.f, 0.f, 0.f);

// Room.obj
// glm::vec3 e(-50., 60., 250.f);
// glm::vec3 c(-50., 0., 0.f);

// glm::vec3 e(-1., -1., 1.f);
// glm::vec3 c(4., -2., -1.f);

// glm::vec3 e(-1., -1., -1.f);
// glm::vec3 c(-1., -2., 1.f);

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

    // printf("materials: %zu\n", obj.materials.size());

    // std::vector<Material> m_____ = obj.materials;

    // for (size_t i = 0; i < obj.materials.size(); ++i) {
        // const Material& m = obj.materials[i];

        // printf("Material %zu:\n", i);

        // printf("  ambient:       %f %f %f %f\n",
        //     m.ambient.x, m.ambient.y, m.ambient.z, m.ambient.w);

        // printf("  diffuse:       %f %f %f %f\n",
        //     m.diffuse.x, m.diffuse.y, m.diffuse.z, m.diffuse.w);

        // printf("  specular:      %f %f %f %f\n",
        //     m.specular.x, m.specular.y, m.specular.z, m.specular.w);

        // printf("  transmittance: %f %f %f %f\n",
        //     m.transmittance.x, m.transmittance.y,
        //     m.transmittance.z, m.transmittance.w);

        // printf("  emission:      %f %f %f %f\n",
        //     m.emission.x, m.emission.y,
        //     m.emission.z, m.emission.w);

        // printf("  shininess:     %f\n", m.shininess);
        // printf("  ior:           %f\n", m.ior);
        // printf("  dissolve:      %f\n", m.dissolve);
    // }

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


    // std::vector<Ray> rays;
    // std::vector<Pixel> pixels;

    // for (int i = 0; i < WIDTH * HEIGHT; i ++) {
        
    //     int x = int(i % WIDTH);
    //     int y = int(i / WIDTH);

    //     glm::vec2 fragCoord = glm::vec2(float(x), float(y));
    //     glm::vec2 st = fragCoord / glm::vec2(WIDTH, HEIGHT) - 0.5f;

    //     st.x *= float(WIDTH)/float(HEIGHT);

    //     Lens l = camera.thinLensRay( st );
    //     // Ray r{ glm::vec4(l.point, 0.), 
    //     //        glm::vec4(l.dir, 0.),
    //     //         glm::vec4( 0.f ),
    //     //         0.f, 1e30f };

    //     Ray r( l.point, l.dir, 0 );
    //     rays.push_back( r );

    //     // Pixel p( i );
    //     pixels.emplace_back( i );
    // }
    
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


    //   0011 2223 3444 5555
    // std::vector<uint32_t> flags = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1 };
    std::vector<uint32_t> flags = { 1, 1, 1, 1 };
    
    // std::vector<uint32_t> bigFlags( 600 * 600 );
    // std::mt19937 rng(12345);
    // std::bernoulli_distribution dist(0.5);

    // for( auto &d : bigFlags ) {
        // d = dist( rng ) ? 1u : 0u;
    // }
    // bigFlags.resize( (( bigFlags.size() + 63 ) / 64) * 64, 0 );

    // flags.resize( 64, 0 );

    // uint32_t wgSize = 64;
   
    // Buffer e(
    //     GL_SHADER_STORAGE_BUFFER,
    //     flags.size() * sizeof(uint32_t),
    //     flags.data(),
    //     GL_DYNAMIC_COPY  
    // );
    // BlellochScan scanner( flags.size(), wgSize );
    // Buffer &sizeCounter = scanner.counter();
    // // Buffer out = blellochScan( e, flags.size(), wgSize );
    // GLuint query;
    // uint32_t eee = flags.size();
    // glGenQueries(1, &query);

    // glBeginQuery(GL_TIME_ELAPSED, query);

    // sizeCounter.update( &eee, sizeof(uint32_t) );
    // Buffer out = scanner(e, 16, wgSize);

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
    
    // std::vector<uint32_t> sc = sizeCounter.toCPU<uint32_t>();
    // printf( "count: %d\n", sc[0] );

    std::vector<uint32_t> data = { 4, 2, 3, 1, 5 };

    // data.resize( 64, 0 );

    Buffer inpA(
        GL_SHADER_STORAGE_BUFFER,
        data.size() * sizeof(uint32_t),
        data.data(),
        GL_DYNAMIC_COPY
    );

    Buffer inpB(
        GL_SHADER_STORAGE_BUFFER,
        data.size() * sizeof(uint32_t),
        data.data(),
        GL_DYNAMIC_COPY
    );

    Program swapper( swapperSrc );

    Radix sort( data.size(), 64 );

    for( int i = 0; i < 32; i ++ ) {
        Buffer &out = sort( inpA, i );
        
        inpA.toGPU( 0 );
        inpB.toGPU( 1 );
        out.toGPU( 2 );
        swapper( (data.size() + 63) / 64, 1, 1 );

        barrier(GL_SHADER_STORAGE_BARRIER_BIT);

        std::swap( inpA, inpB );
    }

    std::vector<uint32_t> a = inpB.toCPU<uint32_t>();
    for( auto &d : a ) {
        printf( "%d\n", d );
    }

    std::cout << "\n\n";

    Buffer trianglesSSBO(
        GL_SHADER_STORAGE_BUFFER,
        obj.triangles.size() * sizeof(Triangle),
        obj.triangles.data(),
        GL_DYNAMIC_COPY
    );

    Buffer normalsSSBO(
        GL_SHADER_STORAGE_BUFFER,
        obj.normals.size() * sizeof(glm::vec4),
        obj.normals.data(),
        GL_DYNAMIC_COPY
    );

    // auto before = obj.materials;

    Buffer materialsSSBO(
        GL_SHADER_STORAGE_BUFFER,
        obj.materials.size() * sizeof(Material),
        obj.materials.data(),
        GL_DYNAMIC_COPY
    );

    Buffer sceneUBO(
        GL_UNIFORM_BUFFER,
        sizeof(AABB),
        &aabb,
        GL_DYNAMIC_COPY
    );

    // auto after = obj.materials;

    // assert(before.size() == after.size());

    // for (size_t i = 0; i < before.size(); ++i) {
    //     if (std::memcmp(&before[i], &after[i], sizeof(Material)) != 0) {
    //         printf("material %zu changed on CPU\n", i);
    //         abort();
    //     }
    // }
    
    LBVH lbvh( aabb, 64, obj.triangles.size() );
    Buffer& lbvhBuffer = lbvh( trianglesSSBO );

    std::vector<uint32_t> ee = lbvh.triIdASSBO.toCPU<uint32_t>();

    // std::vector<Triangle> tr  = trianglesSSBO.toCPU<Triangle>();
    // for( auto& t : ee ) {
    //     // printf( "%f, %f, %f\n", t.aabb.bmax.x, t.aabb.bmax.y, t.aabb.bmax.z );
    //     printf( "%d\n", t );
    // }

    lbvhBuffer.toGPU(0);
    trianglesSSBO.toGPU(1);
    lbvh.triIdASSBO.toGPU(2);

    GLuint numElements = obj.triangles.size();
    GLuint localSize   = 64;

    GLuint groups = (numElements + localSize - 1) / localSize;
    Program aabbs( aabbSrc );

    aabbs( groups, 1, 1 );
    barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    // printf(
    //     "%f, %f, %f\n"
    //     "%f, %f, %f\n\n",
    //     aabb.bmin.x,
    //     aabb.bmin.y,
    //     aabb.bmin.z, 
    //     aabb.bmax.x,
    //     aabb.bmax.y,
    //     aabb.bmax.z 
    // );

    // for( auto& d : lbvh.triIdASSBO.toCPU<uint32_t>()) {
    //     glm::vec4 c = obj.triangles[ d ].aabb.centroid();
    //     printf( "%f, %f, %f\n", c.x, c.y, c.z );
    //     // printf( "%d\n", d );
    // }

    std::vector<Node> bvh = lbvhBuffer.toCPU<Node>();
    
    Light light( 
        glm::vec3(0.f, 270.f, 0.f), 
        50.f, 
        glm::vec4(100.),
        glm::vec4( 0.f, -1.f, 0.f, 0.f ) 
    );
    
    // Room.obj
    // Light light( 
    //     glm::vec3(-50.f, 140.f, -50.f), 
    //     10.f, 
    //     glm::vec4(400.),
    //     glm::vec4( 0.f, -1.f, 0.f, 0.f ) 
    // );
        
    Film film( WIDTH, HEIGHT, camera, light );
    uint32_t i = 0;
    film.iteration.update( &i, sizeof( uint32_t ) );

    uint32_t s = WIDTH * HEIGHT;
    ThinLens thinLens( s );
    
    // WhiteNoise random( s );

    // random.seed( s, film.iteration );

    // Buffer &out = random.random1D( s );

    // for(auto &d : out.toCPU<float>()) {
    //     printf( "%f\n", d );
    // }

    // Random2D rand = random.random2D( s );

    Sobol sobol( s );
    Traverse traverse( s );
    ShadowRays shadows( s );
    Contribution contrib( s );

    AtrousDenoiser denoiser( s );

    const int MAX_ITER = 32;
    uint32_t iter = uint32_t( MAX_ITER );
    film.iteration.update( &iter, sizeof( uint32_t ) );

    std::vector<glm::vec4> zero4(s, glm::vec4(0.0f));
    std::vector<glm::vec4> one4(s, glm::vec4(1.0f));
    std::vector<uint32_t> zeroU32(s, 0);
    std::vector<float> zeroF32(s, 0.0f);

    for( int i = 0; i < MAX_ITER; i ++ ) {

        uint32_t dims = 0;

        film.L.update(zero4.data(), s * sizeof(glm::vec4));
        film.beta.update(one4.data(), s * sizeof(glm::vec4));

        traverse.dead.update(zeroU32.data(), s * sizeof(uint32_t));
        traverse.hitEmissive.update(zeroU32.data(), s * sizeof(uint32_t));
        // contrib.pdf_bsdf.update(zeroF32.data(), s * sizeof(float));
        
        uint32_t it = uint32_t( i ); 

        Random2D &rand = sobol.random2D( s, dims, 23757628, it );
        Buffer &out = thinLens(s, rand, film);

        for( int bounces = 0; bounces < 7; bounces ++ ) {


            // auto ox = out.toCPU<float>();

            // uint32_t N = ox.size() / 6;

            // for (int i = 0; i < 10; ++i) {

            //     glm::vec3 o = glm::vec3( ox[i + N * 0],
            //                              ox[i + N * 1],
            //                              ox[i + N * 2] );

            //     glm::vec3 d = glm::vec3( ox[i + N * 3],
            //                              ox[i + N * 4],
            //                              ox[i + N * 5] );
                        
            //     printf(
            //         "%d: O=(%f, %f, %f) D=(%f, %f, %f)\n",
            //         i,
            //         o.x, o.y, o.z,
            //         d.x, d.y, d.z
            //     );
            // }    

            Buffer &tOut = traverse( s, out, lbvhBuffer, trianglesSSBO, lbvh.triIdASSBO, film.light );

            if( bounces == 0 && i == 0 ) {
                denoiser.saveGBuffers(
                    s, 
                    traverse.dead, 
                    normalsSSBO,
                    traverse.triIds,
                    trianglesSSBO,
                    materialsSSBO,
                    out,
                    tOut,
                    sceneUBO
                );
            }

            // dead rays: 0/2560000
            // hit emissive rays: 4276/2560000
            // std::vector<uint32_t> dead = traverse.dead.toCPU<uint32_t>();
            // int sum = 0;
            // for( auto &d : dead ) {
            //     if( d == 1 ) sum ++;
            // }
            
            // std::vector<uint32_t> hitEm = traverse.hitEmissive.toCPU<uint32_t>();
            // int sum_ = 0;
            // for( auto &d : hitEm ) {
            //     if( d == 1 ) sum_ ++;
            // }

            // printf( "dead rays: %d/%d\n", sum, int(dead.size()) );
            // printf( "hit emissive rays: %d/%d\n", sum_, int(hitEm.size()) );


            Random2D &randShadow = sobol.random2D( s, dims, 23757628, it );

            Buffer &shadowOut = shadows.generate( s,
                out, 
                tOut, 
                traverse.triIds, 
                normalsSSBO, 
                traverse.dead, 
                film.light, 
                randShadow, 
                traverse.hitEmissive );
            
            // std::vector<float> outDirs = shadowOut.toCPU<float>();
            // std::vector<float> tmaxs = shadows.tmax.toCPU<float>();

            // uint32_t G = outDirs.size() / 6;

            // for( int i = 0; i < 20; i ++ ) {
            //     glm::vec3 d = glm::vec3(
            //         outDirs[ i + G * 3 ],
            //         outDirs[ i + G * 4 ],
            //         outDirs[ i + G * 5 ]
            //     );

            //     printf( "%f, %f, %f, %f\n", d.x, d.y, d.z, tmaxs[i] );
            // }
            
            Buffer &occ = shadows.traverse( s, 
                lbvhBuffer, 
                trianglesSSBO, 
                traverse.dead, 
                lbvh.triIdASSBO, 
                film.light, 
                normalsSSBO,
                traverse.hitEmissive );

            // std::vector<uint32_t> occluded = occ.toCPU<uint32_t>();
            // sum = 0;
            // for( auto &d : occluded ) {
            //     if( d == 1 ) sum ++;
            // }

            // occluded: 455703/2560000
            // printf( "occluded: %d/%d\n", sum, int( occluded.size() ) );
            
            contrib.emissive( s, 
                out,
                tOut,
                traverse.dead, 
                traverse.hitEmissive, 
                film.L, 
                film.beta, 
                film.light,
                uint32_t(bounces)
            );

            contrib.contribute( s, 
                out,
                shadowOut, 
                tOut,
                traverse.dead, 
                trianglesSSBO,
                traverse.triIds, 
                film.L,
                film.beta, 
                film.light, 
                materialsSSBO,
                normalsSSBO,
                occ,
                shadows.tmax 
            );


            Random2D &randBSDF = sobol.random2D( s, dims, 23757628, it );
            Buffer &randRR = sobol.random1D( s, dims, 23757628, it );
            contrib.sampleBSDF(
                s,
                out,
                traverse.dead,
                randBSDF,
                randRR,
                traverse.triIds,
                normalsSSBO,
                trianglesSSBO,
                materialsSSBO,
                tOut,
                film.beta
            );
        }

        contrib.addColor( s, film.L, film.pixel, film.iteration );
    }

    Buffer &outDenoised = denoiser( s, film.pixel, film.camera );
    std::vector<glm::vec4> outCol = outDenoised.toCPU<glm::vec4>();

    for( int i = 0; i < 10; i ++ ) {
        glm::vec4 c = outCol[i];
        printf( "%f, %f, %f\n", c.x, c.y, c.z );
    }

    // std::vector<float> pixels1 = sobol.random1D( s, 3, 294835623 ).toCPU<float>();
    // std::vector<float> pixels2 = sobol.random1D( s, 4, 586289980 ).toCPU<float>();

    // std::vector<glm::vec2> pixels2D = sobol.random2D( s, 0, 23757628 ).toCPU();

    std::vector<unsigned char> img(WIDTH * HEIGHT * 3);

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {

            size_t src = y * WIDTH + x;
            size_t dst = (HEIGHT - 1 - y) * WIDTH + x;

            // Ray r = res[src];
            // float p = pixels2D[ src ].x;
            // glm::vec4 color = glm::vec4( p, p, p, 1. );
            glm::vec4 color = outCol[ src ];

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
        "sobol2D4.jpg",
        WIDTH,
        HEIGHT,
        3,
        img.data(),
        100
    );








    // Integrator tracer( camera, aabb, WIDTH, HEIGHT );
    // tracer.generateCameraRays();
    // tracer.traverse( lbvhBuffer, trianglesSSBO, lbvh.triIdASSBO );
    // // tracer.radixSortPosAndDir();
    // tracer.sampleNewDir( normalsSSBO );
    // // tracer.radixSortPosAndDir();
    // tracer.traverse( lbvhBuffer, trianglesSSBO, lbvh.triIdASSBO );

    // barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    // std::vector<Triangle> tri = triBuffer.toCPU<Triangle>();
    // for( auto &t : tri ) {
    //     printf( "", t. );
    // }

    // int THREADS = 64;
    // size_t N = obj.triangles.size();
    // obj.triangles.resize( ((obj.triangles.size() + THREADS - 1) / THREADS) * THREADS, dummy );

    // Buffer trianglesSSBO(
    //     GL_SHADER_STORAGE_BUFFER,
    //     obj.triangles.size() * sizeof(Triangle),
    //     obj.triangles.data(),
    //     GL_DYNAMIC_COPY
    // );

    // Buffer mortonSSBO(
    //     GL_SHADER_STORAGE_BUFFER,
    //     obj.triangles.size() * sizeof(uint32_t),
    //     nullptr,
    //     GL_DYNAMIC_COPY
    // );

    // Buffer sceneUBO(
    //     GL_UNIFORM_BUFFER,
    //     sizeof(AABB),
    //     &aabb,
    //     GL_DYNAMIC_COPY
    // );


    // GLuint numElements = static_cast<GLuint>(obj.triangles.size());
    // GLuint localSize   = 64;

    // GLuint groups = (numElements + localSize - 1) / localSize;

    // Program morton( mortonSrc );

    // mortonSSBO.toGPU(0);
    // trianglesSSBO.toGPU(1);
    // sceneUBO.toGPU(0);
    // morton( groups, 1, 1 );

    // Buffer extractSSBO(
    //     GL_SHADER_STORAGE_BUFFER,
    //     obj.triangles.size() * sizeof(uint32_t),
    //     nullptr,
    //     GL_DYNAMIC_COPY
    // );

    // Buffer outputSSBO(
    //     GL_SHADER_STORAGE_BUFFER,
    //     obj.triangles.size() * sizeof(uint32_t),
    //     nullptr,
    //     GL_DYNAMIC_COPY
    // );

    // Buffer uniformUBO(
    //     GL_UNIFORM_BUFFER,
    //     sizeof(uint32_t),
    //     nullptr,
    //     GL_DYNAMIC_DRAW
    // );

    // std::vector<uint32_t> triIds(obj.triangles.size());
    // std::iota(triIds.begin(), triIds.end(), 0u);

    // // triIds.resize( 64, 100 );

    // Buffer triIdsA(
    //     GL_SHADER_STORAGE_BUFFER,
    //     triIds.size() * sizeof(uint32_t),
    //     triIds.data(),
    //     GL_DYNAMIC_COPY
    // );
    
    // Buffer triIdsB(
    //     GL_SHADER_STORAGE_BUFFER,
    //     triIds.size() * sizeof(uint32_t),
    //     nullptr,
    //     GL_DYNAMIC_COPY
    // );

    // // trianglesSSBO.toGPU(1);
    // // extractSSBO.toGPU(2);
    // // outputSSBO.toGPU(3);
    // // trianglesSortedSSBO.toGPU(6);
    // // uniformUBO.toGPU(0);

    // // GLuint extractProgram = createExtractShader();
    // // GLuint scanProgram    = createScanShader();
    // // GLuint scatterProgram = createScatterShader();


    
    // // parent: -1, left: 21, right: 22
    // // parent: 11, left: 35, right: 36
    // // parent: 11, left: 9, right: 10
    // // parent: 9, left: 37, right: 38
    // // parent: 9, left: 6, right: 7
    // // parent: 6, left: 40, right: 41
    // // parent: 4, left: 39, right: 5
    // // parent: 4, left: 42, right: 8
    // // parent: 7, left: 43, right: 44
    // // parent: 2, left: 3, right: 4
    // // parent: 2, left: 45, right: 46
    // // parent: 13, left: 1, right: 2
    // // parent: 13, left: 47, right: 48
    // // parent: 21, left: 11, right: 12
    // // parent: 21, left: 15, right: 16
    // // parent: 14, left: 49, right: 50
    // // parent: 14, left: 17, right: 18
    // // parent: 16, left: 51, right: 52
    // // parent: 16, left: 19, right: 20
    // // parent: 18, left: 53, right: 54
    // // parent: 18, left: 55, right: 56
    // // parent: 0, left: 13, right: 14
    // // parent: 0, left: 31, right: 32
    // // parent: 31, left: 57, right: 58
    // // parent: 31, left: 29, right: 30
    // // parent: 29, left: 59, right: 60
    // // parent: 29, left: 27, right: 28
    // // parent: 26, left: 61, right: 62
    // // parent: 26, left: 63, right: 64
    // // parent: 24, left: 25, right: 26
    // // parent: 24, left: 65, right: 66
    // // parent: 22, left: 23, right: 24

    // Program extract( radixExtractSrc );
    // Program scan( radixScanSrc );
    // Program scatter( radixScatterSrc );

    // BlellochScan radixScan( triIds.size(), THREADS );
    // // radixScan.counter().update( &N, sizeof( int ) );

    // for (int i = 0; i < 30; ++i) {

    //     uniformUBO.update(
    //         &i,
    //         sizeof(i)
    //     );

    //     mortonSSBO.toGPU(0);
    //     extractSSBO.toGPU(1);
    //     uniformUBO.toGPU(0);

    //     extract( groups, 1, 1 );
    //     barrier( GL_SHADER_STORAGE_BARRIER_BIT );
        
    //     // scan( groups, 1, 1 );
    //     // barrier( GL_SHADER_STORAGE_BARRIER_BIT );
    //     // Buffer scanned = blellochScan( extractSSBO, triIds.size(), 64 );
    //     Buffer &scanned = radixScan( extractSSBO, triIds.size(), 64 );
    //     barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    //     mortonSSBO.toGPU(0);
    //     scanned.toGPU(1);
    //     triIdsA.toGPU(2);
    //     outputSSBO.toGPU(3);
    //     triIdsB.toGPU(4);

    //     scatter( groups, 1, 1 );
    //     barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    //     swap(mortonSSBO, outputSSBO);
    //     swap(triIdsA, triIdsB);

    //     // mortonSSBO.toGPU(0);
    //     // outputSSBO.toGPU(3);

    //     // swap(trianglesSSBO, trianglesSortedSSBO);

    //     // trianglesSSBO.toGPU(1);
    //     // trianglesSortedSSBO.toGPU(6);
    // }

    // // std::vector<uint32_t> mortons = mortonSSBO.toCPU<uint32_t>();
    // // for( int i =0; i < 100; i ++ ) {
    // //     printf( "%d\n", mortons[i] );
    // // }

    // // std::vector<uint32_t> triOut = triIdsA.toCPU<uint32_t>();
    // // for( auto& i: triOut ) {
    // //     printf( "%d ", i );
    // // }
    // // printf( "\n" );

    // // Program reorder( radixReorderTrianglesSrc );
    // // reorder( groups, 1, 1 );
    // // barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    // // // trianglesSortedSSBO.toCPU();

    // // // Triangle* sortedTriangles = trianglesSortedSSBO.get<Triangle>();

    // // // std::cout << '\n';

    // std::vector<Node> nodes(2 * N - 1);

    // Buffer bvhSSBO(
    //     GL_SHADER_STORAGE_BUFFER,
    //     nodes.size() * sizeof(Node),
    //     nodes.data(),
    //     GL_DYNAMIC_COPY
    // );
    
    // Buffer matSSBO(
    //     GL_SHADER_STORAGE_BUFFER,
    //     obj.materials.size() * sizeof(Material),
    //     obj.materials.data(),
    //     GL_DYNAMIC_COPY
    // );

    // Buffer normalsSSBO(
    //     GL_SHADER_STORAGE_BUFFER,
    //     obj.normals.size() * sizeof(glm::vec4),
    //     obj.normals.data(),
    //     GL_DYNAMIC_COPY
    // );

    // Buffer pixelSSBO(
    //     GL_SHADER_STORAGE_BUFFER,
    //     rays.size() * sizeof(Pixel),
    //     pixels.data(),
    //     GL_DYNAMIC_COPY
    // );

    // Buffer cameraUBO(
    //     GL_UNIFORM_BUFFER,
    //     sizeof( Camera ),
    //     &camera,
    //     GL_DYNAMIC_COPY
    // );

    // Buffer triangleSizeUBO(
    //     GL_UNIFORM_BUFFER,
    //     sizeof( uint32_t ),
    //     &N,
    //     GL_DYNAMIC_COPY
    // );

    // // bvhSSBO.toGPU(4);
    // // matSSBO.toGPU(6);
    // // pixelSSBO.toGPU(7);

    // size_t raysSizePadded = ((pixels.size() + THREADS - 1) / THREADS) * THREADS;

    // Buffer traverseSSBO(
    //     GL_SHADER_STORAGE_BUFFER,
    //     rays.size() * sizeof(Ray),
    //     // rays.data(),
    //     nullptr,
    //     GL_DYNAMIC_COPY
    // );

    // // traverseSSBO.toGPU(5);

    // Program lbvh( lbvhSrc );
    // Program aabbs( aabbSrc );

    // mortonSSBO.toGPU(0);
    // bvhSSBO.toGPU(1);
    // triangleSizeUBO.toGPU(0);

    // lbvh( groups, 1, 1 );
    // barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    // bvhSSBO.toGPU(0);
    // trianglesSSBO.toGPU(1);
    // triIdsA.toGPU(2);

    // aabbs( groups, 1, 1 );
    // barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    // // GLuint bvhProgram = createLBVHShader();
    // // dispatchProgram(groups, 1, 1, bvhProgram);

    // // GLuint aabbProgram = createAABBShader();
    // // dispatchProgram(groups, 1, 1, aabbProgram);

    // // std::vector<Node> ef = bvhSSBO.toCPU<Node>();
    // // for( auto& d : ef ) {
    // //     glm::vec4 e = (d.aabb.bmax + d.aabb.bmin) * .5f;
    // //     printf( "%d, %d, %d, %f, %f, %f\n", d.parent, d.left, d.right, e.x, e.y, e.z );
    // // }

    // mortonSSBO.destroy();
    
    // std::vector<Node> bvh = bvhSSBO.toCPU<Node>();
    
    // // printf( "LBVH built, length: %d\n", bvh.size() );

    // for( int i = 0; i < 32; i ++ ) {
    //     printf( "parent: %d, left: %d, right: %d\n", bvh[i].parent, bvh[i].left, bvh[i].right );        
    // }
    // GLuint traverseProgram = createTraversalShader();
    // dispatchProgram(groups, 1, 1, traverseProgram);
    
//     Program generatePrimaryRays( generatePrimaryRaySrc );
//     Program traverse( traverseSrc );
//     Program bounce( computeBounceSrc );
//     Program evalBounce( evalBouncesSrc );

//     Buffer shadowRaySSBO(
//         GL_SHADER_STORAGE_BUFFER,
//         rays.size() * sizeof( ShadowRay ),
//         nullptr,
//         GL_DYNAMIC_COPY
//     );

//     Buffer rayActiveSSBO(
//         GL_SHADER_STORAGE_BUFFER,
//         raysSizePadded * sizeof( uint32_t ),
//         nullptr,
//         GL_DYNAMIC_COPY
//     );

//     Buffer rayCompactedSSBO(
//         GL_SHADER_STORAGE_BUFFER,
//         rays.size() * sizeof( Ray ),
//         nullptr,
//         GL_DYNAMIC_COPY
//     );


//     std::vector<glm::ivec2> off;
//     off.reserve( 25 );

//     for( int i = -2; i <= 2; i ++ ) {
//         for( int j = -2; j <= 2; j ++ ) {
//             off.push_back( glm::ivec2( i, j ) );
//         }
//     }

//     Buffer atrousSSBO(
//         GL_SHADER_STORAGE_BUFFER,
//         rays.size() * sizeof( Atrous ),
//         nullptr,
//         GL_DYNAMIC_COPY
//     );
    
//     Buffer offsetsSSBO(
//         GL_SHADER_STORAGE_BUFFER,
//         off.size() * sizeof( glm::ivec2 ),
//         off.data(),
//         GL_DYNAMIC_COPY
//     );

//     std::vector<float> kernel = {
//         1.f/256.f,  4.f/256.f,  6.f/256.f,  4.f/256.f, 1.f/256.f,
//         4.f/256.f, 16.f/256.f, 24.f/256.f, 16.f/256.f, 4.f/256.f,
//         6.f/256.f, 24.f/256.f, 36.f/256.f, 24.f/256.f, 6.f/256.f,
//         4.f/256.f, 16.f/256.f, 24.f/256.f, 16.f/256.f, 4.f/256.f,
//         1.f/256.f,  4.f/256.f,  6.f/256.f,  4.f/256.f, 1.f/256.f
//     };

//     Buffer kernelSSBO(
//         GL_SHADER_STORAGE_BUFFER,
//         kernel.size() * sizeof( float ),
//         kernel.data(),
//         GL_DYNAMIC_COPY
//     );

//     uint32_t rSize = rays.size();
//     // Buffer rayCountSSBO(
//     //     GL_SHADER_STORAGE_BUFFER,
//     //     sizeof( uint32_t ),
//     //     &rSize,
//     //     GL_DYNAMIC_COPY
//     // );
    
//     groups = (rSize + THREADS - 1) / THREADS;
//     DispatchArgs argsInit { groups, 1, 1 };
//     Buffer indirectDispatchBuffer(
//         GL_SHADER_STORAGE_BUFFER,
//         sizeof( DispatchArgs ),
//         &argsInit,
//         GL_DYNAMIC_COPY
//     );

//     Program generateShadowRays( generateShadowRaySrc );
//     Program traceShadowRays( traverseShadowRaySrc );
//     Program compactRaysScatter( compactRaysScatterSrc );
//     Program updateRayCount( updateRayCountSrc );
//     Program saveDenoiseInfo( saveDenoiseInfoSrc );
//     Program bsdfSample( bsdfSampleSrc );

//     BlellochScan compactScan( raysSizePadded, THREADS );
//     Buffer &rayCountSSBO = compactScan.counter();

//     // GLuint query;
//     // glGenQueries(1, &query);

//     // glBeginQuery(GL_TIME_ELAPSED, query);

//     const int MAX_ITER =10;
//     for( int i = 0; i < MAX_ITER; i ++ ) {

//         // printf( "starting iteration: %d\n", i );
        
//         traverseSSBO.toGPU( 0 );
//         pixelSSBO.toGPU( 1 );
//         cameraUBO.toGPU( 1 );
    
//         measureGPU( "gen rays", [&] {
//             generatePrimaryRays( groups, 1, 1 );
//             barrier(GL_SHADER_STORAGE_BARRIER_BIT);
//         } );

//         measureGPU( "ray count", [&] {
            
//             rayCountSSBO.update( &rSize, sizeof(uint32_t) );
//             barrier(GL_BUFFER_UPDATE_BARRIER_BIT);
//             indirectDispatchBuffer.update( &argsInit, sizeof( DispatchArgs ) );
//             barrier(GL_BUFFER_UPDATE_BARRIER_BIT);
//         } );


//         printf( "fuckyou\n" );
//         glFinish();

//         for( int bounces = 0; bounces < 7; bounces ++ ) {
    
//             trianglesSSBO.toGPU( 0 );
//             triIdsA.toGPU( 1 );
//             bvhSSBO.toGPU( 2 );
//             traverseSSBO.toGPU( 3 );
//             rayActiveSSBO.toGPU( 4 );
//             rayCountSSBO.toGPU( 5 );
//             triangleSizeUBO.toGPU( 0 );


//             // auto rr = traverseSSBO.toCPU<Ray>();

//             // for (int k = 0; k < 8; ++k) {
//             //     printf(
//             //         "before: %d: o=(%f,%f,%f) dir=(%f,%f,%f) t=%f tri=%d dead=%u pixel=%u\n",
//             //         k,
//             //         rr[k].o.x, rr[k].o.y, rr[k].o.z,
//             //         rr[k].dir.x, rr[k].dir.y, rr[k].dir.z,
//             //         rr[k].t,
//             //         rr[k].triId,
//             //         rr[k].dead,
//             //         rr[k].pixelId
//             //     );
//             // }


//             // printf( "traverse\n" );
//             // traverse( groups, 1, 1 );
//             measureGPU( "traverse prim rays", [&] {            
//                 traverse.indirect( indirectDispatchBuffer );
//                 barrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
//             } );
            
//             printf( "bitvchyou\n" );
//             glFinish();

//             if( i == 0 && bounces == 0 ) {
//             measureGPU( "save g buffers", [&] {            
//                     traverseSSBO.toGPU( 0 );
//                     atrousSSBO.toGPU( 1 );
//                     normalsSSBO.toGPU( 2 );
//                     sceneUBO.toGPU(0);
    
//                     saveDenoiseInfo( groups, 1, 1 );
//                     barrier(GL_SHADER_STORAGE_BARRIER_BIT);
//                 } );
//             } 

//             // rr = traverseSSBO.toCPU<Ray>();

//             // for (int k = 0; k < 8; ++k) {
//             //     printf(
//             //         "after: %d: o=(%f,%f,%f) dir=(%f,%f,%f) t=%f tri=%d dead=%u pixel=%u\n",
//             //         k,
//             //         rr[k].o.x, rr[k].o.y, rr[k].o.z,
//             //         rr[k].dir.x, rr[k].dir.y, rr[k].dir.z,
//             //         rr[k].t,
//             //         rr[k].triId,
//             //         rr[k].dead,
//             //         rr[k].pixelId
//             //     );
//             // }

//             // glFinish();
//             // uint32_t activeCount = rayCountSSBO.toCPU<uint32_t>()[0];
//             // printf( "active ray count BEFORE: %d\n", activeCount );

//             // DispatchArgs indirect = indirectDispatchBuffer.toCPU<DispatchArgs>()[0];
//             // printf( "merry fucking dispatch: %d, %d, %d\n", indirect.x, indirect.y, indirect.z );

//             // printf( "compact\n" );
//             if( bounces > 2 ) {
                
//                 // Buffer scanned = blellochScan( rayActiveSSBO, ((activeCount + THREADS - 1) / THREADS) * THREADS, THREADS );
//                 Buffer &scanned = compactScan( rayActiveSSBO, 0, THREADS );
//                 // uint32_t activeCount = rayCountSSBO.toCPU<uint32_t>()[0];
                
//                 // printf( "active ray count AFTER: %d\n", activeCount );

//                 traverseSSBO.toGPU( 0 );
//                 scanned.toGPU( 1 );
//                 rayCompactedSSBO.toGPU( 2 );
//                 rayCountSSBO.toGPU( 3 );
                
//                 compactRaysScatter( groups, 1, 1 );
//                 barrier(GL_SHADER_STORAGE_BARRIER_BIT);
                
//                 // indirectDispatchBuffer.updateTarget( GL_SHADER_STORAGE_BUFFER );

//                 traverseSSBO.toGPU( 0 );
//                 scanned.toGPU( 1 );
//                 rayCountSSBO.toGPU( 2 );
//                 indirectDispatchBuffer.toGPU( 3 );

//                 updateRayCount( 1, 1, 1 );
//                 barrier( GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT );
                
//                 // indirectDispatchBuffer.updateTarget( GL_DISPATCH_INDIRECT_BUFFER );

//                 std::swap( traverseSSBO, rayCompactedSSBO );
//             }
        
//             measureGPU( "gen shadow", [&] {
            
//                 // printf( "shadows\n" );
//                 trianglesSSBO.toGPU( 0 );
//                 traverseSSBO.toGPU( 1 );
//                 pixelSSBO.toGPU( 2 );
//                 shadowRaySSBO.toGPU( 3 );
//                 normalsSSBO.toGPU( 4 );
//                 rayCountSSBO.toGPU( 5 );
            
//                 // generateShadowRays( groups, 1, 1 );
//                 generateShadowRays.indirect( indirectDispatchBuffer );
    
//                 barrier( GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT );
//             } );

//             // printf( "trace shadows\n" );

//             measureGPU( "trace shadow", [&] {
//                 trianglesSSBO.toGPU( 0 );
//                 triIdsA.toGPU( 1 );
//                 bvhSSBO.toGPU( 2 );
//                 shadowRaySSBO.toGPU( 3 );
//                 rayCountSSBO.toGPU( 4 );
//                 traverseSSBO.toGPU( 5 );

//                 // traceShadowRays( groups, 1, 1 );
//                 traceShadowRays.indirect( indirectDispatchBuffer );
            
//                 barrier( GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT );
//             } );
        
//             // shadowRaySSBO.toCPU();
        
//             // ShadowRay* shadows = shadowRaySSBO.get<ShadowRay>();
//             // for( int i = 0; i < 10; i ++ ) {
//             //     glm::vec4 dir = shadows[i].dir;
//             //     printf( "%f, %f, %f, %d\n", dir.x, dir.y, dir.z, shadows[i].occluded );
//             // }

//             measureGPU( "bounce", [&] {
                
//                 traverseSSBO.toGPU( 0 );
//                 matSSBO.toGPU( 1 );
//                 pixelSSBO.toGPU( 2 );
//                 shadowRaySSBO.toGPU( 3 );
//                 normalsSSBO.toGPU( 4 );
//                 rayActiveSSBO.toGPU( 5 );
//                 rayCountSSBO.toGPU( 6 );
//                 trianglesSSBO.toGPU( 7 );
    
//                 // bounce( groups, 1, 1 );
//                 bounce.indirect( indirectDispatchBuffer );
//                 barrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
                
//             } );    
  
//             // printf( "shityou\n" );
//             // glFinish();

//             trianglesSSBO.toGPU( 0 );
//             triIdsA.toGPU( 1 );
//             bvhSSBO.toGPU( 2 );
//             traverseSSBO.toGPU( 3 );
//             rayActiveSSBO.toGPU( 4 );
//             rayCountSSBO.toGPU( 5 );
//             triangleSizeUBO.toGPU( 0 );

//             measureGPU( "traverse prim rays", [&] {            
//                 traverse.indirect( indirectDispatchBuffer );
//                 barrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
//             } );


//             // std::vector<Ray> rr = traverseSSBO.toCPU<Ray>();

//             // for (int k = 0; k < 8; ++k) {
//             //     printf(
//             //         "before: %d: o=(%f,%f,%f) dir=(%f,%f,%f) t=%f tri=%d dead=%u pixel=%u\n",
//             //         k,
//             //         rr[k].o.x, rr[k].o.y, rr[k].o.z,
//             //         rr[k].dir.x, rr[k].dir.y, rr[k].dir.z,
//             //         rr[k].t,
//             //         rr[k].triId,
//             //         rr[k].dead,
//             //         rr[k].pixelId
//             //     );
//             // }

//             traverseSSBO.toGPU( 0 );
//             matSSBO.toGPU( 1 );
//             pixelSSBO.toGPU( 2 );
//             shadowRaySSBO.toGPU( 3 );
//             normalsSSBO.toGPU( 4 );
//             rayActiveSSBO.toGPU( 5 );
//             rayCountSSBO.toGPU( 6 );
//             trianglesSSBO.toGPU( 7 );
//             // bounce( groups, 1, 1 );
//             bsdfSample.indirect( indirectDispatchBuffer );
//             barrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

//             std::vector<Pixel> pr = pixelSSBO.toCPU<Pixel>();

//             for (int k = 0; k < 8; ++k) {
//                 printf(
//                     "col: %f,%f,%f, L: %f,%f,%f\n",
//                     pr[k].col.x,
//                     pr[k].col.y,
//                     pr[k].col.z,
//                     pr[k].L.x,
//                     pr[k].L.y,
//                     pr[k].L.z
//                 );
//             }


//         }
        
//         // printf( "average\n" );
//         measureGPU( "eval bounce", [&] {
            
//             pixelSSBO.toGPU( 0 );
//             evalBounce( groups, 1, 1 );
//             barrier(GL_SHADER_STORAGE_BARRIER_BIT);
//         } );

//         glFinish();
//         printf("FINISHED ITER %d\n", i);
//     }

//     // glEndQuery(GL_TIME_ELAPSED);

//     // GLuint64 elapsedNs;
//     // glGetQueryObjectui64v(
//     //     query,
//     //     GL_QUERY_RESULT,
//     //     &elapsedNs
//     // );

//     // double elapsedMs = elapsedNs / 1'000'000.0;

//     // // without compaction GPU time: 26.359 s
//     // printf("GPU time: %.3f s\n", elapsedMs * .001);

//     // glDeleteQueries(1, &query);

//     Program denoise( denoiseSrc );

//    Buffer pixelBSSBO(
//         GL_SHADER_STORAGE_BUFFER,
//         rays.size() * sizeof(Pixel),
//         nullptr,
//         GL_DYNAMIC_COPY
//     );

//     Buffer stepUBO(
//         GL_UNIFORM_BUFFER,
//         sizeof(int),
//         nullptr,
//         GL_DYNAMIC_COPY
//     );

//     // for( int i = 0; i < 4; i ++ ) {

//     //     int step = 1 << i;

//     //     stepUBO.update( &step, sizeof( int ) );

//     //     pixelSSBO.toGPU( 0 );
//     //     atrousSSBO.toGPU( 1 );
//     //     offsetsSSBO.toGPU( 2 );
//     //     kernelSSBO.toGPU( 3 );
//     //     pixelBSSBO.toGPU( 4 );
        
//     //     stepUBO.toGPU( 0 );
//     //     cameraUBO.toGPU( 1 );

//     //     denoise( groups, 1, 1 );
//     //     barrier(GL_SHADER_STORAGE_BARRIER_BIT);

//     //     std::swap( pixelSSBO, pixelBSSBO );
//     // }

//     // std::vector<Pixel> pixelsB = pixelSSBO.toCPU<Pixel>();
//     // for( int i = 0; i < 20; i ++ ) {
//     //     Pixel p = pixelsB[ i ];
//     //     printf( "%f, %f, %f\n", p.col.x, p.col.y, p.col.z );
//     // }   

//     // hitSSBO.toCPU();
//     std::vector<Ray> res = traverseSSBO.toCPU<Ray>();
//     // Hit* res = hitSSBO.get<Hit>();
//     // Ray* res = traverseSSBO.get<Ray>();

//     std::vector<Pixel> pixelRes = pixelSSBO.toCPU<Pixel>();
    
//     // for (int i = 0; i < pixelRes.size(); ++i) {
//     //     glm::vec4 c = pixelRes[i].col;

//     //     float lum =
//     //         0.2126f * c.r +
//     //         0.7152f * c.g +
//     //         0.0722f * c.b;

//     //     if (lum > 100.0f) {
//     //         printf(
//     //             "%d: rgb=(%f,%f,%f), lum=%f\n",
//     //             i, c.r, c.g, c.b, lum
//     //         );
//     //     }
//     // }
    
//     // std::vector<Atrous> pixelRes = atrousSSBO.toCPU<Atrous>();
//     // Pixel* pixelRes = pixelSSBO.get<Pixel>();

//     // for (size_t i = 0; i < 10; ++i) {
        
//     //     Ray r = res[i];
//     //     Pixel p = pixelRes[i];

//     //     glm::vec4 color, hit;
//     //     color = p.col / float(MAX_ITER);
//     //     if( r.matId >= 0 ) {
//     //         Material mat = obj.materials[ r.matId ];
//     //         hit = r.o + r.t * r.dir;
//     //     } else {
//     //         // color = glm::vec4(0.f);
//     //         hit = glm::vec4(0.f);
//     //     }
        
//     //     printf(
//     //         "hit: %f, %f, %f\t" 
//     //         "color: %f, %f, %f\n",
//     //         hit.x,
//     //         hit.y,
//     //         hit.z,
//     //         color.x,
//     //         color.y,
//     //         color.z
//     //     );
//     // }

//     std::vector<unsigned char> img(WIDTH * HEIGHT * 3);

//     for (int y = 0; y < HEIGHT; ++y) {
//         for (int x = 0; x < WIDTH; ++x) {

//             size_t src = y * WIDTH + x;
//             size_t dst = (HEIGHT - 1 - y) * WIDTH + x;

//             // Ray r = res[src];
//             Pixel p = pixelRes[ src ];
//             glm::vec4 color = p.col;
            
//             // glm::vec4 color = p.col / float(MAX_ITER);

//             // glm::vec4 color;
//             // if( r.matId >= 0 ) {
//             //     Material mat = obj.Materials[ r.matId ];
//             //     color = mat.diffuse;
//             // } else {
//             //     color = glm::vec4(0.f);
//             // }

//             glm::vec3 c = glm::clamp(
//                 glm::vec3(color),
//                 glm::vec3(0.0f),
//                 glm::vec3(1.0f)
//             );

//             img[dst * 3 + 0] = static_cast<unsigned char>(c.r * 255.0f);
//             img[dst * 3 + 1] = static_cast<unsigned char>(c.g * 255.0f);
//             img[dst * 3 + 2] = static_cast<unsigned char>(c.b * 255.0f);
//         }
//     }

//     stbi_write_jpg(
//         "output5.jpg",
//         WIDTH,
//         HEIGHT,
//         3,
//         img.data(),
//         100
//     );

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