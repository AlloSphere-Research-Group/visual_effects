#version 330

in vec3 normal_world;
in vec3 pos_world;
in vec4 _color;

layout (location = 0) out vec4 frag_normal;
layout (location = 1) out vec4 frag_pos_world;
layout (location = 2) out vec4 frag_color;

void main() {
	vec3 nw = normalize(normal_world);
  
    frag_normal = vec4(nw, 1.0);
    frag_pos_world = vec4(pos_world, 1.0);
    frag_color= _color;
}
