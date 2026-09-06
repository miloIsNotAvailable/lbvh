#pragma once
#include<utils.hpp>
#include<glm/glm.hpp>
#include <lens.hpp>
#include <randoms.hpp>

class Vec3 {
public:
    Buffer x;
    Buffer y;
    Buffer z;

    Vec3() = default;

    Vec3(uint32_t size)
        : x(
            GL_SHADER_STORAGE_BUFFER,
            size * sizeof(float),
            nullptr,
            GL_DYNAMIC_COPY
        ),
          y(
            GL_SHADER_STORAGE_BUFFER,
            size * sizeof(float),
            nullptr,
            GL_DYNAMIC_COPY
        ),
          z(
            GL_SHADER_STORAGE_BUFFER,
            size * sizeof(float),
            nullptr,
            GL_DYNAMIC_COPY
        )
    {}

    void toGPU(GLuint& idx) {
        x.bindGPU(idx);
        y.bindGPU(idx);
        z.bindGPU(idx);
    }

    std::vector<glm::vec3> toCPU() {
        std::vector<float> xs = x.toCPU<float>();
        std::vector<float> ys = y.toCPU<float>();
        std::vector<float> zs = z.toCPU<float>();

        std::vector<glm::vec3> result(xs.size());

        for (size_t i = 0; i < xs.size(); ++i) {
            result[i] = glm::vec3(
                xs[i],
                ys[i],
                zs[i]
            );
        }

        return result;
    }
};

struct Vec3View {
    Buffer& x;
    Buffer& y;
    Buffer& z;

    Vec3View(Buffer& x, Buffer& y, Buffer& z)
        : x(x), y(y), z(z)
    {}

    Vec3View(Vec3& v)
        : x(v.x), y(v.y), z(v.z)
    {}

    void toGPU(GLuint& idx) {
        x.bindGPU(idx);
        y.bindGPU(idx);
        z.bindGPU(idx);
    }
};

class Rays {
    private:
    
    public: 
    Buffer oX;
    Buffer oY;
    Buffer oZ;
    
    Buffer dX;
    Buffer dY;
    Buffer dZ;

    Rays() = default;

    Rays( uint32_t size ) : 
    oX(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(float),
        nullptr,
        GL_DYNAMIC_COPY   
    ) ,
    oY(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(float),
        nullptr,
        GL_DYNAMIC_COPY   
    ) ,
    oZ(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(float),
        nullptr,
        GL_DYNAMIC_COPY   
    ),

    dX(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(float),
        nullptr,
        GL_DYNAMIC_COPY   
    ) ,
    dY(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(float),
        nullptr,
        GL_DYNAMIC_COPY   
    ) ,
    dZ(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(float),
        nullptr,
        GL_DYNAMIC_COPY   
    ){}

    void toGPU( GLuint &idx ) {
        
        // GLuint idx = 0;
        
        oX.bindGPU( idx );
        oY.bindGPU( idx );
        oZ.bindGPU( idx );
        
        dX.bindGPU( idx );
        dY.bindGPU( idx );
        dZ.bindGPU( idx );
    }

    Vec3View d() {
        return Vec3View( dX, dY, dZ );
    }
};

struct RaysView {
    Buffer* oX;
    Buffer* oY;
    Buffer* oZ;

    Buffer* dX;
    Buffer* dY;
    Buffer* dZ;

    RaysView() = default;
    RaysView(Rays& rays)
        : oX(&rays.oX), oY(&rays.oY), oZ(&rays.oZ),
          dX(&rays.dX), dY(&rays.dY), dZ(&rays.dZ)
    {}

    RaysView(Vec3View origin, Vec3View direction)
        : oX(&origin.x), oY(&origin.y), oZ(&origin.z),
          dX(&direction.x), dY(&direction.y), dZ(&direction.z)
    {}

    Vec3View o() {
        return Vec3View( *oX, *oY, *oZ );
    }

    Vec3View d() {
        return Vec3View( *dX, *dY, *dZ );
    }

    void toGPU(GLuint& idx) {
        oX->bindGPU(idx);
        oY->bindGPU(idx);
        oZ->bindGPU(idx);

        dX->bindGPU(idx);
        dY->bindGPU(idx);
        dZ->bindGPU(idx);
    }
};

inline const std::string genCameraRaysSrc = R"(#version 430

    layout(local_size_x = 64) in;)" + structs + R"(


    layout(std430, binding = 0) buffer RaysPosDir
    {
        float rays[];
    };
    
    // layout(std430, binding = 1) buffer RayPosY
    // {
    //     float oY[];
    // };
    
    // layout(std430, binding = 2) buffer RayPosZ
    // {
    //     float oZ[];
    // };
    
    // layout(std430, binding = 3) buffer RDirsX
    // {
    //     float dX[];
    // };
    // layout(std430, binding = 4) buffer RDirsY
    // {
    //     float dY[];
    // };
    // layout(std430, binding = 5) buffer RDirsZ
    // {
    //     float dZ[];
    // };

    layout(std430, binding = 1) buffer StatesX
    {
        float rx[];
    };

    layout(std430, binding = 2) buffer StatesY
    {
        float ry[];
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

    void main() {
    
        uint id = gl_GlobalInvocationID.x;
        
        uint N = rays.length() / 6;

        if( id >= N ) return;

        uint WIDTH = dims.x;
        uint HEIGHT = dims.y;

        uint x = id % WIDTH;
        uint y = id / WIDTH;

        vec2 fragCoord = vec2(float(x), float(y)) + 0.5;

        vec2 st = fragCoord / vec2(float(WIDTH), float(HEIGHT)) - 0.5;

        st.x *= float(WIDTH) / float(HEIGHT);

        float r = rx[id] * camera.apertureSize;
        float a = ry[id] * (2.0 * PI);

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

    
        // oX[id] = rndAperturePointWrld.x;
        // oY[id] = rndAperturePointWrld.y;
        // oZ[id] = rndAperturePointWrld.z;

        // dX[id] = rayDir.x;
        // dY[id] = rayDir.y;
        // dZ[id] = rayDir.z;
     
        rays[id + N * 0] = rndAperturePointWrld.x;
        rays[id + N * 1] = rndAperturePointWrld.y;
        rays[id + N * 2] = rndAperturePointWrld.z;

        rays[id +  N * 3] = rayDir.x;
        rays[id +  N * 4] = rayDir.y;
        rays[id +  N * 5] = rayDir.z;

    }

)";

class ThinLens {

    private:
    Rays rays; 
    Buffer camera, outRays;

    Program genRays;

    public:
    ThinLens( uint32_t size ) : 
    outRays(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(float) * 6,
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    rays( size ), 
    genRays( genCameraRaysSrc ) {}

    Buffer &operator()( uint32_t size, Random2D &states, Film &film );
};