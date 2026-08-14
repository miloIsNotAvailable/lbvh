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
    void toCPU();

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

    ~Buffer()
    {
        glDeleteBuffers(1, &buffer);

        if (data)
            ::operator delete(data, std::align_val_t(16));
    }
};