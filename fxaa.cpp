#include "common.hpp"

using namespace al;

const int w = 960, h = 540;

struct fxaa_app : App {
    VAOMesh obj {Mesh::TRIANGLES};
    VAOMesh quad {Mesh::TRIANGLE_STRIP};

    ShaderProgram fxaa_shader;

    EasyFBO scene_data, fbo_fxaa;
    bool do_fxaa = false;

    void onCreate() override;
    void onDraw(Graphics &g) override;
    bool onKeyDown(const Keyboard& k) override;
};

void fxaa_app::onCreate() {
    nav().pos(0, 0, 10);
    nav().faceToward({0, 0, 0}, {0, 1, 0});
    nav().setHome();
    lens().fovy(24);
    lens().near(1);
    lens().far(1000);

    // obj to render
    prepare_obj(obj);

    addTexQuad(quad);
    quad.update();

    load_shader(fxaa_shader, "fxaa.vert", "fxaa.frag");

    {
        EasyFBOSetting fbo_setting;
        fbo_setting.filterMin = GL_LINEAR;
        fbo_setting.filterMag = GL_LINEAR;
        scene_data.init(w, h, fbo_setting);
        fbo_fxaa.init(w, h, fbo_setting);
    }
}

void fxaa_app::onDraw(Graphics &g) {
    g.cullFace(true);

    // prepare scene data textures
    {
        g.framebuffer(scene_data);
        g.viewport(0, 0, w, h);
        g.camera(view());

        g.blending(false);
        g.depthTesting(true);

        g.clearColorBuffer(0, 0, 0, 0, 0); // r, g, b, a, index
        g.clearDepth(1);

        g.meshColor();
        g.draw(obj);
    }

    // FXAA
    {
        g.depthTesting(false);
        g.blending(false);
        g.camera(Viewpoint::IDENTITY);
        g.viewport(0, 0, w, h);
        g.shader(fxaa_shader);
        g.shader().uniform("tex0", 0);
        g.shader().uniform("resolution", w, h);
        g.framebuffer(fbo_fxaa);
        scene_data.tex().bind(0);
        g.draw(quad);
    }

    // now draw result to window
    {
        g.framebuffer(FBO::DEFAULT);
        g.viewport(0, 0, fbWidth(), fbHeight());
        g.clearColorBuffer(1.0f, 0.0f, 0.0f, 1.0f, 0);
        g.camera(Viewpoint::IDENTITY);
        float vx = -1;
        float vy = -1;
        float vw = 2;
        float vh = 2;
        float w_ar = float(width()) / height();
        float aspect_ratio = float(w) / h;
        if (aspect_ratio > w_ar) {
          vy *= w_ar / aspect_ratio;
          vh *= w_ar / aspect_ratio;
        } else {
          vx *= aspect_ratio / w_ar;
          vw *= aspect_ratio / w_ar;
        }
        if (do_fxaa) {
            g.quad(fbo_fxaa.tex(), vx, vy, vw, vh);
        } else {
            g.quad(scene_data.tex(), vx, vy, vw, vh);
        }
    }
}

bool fxaa_app::onKeyDown(const Keyboard& k){
    switch (k.key()) {
        case ' ': do_fxaa = !do_fxaa; break;
    }
    return true;
}

int main() {
  fxaa_app app;
  app.dimensions(w, h);
  app.start();
}
