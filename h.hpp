#ifndef INCLUDE_H_HPP
#define INCLUDE_H_HPP

#include "al/core.hpp"
#include "al/util/al_Image.hpp"

#include "FreeImage.h"
#define LIB_FREEIMAGE_INCLUDED

#include <algorithm>
#include <cmath> // std::nextafter
#include <cstdlib>  // std::exit std::size_t
#include <fstream>
#include <iostream>
#include <string>
#include <utility> // std::swap
#include <complex>
#include <valarray>
#include <cfloat> // FLT_MAX

typedef std::complex<double> Complex;
typedef std::valarray<Complex> CArray;

template <typename T, typename T2, typename T3>
inline T clamp(T val, T2 min, T3 max) {
  return std::min(static_cast<T>(max), std::max(static_cast<T>(min), val));
}

template <typename T>
inline T clamp(T val) {
  return std::min(static_cast<T>(1), std::max(static_cast<T>(0), val));
}

inline float smootherstep(float t) {
    return ((6.0f * t - 15.0f) * t + 10.0f) * t * t * t;
}

inline std::string file_to_string(std::string path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    std::cout << "couldn't open " << path << std::endl;
    return "";
  } else
    return std::string{
        std::istreambuf_iterator<char>{f},  // from
        std::istreambuf_iterator<char>{}    // to (no arg returns end iterator)
    };
}

inline void load_image(al::Texture& tex, std::string const& path) {
  al::Image image;
  if (!image.load(path)) {
    std::cout << "Failed to read image " << path << std::endl;
    exit(EXIT_FAILURE);
  }
  image.sendToTexture(tex);
}

inline void load_shader(al::ShaderProgram& p, std::string const& vert_path,
                        std::string const& frag_path) {
  const auto vert_source = file_to_string(vert_path);
  const auto frag_source = file_to_string(frag_path);
  if (!p.compile(vert_source, frag_source)) {
    cout << "shader failed to compile with " << vert_path << " and "
         << frag_path << endl;
    exit(EXIT_FAILURE);
  }
}

inline void load_shader(al::ShaderProgram& p, std::string const& name) {
  load_shader(p, "data/" + name + ".vert", "data/" + name + ".frag");
}

// fill some defautl uniforms
inline void load_shader(al::ShaderProgram &s, std::string const& name,
                         float w, float h)
{
  load_shader(s, name);
  s.begin();
  for (int i = 0; i < 5; i += 1) {
    std::string texi_str = "tex" + to_string(i);
    int texi = glGetUniformLocation(s.id(), texi_str.c_str());
    if (texi != -1) s.uniform(texi, i);
  }
  int resolution_loc = glGetUniformLocation(s.id(), "resolution");
  if (resolution_loc != -1) s.uniform(resolution_loc, w, h);
  s.end();
}

inline void print_mat(al::Mat4f const& mat) {
  std::cout << mat[0] << ", " << mat[4] << ", " << mat[8] << ", " << mat[12]
            << '\n'
            << mat[1] << ", " << mat[5] << ", " << mat[9] << ", " << mat[13]
            << '\n'
            << mat[2] << ", " << mat[6] << ", " << mat[10] << ", " << mat[14]
            << '\n'
            << mat[3] << ", " << mat[7] << ", " << mat[11] << ", " << mat[15]
            << std::endl;
}

struct sampler {
  unsigned int _id = 0;

  unsigned int id() const { return _id; }
  bool created() const { return glIsSampler(_id); }

  void create() {
    if (created()) destroy();
    glGenSamplers(1, &_id);
  }

  void destroy() {
    glDeleteSamplers(1, &_id);
    _id = 0;
  }

  void bind(unsigned int binding_point) const {
    glBindSampler(binding_point, _id);
  }

  void unbind(unsigned int binding_point) const {
    glBindSampler(binding_point, 0);
  }

  void min_filter(int param) {
    glSamplerParameteri(_id, GL_TEXTURE_MIN_FILTER, param);
  }
  void mag_filter(int param) {
    glSamplerParameteri(_id, GL_TEXTURE_MAG_FILTER, param);
  }
  void wrap_s(int param) { glSamplerParameteri(_id, GL_TEXTURE_WRAP_S, param); }
  void wrap_t(int param) { glSamplerParameteri(_id, GL_TEXTURE_WRAP_T, param); }
  void wrap_r(int param) { glSamplerParameteri(_id, GL_TEXTURE_WRAP_R, param); }

  sampler() = default;
  sampler(const sampler&) = delete;
  sampler& operator=(const sampler&) = delete;
  sampler(sampler&& other) noexcept : _id(other._id) {
    other._id = 0;  // so other's dtor does not delete gl object
  }
  sampler& operator=(sampler&& other) noexcept {
    if (this != &other) {  // checking self assignment
      destroy();           // delete current gl object
      _id = other._id;     // obtain given gl object
      other._id = 0;       // prevent other's dtor deleting the gl object
    }
    return *this;
  }
  ~sampler() { destroy(); }
};

struct timer
{
  double t0_ms = 0;
  double t1_ms = 0;
  double avg = 0;
  void begin() { t0_ms = double(al::al_steady_time_nsec()); }
  void end() {
    t1_ms = double(al::al_steady_time_nsec());
    double dt = t1_ms - t0_ms;
    avg += 0.05 * (dt - avg);
  }
};


void add_tri_prism(al::Mesh &m, float r, float h, al::Vec3f const &pos,
                          float start_angle)
{
  /*  F   [!] m should be primitive:TRIANGLES
     /|\
    D-+-E
    | C |
    |/ \|
    A---B */
  const float x = pos.x;
  const float y = pos.y;
  const float z = pos.z;
  const float a = 2 * M_PI / 3;
  const float a0 = start_angle;
  const al::Vec3f A {r * std::sin(0*a + a0) + x, y - h / 2, r * std::cos(0*a + a0) + z};
  const al::Vec3f B {r * std::sin(1*a + a0) + x, y - h / 2, r * std::cos(1*a + a0) + z};
  const al::Vec3f C {r * std::sin(2*a + a0) + x, y - h / 2, r * std::cos(2*a + a0) + z};
  const al::Vec3f D {r * std::sin(0*a + a0) + x, y + h / 2, r * std::cos(0*a + a0) + z};
  const al::Vec3f E {r * std::sin(1*a + a0) + x, y + h / 2, r * std::cos(1*a + a0) + z};
  const al::Vec3f F {r * std::sin(2*a + a0) + x, y + h / 2, r * std::cos(2*a + a0) + z};

  const auto tri = [&](al::Vec3f const &p0, al::Vec3f const &p1,
                       al::Vec3f const &p2)
  {
    m.vertex(p0); m.vertex(p1); m.vertex(p2);
    auto n = al::cross(p1 - p0, p2 - p1).normalize();
    m.normal(n); m.normal(n); m.normal(n);
  };

  tri(A, C, B); // bottom
  tri(A, B, D); tri(D, B, E); // side ABED
  tri(B, C, E); tri(E, C, F); // side BCFE
  tri(C, A, F); tri(F, A, D); // side CADF
  tri(D, E, F); // top
}

struct color_t {
  color_t(): r(0), g(0), b(0), a(255) {}
  color_t(uint8_t R, uint8_t G, uint8_t B, uint8_t A=255)
  : r(R), g(G), b(B), a(A) {}
  uint8_t r, g, b, a;
};

struct colorf_t {
  colorf_t(): r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}
  colorf_t(float R, float G, float B, float A=1.0f)
  : r(R), g(G), b(B), a(A) {}
  float r, g, b, a;
};

inline float mix(float val0, float val1, float ratio) {
    return (1 - ratio) * val0 + ratio * val1;
}

al::Vec3f rand_on_sphere(float radius = 1) {
  float theta = std::acosf(1 - 2 * al::rnd::uniform()); // rnd::uniform returns [0:1]
  float phi = M_2PI * al::rnd::uniform();
  float x = radius * std::cosf(phi) * std::sinf(theta);
  float y = radius * std::sinf(phi) * std::sinf(theta);
  float z = radius * std::cosf(theta);
  return al::Vec3f{ x, y, z };
}

struct image_data_t
{
  int width;
  int height;
  std::vector<color_t> _data;

  color_t& operator[](size_t index) { return _data[index]; }
  const color_t& operator[](size_t index) const { return _data[index]; }
  color_t& operator()(int i, int j) { return _data[i + width * j]; }
  const color_t& operator()(int i, int j) const { return _data[i + width * j]; }
  colorf_t sample(float x_normalized, float y_normalized) const
  {
    // wrap to [0:1]
    float x = x_normalized - std::floor(std::nextafter(x_normalized, 0.0f));
    float y = y_normalized - std::floor(std::nextafter(y_normalized, 0.0f));

    x = x * (width - 1);
    y = y * (height - 1);
    int i0 = std::floor(x);
    int j0 = std::floor(y);
    float di = x - i0;
    float dj = y - j0;
    int i1 = (i0 >= width - 1)? i0 : i0 + 1;
    int j1 = (j0 >= height - 1)? j0 : j0 + 1;
    const color_t& c00 = (*this)(i0, j0);
    const color_t& c01 = (*this)(i0, j1);
    const color_t& c10 = (*this)(i1, j0);
    const color_t& c11 = (*this)(i1, j1);
    return colorf_t {
      mix(mix(c00.r / 255.0f, c10.r / 255.0f, di),
          mix(c01.r / 255.0f, c11.r / 255.0f, di),
          dj),
      mix(mix(c00.g / 255.0f, c10.g / 255.0f, di),
          mix(c01.g / 255.0f, c11.g / 255.0f, di),
          dj),
      mix(mix(c00.b / 255.0f, c10.b / 255.0f, di),
          mix(c01.b / 255.0f, c11.b / 255.0f, di),
          dj)
    };
  }
  color_t* data() { return _data.data(); }
  void resize(int i, int j) {
    width = i;
    height = j;
    _data.resize(i * j);
  }
  void swap_rb() {
    for (int i = 0; i < width * height; i += 1) {
      std::swap(_data[i].r, _data[i].b);
    }
  }
};

struct FreeImage_initializer_t {
  FreeImage_initializer_t() { FreeImage_Initialise(); }
  ~FreeImage_initializer_t() { FreeImage_DeInitialise(); }
  static FreeImage_initializer_t* check() {
    static FreeImage_initializer_t i;
    return &i;
  }
};

inline bool load_image(std::string fname, image_data_t &arr) {
  // make sure freeimage lib is initialized
  static FreeImage_initializer_t* fi = nullptr;
  fi = FreeImage_initializer_t::check();
  if (fi == nullptr) return false;

  /*
      check image format and load
  */
  FREE_IMAGE_FORMAT img_format = FreeImage_GetFIFFromFilename(fname.c_str());
  if (img_format == FIF_UNKNOWN) {
    std::cout << "format not recognized: " << fname << std::endl;
    return false;
  }
  if (!FreeImage_FIFSupportsReading(img_format)) {
    std::cout << "format not supported by FreeImage: " << fname << std::endl;
    return false;
  }
  FIBITMAP *bitmap = FreeImage_Load(img_format, fname.c_str(), 0);
  if (bitmap == nullptr) {
    std::cout << "failed to load: " << fname << std::endl;
    return false;
  }

  /*
     convert to RGBA32 bitmap
  */
  FREE_IMAGE_COLOR_TYPE color_type = FreeImage_GetColorType(bitmap);
  unsigned int bits_per_pixel = FreeImage_GetBPP(bitmap);
  FREE_IMAGE_TYPE data_type = FreeImage_GetImageType(bitmap);
  if (color_type == FIC_CMYK) {
    std::cout << "CMYK currently not supported" << std::endl;
    return false;
  }
  if (data_type != FIT_BITMAP) {
    std::cout << "non bitmap image not supported: " << fname << endl;
    return false;
  }
  if (bits_per_pixel != 32 || color_type != FIC_RGBALPHA) {
    std::cout << "converting " << fname << " from " << bits_per_pixel << "bits "
              << "type " << color_type << " image to 32bit RGBA" << std::endl;
    std::cout << "(FIC_MINISWHITE: " << FIC_MINISWHITE
              << ", FIC_MINISBLACK: " << FIC_MINISBLACK
              << ", FIC_RGB: " << FIC_RGB
              << ", FIC_PALETTE: " << FIC_PALETTE << ')' << std::endl;
    FIBITMAP *converted = FreeImage_ConvertTo32Bits(bitmap);
    FreeImage_Unload(bitmap);
    bitmap = converted;
  }

  /*
     set dimensions to output array and copy data. 'rgba vs bgra' is not dealt
  */
  const int w = FreeImage_GetWidth(bitmap);
  const int h = FreeImage_GetHeight(bitmap);
  arr.width = w;
  arr.height = h;
  arr._data.resize(w * h);
  uint8_t *bitmap_data = FreeImage_GetBits(bitmap);
  std::memcpy(arr.data(), bitmap_data, w * h * 4 * sizeof(uint8_t));

  FreeImage_Unload(bitmap); // cleanup
  return true;
};

// https://rosettacode.org/wiki/Fast_Fourier_transform#C.2B.2B
// Cooley-Tukey FFT (in-place, breadth-first, decimation-in-frequency)
// Better optimized but less intuitive
// !!! Warning : in some cases this code make result different from not optimased version above (need to fix bug)
// The bug is now fixed @2017/05/30 
inline void fft(CArray &x)
{
  // DFT
  unsigned int N = x.size(), k = N, n;
  double thetaT = 3.14159265358979323846264338328L / N;
  Complex phiT = Complex(cos(thetaT), -sin(thetaT)), T;
  while (k > 1)
  {
    n = k;
    k >>= 1; // move in power of two
    phiT = phiT * phiT; // rotate
    T = 1.0L;
    for (unsigned int l = 0; l < k; l++)
    {
      for (unsigned int a = l; a < N; a += n)
      {
        unsigned int b = a + k;
        Complex t = x[a] - x[b];
        x[a] += x[b];
        x[b] = t * T;
      }
      T *= phiT;
    }
  }
  // Decimate
  unsigned int m = (unsigned int)log2(N);
  for (unsigned int a = 0; a < N; a++)
  {
    unsigned int b = a;
    // Reverse bits
    b = (((b & 0xaaaaaaaa) >> 1) | ((b & 0x55555555) << 1));
    b = (((b & 0xcccccccc) >> 2) | ((b & 0x33333333) << 2));
    b = (((b & 0xf0f0f0f0) >> 4) | ((b & 0x0f0f0f0f) << 4));
    b = (((b & 0xff00ff00) >> 8) | ((b & 0x00ff00ff) << 8));
    b = ((b >> 16) | (b << 16)) >> (32 - m);
    if (b > a)
    {
      Complex t = x[a];
      x[a] = x[b];
      x[b] = t;
    }
  }
  //// Normalize (This section make it not working correctly)
  //Complex f = 1.0 / sqrt(N);
  //for (unsigned int i = 0; i < N; i++)
  //  x[i] *= f;
}

#endif