#version 330

uniform mat4 MV;
uniform mat4 P;

layout (location = 0) in vec3 position;

void main() {
    vec4 vert_eye = MV * vec4(position, 1.0);
    gl_Position = P * vert_eye;
}