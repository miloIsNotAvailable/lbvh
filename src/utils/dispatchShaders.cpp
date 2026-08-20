#include <utils.hpp>

void dispatchProgram( GLuint u, GLuint v, GLuint w, GLuint computeProgram ) {
   
    glUseProgram(computeProgram); 

    glDispatchCompute(u, v, w);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

Program::Program( const std::string &source ) {
    std::string src = source;
    GLuint shader = compileShader( src );
    GLuint computeProgram = linkProgram( shader );
    program = computeProgram;
}

void Program::operator()( GLuint groupsX, GLuint groupsY, GLuint threads ) {
    glUseProgram(program); 

    glDispatchCompute(groupsX, groupsY, threads);
}

void barrier( GLbitfield barrier ) {
    glMemoryBarrier(barrier);
}

Buffer::Buffer( GLenum target, GLsizeiptr size, const void * src, GLenum usage ):
target( target ), size( size ), usage(usage) {
    // if (src)
    //     std::memcpy(data.data(), src, size);

    glGenBuffers(1, &buffer);
    glBindBuffer(target, buffer);
    glBufferData(target, size, src, usage);
}

void Buffer::toGPU( GLuint idx ) {
    glBindBuffer( target, buffer );
    glBindBufferBase(target, idx, buffer);
}

void Buffer::update( const void* data, GLsizeiptr dataSize, GLintptr off ) {
    glBindBuffer(target, buffer);

    glBufferSubData(
        target,
        off,
        dataSize,
        data
    );
}

GLuint Buffer::id() {
    return buffer;
}

void Program::indirect( Buffer &buffer ) {
    glUseProgram(program);

    glBindBuffer(
        GL_DISPATCH_INDIRECT_BUFFER,
        buffer.id()
    );

    glDispatchComputeIndirect(0);
}


// template<typename T> std::vector<T> Buffer::toCPU( ) {
//     // data = ::operator new(size, std::align_val_t(16));
  
//     // if ( data == nullptr ) {
//     //     data = ::operator new(size, std::align_val_t(16));
//     // }
//     std::vector<T> v(size / sizeof(T));
  
//     glBindBuffer(target, buffer);

//     glGetBufferSubData(
//         target,
//         0,
//         size,
//         v.data()
//     );

//     return v;
// }