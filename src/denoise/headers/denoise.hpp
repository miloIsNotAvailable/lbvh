#pragma once 

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <string>
#include <glm/glm.hpp>
#include <utils.hpp>
#include<array>

struct Atrous {
    glm::vec4 pos;
    glm::vec4 nor;
    uint32_t valid;
};

inline const std::string saveDenoiseInfoSrc = R"(
#version 430

layout(local_size_x = 64) in;
)" + structs + R"(
layout(std430, binding = 0) buffer Depth
{
    float depth[];
};

layout(std430, binding = 1) buffer OutNormals
{
    float outNormals[];
};

layout(std430, binding = 2) buffer Valid
{
    uint valid[];
};

layout(std430, binding = 3) buffer Dead
{
    uint dead[];
};

layout(std430, binding = 4) buffer Normals
{
    vec4 normals[];
};

layout(std430, binding = 5) buffer TriIds
{
    uint triIds[];
};

layout(std430, binding = 6) buffer Triangles
{
    Triangle triangles[];
};

layout(std430, binding = 7) buffer Materials
{
    Material materials[];
};

layout(std430, binding = 8) buffer RayPosDir
{
    float rays[];
};

layout(std430, binding = 9) buffer HitT
{
    float hitT[];
};

layout(std140, binding = 0) uniform SceneData
{
    AABB scene;
};


void main() {

    uint id = gl_GlobalInvocationID.x;

    uint N = rays.length() / 6;

    if (id >= N)
        return;

    if (dead[id] == 1u)
    {
        depth[id + N * 0] = 0.f;
        depth[id + N * 1] = 0.f;
        depth[id + N * 2] = 0.f;
        
        outNormals[id + N * 0] = -1.f;
        outNormals[id + N * 1] = 0.f;
        outNormals[id + N * 2] = 0.f;

        valid[id] = 0u;
        
        return;
    }

    vec3 o = vec3(
        rays[ id + N * 0 ],
        rays[ id + N * 1 ],
        rays[ id + N * 2 ]
    );

    vec3 d = vec3(
        rays[ id + N * 3 ],
        rays[ id + N * 4 ],
        rays[ id + N * 5 ]
    );

    vec3 pos = o + hitT[id] * d;

    uint triIdx = uint(triIds[id]);

    vec3 nor = normals[triIdx].xyz;

    if (dot(d, nor) > 0.0)
        nor = -nor;

    vec3 pos01 = (pos - scene.bmin.xyz) / (scene.bmax.xyz - scene.bmin.xyz);

    depth[id + N * 0] = pos01.x;
    depth[id + N * 1] = pos01.y;
    depth[id + N * 2] = pos01.z;

    vec3 nor01 = nor * .5 + .5;

    outNormals[id + N * 0] = nor01.x;
    outNormals[id + N * 1] = nor01.y;
    outNormals[id + N * 2] = nor01.z;

    valid[id] = 1u;
}
)";

inline const std::string denoiseSrc = R"(

#version 430

layout(local_size_x = 64) in;
)" + structs + 
R"(
layout(std430, binding = 0) buffer Colors
{
    vec4 colors[];
};

layout(std430, binding = 1) buffer Normals
{
    float normals[];
};

layout(std430, binding = 2) buffer Depth
{
    float depth[];
};

layout(std430, binding = 3) buffer Valid
{
    uint valid[];
};

layout(std430, binding = 4) buffer Offsets
{
    ivec2 offsets[];
};

layout(std430, binding = 5) buffer Kernel
{
    float kernel[];
};

layout(std430, binding = 6) buffer DenoiseOut
{
    vec4 outPixel[];
};

layout(std140, binding = 0) uniform Step
{
    int sw;
};

layout(std140, binding = 1) uniform CameraData
{
    Camera camera;
};

void main() {
    uint id = gl_GlobalInvocationID.x;

    uint N = normals.length() / 3;

    if( id >= colors.length() )
        return;

    int width  = int(camera.WIDTH);
    int height = int(camera.HEIGHT);

    int x = int(id) % width;
    int y = int(id) / width;

    vec3 cval = colors[ id ].xyz;
    vec3 nval = vec3(
        normals[id + N * 0],
        normals[id + N * 1],
        normals[id + N * 2]
    );
    vec3 pval = vec3(
        depth[ id + N * 0 ],
        depth[ id + N * 1 ],
        depth[ id + N * 2 ]
    );
    
    uint validC = valid[id];

    // for 4 iter
    float c_phi = .5;
    float n_phi = .001;
    float p_phi = .001;


    vec3 sum = vec3(0.0);
    float cum_w = 0.0;

    float stepwidth = float(sw);

    for( int i = 0; i < 25; i ++ ) {
        int offX = offsets[i].x * sw;
        int offY = offsets[i].y * sw;

        int row = y + offY;
        int col = x + offX;

        if (row < 0 || row >= height || col < 0 || col >= width)
            continue;

        int ind = row * width + col;
    
        uint validN = valid[uint(ind)];

        if( validN != validC ) continue;

        float n_w = 1.;
        float p_w = 1.;

        vec3 ctmp = colors[ uint(ind) ].xyz;
        vec3 t = cval - ctmp;
        float dist2 = dot(t.xyz,t.xyz);
        float c_w = min(exp(-(dist2)/c_phi), 1.0);

        if( validC == 1u ) {
            vec3 ntmp = vec3(
                normals[uint(ind) + N * 0],
                normals[uint(ind) + N * 1],
                normals[uint(ind) + N * 2]
            );
            t = nval - ntmp;
            dist2 = max(dot(t,t)/(stepwidth*stepwidth),0.0);
            n_w = min(exp(-(dist2)/n_phi), 1.0);
    
            vec3 ptmp = vec3(
                depth[uint(ind) + N * 0],
                depth[uint(ind) + N * 1],
                depth[uint(ind) + N * 2]
            );
            t = pval - ptmp;
            dist2 = dot(t,t);
            p_w = min(exp(-(dist2)/p_phi),1.0);
        }

        // float weight = c_w * n_w * p_w;
        float weight = c_w * n_w * p_w;
        sum += ctmp * weight * kernel[i];
        cum_w += weight*kernel[i];
    }

    outPixel[id] = vec4(sum / cum_w, 0.f);
}
)";

static constexpr std::array<float, 25> kernelArr = {
    1.f/256.f,  4.f/256.f,  6.f/256.f,  4.f/256.f, 1.f/256.f,
    4.f/256.f, 16.f/256.f, 24.f/256.f, 16.f/256.f, 4.f/256.f,
    6.f/256.f, 24.f/256.f, 36.f/256.f, 24.f/256.f, 6.f/256.f,
    4.f/256.f, 16.f/256.f, 24.f/256.f, 16.f/256.f, 4.f/256.f,
    1.f/256.f,  4.f/256.f,  6.f/256.f,  4.f/256.f, 1.f/256.f
};


class AtrousDenoiser {

    private:
    Buffer offstes, kernel, sw, outPixel, offsets;
    Buffer normals, depth, valid;
    
    Program saveBufferInfo, denoise;
    
    public:
    AtrousDenoiser( uint32_t size ) : 
    saveBufferInfo( saveDenoiseInfoSrc ),
    denoise( denoiseSrc ),
    outPixel(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(glm::vec4),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    normals(
        GL_SHADER_STORAGE_BUFFER,
        3 * size * sizeof(float),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    depth(
        GL_SHADER_STORAGE_BUFFER,
        3 * size * sizeof(float),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    valid(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    kernel(
        GL_SHADER_STORAGE_BUFFER,
        kernelArr.size() * sizeof(float),
        kernelArr.data(),
        GL_DYNAMIC_COPY  
    ),
    sw(
        GL_UNIFORM_BUFFER,
        sizeof(int),
        nullptr,
        GL_DYNAMIC_COPY  
    ),
    offsets(
        GL_SHADER_STORAGE_BUFFER,
        25 * sizeof(glm::ivec2),
        nullptr,
        GL_DYNAMIC_COPY         
    )
    
    {
        std::vector<glm::ivec2> off;
        off.reserve( 25 );

        for( int i = -2; i <= 2; i ++ ) {
            for( int j = -2; j <= 2; j ++ ) {
                off.push_back( glm::ivec2( i, j ) );
            }
        }

        Buffer off__(
            GL_SHADER_STORAGE_BUFFER,
            off.size() * sizeof(glm::ivec2),
            off.data(),
            GL_DYNAMIC_COPY  
        );

        offsets = std::move( off__ );
    }

    void saveGBuffers(

        uint32_t size,
        Buffer &dead,
        Buffer &normals,
        Buffer &triIds,
        Buffer &triangles,
        Buffer &materials,
        Buffer &rays,
        Buffer &hitT,
        Buffer &scene
    );

    Buffer& operator()(
        uint32_t size,
        Buffer &colors,
        Buffer &camera
    );
};