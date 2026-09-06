#pragma once

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <utils.hpp>
#include <lens.hpp>
#include <radix.hpp>
#include <triangle.hpp>
#include <algorithm>
#include <numeric>
#include <array>
#include <scan.hpp>


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
    
    layout(std430, binding = 6) buffer RayCount
    {
        uint activeRays;
    };

    layout(std430, binding = 7) buffer Triangles
    {
        Triangle triangles[];
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
            uint dst = id - scans[id];
            outputRays[dst] = rays[id];
        }

        // if( id == 0u ) {
        //     rayCount = scans[ rayCount - 1 ] + 1u - rays[rayCount - 1].dead;
        // }
    }
)";  

const std::string updateCountLayout = R"(#version 430

    layout(local_size_x = 1) in;)";

inline const std::string updateRayCountSrc = updateCountLayout + structs + R"(
    
    layout(std430, binding = 0) buffer Rays
    {
        Ray rays[];
    };
    
    layout(std430, binding = 1) buffer Scans
    {
        uint scans[];
    };

    layout(std430, binding = 2) buffer RayCount
    {
        uint rayCount;
    };

    layout(std430, binding = 3) buffer Dispatch
    {
        uint groupX;
        uint groupY;
        uint groupZ;
    };

    void main()
    {
        uint id = gl_GlobalInvocationID.x;
        if( rayCount == 0u ) return;

        uint deadTotal = scans[ rayCount - 1 ] + rays[rayCount - 1].dead;

        rayCount = rayCount - deadTotal;
        groupX = (rayCount + 64 - 1u) / 64;
        groupY = 1;
        groupZ = 1;
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

        pixels[id].col += pixels[id].L / 10;
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
        
        return (worldDir);
    }

    float PowerHeuristic( float nf, float fPdf, float ng, float gPdf ) {
        float f = nf * fPdf;
        float g = ng * gPdf;
        return ( f * f ) / ( f * f + g * g );
    }

    void main()
    {
        uint id = gl_GlobalInvocationID.x;
        
        if( id >= activeRays ) return;

        if( scans[id] == 1u ) {
            // pixels[id].L = vec4(0.f);
            return;
        }

        uint triIdx = uint(rays[id].triId);
        uint pixelId = uint( rays[id].pixelId );
        vec3 Le = vec3( 200.f );

        if( rays[id].triId == -1u ) {
            // pixels[pixelId].L += vec4(pixels[pixelId].beta.xyz * Le, 0.f); 
            rays[id].dead = 1u;
            return;
        }

        uint matIdx = uint(triangles[triIdx].matId);

        vec4 pos = rays[id].o + rays[id].t * rays[id].dir;

        vec3 color = materials[matIdx].diffuse.xyz / PI;
        vec3 beta = pixels[pixelId].beta.xyz;
        vec3 nor = normals[triIdx].xyz;
        vec3 lightPos = vec3( 0., 200., 0. );
        float r = 20.;

        if (dot(rays[id].dir.xyz, nor) > 0.0)
            nor = -nor;

        vec3 liray = shadowRays[id].dir.xyz;
        float cosTheta = max(dot(nor, liray), 0.);
        // if (cosTheta <= 0.) return;

        vec3 normal = shadowRays[id].normal.xyz;
        float dist2 = (shadowRays[id].tmax + EPS ) * (shadowRays[id].tmax + EPS);

        float cosThetaL = max(dot(normal, -liray), 0.);
        float pdf;
        if( cosThetaL > 0.f && cosTheta > 0.f ) {        
            // float pdf_area = 1. / (4. * PI * r * r);
            // float pdf_area = 1. / (PI * r * r);
            
            // float d2 = dot(lightPos - pos.xyz, lightPos - pos.xyz);
            
            // float cosThetaMax = sqrt(
            //     max(0.0, 1.0 - r*r / d2)
            // );

            // float pdf_omega = 1.0 / (2.0 * PI * (1.0 - cosThetaMax));

            float pdf_area = 1. / (PI * r * r);
            float pdf_omega = pdf_area * dist2 / cosThetaL;
            vec3 Li = Le / pdf_omega;

            pdf = cosTheta / PI; 

            // pixels[id].L += materials[matIdx].diffuse;
            float occ = 1.f - float(shadowRays[id].occluded);
            
            float w = PowerHeuristic( 1., pdf_omega, 1., pdf );
            // if( cosThetaL > 0.f && cosTheta > 0.f ) 
            // pixels[pixelId].L  += vec4( w * occ * beta * color * Li * cosTheta, 0.f);   
        }

        float bx = rand( pixels[pixelId].state );
        float by = rand( pixels[pixelId].state );
        
        vec3 bounceDir = RandomUnitVectorInHemisphereOf( nor, vec2(bx, by) );
        
        cosTheta = max(dot(nor, bounceDir), 0.);
        if (cosTheta <= 0.) return;

        pdf = cosTheta / PI;
        // pixels[pixelId].col = vec4( cosTheta ); 

        // pixels[pixelId].beta *= vec4(color * cosTheta / pdf, 0.f);

        // float p = clamp(
        //     max(pixels[pixelId].beta.x, max(pixels[pixelId].beta.y, pixels[pixelId].beta.z)),
        //     0.05,
        //     0.95
        // );
        
        // if( p < rand( pixels[pixelId].state ) ) {
        //     rays[id].dead = 1u;
        //     scans[id] = 1u;
        //     return;
        // }

        // pixels[pixelId].beta.xyz /= p;

        rays[id].o = pos + vec4(nor * EPS, 0.f);
        rays[id].dir = vec4( bounceDir, 0.f );

    }
)";


inline const std::string bsdfSampleSrc = bounceHeader + R"(

    void main()
    {
        uint id = gl_GlobalInvocationID.x;
        
        if( id >= activeRays ) return;

        if( scans[id] == 1u ) {
            // pixels[id].L = vec4(0.f);
            return;
        }

        uint triIdx = uint(rays[id].triId);
        uint pixelId = uint( rays[id].pixelId );
        vec3 Le = vec3( 200.f );

        if( rays[id].triId == -1u ) {
            // pixels[pixelId].L += vec4(pixels[pixelId].beta.xyz * Le, 0.f); 
            rays[id].dead = 1u;
            return;
        }

        uint matIdx = uint(triangles[triIdx].matId);

        vec4 pos = rays[id].o + rays[id].t * rays[id].dir;

        vec3 color = materials[matIdx].diffuse.xyz / PI;
        vec3 beta = pixels[pixelId].beta.xyz;
        vec3 nor = normals[triIdx].xyz;
        vec3 lightPos = vec3( 0., 200., 0. );
        float r = 20.;

        if (dot(rays[id].dir.xyz, nor) > 0.0)
            nor = -nor;

        vec3 liray = rays[id].dir.xyz;
        float cosTheta = max(dot(nor, -liray), 0.);
        // if (cosTheta <= 0.) return;

        vec3 normal = shadowRays[id].normal.xyz;
        float dist2 = (rays[id].t ) * (rays[id].t);

        float cosThetaL = max(dot(normal, -liray), 0.);
        float pdf = cosTheta / PI;

        if( cosThetaL > 0.f && cosTheta > 0.f ) {        

        
            float pdf_area = 1. / (PI * r * r);
            float pdf_omega = pdf_area * dist2 / cosThetaL;
            vec3 Li = Le / pdf_omega;
            // float w = PowerHeuristic( 1., pdf, 1., pdf_omega );

            pixels[pixelId].L  += vec4( beta * color * Le * cosTheta, 0.f);   
        }


        pixels[pixelId].beta *= vec4(color * cosTheta / pdf, 0.f);
        pixels[pixelId].col = vec4(cosThetaL, 0., 0., 0.f);

        // rays[id].o = pos + vec4(nor * EPS, 0.f);
        // rays[id].dir = vec4( bounceDir, 0.f );
    }
)";

inline const std::string generatePrimaryRayHeader = raysLayout + structs + R"(
    
    layout(std430, binding = 0) buffer RayPosX
    {
        float oX[];
    };
    
    layout(std430, binding = 1) buffer RayPosY
    {
        float oY[];
    };
    
    layout(std430, binding = 2) buffer RayPosZ
    {
        float oZ[];
    };
    
    layout(std430, binding = 3) buffer RDirsX
    {
        float dX[];
    };
    layout(std430, binding = 4) buffer RDirsY
    {
        float dY[];
    };
    layout(std430, binding = 5) buffer RDirsZ
    {
        float dZ[];
    };
    
    layout(std430, binding = 6) buffer Radiance
    {
        vec4 L[];
    };

    layout(std430, binding = 7) buffer Throughput
    {
        vec4 beta[];
    };

    layout(std430, binding = 8) buffer States
    {
        uint states[];
    };

    layout(std140, binding = 0) uniform CameraData
    {
        Camera camera;
    };

    layout(std140, binding = 1) uniform Dimensions
    {
        uvec2 dims;
    };

    layout(std140, binding = 2) uniform Iter
    {
        uint iter;
    };
)";

inline const std::string generatePrimaryRaySrc = generatePrimaryRayHeader + R"(

    void main() {
    
        uint id = gl_GlobalInvocationID.x;
        
        if( id >= oX.length() ) return;

        uint WIDTH = dims.x;
        uint HEIGHT = dims.y;

        uint x = id % WIDTH;
        uint y = id / WIDTH;

        vec2 fragCoord = vec2(float(x), float(y)) + 0.5;

        vec2 st = fragCoord / vec2(float(WIDTH), float(HEIGHT)) - 0.5;

        st.x *= float(WIDTH) / float(HEIGHT);

        // third one in case of 0 iter., 0 id
        uint state = (id * 747796405u) ^ (iter * 2891336453u) ^  277803737u;
        
        states[id] = pcg(state);

        float r = rand(states[id]) * camera.apertureSize;
        float a = rand(states[id]) * (2.0 * PI);

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

        // pos[id] = vec4(rndAperturePointWrld, 0.0);
        // rayDirs[id] = vec4(rayDir, 0.0);

        oX[id] = rndAperturePointWrld.x;
        oY[id] = rndAperturePointWrld.y;
        oZ[id] = rndAperturePointWrld.z;

        dX[id] = rayDir.x;
        dY[id] = rayDir.y;
        dZ[id] = rayDir.z;

        // L[id]=vec4(0.);
        // beta[id]=vec4(1.);
        // rays[id].dead = 0;
        // rays[id].pixelId = id;
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

    layout(std430, binding = 5) buffer RayCount
    {
        uint raysActive;
    };
)";


inline const std::string encodeRaysSrc = raysLayout + structs + R"(

layout(std430, binding = 0) buffer RayOX
{
    float oX[];
};

layout(std430, binding = 1) buffer RayOY
{
    float oY[];
};

layout(std430, binding = 2) buffer RayOZ
{
    float oZ[];
};

layout(std430, binding = 3) buffer RayDX
{
    float dX[];
};

layout(std430, binding = 4) buffer RayDY
{
    float dY[];
};

layout(std430, binding = 5) buffer RayDZ
{
    float dZ[];
};

layout(std430, binding = 6) buffer Output
{
    uint outData[];
};

layout(std140, binding = 0) uniform Params
{
    AABB scene;
};

uint expandBits5(uint v)
{
    v &= 0x1Fu;

    v = (v | (v << 8)) & 0x0000F00Fu;
    v = (v | (v << 4)) & 0x000C30C3u;
    v = (v | (v << 2)) & 0x00249249u;

    return v;
}

uint morton3D_15(vec3 p)
{
    p = clamp(p, vec3(0.0), vec3(1.0));

    uvec3 q = uvec3(p * 31.0);

    uint xx = expandBits5(q.x);
    uint yy = expandBits5(q.y);
    uint zz = expandBits5(q.z);

    return (xx << 2) | (yy << 1) | zz;
}

uint morton6D_30(vec3 o, vec3 d)
{
    o = clamp(o, vec3(0.0), vec3(1.0));
    d = clamp(d, vec3(0.0), vec3(1.0));

    uvec3 qo = uvec3(o * 31.0);
    uvec3 qd = uvec3(d * 31.0);

    uint key = 0u;

    for (uint bit = 0u; bit < 5u; ++bit)
    {
        uint shift = bit * 6u;

        key |= ((qo.x >> bit) & 1u) << (shift + 5u);
        key |= ((qo.y >> bit) & 1u) << (shift + 4u);
        key |= ((qo.z >> bit) & 1u) << (shift + 3u);

        key |= ((qd.x >> bit) & 1u) << (shift + 2u);
        key |= ((qd.y >> bit) & 1u) << (shift + 1u);
        key |= ((qd.z >> bit) & 1u) << (shift + 0u);
    }

    return key;
}

void main()
{
    uint id = gl_GlobalInvocationID.x;

    if (id >= outData.length())
        return;

    vec3 o = vec3(
        oX[id],
        oY[id],
        oZ[id]
    );

    vec3 d = vec3(
        dX[id],
        dY[id],
        dZ[id]
    );

    vec3 sceneMin = scene.bmin.xyz;
    vec3 sceneMax = scene.bmax.xyz;

    o = (o - sceneMin) / (sceneMax - sceneMin);
    d = d * 0.5 + 0.5;

    uint orCode  = morton3D_15(o) >> 9;
    uint dirCode = morton3D_15(d) >> 9;

    // uint key = (orCode << 16) | dirCode;
    uint bucketId = (dirCode << 6) | orCode;

    outData[id] = morton6D_30(o, d) >> 17;
}

)";

inline const std::string generateShadowRaySrc = generateShadowRaysHeader + R"(

    void main()
    {
        uint id = gl_GlobalInvocationID.x;
        
        if( id >= raysActive ) return;

        if( rays[id].dead == 1 ) return;

        vec4 pos = rays[id].o + rays[id].t * rays[id].dir;
        uint pixelId = uint( rays[id].pixelId );

        // Triangle tri = triangles[rays[id].triId];
        // vec3 nor = normalize(cross(
        //     tri.v.xyz - tri.u.xyz,
        //     tri.w.xyz - tri.u.xyz
        // ));

        vec3 nor = normals[ rays[id].triId ].xyz;

        if( dot( rays[id].dir.xyz, nor ) > 0 ) nor = -nor;

        vec3 lightPos = vec3( 0., 200., 0. );
        float r = 20.;

        // uint state = id * 747796405u + 2891336453u;
        float zx = rand(pixels[pixelId].state);
        float zy = rand(pixels[pixelId].state);

        // float rndTheta = zx * 2. * PI;
        // float rndZ = zy * 2. - 1.;
        // float rndX = sqrt( 1. - rndZ * rndZ ) * cos( rndTheta );
        // float rndY = sqrt( 1. - rndZ * rndZ ) * sin( rndTheta );
        
        // vec3 rndPoint = lightPos + r * vec3( rndX, rndY, rndZ );
        // vec3 normal = normalize( rndPoint - lightPos );

        float rho = r * sqrt(zx);
        float phi = 2.0 * PI * zy;

        float rndX = rho * cos(phi);
        float rndZ = rho * sin(phi);

        vec3 rndPoint = lightPos + vec3(rndX, 0.0, rndZ);

        vec3 normal = vec3(0.0, -1.0, 0.0);

        // float d2 = dot(lightPos - pos.xyz, lightPos - pos.xyz);
        // float d = sqrt(d2);

        // vec3 wc = (lightPos - pos.xyz) / d;

        // float cosThetaMax = sqrt(
        //     max(0.0, 1.0 - r*r / d2)
        // );

        // float rho = 1.0 - zx * (1.0 - cosThetaMax);
        // float phi = 2.0 * PI * zy;

        // float rndX = sqrt(max(0.0, 1.0 - rho*rho)) * cos(phi);
        // float rndZ = sqrt(max(0.0, 1.0 - rho*rho)) * sin(phi);

        // vec3 up = abs(wc.y) < 0.999
        //     ? vec3(0.0, 1.0, 0.0)
        //     : vec3(1.0, 0.0, 0.0);

        // vec3 tangentX = normalize(cross(up, wc));
        // vec3 tangentY = cross(wc, tangentX);

        // vec3 liray =
        //     rndX * tangentX
        //     + rndZ * tangentY
        //     + rho  * wc;

        // vec3 oc = pos.xyz - lightPos;

        // float b = dot(oc, liray);
        // float c = dot(oc, oc) - r*r;

        // float t = -b - sqrt(max(0.0, b*b - c));

        // vec3 rndPoint = pos.xyz + t * liray;
        
        // vec3 normal = normalize(rndPoint - lightPos);

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


inline const std::string raySwapperSrc = R"(
#version 430

layout(local_size_x = 64) in;

layout(std430, binding = 0) buffer TriIn
{
    uint nums[];
};

layout(std430, binding = 1) buffer TriOut
{
    uint numsOut[];
};

layout(std430, binding = 2) buffer MIn
{
    uint mortonIn[];
};

layout(std430, binding = 3) buffer MOut
{
    uint mortonOut[];
};

layout(std430, binding = 4) buffer IdsIn
{
    uint offsets[];
};

void main() {
    uint id = gl_GlobalInvocationID.x;

    if (id >= nums.length())
        return;

    numsOut[ offsets[id] ] = nums[id];
    mortonOut[ offsets[id] ] = mortonIn[id];
}
)";

inline const std::string bucketCounts = R"(

#version 430

layout(local_size_x = 64) in;

layout(std430, binding = 0) readonly buffer BucketIds
{
    uint bucketIds[];
};

layout(std430, binding = 1) buffer BucketCounts
{
    uint counts[];
};

void main()
{
    uint id = gl_GlobalInvocationID.x;

    if (id >= bucketIds.length())
        return;

    atomicAdd(counts[bucketIds[id]], 1u);
}
)";

inline const std::string scatterCounts = R"(

#version 430

layout(local_size_x = 64) in;

layout(std430, binding = 0) buffer rayIds
{
    uint rayIds[];
};

layout(std430, binding = 1) buffer BucketCounts
{
    uint counts[];
};

void main()
{
    uint id = gl_GlobalInvocationID.x;

    if (id >= bucketIds.length())
        return;

    raysIds[ counts[id] ] = c;
}
)";

inline const std::string traverseRaysSrc = raysLayout + structs + R"(

layout(std430, binding = 0) buffer RayOX
{
    float oX[];
};

layout(std430, binding = 1) buffer RayOY
{
    float oY[];
};

layout(std430, binding = 2) buffer RayOZ
{
    float oZ[];
};

layout(std430, binding = 3) readonly buffer RayDX
{
    float dX[];
};

layout(std430, binding = 4) readonly buffer RayDY
{
    float dY[];
};

layout(std430, binding = 5) readonly buffer RayDZ
{
    float dZ[];
};

layout(std430, binding = 6) buffer Dead
{
    uint dead[];
};

layout(std430, binding = 7) buffer Hits
{
    float hits[];
};

layout(std430, binding = 8) buffer LBVH
{
    Node nodes[];
};

layout(std430, binding = 9) buffer Triangles
{
    Triangle triangles[];
};

layout(std430, binding = 10) buffer TriangleIds
{
    uint triIds[];
};

layout(std430, binding = 11) buffer TriIds
{
    int outTriId[];
};


TriangleHit intersectTriangle(
    vec3 o,
    vec3 dir,
    Triangle tri,
    float tmin,
    float tmax
) {
    vec3 e1 = tri.v.xyz - tri.u.xyz;
    vec3 e2 = tri.w.xyz - tri.u.xyz;

    vec3 h = cross(dir, e2);
    float a = dot(e1, h);

    if (abs(a) < 1e-8)
        return TriangleHit(false, tmin, 0.0, 0.0, 0.0);

    float f = 1.0 / a;

    vec3 s = o - tri.u.xyz;

    float u = f * dot(s, h);

    if (u < 0.0 || u > 1.0)
        return TriangleHit(false, tmin, u, 0.0, 0.0);

    vec3 q = cross(s, e1);

    float v = f * dot(dir, q);

    if (v < 0.0 || u + v > 1.0)
        return TriangleHit(false, tmin, u, v, 0.0);

    float t = f * dot(e2, q);

    if (t < tmin || t > tmax)
        return TriangleHit(false, tmin, u, v, 0.0);

    return TriangleHit(
        true,
        t,
        u,
        v,
        1.0 - u - v
    );
}


AABBHit intersectAABB(
    vec3 o,
    vec3 r,
    vec3 bmin,
    vec3 bmax
) {
    vec3 tLow  = (bmin - o) / r;
    vec3 tHigh = (bmax - o) / r;

    vec3 tCloseI = min(tLow, tHigh);
    vec3 tFarI   = max(tLow, tHigh);

    float tClose = max(
        max(tCloseI.x, tCloseI.y),
        tCloseI.z
    );

    float tFar = min(
        min(tFarI.x, tFarI.y),
        tFarI.z
    );

    return AABBHit(
        tClose <= tFar,
        tClose,
        tFar
    );
}


void main()
{
    uint id = gl_GlobalInvocationID.x;

    if (id >= oX.length())
        return;

    // if (dead[id] != 0u)
    //     return;

    // uint sortedId = sortedIds[id];

    vec3 o = vec3(
        oX[id],
        oY[id],
        oZ[id]
    );

    vec3 dir = vec3(
        dX[id],
        dY[id],
        dZ[id]
    );

    const int STACK_SIZE = 64;

    uint V[STACK_SIZE];
    int stackSize = 0;

    V[stackSize++] = 0u;

    float closestT = 1e30;
    int triId = -1;
    bool isDead = true;

    dead[id]=1u;

    uint N = triangles.length();

    while (stackSize > 0)
    {
        uint idx = V[--stackSize];
        Node node = nodes[idx];

        if (idx >= N - 1u)
        {
            uint trIdx = triIds[idx - N + 1u];

            if (triangles[trIdx].matId < 0)
                continue;

            TriangleHit hit = intersectTriangle(
                o,
                dir,
                triangles[trIdx],
                0.0,
                closestT
            );

            if (hit.hit && hit.t < closestT)
            {
                closestT = hit.t;
                triId = int(trIdx);
                
                if( isDead ) {
                    dead[id] = 0u;
                    isDead = false;
                }
            }
        }
        else
        {
            Node left  = nodes[node.left];
            Node right = nodes[node.right];

            AABBHit leftHit = intersectAABB(
                o,
                dir,
                left.aabb.bmin.xyz,
                left.aabb.bmax.xyz
            );

            AABBHit rightHit = intersectAABB(
                o,
                dir,
                right.aabb.bmin.xyz,
                right.aabb.bmax.xyz
            );

            bool hitLeft =
                leftHit.hit &&
                leftHit.tFar >= 0.0 &&
                leftHit.tClose <= closestT;

            bool hitRight =
                rightHit.hit &&
                rightHit.tFar >= 0.0 &&
                rightHit.tClose <= closestT;


            if (hitLeft && hitRight)
            {
                bool leftFirst =
                    leftHit.tClose <= rightHit.tClose;

                uint nearNode =
                    leftFirst ? uint(node.left) : uint(node.right);

                uint farNode =
                    leftFirst ? uint(node.right) : uint(node.left);

                // Far first because stack is LIFO.
                V[stackSize++] = farNode;
                V[stackSize++] = nearNode;
            }
            else if (hitLeft)
            {
                V[stackSize++] = uint(node.left);
            }
            else if (hitRight)
            {
                V[stackSize++] = uint(node.right);
            }
        }
    }

    vec3 newPos = o + closestT * dir;

    hits[id] = closestT;
    outTriId[id]=triId;

    oX[id] = newPos.x;
    oY[id] = newPos.y;
    oZ[id] = newPos.z;
}

)";

inline const std::string scatterBucketsSrc = R"(
#version 430

layout(local_size_x = 64) in;

layout(std430, binding = 0) readonly buffer BucketIds
{
    uint bucketIds[];
};

layout(std430, binding = 1) readonly buffer BucketOffsets
{
    uint bucketOffsets[];
};

layout(std430, binding = 2) buffer BucketCursor
{
    uint bucketCursor[];
};

layout(std430, binding = 3) buffer SortedRayIds
{
    uint sortedRayIds[];
};

void main()
{
    uint id = gl_GlobalInvocationID.x;

    if (id >= bucketIds.length())
        return;

    uint bucket = bucketIds[id];

    uint localIndex =
        atomicAdd(bucketCursor[bucket], 1u);

    uint dst =
        bucketOffsets[bucket] + localIndex;

    sortedRayIds[dst] = id;
}
)";

inline const std::string genDirections = raysLayout + structs + R"(

layout(std430, binding = 0) buffer RayDX
{
    float dX[];
};

layout(std430, binding = 1) buffer RayDY
{
    float dY[];
};

layout(std430, binding = 2) buffer RayDZ
{
    float dZ[];
};

layout(std430, binding = 3) buffer States
{
    uint states[];
};

layout(std430, binding = 4) buffer TriIds
{
    int triIds[];
};

layout(std430, binding = 5) buffer Normals
{
    vec4 normals[];
};

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
    
    return (worldDir);
}

void main() {
    
    uint id = gl_GlobalInvocationID.x;

    float rx = rand( states[id] );
    float ry = rand( states[id] );

    vec4 normal = normals[ triIds[id] ];

    vec3 newDir = RandomUnitVectorInHemisphereOf( normal.xyz, vec2(rx, ry) );

    dX[ id ] = newDir.x;
    dY[ id ] = newDir.y;
    dZ[ id ] = newDir.z;
}
)";

struct TriangleGPU
{
    glm::vec4 u;
    glm::vec4 v;
    glm::vec4 w;
};

struct RayDir {

    glm::vec4 o; 
    glm::vec4 dir; 
};

class Integrator {

    private:
    glm::uvec2 dimensions;
    
    // SSBOs
    Buffer rays, states, L, beta, color, pos, dir, dead, hits, triIds;
    Buffer inpA, inpB, inpC, inpD;

    Buffer count;

    Buffer oX, oY, oZ;
    Buffer dX, dY, dZ;

    // Buffer lbvh;

    // UBOs
    Buffer camera, dims, iteration, scene;
    uint32_t width, height;

    Program cameraRays, 
    encodeRays, raySwapper, 
    traverse_,
    countBuckets, scatterBuckets, sampleDirection;

    Radix sort;
    BlellochScan scan;

    std::vector<glm::vec4> zeros, ones;

    public:
    Integrator( Camera& camera, AABB &aabb, uint32_t w, uint32_t h ) : 
    // camera(camera), 
    dimensions(w, h),
    sort( w * h, 64 ),
    scan( 8192, 64 ),
    encodeRays( encodeRaysSrc ),
    raySwapper( raySwapperSrc ),
    traverse_( traverseRaysSrc ),
    countBuckets( bucketCounts ),
    scatterBuckets( scatterBucketsSrc ),
    sampleDirection(genDirections),
    // lbvh( lbvh ),
    rays(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(RayDir),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
  
  
    oX(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(RayDir),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    oY(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(RayDir),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    oZ(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(RayDir),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    dX(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(RayDir),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    dY(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(RayDir),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    dZ(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(RayDir),
        nullptr,
        GL_DYNAMIC_COPY   
    ),

  
    dead(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY
    ),

    count(
        GL_SHADER_STORAGE_BUFFER,
        8192 * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY
    ),

    hits(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(float),
        nullptr,
        GL_DYNAMIC_COPY
    ),
    triIds(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(int),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    inpA(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    inpB(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    inpC(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    inpD(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    pos(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(glm::vec4),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    dir(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(glm::vec4),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    states(
        GL_SHADER_STORAGE_BUFFER,
        w * h * sizeof(glm::vec4),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    L(
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
    ),
    scene(
        GL_UNIFORM_BUFFER,
        sizeof(AABB),
        &aabb,
        GL_DYNAMIC_COPY   
    ),
    width( w ),
    height( h ),
    
    cameraRays( generatePrimaryRaySrc ) {

        const size_t N = size_t(w) * h;

        std::vector<glm::vec4> zeros(N, glm::vec4(0.0f));
        std::vector<glm::vec4> ones (N, glm::vec4(1.0f));

        std::vector<uint32_t> triIds(width * height);
        std::iota(triIds.begin(), triIds.end(), 0u);

        Buffer inpA__(
            GL_SHADER_STORAGE_BUFFER,
            w * h * sizeof(uint32_t),
            triIds.data(),
            GL_DYNAMIC_COPY   
        );

        inpC = std::move( inpA__ );
    }

    void generateCameraRays();
    void radixSortPosAndDir();
    void traverse( Buffer &lbvh, Buffer &triangles, Buffer &triIds );
    void sampleNewDir( Buffer &normals );
};