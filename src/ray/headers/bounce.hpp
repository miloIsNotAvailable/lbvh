#include <utils.hpp>
#include <randoms.hpp>

inline const std::string getLightSampleSrc = R"(
    #version 430
    layout(local_size_x = 64) in;
)" 
+ structs + 
R"(

    layout(std430, binding = 0) buffer RIn
    {
        float raysIn[];
    };

    layout(std430, binding = 1) buffer RInShadow
    {
        float shadowRays[];
    };

    layout(std430, binding = 2) buffer RHit
    {
        float hitT[];
    };

    layout(std430, binding = 3) buffer Dead
    {
        uint dead[];
    };

    layout(std430, binding = 4) buffer Triangles
    {
        Triangle triangles[];
    };
    
    layout(std430, binding = 5) buffer TriIds
    {
        int triIds[];
    };

    layout(std430, binding = 6) buffer Radiance
    {
        vec4 L[];
    };
    
    layout(std430, binding = 7) buffer Throughput
    {
        vec4 beta[];
    };
    
    layout(std430, binding = 8) buffer Lights
    {
        Light light;
    };
    
    layout(std430, binding = 9) buffer Materials
    {
        Material materials[];
    };
    
    layout(std430, binding = 10) buffer Normals
    {
        vec4 normals[];
    };

    layout(std430, binding = 11) buffer Occluded
    {
        uint occluded[];
    };

    layout(std430, binding = 12) buffer Dist2
    {
        float dist2[];
    };

    float PowerHeuristic( float nf, float fPdf, float ng, float gPdf ) {
        float f = nf * fPdf;
        float g = ng * gPdf;
        return ( f * f ) / ( f * f + g * g );
    }    

    void main()
    {
        uint id = gl_GlobalInvocationID.x;
        
        uint M = raysIn.length() / 6;

        if( id >= M ) return;

        if( dead[id] == 1u ) {
            return;
        }

        vec3 o = vec3(
            raysIn[id + M * 0],
            raysIn[id + M * 1],
            raysIn[id + M * 2]
        );

        vec3 dir = vec3(
            raysIn[id + M * 3],
            raysIn[id + M * 4],
            raysIn[id + M * 5]
        );

        uint triIdx = uint(triIds[id]);
        vec3 Le = light.Le.xyz;
        
        uint matIdx = uint(triangles[triIdx].matId);

        vec3 pos = o + hitT[id] * dir;
        
        vec3 color = materials[matIdx].diffuse.xyz / PI;
        vec3 throughput = beta[id].xyz;
        vec3 nor = normals[triIdx].xyz;

        vec3 lightPos = light.pos.xyz;
        float r = light.pos.w;

        if (dot(dir, nor) > 0.0)
            nor = -nor;

        vec3 liray = vec3(
            shadowRays[id + M * 3],
            shadowRays[id + M * 4],
            shadowRays[id + M * 5]
        );

        float cosTheta = max(dot(nor, liray), 0.);

        vec3 normal = light.normal.xyz;

        float cosThetaL = max(dot(normal, -liray), 0.);
        if( cosThetaL > 0.f && cosTheta > 0.f ) {        

            float pdf_area = 1. / (PI * r * r);
            float pdf_omega = pdf_area * dist2[id] / cosThetaL;
            vec3 Li = Le / pdf_omega;

            float pdf_bsdf = cosTheta / PI; 

            float occ = 1.f - float(occluded[id]);
            
            float w = PowerHeuristic( 1., pdf_omega, 1., pdf_bsdf );
            L[id] += vec4( w * occ * throughput * color * Li * cosTheta, 0.f);   
        }
    }
)";

inline const std::string sampleDirsSrc = R"(
    #version 430
    layout(local_size_x = 64) in;
)" 
+ structs + 
R"(

    layout(std430, binding = 0) buffer RIn
    {
        float raysIn[];
    };

    layout(std430, binding = 1) buffer Dead
    {
        uint dead[];
    };

    layout(std430, binding = 2) buffer RandX
    {
        float bx[];
    };

    layout(std430, binding = 3) buffer RandY
    {
        float by[];
    };

    layout(std430, binding = 4) buffer RandZ
    {
        float bz[];
    };

    layout(std430, binding = 5) buffer BSDF_PDF
    {
        float pdf_bsdf[];
    };

    layout(std430, binding = 6) buffer TriIds
    {
        int triIds[];
    };

    layout(std430, binding = 7) buffer Normals
    {
        vec4 normals[];
    };

    layout(std430, binding = 8) buffer Triangles
    {
        Triangle triangles[];
    };

    layout(std430, binding = 9) buffer Materials
    {
        Material materials[];
    };

    layout(std430, binding = 10) buffer Bounces
    {
        uint bounces;
    };

    layout(std430, binding = 11) buffer HitT
    {
        float hitT[];
    };

    layout(std430, binding = 12) buffer Throughput
    {
        vec4 beta[];
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
        
        uint M = raysIn.length() / 6;

        if( id >= M ) return;
        if( dead[id] == 1u ) return;

        vec3 o = vec3(
            raysIn[id + M * 0],
            raysIn[id + M * 1],
            raysIn[id + M * 2]
        );

        vec3 dir = vec3(
            raysIn[id + M * 3],
            raysIn[id + M * 4],
            raysIn[id + M * 5]
        );

        vec3 pos = o + hitT[id] * dir;

        uint tId = uint(triIds[ id ]);
        vec3 nor = normals[ tId ].xyz;

        if (dot(dir, nor) > 0.0)
            nor = -nor;

        uint matIdx = uint(triangles[tId].matId);
        vec3 color = materials[matIdx].diffuse.xyz / PI;

        vec3 bounceDir = RandomUnitVectorInHemisphereOf( nor, vec2(bx[id], by[id]) );
        
        float cosTheta = max(dot(nor, bounceDir), 0.);
        if (cosTheta <= 0.) {
            dead[id] = 1u;
            return;
        };

        float pdf_bounce = cosTheta / PI;

        beta[id] *= vec4(color * cosTheta / pdf_bounce, 0.f);

        // if( bounces > 2 ) {
        //     float p = clamp(
        //         max(beta[id].x, max(beta[id].y, beta[id].z)),
        //         0.05,
        //         0.95
        //     );
            
        //     if( p < bz[id] ) {
        //         dead[id] = 1u;
        //         return;
        //     }
    
        //     beta[id] /= p;
        // }

        // rays[id].o = pos + nor * EPS;
        // rays[id].dir = vec4( bounceDir, 0.f );

        raysIn[ id + M * 0 ] = pos.x + nor.x * EPS;
        raysIn[ id + M * 1 ] = pos.y + nor.y * EPS;
        raysIn[ id + M * 2 ] = pos.z + nor.z * EPS;
        
        raysIn[ id + M * 3 ] = bounceDir.x;
        raysIn[ id + M * 4 ] = bounceDir.y;
        raysIn[ id + M * 5 ] = bounceDir.z;

        pdf_bsdf[id] = pdf_bounce;
    }
)";

inline const std::string addColorSrc = R"(
    #version 430
    layout(local_size_x = 64) in;
)" 
+ structs + R"(

layout(std430, binding = 0) buffer Radiance
{
    vec4 L[];
};

layout(std430, binding = 1) buffer OutCol
{
    vec4 outCol[];
};

layout(std140, binding = 0) uniform Iter
{
    uint iter;
};

void main() {
        uint id = gl_GlobalInvocationID.x;
        
        if( id >= outCol.length() ) return;

        outCol[id] += L[id] / iter;
}
)";

// Program addColor( addColorSrc );

// void sumColor( uint32_t size, Buffer &triangles, Buffer &triIds, Buffer &materials, Buffer &outColor ) {

//     GLuint idx = 0;
//     triangles.bindGPU( idx );
//     triIds.bindGPU( idx );
//     materials.bindGPU( idx );
//     outColor.bindGPU( idx );

//     addColor( (size + 63) / 64, 1, 1 );
//     barrier(GL_SHADER_STORAGE_BARRIER_BIT);
// }

inline const std::string contributeEmissiveSrc = R"(
    #version 430
    layout(local_size_x = 64) in;
)" 
+ structs + R"(

    layout(std430, binding = 0) buffer RIn
    {
        float raysIn[];
    };

    layout(std430, binding = 1) buffer RHit
    {
        float raysHitT[];
    };

    layout(std430, binding = 2) buffer Dead
    {
        uint dead[];
    };

    layout(std430, binding = 3) buffer HitEm
    {
        uint hitEmissive[];
    };

    layout(std430, binding = 4) buffer Radiance
    {
        vec4 L[];
    };
    
    layout(std430, binding = 5) buffer Throughput
    {
        vec4 beta[];
    };
    
    layout(std430, binding = 6) buffer Lights
    {
        Light light;
    };
    
    layout(std430, binding = 7) buffer PdfBsdf
    {
        float pdf_bsdf[];
    };
    
    layout(std430, binding = 8) buffer Bounces
    {
        uint bounce;
    };

    float PowerHeuristic( float nf, float fPdf, float ng, float gPdf ) {
        float f = nf * fPdf;
        float g = ng * gPdf;
        return ( f * f ) / ( f * f + g * g );
    }

    void main() {
        uint id = gl_GlobalInvocationID.x;
        
        uint N = raysIn.length() / 6;

        if( id >= N ) return;
        if( dead[id] == 1u ) return;
        if( hitEmissive[id] == 0u ) return;
    
        float w = 1.f;
        float r = light.pos.w;

        vec3 o = vec3(
            raysIn[ id + N * 0 ],
            raysIn[ id + N * 1 ],
            raysIn[ id + N * 2 ]
        );
        
        vec3 d = vec3(
            raysIn[ id + N * 3 ],
            raysIn[ id + N * 4 ],
            raysIn[ id + N * 5 ]
        );

        vec3 hit = o + raysHitT[id] * d;

        vec3 lDist = hit - o;
        vec3 liray = normalize( lDist );
        float dist2 = dot( lDist, lDist );

        float cosThetaL = max( 0.f, dot( light.normal.xyz, -liray ) );
        
        if( cosThetaL <= 0.f ) {
            dead[id] = 1;
            return;
        }

        if( bounce > 0 ) {
          
            float pdf_area = 1. / (PI * r * r);
            float pdf_omega = pdf_area * dist2 / cosThetaL;
            w = PowerHeuristic( 1., pdf_bsdf[id], 1., pdf_omega);
        }

        L[id] += beta[id] * w * light.Le;
        dead[id] = 1u;
    }
)";

class Contribution {
    private:

    Buffer bounce;
    Program addEmissive, addDirect, sampleDir, addCol;

    public:
    Buffer pdf_bsdf;

    Contribution( uint32_t size ):

    pdf_bsdf(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    ),

    bounce(
        GL_SHADER_STORAGE_BUFFER,
        sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    ),

    addEmissive( contributeEmissiveSrc ),
    addDirect( getLightSampleSrc ),
    sampleDir( sampleDirsSrc ),
    addCol( addColorSrc )
    {}

    void emissive( 
        uint32_t size,
        Buffer &raysIn, 
        Buffer &hitT, 
        Buffer &dead, 
        Buffer &hitEm, 
        Buffer &L, 
        Buffer &beta,
        Buffer &light,
        uint32_t b );

    void contribute(
        uint32_t size,
        Buffer &raysIn,
        Buffer &shadowRays,
        Buffer &hitT,
        Buffer &dead, 
        Buffer &triangles,
        Buffer &triIds, 
        Buffer &L,
        Buffer &beta,
        Buffer &light,
        Buffer &materials,
        Buffer &normals,
        Buffer &occluded,
        Buffer &dist2
    );

    void sampleBSDF(
        uint32_t size,
        Buffer &raysIn,
        Buffer &dead, 
        Random2D &rand2d,
        Buffer &rand1d,
        Buffer &triIds, 
        Buffer &normals,
        Buffer &triangles,
        Buffer &materials,
        Buffer &hitT,
        Buffer &beta
    );

    void addColor( uint32_t size, Buffer &L, Buffer &outCol, Buffer &iter );
};