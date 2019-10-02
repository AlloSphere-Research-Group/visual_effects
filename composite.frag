#version 330

uniform sampler2D tex0; // color
uniform sampler2D tex1; // ambient
uniform vec2 resolution;
uniform float do_ssao;

out vec4 frag_color;

void main()
{
  vec2 p = gl_FragCoord.xy / resolution;

  vec3 c = texture(tex0, p).rgb;
  float a = texture(tex1, p).r; // ambient
  a = mix(a, 1.0, do_ssao);

  frag_color = vec4(a * c, 1.0);
}
