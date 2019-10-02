#include "common.hpp"

std::string file_to_string(const char* path) {
    std::ifstream f(path);
    if (f.is_open()) {
        return std::string{
            std::istreambuf_iterator<char>{f},  // from
            std::istreambuf_iterator<char>{}    // to (no arg returns end iterator)
        };
    }
    else {
        std::cerr << "failed to load file: " << path << '\n';
        return "";
    }
}

void load_shader(al::ShaderProgram& p,
                 const char* vert_path,
                 const char* frag_path) {
    const std::string vert_source = file_to_string(vert_path);
    const std::string frag_source = file_to_string(frag_path);
    if (!p.compile(vert_source, frag_source)) {
        std::cerr << "shader failed to compile with "
                  << vert_path << " and "
                  << frag_path << '\n';
    }
}

void prepare_obj(al::VAOMesh& obj) {
    al::Mat4f xfm;
    for (int i = 0; i < 64; i += 1) {
        int num_added_vertices = al::addIcosahedron(obj);
        xfm.setIdentity();
        xfm.translate(al::Vec3f{al::rnd::uniformS() * 4.0f, al::rnd::uniformS() * 4.0f, 0.0f});
        xfm.rotate(M_2PI * al::rnd::uniformS(), 0, 1);
        xfm.rotate(M_2PI * al::rnd::uniformS(), 1, 2);
        xfm.rotate(M_2PI * al::rnd::uniformS(), 2, 0);
        obj.transform(xfm, obj.vertices().size() - num_added_vertices);
        float r = al::rnd::uniform();
        float g = al::rnd::uniform();
        float b = al::rnd::uniform();
        for (int k = 0; k < num_added_vertices; k += 1) {
            obj.color(r, g, b);
        }
    }
    obj.decompress();
    obj.generateNormals();
    obj.update();
}
