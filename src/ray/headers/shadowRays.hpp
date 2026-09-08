#pragma once
#include <utils.hpp>
#include <primaryRays.hpp>
#include <randoms.hpp>

const inline std::string generateShadowSrc = R"(
    #version 430
    layout(local_size_x = 64) in;
)" 
+ structs + R"(
    layout(std430, binding = 0) buffer RayDirPos
    {
        float rays[];
    };

    layout(std430, binding = 1) buffer OutRayDirPos
    {
        float outRays[];
    };
    
    layout(std430, binding = 2) buffer RayT
    {
        float hitT[];
    };
    
    layout(std430, binding = 3) buffer Lights
    {
        Light light;
    };

    layout(std430, binding = 4) buffer Dead
    {
        uint dead[];
    };

    layout(std430, binding = 5) buffer TriIds
    {
        uint triIds[];
    };

    layout(std430, binding = 6) buffer Normals
    {
        vec4 normals[];
    };

    layout(std430, binding = 7) buffer Tmax
    {
        float tmax[];
    };

    layout(std430, binding = 8) buffer RandX
    {
        float zx[];
    };
    layout(std430, binding = 9) buffer RandY
    {
        float zy[];
    };
    layout(std430, binding = 10) buffer HitEm
    {
        uint hitEmissive[];
    };

    void main()
    {
        uint id = gl_GlobalInvocationID.x;
        
        uint M = rays.length() / 6;

        if( id >= M ) return;

        if( dead[id] == 1 ) return;
        if( hitEmissive[id] == 1 ) return;

        vec3 o = vec3(
            rays[id + M * 0],
            rays[id + M * 1],
            rays[id + M * 2]
        );
        vec3 d = vec3(
            rays[id + M * 3],
            rays[id + M * 4],
            rays[id + M * 5]
        );

        vec3 pos = o + hitT[id] * d;

        vec3 nor = normals[ triIds[id] ].xyz;

        if( dot( d, nor ) > 0 ) nor = -nor;

        vec3 lightPos = light.pos.xyz;
        float r = light.pos.w;

        float rho = r * sqrt(zx[id]);
        float phi = 2.0 * PI * zy[id];

        float rndX = rho * cos(phi);
        float rndY = rho * sin(phi);

        vec3 N = light.normal.xyz;

        vec3 helper = abs(N.y) < 0.999
            ? vec3(0.0, 1.0, 0.0)
            : vec3(1.0, 0.0, 0.0);

        vec3 T = normalize(cross(helper, N));
        vec3 B = cross(N, T);

        vec3 rndPoint = lightPos + T * rndX + B * rndY;

        vec3 lDir = rndPoint - pos.xyz;
        vec3 liray = normalize( lDir );
        float dist2 = dot( lDir, lDir );
        
        outRays[id + M * 0] = pos.x + nor.x * EPS;
        outRays[id + M * 1] = pos.y + nor.y * EPS;
        outRays[id + M * 2] = pos.z + nor.z * EPS;

        outRays[id + M * 3] = liray.x;
        outRays[id + M * 4] = liray.y;
        outRays[id + M * 5] = liray.z;

        tmax[id] = dist2;
    }
)";

inline const std::string traverseShadowRaysSrc__ = R"(
#version 430

layout(local_size_x = 64) in;

)" + structs + R"(

layout(std430, binding = 0) buffer RayPosDir
{
    float rays[];
};

layout(std430, binding = 1) buffer Occ
{
    uint occluded[];
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

layout(std430, binding = 6) buffer Tmax
{
    float tmax[];
};

layout(std430, binding = 7) buffer Lights
{
    Light light;
};

layout(std430, binding = 8) buffer Normals
{
    vec4 normals[];
};

layout(std430, binding = 9) buffer HitEm
{
    uint hitEmissive[];
};


bool intersectTriangleShadow(
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
        return false;

    float f = 1.0 / a;

    vec3 s = o - tri.u.xyz;

    float u = f * dot(s, h);

    if (u < 0.0 || u > 1.0)
        return false;

    vec3 q = cross(s, e1);

    float v = f * dot(dir, q);

    if (v < 0.0 || u + v > 1.0)
        return false;

    float t = f * dot(e2, q);

    return t >= tmin && t <= tmax;
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

    occluded[id] = 0;

    if (dead[id] != 0u) return;
    if (hitEmissive[id] != 0u) return;

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

    float closestT = 1e30f;
    int triId = -1;
    bool isDead = true;

    uint occ = 0;

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

            bool hit = intersectTriangleShadow(
                o,
                dir,
                triangles[trIdx],
                EPS,
                sqrt(tmax[id]) - EPS
            );

            if (hit)
            {
                occ = 1;
                break;
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
                leftHit.tFar >= EPS &&
                leftHit.tClose <= sqrt(tmax[id]) - EPS;

            bool hitRight =
                rightHit.hit &&
                rightHit.tFar >= EPS &&
                rightHit.tClose <= sqrt(tmax[id]) - EPS;


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

    occluded[id] = occ;
}
)";

class ShadowRays {

    private: 
    RaysView shadowRays;
    Vec3 dir;
    Buffer outRays, occluded, nor;

    Program genShadowRays, traverseShadowRays;

    public:
    Buffer tmax;
    ShadowRays( uint32_t size ) :
    genShadowRays( generateShadowSrc ),
    traverseShadowRays( traverseShadowRaysSrc__ ),
    dir(size),
    tmax(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(float),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    nor(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(glm::vec4),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    outRays(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(float) * 6,
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    occluded(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    )
    {}

    Buffer& generate( uint32_t size, 
        Buffer& rays, 
        Buffer& hitT, 
        Buffer &triIds, 
        Buffer &normals, 
        Buffer &dead,
        Buffer &light,
        Random2D random2d,
        Buffer &hitEmissive
    );

    Buffer& traverse( uint32_t size, 
        Buffer &lbvh, 
        Buffer &triangles, 
        Buffer &dead, 
        Buffer &tIds, 
        Buffer &light,
        Buffer &normals,
        Buffer &hitEmissive );
};