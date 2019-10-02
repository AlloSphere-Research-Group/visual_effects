#version 330

// http://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/

uniform sampler2D tex0;
uniform vec2 resolution;

out vec4 frag_color;

void main()
{
  // kernel size: 9
  // 66 220 495 792 924 792 495 220 66
  const float offset[3] = float[]( 0.0, 1.3846153846, 3.2307692308 );
  const float weight[3] = float[]( 0.2270270270, 0.3162162162, 0.0702702703 );

  float dx = 1.0 / resolution.x;
  vec2 p = gl_FragCoord.xy / resolution;

  vec4 c = texture(tex0, p) * weight[0];
  for (int i = 1; i < 3; i += 1) {
    c += texture(tex0, p + vec2(dx * offset[i], 0.0)) * weight[i];
    c += texture(tex0, p - vec2(dx * offset[i], 0.0)) * weight[i];
  }

  frag_color = c;
}