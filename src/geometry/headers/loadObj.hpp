#pragma once
// #define TINYOBJLOADER_IMPLEMENTATION

#include <cstddef>
#include <string>
#include <vector>
#include <tinyobjloader.h>
#include <triangle.hpp>


// extern std::vector<Triangle> triangles;
// extern std::vector<Triangle> areaLights;
// extern std::vector<tinyobj::material_t> Materials;

static glm::vec4 toVec4(const tinyobj::real_t v[3], float w = 0.0f)
{
    return glm::vec4(v[0], v[1], v[2], w);
}

struct Material {
    glm::vec4 ambient, diffuse, specular, transmittance, emission;
    float shininess, ior, dissolve;
    
    Material( tinyobj::material_t m ) : 
    ambient( toVec4(m.ambient )),
    diffuse( toVec4(m.diffuse )),
    specular( toVec4(m.specular )),
    transmittance( toVec4(m.transmittance) ),
    shininess( m.shininess ),
    ior( m.ior ),
    dissolve( m.dissolve )
    {}
};

struct Mesh {
    std::vector<Triangle> triangles;
    std::vector<Triangle> areaLights;
    std::vector<Material> Materials;

    Mesh( std::vector<Triangle> triangles, std::vector<Material> Materials ) 
    : triangles( triangles ), Materials(Materials) {};
};

std::string loadFile();
Mesh LoadObj();