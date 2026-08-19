#pragma once

#include <glad/gl.h>
#include <vector>
#include <string>
#include <iostream>

GLuint generateBuffer( GLenum target, GLsizeiptr size, const void * data, GLenum usage );
void modifyBufferData(
    GLuint buffer,
    GLenum target,
    GLintptr offset,
    GLsizeiptr size,
    const void* data);

void dispatchProgram( GLuint u, GLuint v, GLuint w, GLuint computeProgram );
GLuint compileShader( std::string &source );
GLuint linkProgram( GLuint shader );

class Program {

    private:
    GLuint program;
    public:
    Program( const std::string &source ); 

    void operator()( GLuint groupsX, GLuint groupsY, GLuint threads  );
    
    GLuint get() {
        return program;
    }
};

void barrier( GLbitfield barrier );

class Buffer {
    private:
    GLuint buffer;
    GLenum target, usage;
    GLsizeiptr size;
    void* data = nullptr;
    public:
    Buffer( GLenum target, GLsizeiptr size, const void * data, GLenum usage );
    void toGPU( GLuint index );
    template<typename T> std::vector<T> toCPU() {
        std::vector<T> v(size / sizeof(T));
  
        glBindBuffer(target, buffer);

        glGetBufferSubData(
            target,
            0,
            size,
            v.data()
        );

        return v;
    };

    template<typename T>
    T* get() {
        return reinterpret_cast<T*>(data);
    }

    template<typename T>
    const T* get() const {
        return reinterpret_cast<const T*>(data);
    }

    size_t count() {
        return size;
    }

    template<typename T>
    size_t count() const
    {
        return size / sizeof(T);
    }

    void update( const void* data, GLsizeiptr dataSize, GLintptr off = 0);

    friend void swap(Buffer& a, Buffer& b) noexcept {
        using std::swap;
        swap(a.buffer, b.buffer);
        swap(a.target, b.target);
        swap(a.usage, b.usage);
        swap(a.size, b.size);
        swap(a.data, b.data);
    }

    void destroy() {
        
        if( buffer ) {
            glDeleteBuffers(1, &buffer);
            buffer = 0;
        }

        if (data) {
            ::operator delete(data, std::align_val_t(16));
            data = nullptr;
        }
    }

    Buffer(Buffer&& other) noexcept
        : buffer(other.buffer),
          target(other.target),
          size(other.size),
          data(other.data)
    {
        other.buffer = 0;
        other.target = 0;
        other.size = 0;
        other.data = nullptr;
    }

    Buffer& operator=(Buffer&& other) noexcept
    {
        if (this != &other) {

            if (buffer)
                glDeleteBuffers(1, &buffer);

            if (data)
                ::operator delete(data, std::align_val_t(16));

            buffer = other.buffer;
            target = other.target;
            size   = other.size;
            data   = other.data;

            other.buffer = 0;
            other.target = 0;
            other.size   = 0;
            other.data   = nullptr;
        }

        return *this;
    }

    Buffer(const Buffer& other)
        : target(other.target),
        size(other.size),
        data(nullptr)
    {
        glGenBuffers(1, &buffer);

        glBindBuffer(target, buffer);
        glBufferData(
            target,
            size,
            nullptr,
            GL_DYNAMIC_COPY
        );

        glBindBuffer(GL_COPY_READ_BUFFER, other.buffer);
        glBindBuffer(GL_COPY_WRITE_BUFFER, buffer);

        glCopyBufferSubData(
            GL_COPY_READ_BUFFER,
            GL_COPY_WRITE_BUFFER,
            0,
            0,
            size
        );
    }

    ~Buffer()
    {
        if( buffer ) {

            glDeleteBuffers(1, &buffer);
            buffer = 0;
        }

        // if (data) {

        //     ::operator delete(data, std::align_val_t(16));
        //     data = nullptr;
        // }
    }
};

inline const std::string structs = R"(    

    #define PI 3.1415926
    #define EPS 1e-3f

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
        float tmin;
        float tmax;
        float t;
        uint triId;
        uint dead;
        uint pixelId;
        int matId;
    };

    struct ShadowRay {
        vec4 o;
        vec4 dir;
        vec4 normal;
        float tmin;
        float tmax;
        uint occluded;
    };

    struct Pixel {
        vec4 L;
        vec4 beta;
        vec4 col;
        uint pixelId;
        uint state;
    };

    struct Camera {
        vec4 eye, center, n;
        vec4 sensor, focalPlane;

        vec4 worldUp, right, up;
        
        float sensorDist, focalPlaneDist;
        float apertureSize;
        uint WIDTH, HEIGHT;
    };

    struct Material {
        vec4 ambient;
        vec4 diffuse;
        vec4 specular;
        vec4 transmittance;
        vec4 emission;

        float shininess, ior, dissolve;
    };

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


    uint pcg(inout uint state)
    {
        state = state * 747796405u + 2891336453u;

        uint word =
            ((state >> ((state >> 28u) + 4u)) ^ state)
            * 277803737u;

        return (word >> 22u) ^ word;
    }
        
    float rand(inout uint state)
    {
        return float(pcg(state) >> 8u) * (1.0 / 16777216.0);
    }
)";