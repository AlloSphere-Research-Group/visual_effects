#version 330

// http://john-chapman-graphics.blogspot.com/2013/01/ssao-tutorial.html

uniform sampler2D tex0;  // normal
uniform sampler2D tex1;  // depth
uniform sampler2D tex2;  // world pos
uniform vec2 resolution;
uniform mat4 view;
uniform mat4 proj;
uniform mat4 inv_view_proj;
uniform vec4 nfr;  // near far radius dummy

out vec4 frag_color;

float linearize_depth(in float z, in float near, in float far) {
  return (2.0 * near) / (far + near - z * (far - near));
}

int argmax(vec3 vec) {
  float crt_max = 0.0;
  int result = 0;
  float value;
  for (int i = 0; i < 3; ++i) {
    value = abs(vec[i]);
    if (value <= crt_max) continue;
    result = i;
    crt_max = value;
  }
  return result;
}

vec3 ssao() {
  float dx = 1.0 / resolution.x;
  float dy = 1.0 / resolution.y;
  vec2 p = gl_FragCoord.xy / resolution;
  int xi = int(gl_FragCoord.x) % 4;
  int yj = int(gl_FragCoord.y) % 4;

  // sampling directions
  // normalized, also guarantees at least PI/32 away from surface to prevent
  // self occlusion and PI/2 - PI/16 from normal to prevent obvious case
  const vec3 samples[16] = vec3[](
      vec3(0.90928984, 0.3195607, 0.26659486),
      vec3(0.8296897, -0.48563117, 0.27527693),
      vec3(-0.23620264, 0.8971226, 0.3733354),
      vec3(0.52471244, -0.7945292, 0.3056143),
      vec3(0.93028086, -0.19524637, 0.31057426),
      vec3(-0.89827156, 0.047160674, 0.4369028),
      vec3(0.6474386, 0.7105619, 0.27554518),
      vec3(-0.9123342, 0.2962049, 0.28268167),
      vec3(0.41934106, -0.86728334, 0.2682772),
      vec3(0.7425517, -0.60192275, 0.29377905),
      vec3(-0.3297949, 0.90552366, 0.26694983),
      vec3(0.057921845, -0.9252314, 0.37495598),
      vec3(-0.8746783, -0.39617, 0.2792619),
      vec3(-0.36253077, -0.8933767, 0.26542345),
      vec3(0.57198554, -0.7730481, 0.2742793),
      vec3(-0.7030947, 0.6535494, 0.28023383));

  const vec2 rotations[16] = vec2[](
    vec2(-0.647136, -0.762375),
    vec2(-0.555463, 0.831541),
    vec2(-0.455758, 0.890104),
    vec2(0.274666, -0.96154),
    vec2(0.750661, -0.660688),
    vec2(0.685684, 0.7279),
    vec2(-0.181961, 0.983306),
    vec2(0.0147136, 0.999892),
    vec2(0.98914, 0.146977),
    vec2(-0.363663, -0.931531),
    vec2(-0.570796, 0.821092),
    vec2(0.732629, 0.680629),
    vec2(0.0369329, -0.999318),
    vec2(0.832579, -0.553906),
    vec2(0.869843, 0.493329),
    vec2(0.737619, -0.675217));

  const int N = 12;
  int rand_idx = xi + 4 * yj;
  vec2 rand_vec = rotations[rand_idx];

  vec3 p_world = texture(tex2, p).rgb;
  vec3 normal_p_world = texture(tex0, p).rgb;
  vec3 rand_dir_for_tangent = normalize(vec3(rand_vec, 1.0));
  float r_n = dot(rand_dir_for_tangent, normal_p_world);
  if (r_n > 0.999) {
    rand_dir_for_tangent = normalize(vec3(rotations[(rand_idx + 1) % 16], 1.0));
    r_n = dot(rand_dir_for_tangent, normal_p_world);
  }
  if (r_n > 0.999) {
    return vec3(1.0, 0.0, 0.0);
  }

  vec3 tangent_p_world = normalize(rand_dir_for_tangent - normal_p_world * r_n);
  vec3 bitangent_p_world = cross(normal_p_world, tangent_p_world);
  mat3 local_p_to_world_rot = mat3(tangent_p_world, bitangent_p_world, normal_p_world);

  float R = nfr.z;
  float light = 0.0;
  float Nf = float(N);
  for (int i = 0; i < N; i += 1) {
    float r = float(i + 1) / Nf;
    r *= r;
    vec3 sample_local = r * R * samples[(rand_idx + i) % 16];
    vec3 sample_world = local_p_to_world_rot * sample_local + p_world;
    vec4 sp = proj * view * vec4(sample_world, 1.0);
    vec3 sample_proj = sp.xyz / sp.w;
    float linear_sample_depth =
        linearize_depth(0.5 * sample_proj.z + 0.5, nfr.x, nfr.y);

    vec2 sample_tc = 0.5 * sample_proj.xy + 0.5;
    float rendered_depth = texture(tex1, sample_tc).r;
    float linear_rendered_depth =
        linearize_depth(rendered_depth, nfr.x, nfr.y);
    vec4 rw =
        inv_view_proj * vec4(sample_proj.xy, 2.0 * rendered_depth - 1.0, 1.0);
    vec3 rendered_world = rw.rgb / rw.w;
    vec3 d = rendered_world - p_world;
    float dist_to_rendered = length(d);

    vec3 rendered_normal_world = texture(tex0, sample_tc).rgb;
    float n_dot_d = dot(rendered_normal_world, normalize(d));
    n_dot_d = max(0.0, n_dot_d);
    n_dot_d = 1.0 - n_dot_d;

    if (dist_to_rendered < R && linear_sample_depth > linear_rendered_depth) {
      // indirect light
      light += (dist_to_rendered / R) * n_dot_d;
    }
    else {
      // direct light
      light += 1.0;
    }
  }
  light /= Nf;
  return vec3(light);
}

void main() {
  vec3 aa = ssao();
  frag_color = vec4(aa, 1.0);
}
