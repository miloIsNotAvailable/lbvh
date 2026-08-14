#include "./headers/radix.hpp"




Radix::Radix( GLuint inputSSBO, size_t size ) : inputSSBO(inputSSBO), size(size) {
    // size = data.size();
    // inputSSBO = generateBuffer( GL_SHADER_STORAGE_BUFFER, data.size() * sizeof(uint32_t), data.data(), GL_DYNAMIC_COPY );
    // inputSSBO = inputSSBO;
    extractSSBO = generateBuffer( GL_SHADER_STORAGE_BUFFER, size * sizeof(uint32_t), nullptr, GL_DYNAMIC_COPY );
    outputSSBO = generateBuffer( GL_SHADER_STORAGE_BUFFER, size * sizeof(uint32_t), nullptr, GL_DYNAMIC_COPY );
    uniformUBO = generateBuffer( GL_UNIFORM_BUFFER, sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW );

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, inputSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, extractSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, outputSSBO);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformUBO);
    
    extractProgram = createExtractShader();
    scanProgram = createScanShader();
    scatterProgram = createScatterShader();
}

GLuint Radix::operator()( uint32_t bit, GLuint gX, GLuint gY, GLuint t ) {

    // for( int i = 0; i < bit; i ++ ) {
    //     // bindBuffer( uniformSSBO, sizeof(uint32_t), &i, GL_DYNAMIC_COPY );
    // }
    modifyBufferData(
        uniformUBO,
        GL_UNIFORM_BUFFER,
        0,
        sizeof(uint32_t),
        &bit
    );

    dispatchProgram( gX, gY, t, extractProgram );
    dispatchProgram( gX, gY, t, scanProgram );
    dispatchProgram( gX, gY, t, scatterProgram );
    // scan( data );
    // scatter( data );
   
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // std::swap(inputSSBO, outputSSBO);

    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, inputSSBO);
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, outputSSBO);

    // glBindBuffer( GL_SHADER_STORAGE_BUFFER, outputSSBO );
    // std::vector<uint32_t> res( size );
    // glGetBufferSubData(
    //     GL_SHADER_STORAGE_BUFFER,
    //     0,
    //     res.size() * sizeof(uint32_t),
    //     res.data()
    // );

    // res.erase(
    //     std::remove(res.begin(), res.end(), 0),
    //     res.end());

    return inputSSBO;
}

// void extract( std::vector<uint32_t> data ) {
//     // GLuint ssbo;
//     // glGenBuffers( 1, &ssbo );


//     // glBindBuffer( GL_SHADER_STORAGE_BUFFER, ssbo );
//     // glBufferData( GL_SHADER_STORAGE_BUFFER, 
//     //                 data.size() * sizeof( uint32_t ), 
//     //                 data.data(), 
//     //                 GL_DYNAMIC_COPY 
//     //             );

//     std::string source = std::string(header) + R"(
//     void main()
//     {
//         uint id = gl_GlobalInvocationID.x;
//         extract[id] = 1u - ((input[id] >> bit) & 1u);
//     }
//     )";

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
//     }

//     GLuint computeProgram = glCreateProgram();

//     glAttachShader(computeProgram, shader);
//     glLinkProgram(computeProgram);

//     glGetProgramiv(computeProgram, GL_LINK_STATUS, &success);

//     if (!success)
//     {
//         char log[1024];
//         glGetProgramInfoLog(computeProgram, sizeof(log), nullptr, log);
//         std::cerr << "Program link error:\n" << log << std::endl;
//     }

//     GLuint numElements = data.size();
//     GLuint localSize = 1;

//     GLuint groups =
//         (numElements + localSize - 1) / localSize;

//     glUseProgram(computeProgram); 

//     glDispatchCompute(groups, 1, 1);
//     glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
//     // run();

//     // return res;
// }

// void scan( std::vector<uint32_t> data ) {

//     std::string source = std::string(header) + R"(
//     void main()
//     {
//         uint id = gl_GlobalInvocationID.x;

//         for( uint i = 1; i < extract.length(); i *= 2 ) {

//             uint id1 = id + 1;
//             uint idx1 = id1 * i * 2;
            
//             uint idx0 = idx1 - 1;
            
//             if (idx0 < extract.length())
//             {
//                 extract[idx0] += extract[idx0 - i];
//             }
//             // scan[ idx0 ] = extract[ idx0 - i ] + extract[ idx0 ];
//             barrier();
//         }

//         if (id == 0)
//             extract[extract.length() - 1] = 0;

//         barrier();

//         for( uint i = extract.length() / 2; i > 0; i /= 2 ) {

//             uint id1 = id + 1;
//             uint idx1 = id1 * i * 2;
            
//             uint idx0 = idx1 - 1;

//             uint idxRight0 = idx0;
//             uint idxLeft0 = idx0 - i;

            
//             if (idx0 < extract.length())
//             {
//                 uint temp = extract[idxLeft0];
//                 extract[idxLeft0] = extract[idxRight0];
//                 extract[idxRight0] += temp;
//             }
//             // scan[ idx0 ] = extract[ idx0 - i ] + extract[ idx0 ];
//             barrier();
//         }
//     }
//     )";

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
//     }

//     GLuint computeProgram = glCreateProgram();

//     glAttachShader(computeProgram, shader);
//     glLinkProgram(computeProgram);

//     glGetProgramiv(computeProgram, GL_LINK_STATUS, &success);

//     if (!success)
//     {
//         char log[1024];
//         glGetProgramInfoLog(computeProgram, sizeof(log), nullptr, log);
//         std::cerr << "Program link error:\n" << log << std::endl;
//     }

//     GLuint numElements = data.size();
//     GLuint localSize = 1;

//     GLuint groups =
//         (numElements + localSize - 1) / localSize;

//     glUseProgram(computeProgram); 

//     // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
//     glDispatchCompute(groups, 1, 1);
//     glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
// }

// void scatter( std::vector<uint32_t> data ) {

//     std::string source = std::string(header) + R"(
//     void main()
//     {
//         uint id = gl_GlobalInvocationID.x;

//         if (id >= input.length())
//             return;

//         uint b = (input[id] >> bit) & 1u;

//         uint totalZeros = extract[extract.length() - 1] + 
//                         (1u - ((input[extract.length() - 1] >> bit) & 1u));

//         uint ind;

//         if (b == 0u)
//         {
//             ind = extract[id];
//         }
//         else
//         {
//             uint onesBefore = id - extract[id];
//             ind = totalZeros + onesBefore;
//         }

//         outputData[ind] = input[id];
//         }
//     )";

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
//     }

//     GLuint computeProgram = glCreateProgram();

//     glAttachShader(computeProgram, shader);
//     glLinkProgram(computeProgram);

//     glGetProgramiv(computeProgram, GL_LINK_STATUS, &success);

//     if (!success)
//     {
//         char log[1024];
//         glGetProgramInfoLog(computeProgram, sizeof(log), nullptr, log);
//         std::cerr << "Program link error:\n" << log << std::endl;
//     }

//     GLuint numElements = data.size();
//     GLuint localSize = 1;

//     GLuint groups =
//         (numElements + localSize - 1) / localSize;

//     glUseProgram(computeProgram); 

//     glDispatchCompute(groups, 1, 1);
//     glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
// }