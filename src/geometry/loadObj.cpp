#define TINYOBJLOADER_IMPLEMENTATION
#include <fstream>
#include <regex>
#include <iostream>
#include <vector>
#include <loadObj.hpp>
#include <filesystem>

std::string loadFile( std::string &inputfile, std::string &mtlfile ) {
    // std::string inputfile;
    // std::string mtlfile;

    std::smatch m;
    std::regex const e1{ R"(obj\s*=\s*(.+))" };
    std::regex const e2{ R"(mtl\s*=\s*(.+))" };

    std::ifstream config("../config.txt");
    if (config.is_open()) {
        std::string line;
        while (std::getline(config, line)) {
            if ( std::regex_search( line, m, e1 ) ) {
                inputfile = m[1].str();
                // break;
            } else if ( std::regex_search( line, m, e2 ) ) {
                mtlfile = m[1].str();
                // break;
            }
        }

    }
    std::cout << "OBJ file: " << inputfile << std::endl;
    std::cout << "MTL file: " << mtlfile << std::endl;

    return inputfile;
}

Mesh __LoadObj__( std::string inputfile, std::string mtlfile ) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::vector<Triangle> triangles;

    std::string warn;
    std::string err;

    std::ifstream ifs(inputfile);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open OBJ file: " << inputfile << std::endl;
        exit(1);
    }

    bool ret = tinyobj::LoadObj(
        &attrib,
        &shapes,
        &materials,
        &warn,
        &err,
        inputfile.c_str(),
        mtlfile.c_str(),
        true
    );

    if (!warn.empty()) {
        std::cout << warn << std::endl;
    }

    if (!err.empty()) {
        std::cerr << err << std::endl;
    }

    if (!ret) {
        exit(1);
    }

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++) {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

            // Loop over vertices in the face.
            std::vector<glm::vec3> verts;
            glm::vec3 normal;
            for (size_t v = 0; v < fv; v++) {
            // access to vertex
            tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

            tinyobj::real_t vx = attrib.vertices[3*size_t(idx.vertex_index)+0];
            tinyobj::real_t vy = attrib.vertices[3*size_t(idx.vertex_index)+1];
            tinyobj::real_t vz = attrib.vertices[3*size_t(idx.vertex_index)+2];

            verts.push_back( glm::vec3( vx, vy, vz ) );
            vertices.push_back( glm::vec3( vx, vy, vz ) );

            // Check if `normal_index` is zero or positive. negative = no normal data
            if (idx.normal_index >= 0) {
                tinyobj::real_t nx = attrib.normals[3*size_t(idx.normal_index)+0];
                tinyobj::real_t ny = attrib.normals[3*size_t(idx.normal_index)+1];
                tinyobj::real_t nz = attrib.normals[3*size_t(idx.normal_index)+2];

                normal = glm::vec3( nx, ny, nz );

                normals.push_back( glm::vec3( nx, ny, nz ) );
            }

            // Check if `texcoord_index` is zero or positive. negative = no texcoord data
            if (idx.texcoord_index >= 0) {
                tinyobj::real_t tx = attrib.texcoords[2*size_t(idx.texcoord_index)+0];
                tinyobj::real_t ty = attrib.texcoords[2*size_t(idx.texcoord_index)+1];

                texcoords.push_back(glm::vec2( tx, ty ));
            }
            }
            Triangle t( verts[ 0], verts[ 1], verts[ 2] );
            
            // per-face material
            int matId = shapes[s].mesh.material_ids[f];
            tinyobj::material_t m = materials[ matId ];
            // t.n=normal;
            // materials.push_back(  );
            // materials[ matId ] = m;
            // printf( "%f, %f, %f\n", m.diffuse[0], m.diffuse[1], m.diffuse[2] );
            
            // if( normal )
            // t.n = normal;

            t.matId = matId;
            // t.name = shapes[s].name;
            triangles.push_back( t );
            
            // if( t.name == "light" ) 
            //     areaLights.push_back( t );

            index_offset += fv;
        }
    }

    std::cout << "vertices: " << vertices.size() << std::endl;
    std::cout << "triangles: " << triangles.size() << std::endl;
    std::cout << "normals: " << normals.size() / 3 << std::endl;
    std::cout << "texcoords: " << texcoords.size() << std::endl;

    // Materials = std::move( materials );

    std::vector<Material> Materials;
    Materials.reserve(materials.size());

    for( auto& m : materials ) {
        Materials.emplace_back( m );
    }

    return Mesh( triangles, Materials );
}

Mesh LoadObj() {
   
    std::string inputfile;
    std::string mtlfile;

    std::smatch m;
    std::regex const e1{ R"(obj\s*=\s*(.+))" };
    std::regex const e2{ R"(mtl\s*=\s*(.+))" };

    // std::cout << "CWD: " << std::filesystem::current_path() << '\n';

    loadFile( inputfile, mtlfile );

    Mesh mesh = __LoadObj__( inputfile, mtlfile );
    
    // AABB scene;

    // for( auto& t : mesh.triangles ) {
    //     scene.grow( t.u );
    //     scene.grow( t.v );
    //     scene.grow( t.w );
    // }
    
    // for( auto& t : mesh.triangles ) {
    //     glm::vec4 center = (scene.bmin + scene.bmax) * .5f;
    //     t.u -= center;
    //     t.v -= center;
    //     t.w -= center;
    // }
    // triangles = std::move(trArr);
    return mesh;
    // triangles = tr.data();
}