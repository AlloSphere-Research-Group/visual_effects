#ifndef INCLUDE_COMMON_HPP
#define INCLUDE_COMMON_HPP

#include "al/app/al_App.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/math/al_Random.hpp"

#include <string>
#include <fstream>
#include <iostream>

std::string file_to_string(const char* path);

void load_shader(al::ShaderProgram& p,
                 const char* vert_path,
                 const char* frag_path);

void prepare_obj(al::VAOMesh& obj);

#endif

