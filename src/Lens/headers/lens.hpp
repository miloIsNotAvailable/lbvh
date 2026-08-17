#pragma once


#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
// #include "constants.hpp"
// #include "random.hpp"
#include <random>

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