#pragma once
#include <utils.hpp>
#include <glm/glm.hpp>
#include <triangle.hpp>

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

    int findSplit( int first, int last)
    {
        uint firstCode = morton[first];
        uint lastCode = morton[last];

        if (firstCode == lastCode)
            return (first + last) >> 1;

        int commonPrefix = clz(firstCode ^ lastCode);
        
        int split = first;
        int step = last - first;

        do
        {
            step = (step + 1) >> 1;
            int newSplit = split + step;

            if (newSplit < last)
            {
                uint splitCode = morton[newSplit];
                int splitPrefix = clz(firstCode ^ splitCode);
                if (splitPrefix > commonPrefix)
                    split = newSplit; // accept proposal
            }
        }
        while (step > 1);

        return split;
    }
    
    int delta(uint i, int j)
    {
        if (j < 0 || j >= int(N))
            return -1;

        return clz(morton[i] ^ morton[uint(j)]);
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
        vec3 hitPoint;
        int matId;
        int triId;

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
        if( closestT >= 1e10f ) {
        
            // hits[id].hit = vec4( -1. );
            // hits[id].t = -1.;
            // hits[id].color = vec4(0.);
            rays[id].t = -1;
            // rays[id].matId = -1;
            rays[id].triId = -1;
            rays[id].dead = 1;
            scan[id]=1;
            
        } else {
            // hits[id].hit = ray.o + closestT * ray.dir;
            // hits[id].t = closestT;
            // hits[id].color = materials[ matId ].diffuse;

            rays[id].t = closestT;
            // rays[id].matId = matId;
            rays[id].triId = triId;
            rays[id].dead = 0;
            scan[id]=0;
        }
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