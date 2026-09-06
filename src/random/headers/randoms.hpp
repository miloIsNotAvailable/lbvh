#pragma once

#include <utils.hpp>
#include <fstream>
#include <regex>
#include <glm/glm.hpp>
// #include <film.hpp>

class Random2D
{
private:
    Buffer x;
    Buffer y;

public:
    Random2D(uint32_t size)
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
        )
    {}

    void toGPU(GLuint& idx)
    {
        x.bindGPU(idx);
        y.bindGPU(idx);
    }

    std::vector<glm::vec2> toCPU()
    {
        std::vector<float> xs = x.toCPU<float>();
        std::vector<float> ys = y.toCPU<float>();

        std::vector<glm::vec2> out(xs.size());

        for (size_t i = 0; i < xs.size(); ++i)
            out[i] = glm::vec2(xs[i], ys[i]);

        return out;
    }

    Buffer& X() { return x; }
    Buffer& Y() { return y; }

    const Buffer& X() const { return x; }
    const Buffer& Y() const { return y; }
};

inline const std::string seedSrc = R"(
#version 430

layout(local_size_x = 64) in;

layout(std430, binding = 0) buffer States
{
    uint states[];
};

layout(std140, binding = 0) uniform Iteration
{
    uint iter;
};

uint pcg(inout uint state)
{
    state = state * 747796405u + 2891336453u;

    uint word =
        ((state >> ((state >> 28u) + 4u)) ^ state)
        * 277803737u;

    return (word >> 22u) ^ word;
}

void main()
{
    uint id = gl_GlobalInvocationID.x;

    if (id >= states.length())
        return;

    uint state =
        (id   * 747796405u) ^
        (iter * 2891336453u) ^
        277803737u;

    pcg(state);

    states[id] = state;
}
)";

inline const std::string rand01Src = R"(
#version 430

layout(local_size_x = 64) in;

layout(std430, binding = 0) buffer States
{
    uint states[];
};

layout(std430, binding = 1) buffer RandomX
{
    float rX[];
};

uint pcg(inout uint state)
{
    uint oldstate = state;

    state = oldstate * 747796405u + 2891336453u;

    uint word =
        ((oldstate >> ((oldstate >> 28u) + 4u)) ^ oldstate)
        * 277803737u;

    return (word >> 22u) ^ word;
}

float random01(inout uint state)
{
    return float(pcg(state)) * (1.0 / 4294967296.0);
}

void main()
{
    uint id = gl_GlobalInvocationID.x;

    if (id >= states.length())
        return;

    uint state = states[id];

    rX[id] = random01(state);

    states[id] = state;
}
)";



class WhiteNoise {

    private: 
    Buffer states;
    Buffer rX, rY;

    Program seedProgram, rand01, rand02;

    void Rand01( Buffer &out, uint32_t size );

    public:
    WhiteNoise( uint32_t size ) : 
    
    seedProgram( seedSrc ),
    rand01( rand01Src ),
    
    states(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    rX(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(float),
        nullptr,
        GL_DYNAMIC_COPY   
    ), 
    rY(
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(float),
        nullptr,
        GL_DYNAMIC_COPY   
    ) 
    {}

    void seed( uint32_t size, Buffer &iter );
    Buffer &random1D( uint32_t size );
    Random2D random2D( uint32_t size );
};

const inline std::string sobolSrc = R"(
#version 430

layout(local_size_x = 64) in;

// layout(std430, binding = 0) buffer Vals
// {
//     uint vals[];
// };

layout(std430, binding = 0) buffer Outs
{
    float outVals[];
};

layout(std430, binding = 1) buffer Directions
{
    uint directions[];
};

layout(std140, binding = 0) uniform Dimension
{
    uint dim;
};

layout(std140, binding = 1) uniform Seed
{
    uint seed;
};

uint reverseBits32(uint x)
{
    x = ((x >> 1u)  & 0x55555555u) | ((x & 0x55555555u) << 1u);
    x = ((x >> 2u)  & 0x33333333u) | ((x & 0x33333333u) << 2u);
    x = ((x >> 4u)  & 0x0F0F0F0Fu) | ((x & 0x0F0F0F0Fu) << 4u);
    x = ((x >> 8u)  & 0x00FF00FFu) | ((x & 0x00FF00FFu) << 8u);

    return (x >> 16u) | (x << 16u);
}

uint fastOwen(uint v, uint seed)
{
    v = reverseBits32(v);

    v ^= v * 0x3d20adeau;
    v += seed;
    v *= (seed >> 16u) | 1u;
    v ^= v * 0x05526c56u;
    v ^= v * 0x53a22864u;

    return reverseBits32(v);
}

uint pcg(uint state)
{
    state = state * 747796405u + 2891336453u;

    uint word =
        ((state >> ((state >> 28u) + 4u)) ^ state)
        * 277803737u;

    return (word >> 22u) ^ word;
}
    

void main() {
    uint id = gl_GlobalInvocationID.x;

    if (id >= outVals.length())
        return;

    uint v = 0;
    uint x = id;
    for (int i = 0; x != 0; ++i, x >>= 1)
        if ((x & 1) != 0u)
            v = (v ^ directions[i + dim * 32] );

    v = fastOwen( v, pcg(id ^ seed ^ (dim * 0x9e3779b9u)));
  
    float outVal = float(v) * (1.0 / 4294967296.0);
    // outVal = (float(id) + outVal) / (outVals.length());

    outVals[id] = outVal;
}
)";

// https://web.maths.unsw.edu.au/~fkuo/sobol/joe-kuo-notes.pdf
// file from: https://web.maths.unsw.edu.au/~fkuo/sobol/
class Sobol {

    private:

    const int DIMS = 10; 
    uint32_t size;
    
    Buffer dim, data, v_k, seed;

    void rand( Buffer &inp, uint32_t size, uint32_t d, uint32_t seed );

    public:

    std::vector<uint32_t> directions;
    Program sample1D;

    Sobol( uint32_t size ) : 
    directions( DIMS * 32 ), 
    sample1D( sobolSrc ),
    size(size),
    dim(
        GL_UNIFORM_BUFFER,
        sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    seed(
        GL_UNIFORM_BUFFER,
        sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    v_k(
        GL_SHADER_STORAGE_BUFFER,
        DIMS * 32 * sizeof(uint32_t),
        nullptr,
        GL_DYNAMIC_COPY   
    ),
    data (
        GL_SHADER_STORAGE_BUFFER,
        size * sizeof(float),
        nullptr,
        GL_DYNAMIC_COPY   
    ) {

        std::ifstream dirs("../new-joe-kuo-6.txt");
        
        if (dirs.is_open()) {

            int i = 0;
            std::string line;
            while (std::getline(dirs, line)  && i < DIMS) {
                
                if( i == 0 ) {
                    
                    for (int k = 0; k < 32; ++k) {
                        directions[k + i * 32] = 1 << (31 - k);
                    }
                    
                    i ++;
                    continue;
                }

                std::regex re(R"(\d+)");
                std::vector<uint32_t> d_;

                for (std::sregex_iterator it(line.begin(), line.end(), re), end; it != end; ++it)
                    d_.push_back(std::stoul(it->str()));

                uint32_t d = d_[0];
                uint32_t s = d_[1];
                uint32_t a = d_[2];

                std::vector<uint32_t> m_i(d_.begin() + 3, d_.begin() + 3 + s);
                m_i.resize( 32 );

                for (uint32_t k = s; k < 32; ++k) {
                    uint32_t m_k = m_i[k - s] ^ (m_i[k - s] << s);

                    for (uint32_t r = 1; r < s; ++r) {
                        uint32_t a_i = (a >> (s - 1 - r)) & 1;
                        m_k ^= a_i * (m_i[k - r] << r);
                    }

                    // printf( "%u\n", m_k );
                    m_i[k] = m_k;
                }

                std::vector<uint32_t> v_k(32, 0);

                for (uint32_t k = 0; k < 32; ++k) {
                    directions[k + 32 * (d-1)] = m_i[k] * (1u << (31 - k));
                }

                this->v_k.update(
                    directions.data(),
                    directions.size() * sizeof(uint32_t)
                );

                i ++;
                // directions[d - 1] = v_k;

            }
        }
    }

    Buffer &random1D( uint32_t size, uint32_t& dim, uint32_t seed );
    Random2D random2D( uint32_t size, uint32_t& dim, uint32_t seed );
};