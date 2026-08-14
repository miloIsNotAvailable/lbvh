#include <utils.hpp>

void modifyBufferData(
    GLuint buffer,
    GLenum target,
    GLintptr offset,
    GLsizeiptr size,
    const void* data)
{
    glBindBuffer(target, buffer);

    glBufferSubData(
        target,
        offset,
        size,
        data
    );
}

GLuint generateBuffer( GLenum target, GLsizeiptr size, const void * data, GLenum usage ) {
    GLuint buffer;
    glGenBuffers( 1, &buffer );

    glBindBuffer( target, buffer );
    glBufferData( target, 
                    size, 
                    data, 
                    usage 
                );
        
    return buffer;
}

GLuint compileShader( std::string &source ) {
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);

    const char * src = source.c_str();

    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error:\n" << log << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLuint linkProgram( GLuint shader ) {
    GLuint computeProgram = glCreateProgram();
    
    glAttachShader(computeProgram, shader);
    glLinkProgram(computeProgram);
    
    GLint success;
    glGetProgramiv(computeProgram, GL_LINK_STATUS, &success);

    if (!success)
    {
        char log[1024];
        glGetProgramInfoLog(computeProgram, sizeof(log), nullptr, log);
        std::cerr << "Program link error:\n" << log << std::endl;
    }

    return computeProgram;
}