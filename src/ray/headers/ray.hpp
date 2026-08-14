#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <utils.hpp>

// note to self: NEVER use vec3 for SSBOs
// only vec4 and vec2 

struct Ray {
    glm::vec4 o;
    glm::vec4 dir;
    glm::vec4 hit;
    float tmin;
    float tmax;
    // glm::vec3 t;

    // Ray( glm::vec4 o, glm::vec4 dir ) : o(o), dir(dir) {};
};

struct Hit {
    glm::vec4 hit;
    glm::vec4 color;
    float t;
};