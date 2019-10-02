#version 330

uniform mat4 al_ModelViewMatrix;
uniform mat4 al_ProjectionMatrix;

layout (location = 0) in vec4 position;

void main() {
    gl_Position = al_ProjectionMatrix * al_ModelViewMatrix * position;
}
