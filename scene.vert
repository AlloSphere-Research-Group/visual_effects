#version 330

uniform mat4 al_ModelViewMatrix;
uniform mat4 al_ProjectionMatrix;
uniform mat4 al_ModelMatrix;
uniform mat4 al_NormalMatrix;

layout (location = 0) in vec4 position;
layout (location = 1) in vec4 color;
layout (location = 3) in vec3 normal;

out vec3 normal_world;
out vec3 pos_world;
out vec4 _color;

void main() {
    vec4 vert_eye = al_ModelViewMatrix * position;
    gl_Position = al_ProjectionMatrix * vert_eye;
    normal_world = (al_NormalMatrix * vec4(normalize(normal), 0.0)).xyz;
    pos_world = (al_ModelMatrix * position).rgb;
    _color = color;
}
