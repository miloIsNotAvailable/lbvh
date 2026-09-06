#pragma once


#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
// #include "constants.hpp"
// #include "random.hpp"
#include <random>
#include <utils.hpp>
#include <randoms.hpp>

#define PI 3.1415926

inline float hash()
{
    static thread_local std::mt19937 rng(std::random_device{}());
    static thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    return dist(rng);
}

struct Lens {
    glm::vec4 point;
    glm::vec4 dir;

    Lens() = default;
    Lens( glm::vec3 point, glm::vec3 dir );
};

class Camera {
    public:

    glm::vec4 eye, center, n;
    glm::vec4 sensor, focalPlane;

    glm::vec4 worldUp, right, up;
    
    float sensorDist, focalPlaneDist;
    float apertureSize;
    uint32_t WIDTH, HEIGHT;

    Camera( float sensorDist, float focalPlaneDist, float apertureSize,
            glm::vec3& eye, glm::vec3& center );
    Lens thinLensRay( glm::vec2& st );
};

struct Light {

    glm::vec4 pos;
    glm::vec4 Le;
    glm::vec4 normal;
    Light( glm::vec3 pos, float r, glm::vec4 Le, glm::vec4 normal ) 
    : pos( glm::vec4(pos, r) ), Le(Le), normal(normal) {}

};

class Film {
    private:
    glm::uvec2 dimensions;

    public:
    Buffer camera, L, beta, pixel, 
    dims, iteration, light;
    Film( uint32_t w, uint32_t h, Camera &camera, Light &light ) : 
    dimensions( w, h ),
    L(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(glm::vec4),
        std::vector<glm::vec4>(w * h, glm::vec4(0.0f)).data(),
        GL_DYNAMIC_COPY   
    ),
    pixel(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(glm::vec4),
        std::vector<glm::vec4>(w * h, glm::vec4(0.0f)).data(),
        GL_DYNAMIC_COPY   
    ),
    beta(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(glm::vec4),
        std::vector<glm::vec4>(w * h, glm::vec4(1.0f)).data(),
        GL_DYNAMIC_COPY   
    ),
    light(
        GL_SHADER_STORAGE_BUFFER,
        sizeof(Light),
        &light,
        GL_DYNAMIC_COPY   
    ),
    camera(
        GL_UNIFORM_BUFFER,
        sizeof(Camera),
        &camera,
        GL_DYNAMIC_COPY   
    ),
    dims(
        GL_UNIFORM_BUFFER,
        sizeof(dimensions),
        &dimensions,
        GL_DYNAMIC_COPY   
    ),
    iteration(
        GL_UNIFORM_BUFFER,
        sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    ) {}
};