#pragma once
#include <utils.hpp>
#include <glm/glm.hpp>
#include <triangle.hpp>
#include <radix.hpp>
#include <morton.hpp>
#include <vector>

struct Node {
    AABB aabb;
    int parent=-1;
    int left=-1;
    int right=-1;
    int isLeaf;
    int visited=0;
};

GLuint createLBVHShader();
GLuint createAABBShader();
GLuint createTraversalShader();

const std::string layout = R"(#version 430

    layout(local_size_x = 64) in;)";

inline const std::string lbvhHeader = layout + structs + R"(

    layout(std430, binding = 0) buffer Morton
    {
        uvec2 morton[];
    };
    
    layout(std430, binding = 1) buffer TriIn
    {
        Triangle triangles[];
    };

    layout(std430, binding = 4) buffer LBVH
    {
        Node nodes[];
    };

    layout(std430, binding = 5) buffer Rays
    {
        Ray rays[];
    };

    layout(std430, binding = 6) buffer Mats
    {
        Material materials[];
    };

    layout(std430, binding = 7) buffer Pixels
    {
        Pixel pixels[];
    };

    layout(std430, binding = 8) buffer ShadowRays
    {
        ShadowRay shadowRays[];
    };
)";

inline const std::string lbvhSrc = layout + structs + R"(
    
    layout(std430, binding = 0) buffer Morton
    {
        uint morton[];
    };

    layout(std430, binding = 1) buffer Nodes
    {
        Node nodes[];
    };

    layout(std140, binding = 0) uniform Sizes
    {
        uint N;
    };

    int clz(uint x)
    {
        return x == 0u ? 32 : 31 - findMSB(x);
    }

    int delta(uint i, int j)
    {
        if (j < 0 || j >= int(N))
            return -1;

        uint a = morton[i];
        uint b = morton[uint(j)];

        if (a == b)
        {
            return 32 + clz(i ^ uint(j));
        }

        return clz(a ^ b);
    }

    int findSplit( int first, int last)
    {
        uint firstCode = morton[first];
        uint lastCode = morton[last];

        // if (firstCode == lastCode)
        //     return (first + last) >> 1;

        // int commonPrefix = clz(firstCode ^ lastCode);
        int commonPrefix = delta( first, last );
        
        int split = first;
        int step = last - first;

        do
        {
            step = (step + 1) >> 1;
            int newSplit = split + step;

            if (newSplit < last)
            {
                // uint splitCode = morton[newSplit];
                int splitPrefix = delta(first, newSplit);
                if (splitPrefix > commonPrefix)
                    split = newSplit; // accept proposal
            }
        }
        while (step > 1);

        return split;
    }
    

    void main()
    {
        uint id = gl_GlobalInvocationID.x;

        if (id >= N - 1)
            return;
        
        int L = delta( id, int(id)-1 );
        int R = delta( id, int(id)+1 );

        int d = (R > L) ? 1 : -1;
        int deltaMin = delta(id, int(id) - d);

        int j = int(id) + d;
        while( j >= 0 && j < int(N) && delta(id, j) > deltaMin) {
            j+=d;
        }

        int first = min( int(id), j-d );
        int last = max( int(id), j-d );

        int gamma = findSplit( first, last );
        int left;
        if (gamma == first) {
            
            left = int(N - 1) + gamma;
        }  
        else
            left = gamma;
    
        int right;
        if (gamma + 1 == last) {  
            right = int(N - 1) + gamma + 1;
        }
        else
            right = gamma + 1;

        nodes[id].left = left;
        nodes[id].right = right;

        nodes[left].parent = int(id);
        nodes[right].parent = int(id);
    }
    )";

inline const std::string aabbSrc = layout + structs + R"(

    layout(std430, binding = 0) buffer Nodes
    {
        Node nodes[];
    };
    
    layout(std430, binding = 1) buffer Triangles
    {
        Triangle triangles[];
    };
    
    layout(std430, binding = 2) buffer TriIds
    {
        uint triIds[];
    };

    layout(std140, binding = 0) uniform Sizes
    {
        uint N;
    };

    void main()
    {
        uint id = gl_GlobalInvocationID.x;

        if (id >= N)
            return;

        uint triIdx = triIds[id];
        uint leafIdx = id + N - 1;

        nodes[leafIdx].aabb = triangles[triIdx].aabb;

        Node node = nodes[leafIdx];

        while( node.parent != -1 ) {
            Node parent = nodes[node.parent];
            
            if (atomicAdd(nodes[node.parent].visited, 1) == 0) {
                break;
            }

            Node left = nodes[parent.left];
            Node right = nodes[parent.right];

            parent.aabb.bmin.xyz = min(left.aabb.bmin.xyz, right.aabb.bmin.xyz);
            parent.aabb.bmax.xyz = max(left.aabb.bmax.xyz, right.aabb.bmax.xyz);

            nodes[node.parent].aabb=parent.aabb;

            node = parent;
        }
    }
    )";


inline const std::string traverseRayHeader = layout + structs + R"(
    
    layout(std430, binding = 0) buffer TriIn
    {
        Triangle triangles[];
    };

    layout(std430, binding = 1) buffer TriIds
    {
        uint triIds[];
    };

    layout(std430, binding = 2) buffer LBVH
    {
        Node nodes[];
    };

    layout(std430, binding = 3) buffer Rays
    {
        Ray rays[];
    };

    layout(std430, binding = 4) buffer Scans
    {
        uint scan[];
    };

    layout(std430, binding = 5) buffer RayCount
    {
        uint activeRays;
    };

    layout(std140, binding = 0) uniform Sizes
    {
        uint N;
    };
)";

inline const std::string traverseSrc = traverseRayHeader + R"(


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

        if (abs(a) < 1e-8) {
            return TriangleHit(false, tmin, 0.0, 0.0, 0.0);
        }

        float f = 1.0 / a;

        vec3 s = o - tri.u.xyz;

        float u = f * dot(s, h);

        if (u < 0.0 || u > 1.0) {
            return TriangleHit(false, tmin, u, 0.0, 0.0);
        }

        vec3 q = cross(s, e1);

        float v = f * dot(dir, q);

        if (v < 0.0 || u + v > 1.0) {
            return TriangleHit(false, tmin, u, v, 0.0);
        }

        float t = f * dot(e2, q);

        if (t < tmin || t > tmax) {
            return TriangleHit(false, tmin, u, v, 0.0);
        }

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

        if (id >= activeRays)
            return;

        Ray ray = rays[id];

        if( ray.dead == 1u ) {
            return;
        }

        const int STACK_SIZE = 64;

        uint V[STACK_SIZE];
        int size = 0;
        
        V[size++]=0;

        float closestT = 1e30f;
        int matId = -1;
        int triId = -1;

        uint dead = 1u;

        uint visits = 0u;
        while( size > 0 ) {
            
            uint idx = V[--size];
            Node node = nodes[ idx ];

            // AABBHit nodeHit = intersectAABB( ray.o.xyz, ray.dir.xyz, node.aabb.bmin.xyz, node.aabb.bmax.xyz );

            // if( !nodeHit.hit || nodeHit.tFar < ray.tmin || nodeHit.tClose > closestT ) {
            //     continue;
            // }

            if( idx >= N - 1 ) {

                // uint trIdx = idx - N + 1;
                uint trIdx = triIds[idx - N + 1];

                if( triangles[trIdx].matId < 0 )
                    continue;
                
                TriangleHit hit = intersectTriangle(
                    ray.o.xyz,
                    ray.dir.xyz,
                    triangles[ trIdx ],
                    0.,
                    closestT
                );

                if( hit.hit && hit.t < closestT ) {
                    dead = 0u;
                    closestT = hit.t;
                    // hitTri = top.tr;
                    triId = int(trIdx);
                    matId = triangles[trIdx].matId;
                    // hitPoint = ray.o + hit.t * ray.dir;
                }
            } else {
            
                Node left = nodes[ node.left ];
                Node right = nodes[ node.right ];

                AABBHit leftHit = intersectAABB( ray.o.xyz, ray.dir.xyz, left.aabb.bmin.xyz, left.aabb.bmax.xyz );
                AABBHit rightHit = intersectAABB( ray.o.xyz, ray.dir.xyz, right.aabb.bmin.xyz, right.aabb.bmax.xyz );

                bool hitLeft =
                    leftHit.hit &&
                    leftHit.tFar >= ray.tmin &&
                    leftHit.tClose <= closestT;

                bool hitRight =
                    rightHit.hit &&
                    rightHit.tFar >= ray.tmin &&
                    rightHit.tClose <= closestT;

                if( hitLeft && hitRight ) {
                    uint frstNode = leftHit.tClose > rightHit.tClose ? node.right : node.left;
                    uint scndNode = leftHit.tClose > rightHit.tClose ? node.left : node.right;
                
                    V[size++] = scndNode;
                    V[size++] = frstNode;
                } else if( hitLeft ) {
                 
                    V[size++]=node.left;
                    } else if( hitRight ) {
                     
                        V[size++]=node.right; 
                    }
            }
        }

        // rays[ id ].hit = ray.o + closestT * ray.dir;
        // if( closestT >= 1e10f ) {
        
        //     // hits[id].hit = vec4( -1. );
        //     // hits[id].t = -1.;
        //     // hits[id].color = vec4(0.);
        //     // rays[id].t = -1;
        //     // rays[id].matId = -1;
        //     // rays[id].triId = -1;
        //     rays[id].dead = 1;
        //     scan[id]=1;
            
        // } else {
        //     // hits[id].hit = ray.o + closestT * ray.dir;
        //     // hits[id].t = closestT;
        //     // hits[id].color = materials[ matId ].diffuse;

        //     }

        rays[id].t = dead == 1u ? -1 : closestT;
        // rays[id].matId = matId;
        rays[id].triId = triId;
        rays[id].dead = dead;
        scan[id]=dead;
    }
    )";

inline const std::string traverseShadowRayHeader = layout + structs + R"(
    
    layout(std430, binding = 0) buffer TriIn
    {
        Triangle triangles[];
    };

    layout(std430, binding = 1) buffer TriIds
    {
        uint triIds[];
    };

    layout(std430, binding = 2) buffer LBVH
    {
        Node nodes[];
    };

    layout(std430, binding = 3) buffer ShadowRays
    {
        ShadowRay shadowRays[];
    };

    layout(std430, binding = 4) buffer RayCount
    {
        uint activeRays;
    };

    layout(std430, binding = 5) buffer RaysActive
    {
        Ray rays[];
    };

    layout(std140, binding = 0) uniform Sizes
    {
        uint N;
    };
)";

inline const std::string traverseShadowRaySrc = traverseShadowRayHeader + R"(

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

        if (id >= activeRays)
            return;

        if( rays[id].dead == 1u ) {
            // shadowRays[id].occluded = 1u;
            return;
        }

        ShadowRay ray = shadowRays[id];
        shadowRays[id].occluded = 0u;

        const int STACK_SIZE = 64;

        uint V[STACK_SIZE];
        int size = 0;
        
        V[size++]=0;

        while( size > 0 ) {
            uint idx = V[--size];
            Node node = nodes[ idx ];

            // AABBHit nodeHit = intersectAABB( ray.o.xyz, ray.dir.xyz, node.aabb.bmin.xyz, node.aabb.bmax.xyz );

            // if( !nodeHit.hit || nodeHit.tFar < ray.tmin || nodeHit.tClose > ray.tmax ) {
            //     continue;
            // }

            if( idx >= N - 1 ) {

                uint trIdx = triIds[idx - N + 1];

                if( triangles[trIdx].matId < 0 )
                    continue;
                
                bool hit = intersectTriangleShadow(
                    ray.o.xyz,
                    ray.dir.xyz,
                    triangles[ trIdx ],
                    ray.tmin,
                    ray.tmax
                );

                if( hit ) {
                    shadowRays[id].occluded = 1u;
                    return;
                }
            } else {
            
                Node left = nodes[ node.left ];
                Node right = nodes[ node.right ];

                AABBHit leftHit = intersectAABB( ray.o.xyz, ray.dir.xyz, left.aabb.bmin.xyz, left.aabb.bmax.xyz );
                AABBHit rightHit = intersectAABB( ray.o.xyz, ray.dir.xyz, right.aabb.bmin.xyz, right.aabb.bmax.xyz );

                bool hitLeft =
                    leftHit.hit &&
                    leftHit.tFar >= ray.tmin &&
                    leftHit.tClose <= ray.tmax;

                bool hitRight =
                    rightHit.hit &&
                    rightHit.tFar >= ray.tmin &&
                    rightHit.tClose <= ray.tmax;

                if( hitLeft && hitRight ) {
                    uint frstNode = leftHit.tClose > rightHit.tClose ? node.right : node.left;
                    uint scndNode = leftHit.tClose > rightHit.tClose ? node.left : node.right;
                
                    V[size++] = scndNode;
                    V[size++] = frstNode;
                } else if( hitLeft ) {
                    V[size++]=node.left;
                    } else if( hitRight ) {
                        V[size++]=node.right; 
                    }
            }
        }
    }
    )";

inline const std::string trSwapperSrc = R"(
#version 430

layout(local_size_x = 64) in;)" +
structs + 
R"(

layout(std430, binding = 0) buffer TriIn
{
    uint triIn[];
};

layout(std430, binding = 1) buffer TriOut
{
    uint triOut[];
};

layout(std430, binding = 2) buffer MortonIn
{
    uint mortonIn[];
};

layout(std430, binding = 3) buffer MortonOut
{
    uint mortonOut[];
};

layout(std430, binding = 4) buffer IdsIn
{
    uint offsets[];
};

void main() {
    uint id = gl_GlobalInvocationID.x;

    if (id >= offsets.length())
        return;

    mortonOut[ offsets[id] ] = mortonIn[id];
    triOut[ offsets[id] ] = triIn[id];
}
)";

class LBVH {

    private: 
    Radix sort;

    Buffer mortonSSBO, mortonBSSBO, trianglesSSBO, 
    sceneUBO, triangleSizeUBO,
    bvhSSBO, triIdBSSBO;

    Program lbvh, aabbs, trSwapper, morton;
    
    size_t size;
    std::vector<Node> nodes;

    public:
    Buffer triIdASSBO;
    LBVH( AABB& aabb, uint32_t THREADS, size_t size ) :  
    size( size ),
    nodes( size * 2  - 1 ),

    triIdBSSBO(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY
    ),
    mortonSSBO(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY
    ),
    mortonBSSBO(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY
    ),
    sceneUBO(
        GL_UNIFORM_BUFFER,
        sizeof(AABB),
        &aabb,
        GL_DYNAMIC_COPY
    ),
    
    triangleSizeUBO(
        GL_UNIFORM_BUFFER,
        sizeof( uint32_t ),
        &size,
        GL_DYNAMIC_COPY
    ),

    lbvh( lbvhSrc ),
    aabbs( aabbSrc ),
    trSwapper( trSwapperSrc ),
    morton( mortonSrc ),
    sort( size, THREADS )
    
    { 

        Buffer bSSBO(
            GL_SHADER_STORAGE_BUFFER,
            nodes.size() * sizeof(Node),
            nodes.data(),
            GL_DYNAMIC_COPY
        );

        bvhSSBO = std::move( bSSBO );

        std::vector<uint32_t> ids(size);

        for (uint32_t i = 0; i < size; ++i)
            ids[i] = i;


        Buffer triIdASSBO__(
            GL_SHADER_STORAGE_BUFFER,
            size * sizeof(uint32_t),
            ids.data(),
            GL_DYNAMIC_COPY
        );
        triIdASSBO = std::move( triIdASSBO__ );

     }

    Buffer& operator() ( Buffer &trianglesSSBO  );
};