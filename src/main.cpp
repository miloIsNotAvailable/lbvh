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

    size_t p = 1;
    while (p < obj.triangles.size())
        p <<= 1;

    Triangle dummy(
        glm::vec3(1e10f, 1e10f, 1e10f),
        glm::vec3(1e10f, 1e10f, 1e10f),
        glm::vec3(1e10f, 1e10f, 1e10f)
    );

    dummy.matId = -1;

    obj.triangles.resize( p, dummy );
    
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

    // for (const Triangle& tri : obj.triangles) {
    //     // printf( "%f, %f, %f\n", tri.u.x, tri.u.y, tri.u.z );
    //     glm::vec3 A = tri.u;
    //     glm::vec3 B = tri.v;
    //     glm::vec3 C = tri.w;

    //     glm::vec3 dir = glm::normalize(
    //         glm::cross(B - A, C - A)
    //     );

    //     glm::vec3 center = (A + B + C) / 3.0f;

    //     // glm::vec3 jitter(
    //     //     ((float)rand() / RAND_MAX) * 0.2f - 0.1f,
    //     //     ((float)rand() / RAND_MAX) * 0.2f - 0.1f,
    //     //     ((float)rand() / RAND_MAX) * 0.2f - 0.1f
    //     // );

    //     glm::vec3 o =
    //         center +
    //         dir * -10.0f;

    //     rays.push_back(Ray{
    //         glm::vec4( o, 0.f ),
    //         glm::vec4(dir, 0.f),
    //         glm::vec4(0.0f),
    //         0.f, 1e30f
    //     });
    // }

    // for( auto& t : triangles ) {
    
    //     printf( "%f, %f, %f\n", t.c.x, t.c.y, t.c.z );

    //     glm::vec3 norm = (t.c - aabb.bmin) / (aabb.bmax - aabb.bmin) ;

    //     t.quantized = glm::uvec3(norm * 1023.f);

    //     printf("%u, %u, %u\n",
    //         t.quantized.x,
    //         t.quantized.y,
    //         t.quantized.z);
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

    Buffer trianglesSSBO(
        GL_SHADER_STORAGE_BUFFER,
        obj.triangles.size() * sizeof(Triangle),
        obj.triangles.data(),
        GL_DYNAMIC_COPY
    );

    Buffer mortonSSBO(
        GL_SHADER_STORAGE_BUFFER,
        obj.triangles.size() * sizeof(glm::uvec2),
        nullptr,
        GL_DYNAMIC_COPY
    );

    Buffer sceneUBO(
        GL_UNIFORM_BUFFER,
        sizeof(AABB),
        &aabb,
        GL_DYNAMIC_COPY
    );

    mortonSSBO.toGPU(0);
    trianglesSSBO.toGPU(1);
    sceneUBO.toGPU(0);

    GLuint numElements = static_cast<GLuint>(rays.size());
    GLuint localSize   = 64;

    GLuint groups =
        (numElements + localSize - 1) / localSize;

    // GLuint quantizeProgram = createQuantizeShader();
    // GLuint mortonProgram   = createMortonShader();

    // dispatchProgram((triangles.size() + 1 - 1) / 1, 1, 1, quantizeProgram);
    // dispatchProgram((triangles.size() + 1 - 1) / 1, 1, 1, mortonProgram);
    
    Program morton( mortonSrc );
    morton( groups, 1, 1 );


    Buffer extractSSBO(
        GL_SHADER_STORAGE_BUFFER,
        obj.triangles.size() * sizeof(glm::uvec2),
        nullptr,
        GL_DYNAMIC_COPY
    );

    Buffer outputSSBO(
        GL_SHADER_STORAGE_BUFFER,
        obj.triangles.size() * sizeof(glm::uvec2),
        nullptr,
        GL_DYNAMIC_COPY
    );

    Buffer uniformUBO(
        GL_UNIFORM_BUFFER,
        sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_DRAW
    );

    Buffer trianglesSortedSSBO(
        GL_SHADER_STORAGE_BUFFER,
        obj.triangles.size() * sizeof(Triangle),
        nullptr,
        GL_DYNAMIC_COPY
    );

    trianglesSSBO.toGPU(1);
    extractSSBO.toGPU(2);
    outputSSBO.toGPU(3);
    uniformUBO.toGPU(0);
    trianglesSortedSSBO.toGPU(6);

    // GLuint extractProgram = createExtractShader();
    // GLuint scanProgram    = createScanShader();
    // GLuint scatterProgram = createScatterShader();

    Program extract( radixExtractSrc );
    Program scan( radixScanSrc );
    Program scatter( radixScatterSrc );

    for (int i = 0; i < 30; ++i) {

        uniformUBO.update(
            &i,
            sizeof(i)
        );

        // dispatchProgram(groups, 1, 1, extractProgram);
        // dispatchProgram(groups, 1, 1, scanProgram);
        // dispatchProgram(groups, 1, 1, scatterProgram);
        extract( groups, 1, 1 );
        barrier( GL_SHADER_STORAGE_BARRIER_BIT );
        scan( groups, 1, 1 );
        barrier( GL_SHADER_STORAGE_BARRIER_BIT );
        scatter( groups, 1, 1 );

        // glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        barrier( GL_SHADER_STORAGE_BARRIER_BIT );

        swap(mortonSSBO, outputSSBO);

        mortonSSBO.toGPU(0);
        outputSSBO.toGPU(3);

        // swap(trianglesSSBO, trianglesSortedSSBO);

        // trianglesSSBO.toGPU(1);
        // trianglesSortedSSBO.toGPU(6);
    }

    Program reorder( radixReorderTrianglesSrc );
    reorder( groups, 1, 1 );
    barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    trianglesSortedSSBO.toCPU();

    Triangle* sortedTriangles = trianglesSortedSSBO.get<Triangle>();

    // for (size_t i = 0; i < obj.triangles.size(); ++i) {
    //     const Triangle& t = sortedTriangles[i];

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
    //         << t.c.x << ", "
    //         << t.c.y << ", "
    //         << t.c.z << '\n';
    // }


    // BVH

    std::cout << '\n';

    std::vector<Node> nodes(2 * obj.triangles.size() - 1);

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

    bvhSSBO.toGPU(4);
    matSSBO.toGPU(6);
    pixelSSBO.toGPU(7);

    Buffer traverseSSBO(
        GL_SHADER_STORAGE_BUFFER,
        rays.size() * sizeof(Ray),
        // rays.data(),
        nullptr,
        GL_DYNAMIC_COPY
    );

    traverseSSBO.toGPU(5);

    Program lbvh( lbvhSrc );
    Program aabbs( aabbSrc );
    
    lbvh( groups, 1, 1 );
    barrier( GL_SHADER_STORAGE_BARRIER_BIT );
    aabbs( groups, 1, 1 );
    barrier( GL_SHADER_STORAGE_BARRIER_BIT );

    // GLuint bvhProgram = createLBVHShader();
    // dispatchProgram(groups, 1, 1, bvhProgram);

    // GLuint aabbProgram = createAABBShader();
    // dispatchProgram(groups, 1, 1, aabbProgram);

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
    
    Program generateShadowRays( generateShadowRaySrc );
    Program traceShadowRays( traverseShadowRaySrc );
    
    const int MAX_ITER = 10;
    for( int i = 0; i < MAX_ITER; i ++ ) {

        traverseSSBO.toGPU( 0 );
        pixelSSBO.toGPU( 1 );
        cameraUBO.toGPU( 1 );
    
        generatePrimaryRays( groups, 1, 1 );
        barrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        
        for( int bounces = 0; bounces < 7; bounces ++ ) {
    
            trianglesSSBO.toGPU( 0 );
            bvhSSBO.toGPU( 1 );
            traverseSSBO.toGPU( 2 );
            cameraUBO.toGPU( 1 );
            
            traverse( groups, 1, 1 );
            barrier(GL_BUFFER_UPDATE_BARRIER_BIT);
            
            // trianglesSSBO.toGPU( 0 );
            // bvhSSBO.toGPU( 1 );
        
            trianglesSSBO.toGPU( 0 );
            traverseSSBO.toGPU( 1 );
            pixelSSBO.toGPU( 2 );
            shadowRaySSBO.toGPU( 3 );
            normalsSSBO.toGPU( 4 );
        
            generateShadowRays( groups, 1, 1 );
            
            barrier( GL_BUFFER_UPDATE_BARRIER_BIT );
        
            trianglesSSBO.toGPU( 0 );
            bvhSSBO.toGPU( 1 );
            shadowRaySSBO.toGPU( 2 );
        
            traceShadowRays( groups, 1, 1 );
        
            barrier( GL_BUFFER_UPDATE_BARRIER_BIT );
        
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
            
            bounce( groups, 1, 1 );
            barrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        }
        
        pixelSSBO.toGPU( 0 );
        evalBounce( groups, 1, 1 );
        barrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    }


    // hitSSBO.toCPU();
    traverseSSBO.toCPU();
    // Hit* res = hitSSBO.get<Hit>();
    Ray* res = traverseSSBO.get<Ray>();

    pixelSSBO.toCPU();
    Pixel* pixelRes = pixelSSBO.get<Pixel>();

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

    // GLuint trianglesSSBO = generateBuffer( GL_SHADER_STORAGE_BUFFER, triangles.size() * sizeof(Triangle), triangles.data(), GL_DYNAMIC_COPY );
    // GLuint mortonSSBO = generateBuffer( GL_SHADER_STORAGE_BUFFER, triangles.size() * sizeof(glm::uvec2), nullptr, GL_DYNAMIC_COPY );
    // GLuint sceneUBO = generateBuffer( GL_UNIFORM_BUFFER, sizeof(AABB), &aabb, GL_DYNAMIC_COPY );
    
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mortonSSBO);
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, trianglesSSBO);
    // glBindBufferBase(GL_UNIFORM_BUFFER, 0, sceneUBO);
    
    // GLuint quantizeProgram = createQuantizeShader();
    // GLuint mortonProgram = createMortonShader();

    // // dispatchProgram( (triangles.size() + 1 - 1) / 1, 1, 1, quantizeProgram );
    // dispatchProgram( (triangles.size() + 1 - 1) / 1, 1, 1, mortonProgram );

    // // glBindBuffer( GL_SHADER_STORAGE_BUFFER, mortonSSBO );
    // // std::vector<glm::uvec2> tres( triangles.size() );
    // // glGetBufferSubData(
    // //     GL_SHADER_STORAGE_BUFFER,
    // //     0,
    // //     tres.size() * sizeof(glm::uvec2),
    // //     tres.data()
    // // );

    // // for( auto &t : tres ) {
    // //     // unsigned int idx = t >> 30;
    // //     // unsigned int morton = t << 30;
    // //     printf( "%u, %u\n", t.x, t.y );
    // // }

    // // GLuint inputSSBO = generateBuffer( GL_SHADER_STORAGE_BUFFER, triangles.size() * sizeof(glm::uvec2), triangles.data(), GL_DYNAMIC_COPY );
    // GLuint extractSSBO = generateBuffer( GL_SHADER_STORAGE_BUFFER, triangles.size() * sizeof(glm::uvec2), nullptr, GL_DYNAMIC_COPY );
    // GLuint outputSSBO = generateBuffer( GL_SHADER_STORAGE_BUFFER, triangles.size() * sizeof(glm::uvec2), nullptr, GL_DYNAMIC_COPY );
    // GLuint uniformUBO = generateBuffer( GL_UNIFORM_BUFFER, sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW );

    // GLuint trianglesSortedSSBO = generateBuffer( GL_SHADER_STORAGE_BUFFER, triangles.size() * sizeof(Triangle), nullptr, GL_DYNAMIC_COPY );
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, trianglesSortedSSBO);

    // // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, inputSSBO);
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, trianglesSSBO);
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, extractSSBO);
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, outputSSBO);
    // glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformUBO);
    
    // GLuint extractProgram = createExtractShader();
    // GLuint scanProgram = createScanShader();
    // GLuint scatterProgram = createScatterShader();

    // GLuint numElements = triangles.size();
    // GLuint localSize = 1;

    // GLuint groups =
    //     (numElements + localSize - 1) / localSize;

    // for( int i = 0; i < 30; i ++ ) {
    //     // bindBuffer( uniformSSBO, sizeof(uint32_t), &i, GL_DYNAMIC_COPY );
    //     modifyBufferData(
    //         uniformUBO,
    //         GL_UNIFORM_BUFFER,
    //         0,
    //         sizeof(uint32_t),
    //         &i
    //     );

    //     dispatchProgram( groups, 1, 1, extractProgram );
    //     dispatchProgram( groups, 1, 1, scanProgram );
    //     dispatchProgram( groups, 1, 1, scatterProgram );
    //     // scan( data );
    //     // scatter( data );
       
    //     glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    //     std::swap(mortonSSBO, outputSSBO);

    //     glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mortonSSBO);
    //     glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, outputSSBO);

    //     std::swap(trianglesSSBO, trianglesSortedSSBO);


    //     glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, trianglesSSBO);
    //     glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, trianglesSortedSSBO);
    // }

    // glDeleteBuffers( 1, &outputSSBO );
    // outputSSBO=0;

    // glDeleteBuffers( 1, &trianglesSortedSSBO );
    // trianglesSortedSSBO=0;

    // std::vector<Triangle> sortedTriangles(triangles.size());

    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, trianglesSSBO);

    // glGetBufferSubData(
    //     GL_SHADER_STORAGE_BUFFER,
    //     0,
    //     sortedTriangles.size() * sizeof(Triangle),
    //     sortedTriangles.data()
    // );

    // for (size_t i = 0; i < sortedTriangles.size(); ++i) {
    //     const Triangle& t = sortedTriangles[i];

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

    // // Radix radix( inputSSBO, data.size() );
    // // for( int i = 0; i < 32; i ++ ) {
    // //     outputSSBO = radix( i, groups, 1, 1 );
    // //     std::swap(inputSSBO, outputSSBO);

    // //     glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, inputSSBO);
    // //     glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, outputSSBO);
    // // }

    // // glBindBuffer( GL_SHADER_STORAGE_BUFFER, outputSSBO );
    // // std::vector<glm::uvec2> res( triangles.size() );
    // // glGetBufferSubData(
    // //     GL_SHADER_STORAGE_BUFFER,
    // //     0,
    // //     res.size() * sizeof(glm::uvec2),
    // //     res.data()
    // // );

    // // // res.erase(
    // // //     std::remove(res.begin(), res.end(), 0),
    // // //     res.end());

    // // for( auto& d : res ) {
    // //     printf( "%d, %d\n", d.x, d.y );
    // // }

    // std::cout << "\n";
    // std::vector<Node> nodes(2 * triangles.size() - 1);

    // GLuint bvhSSBO = generateBuffer( GL_SHADER_STORAGE_BUFFER, nodes.size() * sizeof(Node), nodes.data(), GL_DYNAMIC_COPY );
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, bvhSSBO);
    
    // GLuint traverseSSBO = generateBuffer( GL_SHADER_STORAGE_BUFFER, rays.size() * sizeof(Ray), rays.data(), GL_DYNAMIC_COPY );
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, traverseSSBO);

    // GLuint bvhProgram = createLBVHShader();
    // dispatchProgram( groups, 1, 1, bvhProgram );

    // GLuint aabbProgram = createAABBShader();
    // dispatchProgram( groups, 1, 1, aabbProgram );

    // glDeleteBuffers( 1, &mortonSSBO );
    // mortonSSBO=0;

    // GLuint traverseProgram = createTraversalShader();
    // dispatchProgram( groups, 1, 1, traverseProgram );

    // glBindBuffer( GL_SHADER_STORAGE_BUFFER, traverseSSBO );
    // std::vector<Ray> res( rays.size() );
    // glGetBufferSubData(
    //     GL_SHADER_STORAGE_BUFFER,
    //     0,
    //     res.size() * sizeof(Ray),
    //     res.data()
    // );

    // for( auto& r : res ) {
    //     printf( "%f, %f, %f\n", r.hit.x, r.hit.y, r.hit.z );
    // }

    // glBindBuffer( GL_SHADER_STORAGE_BUFFER, bvhSSBO );
    // // std::vector<Node> res( triangles.size() * 2 - 1 );
    // glGetBufferSubData(
    //     GL_SHADER_STORAGE_BUFFER,
    //     0,
    //     nodes.size() * sizeof(Node),
    //     nodes.data()
    // );

    // glBindBuffer( GL_SHADER_STORAGE_BUFFER, mortonSSBO );
    // std::vector<glm::uvec2> sorted( triangles.size() );
    // glGetBufferSubData(
    //     GL_SHADER_STORAGE_BUFFER,
    //     0,
    //     sorted.size() * sizeof(glm::uvec2),
    //     sorted.data()
    // );

    // for( int i =0; i < nodes.size(); i ++ ) {
    //     Node d = nodes[ i ];
    //     if( i > triangles.size() - 1 ) {
    //         uint32_t id = sorted[ i - triangles.size() + 1 ].y;
    //         printf( "%d, %d, %d, %d\n", d.parent, d.left, d.right, id );
    //     } else {
    //         printf( "%d, %d, %d, %d\n", d.parent, d.left, d.right, d.isLeaf );
    //     }
    //     // printf( "%f, %f, %f\n", d.aabb.bmin.x, d.aabb.bmin.y, d.aabb.bmin.z );
    //     // printf( "%f, %f, %f\n", d.aabb.bmax.x, d.aabb.bmax.y, d.aabb.bmax.z );
    //     // printf( "\n\n" );
    // }

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