#pragma once 

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <string>
#include <glm/glm.hpp>
#include <utils.hpp>

struct Atrous {
    glm::vec4 pos;
    glm::vec4 nor;
    uint32_t valid;
};

inline const std::string denoiseLayout = R"(
#version 430

layout(local_size_x = 64) in;
)";

inline const std::string denoiseHeader = denoiseLayout + structs;

inline const std::string saveDenoiseInfoSrc = denoiseHeader + R"(

layout(std430, binding = 0) buffer HitInfo
{
    Ray rays[];
};

layout(std430, binding = 1) buffer AtrousData
{
    Atrous atrous[];
};

layout(std430, binding = 2) buffer Normals
{
    vec4 normals[];
};

layout(std140, binding = 0) uniform SceneData
{
    AABB scene;
};


void main() {

    uint id = gl_GlobalInvocationID.x;

    if (id >= rays.length())
        return;

    uint pixelId = uint(rays[id].pixelId);

    if (rays[id].t < 0.0 || rays[id].triId < 0)
    {
        atrous[pixelId].pos = vec4(0.0);
        atrous[pixelId].nor = vec4(0.0);
        atrous[pixelId].valid = 0u;
        return;
    }

    uint triIdx = uint(rays[id].triId);

    vec3 pos = rays[id].o.xyz + rays[id].t * rays[id].dir.xyz;

    vec3 nor = normals[triIdx].xyz;

    if (dot(rays[id].dir.xyz, nor) > 0.0)
        nor = -nor;

    atrous[pixelId].pos   = vec4 ((pos - scene.bmin.xyz) / (scene.bmax.xyz - scene.bmin.xyz) , 1.0);
    // atrous[pixelId].pos   = vec4 (pos, 1.0);
    atrous[pixelId].nor   = vec4(nor * .5 + .5, 0.0);
    atrous[pixelId].valid = 1u;
}
)";

inline const std::string denoiseSrc = denoiseHeader + R"(

layout(std430, binding = 0) buffer Colors
{
    Pixel pixels[];
};

layout(std430, binding = 1) buffer AtrousData
{
    Atrous atrous[];
};

layout(std430, binding = 2) buffer Offsets
{
    ivec2 offsets[];
};

layout(std430, binding = 3) buffer Kernel
{
    float kernel[];
};

layout(std430, binding = 4) buffer DenoiseOut
{
    Pixel outPixel[];
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

    if( id >= pixels.length() )
        return;

    int width  = int(camera.WIDTH);
    int height = int(camera.HEIGHT);

    int x = int(id) % width;
    int y = int(id) / width;

    vec4 cval = pixels[id].col;
    vec4 nval = atrous[id].nor;
    vec4 pval = atrous[id].pos;
    
    uint validC = atrous[id].valid;

    float c_phi = .1;
    float n_phi = .01;
    float p_phi = .01;

    vec4 sum = vec4(0.0);
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
    
        uint validN = atrous[uint(ind)].valid;

        if( validN != validC ) continue;

        float n_w = 1.;
        float p_w = 1.;

        vec4 ctmp = pixels[uint(ind)].col;
        vec4 t = cval - ctmp;
        float dist2 = dot(t.xyz,t.xyz);
        float c_w = min(exp(-(dist2)/c_phi), 1.0);

        if( validC == 1u ) {
            vec4 ntmp = atrous[uint(ind)].nor;
            t = nval - ntmp;
            dist2 = max(dot(t.xyz,t.xyz)/(stepwidth*stepwidth),0.0);
            n_w = min(exp(-(dist2)/n_phi), 1.0);
    
            vec4 ptmp = atrous[uint(ind)].pos;
            t = pval - ptmp;
            dist2 = dot(t.xyz,t.xyz);
            p_w = min(exp(-(dist2)/p_phi),1.0);
        }

        // float weight = c_w * n_w * p_w;
        float weight = c_w * n_w * p_w;
        sum += ctmp * weight * kernel[i];
        cum_w += weight*kernel[i];
    }

    outPixel[id].col = sum / cum_w;
}
)";

class Atrous {

    private:
    Buffer colors, normals, depth, valid;
    Buffer offstes;
    public:
    Atrous( uint32_t size ) : 
    
    colors(
        GL_SHADER_STORAGE_BUFFER,
        3 * size * sizeof(float),
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
        size * sizeof(float),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    valid(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    )
    
    {}

    void saveGBuffers();
};