#include "h.hpp"

#include "FastNoise/FastNoise.cpp"
#include <cstring> // memcpy
#include <cstdint> // uint8_t

// #define RATIO_SCALER 1 // 840 * 360
#define RATIO_SCALER 2 // 1680 * 720
// #define RATIO_SCALER 3 // 2520 * 1080
// #define RATIO_SCALER 4 // 3360 * 1440

using namespace al;
using namespace std;

struct work_o : App {
  const int w = 21 * 8 * 5 * RATIO_SCALER;
  const int h = 9 * 8 * 5 * RATIO_SCALER;
  const float aspect_ratio = 21.0 / 9.0;

  VAOMesh obj {Mesh::TRIANGLES};
  VAOMesh quad {Mesh::TRIANGLE_STRIP};
  VAOMesh fps_indicator;

  ShaderProgram scene_shader, edge_shader, ssao_shader, ssao_blur_ver_shader,
                ssao_blur_hor_shader, composite_shader, fxaa_shader;

  EasyFBO scene_data, edge, ssao_blur, ssao_blur_temp, ssao, raw, filtered;
  Texture tex_pos_world, tex_obj_color;
  sampler linear_sampler;

  FastNoise myNoise;
  bool show_fps = false;

  void onCreate() override;
  void onDraw(Graphics &g) override;
  void onKeyDown(Keyboard const &k) override;
};

void work_o::onCreate() {
  nav().pos(0, 0, 10);
  nav().faceToward({0, 0, 0}, {0, 1, 0});
  nav().setHome();
  lens().fovy(24);
  lens().near(1);
  lens().far(1000);

  rnd::global().seed(17);
  rnd::global_normal().seed(21);

  myNoise.SetSeed(1337);      // default 1337
  myNoise.SetFrequency(0.1); // default 0.01
  myNoise.SetNoiseType(FastNoise::SimplexFractal);
  myNoise.SetFractalOctaves(3);    // default 3
  myNoise.SetFractalLacunarity(2); // default 2
  myNoise.SetFractalGain(0.5);       // default 0.5

  image_data_t img;
  if (!load_image("data/textures/background.png", img)) exit(EXIT_FAILURE);
  cout << img.width << ", " << img.height << endl;

  // obj to render
  {
    Mat4f xfm;
    int idx;
    // const float iw = image.width();
    // const float ih = image.height();
    const float iw = img.width;
    const float ih = img.height;
    for (int i = 0; i < iw; i += 5) {
      for (int j = 0; j < ih; j += 5) {
        int num_added_vertices = addIcosahedron(obj, 2.0);
        idx = i + j * iw;
        xfm.setIdentity();
        xfm.translate(Vec3f{(i - iw/2)/10, img[idx].b/20.0, (-j-ih/2)/10});
        xfm.rotate(M_2PI * rnd::uniformS(), 0, 1);
        xfm.rotate(M_2PI * rnd::uniformS(), 1, 2);
        xfm.rotate(M_2PI * rnd::uniformS(), 2, 0);
        obj.transform(xfm, obj.vertices().size() - num_added_vertices);
        for (int k = 0; k < num_added_vertices; k += 1) {
          obj.color(img[idx].b/255.0, img[idx].g/255.0, img[idx].r/255.0);
        }
      }
    }
    obj.decompress();
    obj.generateNormals();
    obj.update();
  }

  addTexQuad(quad);
  quad.update();

  addRect(fps_indicator, 10, 10, 30, 30);
  fps_indicator.update();

  load_shader(scene_shader, "o/scene_data_o", w, h);
  load_shader(edge_shader, "o/edge_03", w, h);
  load_shader(ssao_shader, "n/ssao_02", w/2, h/2);
  load_shader(ssao_blur_ver_shader, "blur_ver", w/2, h/2);
  load_shader(ssao_blur_hor_shader, "blur_hor", w/2, h/2);
  load_shader(composite_shader, "o/composite_o", w, h);
  load_shader(fxaa_shader, "n/fxaa_02", w, h);

  edge_shader.begin();
  edge_shader.uniform("edge_threshold", 0.0625);
  edge_shader.end();

  // nfr: near, far, min and max radius of ssao
  ssao_shader.begin();
  ssao_shader.uniform("nfr", lens().near(), lens().far(), 2.0, 0.0);
  ssao_shader.end();

  // scene data fbo
  {
    EasyFBOSetting float_setting;
    float_setting.internal = GL_RGBA32F;
    float_setting.depth_format = GL_DEPTH_COMPONENT32F;
    float_setting.use_depth_texture = true;
    scene_data.init(w, h, float_setting);

    tex_pos_world.create2D(w, h, GL_RGBA32F, GL_RGBA, GL_FLOAT);
    tex_obj_color.create2D(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    auto &fbo = scene_data.fbo();
    fbo.bind();
    fbo.attachTexture2D(tex_pos_world, GL_COLOR_ATTACHMENT1);
    fbo.attachTexture2D(tex_obj_color, GL_COLOR_ATTACHMENT2);
    unsigned int scene_buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, scene_buffers);
    fbo.unbind();
  }

  // effect fbos
  {
    EasyFBOSetting fbo_setting;
    fbo_setting.internal = GL_RGBA8;
    fbo_setting.depth_format = GL_DEPTH_COMPONENT24;
    edge.init(w, h, fbo_setting);
    ssao.init(w / 2, h / 2, fbo_setting);
    ssao_blur.init(w / 2, h / 2, fbo_setting);
    ssao_blur_temp.init(w / 2, h / 2, fbo_setting);
    raw.init(w, h, fbo_setting);
    filtered.init(w, h, fbo_setting);
  }

  linear_sampler.create();
  linear_sampler.min_filter(GL_LINEAR);
  linear_sampler.mag_filter(GL_LINEAR);
  linear_sampler.wrap_s(GL_CLAMP_TO_EDGE);
  linear_sampler.wrap_t(GL_CLAMP_TO_EDGE);

  linear_sampler.bind(0);
  linear_sampler.bind(1);
  linear_sampler.bind(2);
  linear_sampler.bind(3);
}

void work_o::onDraw(Graphics &g) {
  g.cullFace(true);

  // save the matrices used to render the scene
  Mat4f view_matrix;
  Mat4f proj_matrix;

  // prepare scene data textures
  {
    g.framebuffer(scene_data);
    g.viewport(0, 0, w, h);
    g.pushCamera(view()); // need to reset cam since viewport changed
    view_matrix = g.viewMatrix();
    proj_matrix = g.projMatrix(); // one used for w, h, viewport

    g.blending(false);
    g.depthTesting(true);

    g.clearColorBuffer(0, 0, 0, 0, 0); // r, g, b, a, index, normal
    g.clearColorBuffer(0, 0, 0, 0, 1); // pos world
    g.clearColorBuffer(0, 0, 0, 0, 2); // bloom
    g.clearDepth(1);

    g.shader(scene_shader);
    auto world_normal_matrix = g.modelMatrix().inversed().transpose();
    g.shader().uniform("N_world", world_normal_matrix);
    g.shader().uniform("M", g.modelMatrix());
    g.draw(obj);

    g.popCamera();
  }

  // beginning of effects
  g.depthTesting(false);
  g.blending(false);
  g.pushCamera(Viewpoint::IDENTITY);
  g.viewport(0, 0, w, h);

  // edge detection
  {
    g.shader(edge_shader);
    g.framebuffer(edge);
    scene_data.tex().bind(0); // using normal for edge detection
    scene_data.depthTex().bind(1); // depth
    g.draw(quad);
    scene_data.tex().unbind(0);
  }
  
  // ssao
  {
    g.pushViewport(0, 0, w/2, h/2);

    g.shader(ssao_shader);
    g.shader().uniform("view", view_matrix);
    g.shader().uniform("proj", proj_matrix);
    g.shader().uniform("inv_view_proj", (proj_matrix * view_matrix).inversed());

    g.framebuffer(ssao);
    scene_data.tex().bind(0);      // normal
    scene_data.depthTex().bind(1); // depth
    tex_pos_world.bind(2);         // world position
    g.draw(quad);

    // and blur
    g.shader(ssao_blur_ver_shader);
    g.framebuffer(ssao_blur_temp);
    ssao.tex().bind(0);
    g.draw(quad);

    g.shader(ssao_blur_hor_shader);
    g.framebuffer(ssao_blur);
    ssao_blur_temp.tex().bind(0);
    g.draw(quad);

    g.popViewport();
  }

  // now composite to raw output and draw to window
  {
    g.shader(composite_shader);
    begin(raw);
    scene_data.tex().bind(0);
    edge.tex().bind(1);
    ssao_blur.tex().bind(2);
    tex_obj_color.bind(3);
    g.draw(quad);
  }

  // filter raw output
  {
    g.shader(fxaa_shader);
    begin(filtered);
    raw.tex().bind(0);
    g.draw(quad);
  }

  g.popCamera(); // end of effects

  // now draw result to window
  {
    FBO::bind(0);
    g.viewport(0, 0, fbWidth(), fbHeight());
    g.clear(1);
    g.pushCamera(Viewpoint::IDENTITY);
    float vx = -1;
    float vy = -1;
    float vw = 2;
    float vh = 2;
    float w_ar = float(width()) / height();
    if (aspect_ratio > w_ar) {
      vy *= w_ar / aspect_ratio;
      vh *= w_ar / aspect_ratio;
    } else {
      vx *= aspect_ratio / w_ar;
      vw *= aspect_ratio / w_ar;
    }
    g.quad(filtered.tex(), vx, vy, vw, vh);
    g.popCamera();

    // check fps
    if (show_fps) {
      static float f = 60;
      f = 0.5 * (f + fps());
      float fps_status = max(0.0f, f - 30.0f) / 30.0f;
      g.pushCamera(Viewpoint::ORTHO_FOR_2D);
      g.color(1 - fps_status, fps_status, 0);
      g.draw(fps_indicator);
      g.popCamera();
    }

    // g.quadViewport(edge.tex(), -1, -1, 0.5, 0.5);
    // g.quadViewport(scene_data.tex(), -1, 0, 0.5, 0.5);
    g.quadViewport(tex_obj_color, -1, 0, 0.5, 0.5);

  }

  // unbind all texture unit for next frame
  Texture::unbind(0, GL_TEXTURE_2D);
  Texture::unbind(1, GL_TEXTURE_2D);
  Texture::unbind(2, GL_TEXTURE_2D);
  Texture::unbind(3, GL_TEXTURE_2D);
}

void work_o::onKeyDown(Keyboard const &k)
{
  if (k.key() == 'f') {
    show_fps = !show_fps;
  }
}

int main(int argc, char *argv[])
{
  work_o app;
  app.dimensions(840 * RATIO_SCALER, 360 * RATIO_SCALER);
  app.start();
}
