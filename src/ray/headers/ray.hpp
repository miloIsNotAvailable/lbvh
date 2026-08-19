#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <utils.hpp>

// note to self: NEVER use vec3 for SSBOs
// only vec4 and vec2 

struct Ray {
    glm::vec4 o;
    glm::vec4 dir;
    // glm::vec4 hit;
    float tmin;
    float tmax;
    float t;
    uint32_t triId;
    uint32_t dead;
    uint32_t pixelId;
    int matId;
    // glm::vec3 t;

    Ray() {};

    Ray( glm::vec3 o, glm::vec3 dir, uint32_t shadowRay ) 
    : o(glm::vec4(o, 0.)), 
    dir(glm::vec4(dir, 0.)), 
    tmin(0.f), tmax(1e30f), 
    matId(-1), dead( 0 )
    {};
};

struct ShadowRay {
    glm::vec4 o;
    glm::vec4 dir;
    glm::vec4 normal;
    float tmin;
    float tmax;
    uint32_t occluded;
};

struct Pixel {
    glm::vec4 L;
    glm::vec4 beta;
    glm::vec4 col;
    uint32_t pixelId;
    uint32_t state;

    Pixel() {};

    Pixel( uint32_t pId ) : 
    pixelId( pId ),
    beta( glm::vec4( 1.f ) ), 
    L( glm::vec4( 0.f ) ) ,
    col( glm::vec4( 0.f ) ) ,
    state( pId * 747796405u + 2891336453u )
    {};
};

const std::string raysLayout = R"(#version 430

    layout(local_size_x = 64) in;)";

// inline const std::string raysHeader = raysLayout + structs + R"(

//     layout(std430, binding = 0) buffer Morton
//     {
//         uvec2 morton[];
//     };
    
//     layout(std430, binding = 1) buffer TriIn
//     {
//         Triangle triangles[];
//     };

//     layout(std430, binding = 4) buffer LBVH
//     {
//         Node nodes[];
//     };

//     layout(std430, binding = 5) buffer Rays
//     {
//         Ray rays[];
//     };

//     layout(std430, binding = 6) buffer Mats
//     {
//         Material materials[];
//     };

//     layout(std430, binding = 7) buffer Pixels
//     {
//         Pixel pixels[];
//     };

//     layout(std430, binding = 8) buffer ShadowRays
//     {
//         ShadowRay shadowRays[];
//     };
// )";

inline const std::string bounceHeader = raysLayout + structs + R"(

    layout(std430, binding = 0) buffer Rays
    {
        Ray rays[];
    };

    layout(std430, binding = 1) buffer Mats
    {
        Material materials[];
    };

    layout(std430, binding = 2) buffer Pixels
    {
        Pixel pixels[];
    };

    layout(std430, binding = 3) buffer ShadowRays
    {
        ShadowRay shadowRays[];
    };

    layout(std430, binding = 4) buffer Normals
    {
        vec4 normals[];
    };

    layout(std430, binding = 5) buffer Scans
    {
        uint scans[];
    };
)";


inline const std::string compactRaysScatterSrc = raysLayout + structs + R"(

    layout(std430, binding = 0) buffer Rays
    {
        Ray rays[];
    };
    
    layout(std430, binding = 1) buffer Scans
    {
        uint scans[];
    };

    layout(std430, binding = 2) buffer OutputRays
    {
        Ray outputRays[];
    };

    layout(std430, binding = 3) buffer RayCount
    {
        uint rayCount;
    };

    void main()
    {
        uint id = gl_GlobalInvocationID.x;

        if( id >= rayCount ) return;  

        if( rays[id].dead == 0u ) {
            uint dst = scans[id];
            outputRays[dst] = rays[id];
        }

        if( id == 0u ) {
            rayCount = scans[ rayCount - 1 ] + 1u - rays[rayCount - 1].dead;
        }
    }
)";  

inline const std::string evalBouncesSrc = raysLayout + structs + R"(

    layout(std430, binding = 0) buffer Pixels
    {
        Pixel pixels[];
    };

    void main() {

        uint id = gl_GlobalInvocationID.x;
        
        if( id >= pixels.length() ) return;

        pixels[id].col += pixels[id].L;
        pixels[id].L = vec4(0.f);
        pixels[id].beta = vec4(1.f);
    }
)";  

inline const std::string computeBounceSrc = bounceHeader + R"(

    vec3 RandomUnitVectorInHemisphereOf(vec3 normal, vec2 r) {
        float r1 = r.x;
        float r2 = r.y;
        
        // Convert to polar coordinates
        float phi = 2.0 * PI * r1;
        float cosTheta = sqrt(1.0 - r2);
        float sinTheta = sqrt(r2);
        
        // Calculate the direction in local space
        vec3 localDir = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
        
        // Create a transformation matrix from the normal
        vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
        vec3 tangentX = normalize(cross(up, normal));
        vec3 tangentY = cross(normal, tangentX);
        
        // Transform the local direction to world space
        vec3 worldDir = localDir.x * tangentX + localDir.y * tangentY + localDir.z * normal;
        
        return normalize(worldDir);
    }

    void main()
    {
        uint id = gl_GlobalInvocationID.x;
        
        if( id >= rays.length() ) return;

        if( scans[id] == 1u ) {
            // pixels[id].L = vec4(0.f);
            return;
        }

        uint matIdx = uint(rays[id].matId);
        uint triIdx = uint(rays[id].triId);

        vec4 pos = rays[id].o + rays[id].t * rays[id].dir;

        vec3 color = materials[matIdx].diffuse.xyz / PI;
        vec3 beta = pixels[id].beta.xyz;
        vec3 nor = normals[triIdx].xyz;
        vec3 lightPos = vec3( 0., 200., 0. );
        float r = 10.;
        vec3 Le = vec3( 200.f );

        if (dot(rays[id].dir.xyz, nor) > 0.0)
            nor = -nor;

        vec3 liray = shadowRays[id].dir.xyz;
        float cosTheta = max(dot(nor, liray), 0.);
        if (cosTheta <= 0.) return;

        vec3 normal = shadowRays[id].normal.xyz;
        float dist2 = (shadowRays[id].tmax ) * (shadowRays[id].tmax );

        float cosThetaL = max(dot(normal, -liray), 0.);
        if( cosThetaL <= 0.0 ) return;

        float pdf_area = 1. / (4. * PI * r * r);
        float pdf_omega = pdf_area * dist2 / cosThetaL;
        vec3 Li = Le / pdf_omega;

        float pdf = cosTheta / PI; 

        // pixels[id].L += materials[matIdx].diffuse;
        float occ = 1.f - float(shadowRays[id].occluded);
        pixels[id].L  += vec4(occ * beta * color * Li * cosTheta, 0.f);
        // pixels[id].L  += vec4(color, 0.f);
        

        float bx = rand( pixels[id].state );
        float by = rand( pixels[id].state );
        
        vec3 bounceDir = RandomUnitVectorInHemisphereOf( nor, vec2(bx, by) );
        
        cosTheta = max(dot(nor, bounceDir), 0.);
        if (cosTheta <= 0.) return;

        pdf = cosTheta / PI; 

        pixels[id].beta *= vec4(color * cosTheta / pdf, 0.f);

        rays[id].o = pos + vec4(nor * EPS, 0.f);
        rays[id].dir = vec4( bounceDir, 0.f );
    }
)";

inline const std::string generatePrimaryRayHeader = raysLayout + structs + R"(
    layout(std430, binding = 0) buffer Rays
    {
        Ray rays[];
    };
    
    layout(std430, binding = 1) buffer Pixels
    {
        Pixel pixels[];
    };

    layout(std140, binding = 1) uniform CameraData
    {
        Camera camera;
    };
)";

inline const std::string generatePrimaryRaySrc = generatePrimaryRayHeader + R"(

    void main() {
    
        uint id = gl_GlobalInvocationID.x;
        
        if( id >= rays.length() ) return;

        uint WIDTH = camera.WIDTH;
        uint HEIGHT = camera.HEIGHT;

        uint x = id % WIDTH;
        uint y = id / WIDTH;

        vec2 fragCoord = vec2(float(x), float(y)) + 0.5;

        vec2 st = fragCoord / vec2(float(WIDTH), float(HEIGHT)) - 0.5;

        st.x *= float(WIDTH) / float(HEIGHT);

        // uint state = id * 747796405u + 2891336453u;
        
        // pixels[id].state = state;

        float r = rand(pixels[id].state) * camera.apertureSize;
        float a = rand(pixels[id].state) * (2.0 * PI);

        vec2 rndPointOnAperture =
            vec2(r * cos(a), r * sin(a));

        vec3 rndAperturePointWrld =
            camera.eye.xyz
            + rndPointOnAperture.x * camera.right.xyz
            + rndPointOnAperture.y * camera.up.xyz;

        vec3 pixelWrld =
            camera.sensor.xyz
            + st.x * camera.right.xyz
            + st.y * camera.up.xyz;

        vec3 primaryRay =
            normalize(pixelWrld - camera.eye.xyz);

        vec3 F_c =
            camera.focalPlane.xyz - camera.eye.xyz;

        float dirDotNor =
            dot(primaryRay, camera.n.xyz);

        float focalPlaneParam =
            dot(F_c, camera.n.xyz) / dirDotNor;

        vec3 focalPlanePoint =
            camera.eye.xyz
            + primaryRay * focalPlaneParam;

        vec3 rayDir =
            normalize(focalPlanePoint - rndAperturePointWrld);

        rays[id].o   = vec4(rndAperturePointWrld, 0.0);
        rays[id].dir = vec4(rayDir, 0.0);

        rays[id].tmin = 0.0;
        rays[id].tmax = 1e30f;
        rays[id].t = -1.0;
        rays[id].triId = -1;
        rays[id].dead = 0;
        rays[id].pixelId = id;
    }

)";

inline const std::string generateShadowRaysHeader = raysLayout + structs + R"(
    
    layout(std430, binding = 0) buffer TriIn
    {
        Triangle triangles[];
    };

    layout(std430, binding = 1) buffer Rays
    {
        Ray rays[];
    };

    layout(std430, binding = 2) buffer Pixels
    {
        Pixel pixels[];
    };

    layout(std430, binding = 3) buffer ShadowRays
    {
        ShadowRay shadowRays[];
    };

    layout(std430, binding = 4) buffer Normals
    {
        vec4 normals[];
    };
)";

inline const std::string generateShadowRaySrc = generateShadowRaysHeader + R"(

    void main()
    {
        uint id = gl_GlobalInvocationID.x;
        
        if( id >= rays.length() ) return;

        if( rays[id].t == -1 ) return;

        vec4 pos = rays[id].o + rays[id].t * rays[id].dir;
        
        // Triangle tri = triangles[rays[id].triId];
        // vec3 nor = normalize(cross(
        //     tri.v.xyz - tri.u.xyz,
        //     tri.w.xyz - tri.u.xyz
        // ));

        vec3 nor = normals[ rays[id].triId ].xyz;

        if( dot( rays[id].dir.xyz, nor ) > 0 ) nor = -nor;

        vec3 lightPos = vec3( 0., 200., 0. );
        float r = 10.;

        // uint state = id * 747796405u + 2891336453u;
        float zx = rand(pixels[id].state);
        float zy = rand(pixels[id].state);

        float rndTheta = zx * 2. * PI;
        float rndZ = zy * 2. - 1.;
        float rndX = sqrt( 1. - rndZ * rndZ ) * cos( rndTheta );
        float rndY = sqrt( 1. - rndZ * rndZ ) * sin( rndTheta );
        
        vec3 rndPoint = lightPos + r * vec3( rndX, rndY, rndZ );
        vec3 normal = normalize( rndPoint - lightPos );

        vec3 lDir = rndPoint - pos.xyz;
        vec3 liray = normalize( lDir );
        float dist2 = dot( lDir, lDir );

        shadowRays[id].tmin = EPS;
        shadowRays[id].tmax = sqrt(dist2) - EPS;
        shadowRays[id].o = pos + vec4(nor, 0.) * EPS;
        shadowRays[id].normal = vec4(normal, 0.f);
        shadowRays[id].dir = vec4(liray, 0.f);
    }
)";