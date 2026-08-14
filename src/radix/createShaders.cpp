#include <radix.hpp>

// GLuint compileShader( std::string &source ) {
//     GLuint shader = glCreateShader(GL_COMPUTE_SHADER);

//     const char * src = source.c_str();

//     glShaderSource(shader, 1, &src, nullptr);
//     glCompileShader(shader);

//     GLint success;
//     glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

//     if (!success)
//     {
//         char log[1024];
//         glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
//         std::cerr << "Shader compile error:\n" << log << std::endl;
//         glDeleteShader(shader);
//         return 0;
//     }

//     return shader;
// }

// GLuint linkProgram( GLuint shader ) {
//     GLuint computeProgram = glCreateProgram();
    
//     glAttachShader(computeProgram, shader);
//     glLinkProgram(computeProgram);
    
//     GLint success;
//     glGetProgramiv(computeProgram, GL_LINK_STATUS, &success);

//     if (!success)
//     {
//         char log[1024];
//         glGetProgramInfoLog(computeProgram, sizeof(log), nullptr, log);
//         std::cerr << "Program link error:\n" << log << std::endl;
//     }

//     return computeProgram;
// }

GLuint createExtractShader() {
    std::string source = std::string(header) + R"(
    void main()
    {
        uint id = gl_GlobalInvocationID.x;
        extract[id].x = 1u - ((input[id].x >> bit) & 1u);
        extract[id].y = input[id].y;
    }
    )";

    GLuint shader = compileShader( source );
    GLuint computeProgram = linkProgram( shader );
    return computeProgram;
}

GLuint createScanShader() {
    std::string source = std::string(header) + R"(
    void main()
    {
        uint id = gl_GlobalInvocationID.x;

        for( uint i = 1; i < extract.length(); i *= 2 ) {

            uint id1 = id + 1;
            uint idx1 = id1 * i * 2;
            
            uint idx0 = idx1 - 1;
            
            if (idx0 < extract.length())
            {
                extract[idx0].x += extract[idx0 - i].x;
            }
            // scan[ idx0 ] = extract[ idx0 - i ] + extract[ idx0 ];
            barrier();
        }

        if (id == 0)
            extract[extract.length() - 1].x = 0;

        barrier();

        for( uint i = extract.length() / 2; i > 0; i /= 2 ) {

            uint id1 = id + 1;
            uint idx1 = id1 * i * 2;
            
            uint idx0 = idx1 - 1;

            uint idxRight0 = idx0;
            uint idxLeft0 = idx0 - i;

            
            if (idx0 < extract.length())
            {
                uint temp = extract[idxLeft0].x;
                extract[idxLeft0].x = extract[idxRight0].x;
                extract[idxRight0].x += temp;
            }
            // scan[ idx0 ] = extract[ idx0 - i ] + extract[ idx0 ];
            barrier();
        }
    }
    )";

    GLuint shader = compileShader( source );
    GLuint computeProgram = linkProgram( shader );
    return computeProgram;
}

GLuint createScatterShader() {
    std::string source = std::string(header) + R"(
    void main()
    {
        uint id = gl_GlobalInvocationID.x;

        if (id >= input.length())
            return;

        uint b = (input[id].x >> bit) & 1u;

        uint totalZeros = extract[extract.length() - 1].x + 
                        (1u - ((input[extract.length() - 1].x >> bit) & 1u));

        uint ind;

        if (b == 0u)
        {
            ind = extract[id].x;
        }
        else
        {
            uint onesBefore = id - extract[id].x;
            ind = totalZeros + onesBefore;
        }

        outputData[ind] = input[id];
        trianglesOut[ind] = triangles[id];
        }
    )";

    GLuint shader = compileShader( source );
    GLuint computeProgram = linkProgram( shader );
    return computeProgram;
}