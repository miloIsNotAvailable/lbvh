#pragma once
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <algorithm>
#include <iostream>

struct AABB {
    glm::vec4 bmin = glm::vec4( float(INT_MAX) ); 
    glm::vec4 bmax = glm::vec4( -float(INT_MAX) );
    AABB() {}

    void grow( glm::vec3 e ) {
        bmin.x = std::min( bmin.x, e.x );
        bmin.y = std::min( bmin.y, e.y );
        bmin.z = std::min( bmin.z, e.z );

        bmax.x = std::max( bmax.x, e.x );
        bmax.y = std::max( bmax.y, e.y );
        bmax.z = std::max( bmax.z, e.z );
    }

    glm::vec4 centroid() {
        return (bmin + bmax) * .5f;
    }
};

struct Triangle {
    glm::vec4 u, v, w;
    glm::uvec4 quantized;
    glm::vec4 c;
    AABB aabb;
    int matId;
    Triangle(glm::vec3 u, glm::vec3 v, glm::vec3 w)
        : u(glm::vec4( u, 0. )), v(glm::vec4( v, 0. )), w(glm::vec4( w, 0. ))
    {
        aabb.grow( u );
        aabb.grow( v );
        aabb.grow( w );

        c = aabb.centroid();
    }
    Triangle() {}
};