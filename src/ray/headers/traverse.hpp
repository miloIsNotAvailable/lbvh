#pragma once

#include <utils.hpp>
#include <primaryRays.hpp>

inline const std::string traverseRaysSrc__ = R"(
#version 430

layout(local_size_x = 64) in;

)" + structs + R"(

layout(std430, binding = 0) buffer RayDirPos
{
    float rays[];
};

layout(std430, binding = 1) buffer OutT
{
    float tOut[];
};


layout(std430, binding = 2) buffer Dead
{
    uint dead[];
};

layout(std430, binding = 3) buffer LBVH
{
    Node nodes[];
};

layout(std430, binding = 4) buffer Triangles
{
    Triangle triangles[];
};

layout(std430, binding = 5) buffer TriangleIds
{
    uint triIds[];
};

layout(std430, binding = 6) buffer TriIds
{
    int outTriId[];
};

layout(std430, binding = 7) buffer Lights
{
    Light light;
};
    
layout(std430, binding = 8) buffer EmissiveHits
{
    uint hitEmissive[];
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

bool intersectDisk(
    vec3 o,
    vec3 dir,
    vec3 center,
    vec3 normal,
    float radius,
    out float t
)
{
    float denom = dot(dir, normal);

    if (abs(denom) < 1e-8)
        return false;

    t = dot(center - o, normal) / denom;

    if (t <= 0.0)
        return false;

    vec3 p = o + t * dir;
    vec3 q = p - center;

    return dot(q, q) <= radius * radius;
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

    uint M = rays.length() / 6;

    if (id >= M) return;

    if (dead[id] == 1u) {
        hitEmissive[id] = 0u;
        // outTriId[id] = -1;
        return;
    };

    // uint sortedId = sortedIds[id];

    vec3 o = vec3(
        rays[id + M * 0],
        rays[id + M * 1],
        rays[id + M * 2]
    );

    vec3 dir = vec3(
        rays[id + M * 3],
        rays[id + M * 4],
        rays[id + M * 5]
    );

    const int STACK_SIZE = 64;

    uint V[STACK_SIZE];
    int stackSize = 0;

    V[stackSize++] = 0u;

    float closestT = 1e30;
    int triId = -1;
    bool isDead = true;

    uint emissiveHit = 0;
    float lightT;

    dead[id]=1u;

    if (intersectDisk(
        o,
        dir,
        light.pos.xyz,
        light.normal.xyz,
        light.pos.w,
        lightT
    )) {
        closestT = lightT;
        emissiveHit = 1;
        dead[id]=0u;
    }

    uint N = triangles.length();

    while (stackSize > 0)
    {
        uint idx = V[--stackSize];
        Node node = nodes[idx];

        if (idx >= N - 1u)
        {
            uint trIdx = triIds[idx - N + 1u];

            // if (triangles[trIdx].matId < 0)
            //     continue;

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
                
                emissiveHit = 0;

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

    // hits[id] = closestT;
    outTriId[id]=triId;
    hitEmissive[id] = emissiveHit;

    tOut[id] = closestT;
}
)";

class Traverse {
    
    public:

    Buffer dead, hitEmissive, triIds;

    Buffer t;

    Program traverse;

    Traverse( uint32_t size ):
    traverse( traverseRaysSrc__ ),
    dead(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    hitEmissive(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    triIds(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    t(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(float),
        nullptr,
        GL_DYNAMIC_COPY   
    )
    {}

    Buffer& operator()( uint32_t size, Buffer &rays, Buffer &lbvh, Buffer &triangles, Buffer &tIds, Buffer &light );
};