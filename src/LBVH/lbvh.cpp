#include <lbvh.hpp>

inline constexpr const char * header= R"(#version 430

    layout(local_size_x = 64) in;

    struct AABB {
        vec4 bmin; 
        vec4 bmax;
    };

    struct Triangle {
        vec4 u, v, w;
        uvec4 quantized;
        vec4 c;
        AABB aabb;
        int matId;
    };
    
    struct Node {
        AABB aabb;
        int parent;
        int left;
        int right;
        int isLeaf;
        int visited;
    };

    struct Ray {
        vec4 o;
        vec4 dir;
        vec4 hit;
        float tmin;
        float tmax;
    };

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
)";

GLuint createLBVHShader() {
    std::string source = std::string(header) + R"(
    
    int clz(uint x)
    {
        return x == 0u ? 32 : 31 - findMSB(x);
    }

    int findSplit( int first, int last)
    {
        uint firstCode = morton[first].x;
        uint lastCode = morton[last].x;

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
                uint splitCode = morton[newSplit].x;
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
        if (j < 0 || j >= morton.length())
            return -1;

        return clz(morton[i].x ^ morton[uint(j)].x);
    }

    void main()
    {
        uint id = gl_GlobalInvocationID.x;

        if (id >= triangles.length() - 1)
            return;
        
        int L = delta( id, int(id)-1 );
        int R = delta( id, int(id)+1 );

        int d = (R > L) ? 1 : -1;
        int deltaMin = delta(id, int(id) - d);

        int j = int(id) + d;
        while( j >= 0 && j < morton.length() && delta(id, j) > deltaMin) {
            j+=d;
        }

        int first = min( int(id), j-d );
        int last = max( int(id), j-d );

        int gamma = findSplit( first, last );
        int left;
        if (gamma == first) {
            
            left = (morton.length() - 1) + gamma;
        }  
        else
            left = gamma;
    
        int right;
        if (gamma + 1 == last) {  
            right = (morton.length() - 1) + gamma + 1;
        }
        else
            right = gamma + 1;

        nodes[id].left = left;
        nodes[id].right = right;

        nodes[left].parent = int(id);
        nodes[right].parent = int(id);
    }
    )";

    GLuint shader = compileShader( source );
    GLuint computeProgram = linkProgram( shader );
    return computeProgram;
}

GLuint createAABBShader() {
    std::string source = std::string(header) + R"(
    
    void main()
    {
        uint id = gl_GlobalInvocationID.x;

        if (id >= triangles.length())
            return;

        uint triIdx = id;
        uint leafIdx = id + triangles.length() - 1;

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

    GLuint shader = compileShader( source );
    GLuint computeProgram = linkProgram( shader );
    return computeProgram;
}

GLuint createTraversalShader() {
    std::string source = std::string(header) + R"(
    struct TriangleHit {
        bool hit;
        float t;
        float u;
        float v;
        float w;
    };

    struct AABBHit {
        bool hit;
        float tClose;
        float tFar;
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

        if (id >= rays.length())
            return;

        Ray ray = rays[id];

        const int STACK_SIZE = 64;

        uint V[STACK_SIZE];
        int size = 0;
        
        V[size++]=0;

        float closestT = ray.tmax;
        vec3 hitPoint;

        while( size > 0 ) {
            uint idx = V[--size];
            Node node = nodes[ idx ];

            AABBHit nodeHit = intersectAABB( ray.o.xyz, ray.dir.xyz, node.aabb.bmin.xyz, node.aabb.bmax.xyz );

            if( !nodeHit.hit || nodeHit.tFar < ray.tmin || nodeHit.tClose > closestT ) {
                continue;
            }

            if( idx >= triangles.length() - 1 ) {

                uint trIdx = idx - triangles.length() + 1;

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
                    // hitPoint = ray.o + hit.t * ray.dir;
                }
            } else {
            
                Node left = nodes[ node.left ];
                Node right = nodes[ node.right ];

                AABBHit leftHit = intersectAABB( ray.o.xyz, ray.dir.xyz, left.aabb.bmin.xyz, left.aabb.bmax.xyz );
                AABBHit rightHit = intersectAABB( ray.o.xyz, ray.dir.xyz, right.aabb.bmin.xyz, right.aabb.bmax.xyz );

                if( leftHit.hit && rightHit.hit ) {
                    uint frstNode = leftHit.tClose > rightHit.tClose ? node.right : node.left;
                    uint scndNode = leftHit.tClose > rightHit.tClose ? node.left : node.right;
                
                    V[size++] = scndNode;
                    V[size++] = frstNode;
                } else if( leftHit.hit ) {
                    V[size++]=node.left;
                    } else if( rightHit.hit ) {
                        V[size++]=node.right; 
                }
            }
        }

        rays[ id ].hit = ray.o + closestT * ray.dir;
    }
    )";

    GLuint shader = compileShader( source );
    GLuint computeProgram = linkProgram( shader );
    return computeProgram;
}