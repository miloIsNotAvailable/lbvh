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
    glm::vec3 point;
    glm::vec3 dir;

    Lens() = default;
    Lens( glm::vec3 point, glm::vec3 dir );
};

class Camera {
    public:

    float sensorDist, focalPlaneDist;
    float apertureSize;
    glm::vec3 eye, center, n;
    glm::vec3 sensor, focalPlane;

    glm::vec3 worldUp, right, up;

    Camera( float sensorDist, float focalPlaneDist, float apertureSize,
            glm::vec3& eye, glm::vec3& center );
    Lens thinLensRay( glm::vec2& st );
};