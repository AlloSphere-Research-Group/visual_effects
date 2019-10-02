#version 330

// http://developer.download.nvidia.com/assets/gamedev/files/sdk/11/FXAA_WhitePaper.pdf
// http://blog.simonrodriguez.fr/articles/30-07-2016_implementing_fxaa.html

uniform sampler2D tex0;
uniform vec2 resolution;

out vec4 frag_color;

const float FXAA_EDGE_THRESHOLD_MIN = 0.0625;
const float FXAA_EDGE_THRESHOLD = 0.125;
const float FXAA_SUBPIXEL_QUALITY = 1.0;
const int FXAA_SEARCH_STEPS = 5;
const float FXAA_SEARCH_ACCELERATION = 2.0;
const float FXAA_SEARCH_THRESHOLD = 0.0625;

// const float strides[7] = float[](1.0, 1.0, 2.0, 3.0, 5.0, 8.0, 13.0);
// const float strides[7] = float[](1.0, 2.0, 4.0, 8.0, 16.0, 24.0, 32.0);
// const float strides[7] = float[](1.0, 1.5, 2.5, 4.0, 6.0, 8.5, 11.5);
const float strides[7] = float[](1.0, 1.5, 2.0, 2.0, 2.0, 3.0, 8.0);

float luma(vec3 color) {
  return dot(color, vec3(0.299, 0.587, 0.114));
}

vec3 fxaa() {
  float dx = 1.0 / resolution.x;
  float dy = 1.0 / resolution.y;
  vec2 p = gl_FragCoord.xy / resolution;

  vec3 c = texture(tex0, p + vec2(0.0, 0.0)).rgb;
  vec3 n = texture(tex0, p + vec2(0.0,  dy)).rgb;
  vec3 e = texture(tex0, p + vec2(+dx, 0.0)).rgb;
  vec3 w = texture(tex0, p + vec2(-dx, 0.0)).rgb;
  vec3 s = texture(tex0, p + vec2(0.0, -dy)).rgb;

  float luma_c = luma(c);
  float luma_n = luma(n);
  float luma_e = luma(e);
  float luma_w = luma(w);
  float luma_s = luma(s);

  float range_min = min(luma_c, min(min(luma_n, luma_w), min(luma_s, luma_e)));
  float range_max = max(luma_c, max(max(luma_n, luma_w), max(luma_s, luma_e)));
  float range = range_max - range_min;

  // local contrast check
  if(range < max(FXAA_EDGE_THRESHOLD_MIN, range_max * FXAA_EDGE_THRESHOLD)) {
    return c; // NOT EDGE!
  }

  vec3 nw = texture(tex0, p + vec2(-dx,  dy)).rgb;
  vec3 ne = texture(tex0, p + vec2( dx,  dy)).rgb;
  vec3 sw = texture(tex0, p + vec2(-dx, -dy)).rgb;
  vec3 se = texture(tex0, p + vec2( dx, -dy)).rgb;

  float luma_nw = luma(nw);
  float luma_ne = luma(ne);
  float luma_sw = luma(sw);
  float luma_se = luma(se);

  // vertical/horizontal edge test
  float edge_vert = abs((0.25 * luma_nw) + (-0.5 * luma_n) + (0.25 * luma_ne))
                  + abs((0.50 * luma_w) + (-1.0 * luma_c) + (0.50 * luma_e))
                  + abs((0.25 * luma_sw) + (-0.5 * luma_s) + (0.25 * luma_se));
  float edge_horz = abs((0.25 * luma_nw) + (-0.5 * luma_w) + (0.25 * luma_sw))
                  + abs((0.50 * luma_n) + (-1.0 * luma_c) + (0.50 * luma_s))
                  + abs((0.25 * luma_ne) + (-0.5 * luma_e) + (0.25 * luma_se));
  bool horz_span = edge_horz > edge_vert;

  // edge orientation, north and east is positive direction
  float luma_negat = horz_span? luma_s : luma_w;
  float luma_posit = horz_span? luma_n : luma_e;

  float gradient_negat = luma_negat - luma_c;
  float gradient_posit = luma_posit - luma_c;

  bool is_negat_steeper = abs(gradient_negat) >= abs(gradient_posit);

  // Gradient in the corresponding direction, normalized
  float gradient_scaled = max(abs(gradient_negat), abs(gradient_posit));
  gradient_scaled *= FXAA_SEARCH_THRESHOLD; // scaling for search of end of edge

  float one_step_length = horz_span? dy : dx;

  float luma_local = 0.0;
  if (is_negat_steeper) {
    one_step_length *= -1.0; // south or west, negative dir
    luma_local = 0.5 * (luma_negat + luma_c); // border of steeper side
  }
  else {
    luma_local = 0.5 * (luma_posit + luma_c);
  }

  // offset to edge orientation by half pixel
  vec2 p_offset = p;
  if (horz_span) {
    p_offset.y += one_step_length * 0.5;
  }
  else {
    p_offset.x += one_step_length * 0.5;
  }

  // end of edge search, along the found edge
  vec2 step = horz_span? vec2(dx, 0.0) : vec2(0.0, dy);
  vec2 p_posit = p_offset + step;
  vec2 p_negat = p_offset - step;

  float luma_end_posit = luma(texture(tex0, p_posit).rgb);
  float luma_end_negat = luma(texture(tex0, p_negat).rgb);
  // difference between starting point and spanned position,
  // compare with gradient at original position
  luma_end_posit -= luma_local;
  luma_end_negat -= luma_local;

  bool reached_posit = abs(luma_end_posit) >= gradient_scaled;
  bool reached_negat = abs(luma_end_negat) >= gradient_scaled;
  bool reached_both = reached_posit && reached_negat;

  // more iterations
  if (!reached_both) {

    p_posit += step * FXAA_SEARCH_ACCELERATION;
    p_negat -= step * FXAA_SEARCH_ACCELERATION;

    for (int i = 1; i < FXAA_SEARCH_STEPS; i++) {
      // If needed, read luma in 1st direction, compute delta.
      if (!reached_posit) {
          luma_end_posit = luma(texture(tex0, p_posit).rgb);
          luma_end_posit = luma_end_posit - luma_local;
      }
      // If needed, read luma in opposite direction, compute delta.
      if(!reached_negat){
          luma_end_negat = luma(texture(tex0, p_negat).rgb);
          luma_end_negat = luma_end_negat - luma_local;
      }
      // If the luma deltas at the current extremities is larger
      // than the local gradient, we have reached the side of the edge.
      reached_posit = abs(luma_end_posit) >= gradient_scaled;
      reached_negat = abs(luma_end_negat) >= gradient_scaled;
      reached_both = reached_posit && reached_negat;

      // If the side is not reached, we continue to explore in this direction
      if (!reached_posit) {
          p_posit += step * strides[i] * FXAA_SEARCH_ACCELERATION;
      }
      if (!reached_negat) {
          p_negat -= step * strides[i] * FXAA_SEARCH_ACCELERATION;
      }

      // If both sides have been reached, stop the exploration.
      if (reached_both) {
        break;
      }
    }
  }

  float dist_posit = horz_span? (p_posit.x - p.x) : (p_posit.y - p.y);
  float dist_negat = horz_span? (p.x - p_negat.x) : (p.y - p_negat.y);
  bool is_posit_edge_closer = dist_posit < dist_negat;
  float dist_closer_end_of_edge = min(dist_posit, dist_negat);
  float length_of_edge = dist_posit + dist_negat;
  float offset_edge = - dist_closer_end_of_edge / length_of_edge + 0.5;
  // if totla length of edge is short and the pixel is closer to edge
  // give less offset (the first term is always smaller than 0.5)

  // Is the luma at center smaller than the local average ?
  bool is_luma_center_smaller = luma_c < luma_local;

  // If the luma at center is smaller than at its neighbour,
  // the delta luma at each end should be positive (same variation).
  // (in the direction of the closer side of the edge.)
  bool correct_variation =
    ((is_posit_edge_closer ? luma_end_posit : luma_end_negat) < 0.0)
    !=
    is_luma_center_smaller;

  // If the luma variation is incorrect, do not offset.
  float final_offset = correct_variation ? offset_edge : 0.0;

  // sub-pixel aliasing test, this helps with edges near 45 degrees
  // lowpass luma, pixel contrast
  float luma_l = 2.0 * (luma_n + luma_e + luma_w + luma_s);
  luma_l += (luma_nw + luma_ne + luma_sw + luma_se);
  luma_l *= (1.0 / 12.0);
  float range_l = abs(luma_l - luma_c);
  // range_l / range is ratio of pixel contrast to local contrast
  //  >> used to detect sub-pixel aliasing
  //     approaches 1.0 in the presence of single pixel dots
  //     and fall off towards 0.0 as more pixels contribute to an edge
  float subpix_0 = smoothstep(0.0, 1.0, range_l / range);
  float subpix_1 = subpix_0 * subpix_0;
  float subpix_offset = subpix_1 * FXAA_SUBPIXEL_QUALITY;

  // Pick the biggest of the two offsets.
  final_offset = max(final_offset, subpix_offset);

  vec2 p_final = p;
  if (horz_span) {
      p_final.y += final_offset * one_step_length;
  } else {
      p_final.x += final_offset * one_step_length;
  }

  // Read the color at the new UV coordinates, and use it.
  return texture(tex0, p_final).rgb;
}

void main()
{
  vec3 aa = fxaa();
  frag_color = vec4(aa, 1.0);
}