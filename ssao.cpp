#include "common.hpp"

using namespace al;

const int w = 960, h = 540;

struct ssao_app : App {
    VAOMesh obj {Mesh::TRIANGLES};
    VAOMesh quad {Mesh::TRIANGLE_STRIP};

    ShaderProgram scene_shader, ssao_shader, ssao_blur_ver_shader,
                  ssao_blur_hor_shader, composite_shader;

    EasyFBO scene_data, ssao_blur, ssao_blur_temp, ssao;
    Texture tex_pos_world, tex_obj_color;

    bool do_ssao = false;

    void onCreate() override;
    void onDraw(Graphics &g) override;
    bool onKeyDown(Keyboard const &k) override;
};

void ssao_app::onCreate() {
    nav().pos(0, 0, 10);
    nav().faceToward({0, 0, 0}, {0, 1, 0});
    nav().setHome();
    lens().fovy(24);
    lens().near(1);
    lens().far(1000);

    // obj to render
    prepare_obj(obj);

    // viewport fill quad
    addTexQuad(quad);
    quad.update();

    load_shader(scene_shader, "scene.vert", "scene.frag");
    load_shader(ssao_shader, "ssao.vert", "ssao.frag");
    load_shader(ssao_blur_ver_shader, "blur_ver.vert", "blur_ver.frag");
    load_shader(ssao_blur_hor_shader, "blur_hor.vert", "blur_hor.frag");
    load_shader(composite_shader, "composite.vert", "composite.frag");

    // scene data fbo: normal(f32) position(f32) color(u8)
    {
        // setup with primary attachment
        EasyFBOSetting float_setting;
        float_setting.internal = GL_RGBA32F;
        float_setting.filterMin = GL_LINEAR;
        float_setting.filterMag = GL_LINEAR;
        float_setting.depth_format = GL_DEPTH_COMPONENT32F;
        float_setting.use_depth_texture = true;
        scene_data.init(w, h, float_setting);

        // attach more textures
        tex_pos_world.create2D(w, h, GL_RGBA32F, GL_RGBA, GL_FLOAT);
        tex_pos_world.filter(Texture::LINEAR);
        tex_obj_color.create2D(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
        tex_obj_color.filter(Texture::LINEAR);
        auto &fbo = scene_data.fbo();
        fbo.bind();
        fbo.attachTexture2D(tex_pos_world, GL_COLOR_ATTACHMENT1);
        fbo.attachTexture2D(tex_obj_color, GL_COLOR_ATTACHMENT2);

        // make sure drawing to all three attachments
        unsigned int scene_buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
        glDrawBuffers(3, scene_buffers);
        fbo.unbind();
    }

    // ssao fbos
    {
        EasyFBOSetting fbo_setting;
        fbo_setting.internal = GL_RGBA8;
        fbo_setting.filterMin = GL_LINEAR;
        fbo_setting.filterMag = GL_LINEAR;
        // work at half res for speed
        ssao.init(w / 2, h / 2, fbo_setting);
        ssao_blur.init(w / 2, h / 2, fbo_setting);
        ssao_blur_temp.init(w / 2, h / 2, fbo_setting);
    }
}

void ssao_app::onDraw(Graphics &g) {
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
        g.depthMask(true);
    
        // clear takes r, g, b, a, and buffer index
        g.clearColorBuffer(0, 0, 0, 0, 0); // normal
        g.clearColorBuffer(0, 0, 0, 0, 1); // pos world
        g.clearColorBuffer(0, 0, 0, 0, 2); // color
        g.clearDepth(1);

        g.shader(scene_shader);
        auto world_normal_matrix = g.modelMatrix().inversed().transpose();
        g.shader().uniform("al_NormalMatrix", world_normal_matrix);
        g.shader().uniform("al_ModelMatrix", g.modelMatrix());
        g.draw(obj);

        g.popCamera();
    }

    // beginning of effects
    g.depthTesting(false);
    g.blending(false);
    g.camera(Viewpoint::IDENTITY);
    g.viewport(0, 0, w, h);
  
    // ssao
    {
        // work at half res for speed
        g.viewport(0, 0, w/2, h/2);

        g.shader(ssao_shader);
        g.shader().uniform("tex0", 0);
        g.shader().uniform("tex1", 1);
        g.shader().uniform("tex2", 2);
        g.shader().uniform("nfr", lens().near(), lens().far(), 3.0, 0.0);
        g.shader().uniform("view", view_matrix);
        g.shader().uniform("proj", proj_matrix);
        g.shader().uniform("inv_view_proj", (proj_matrix * view_matrix).inversed());
        g.shader().uniform("resolution", w/2, h/2);

        g.framebuffer(ssao);
        scene_data.tex().bind(0);      // normal
        scene_data.depthTex().bind(1); // depth
        tex_pos_world.bind(2);         // world position
        g.draw(quad);

        // and blur
        g.shader(ssao_blur_ver_shader);
        g.shader().uniform("tex0", 0);
        g.shader().uniform("resolution", w/2, h/2);
        g.framebuffer(ssao_blur_temp);
        ssao.tex().bind(0);
        g.draw(quad);

        g.shader(ssao_blur_hor_shader);
        g.shader().uniform("tex0", 0);
        g.shader().uniform("resolution", w/2, h/2);
        g.framebuffer(ssao_blur);
        ssao_blur_temp.tex().bind(0);
        g.draw(quad);
    }

    // now composite to raw output and draw to window
    {
        g.framebuffer(FBO::DEFAULT);
        g.viewport(0, 0, fbWidth(), fbHeight());
        g.clearColorBuffer(1.0f, 0.0f, 0.0f, 1.0f, 0);
        g.shader(composite_shader);
        g.shader().uniform("tex0", 0);
        g.shader().uniform("tex1", 1);
        g.shader().uniform("do_ssao", do_ssao? 1.0f : 0.0f);
        g.shader().uniform("resolution", fbWidth(), fbHeight());
        tex_obj_color.bind(0);
        ssao_blur.tex().bind(1);
        g.draw(quad);

        float y = -1.0f;
        g.quadViewport(tex_obj_color, -1.0f, y, 0.3f, 0.3f); y += 0.3f;
        g.quadViewport(ssao.tex(), -1.0f, y, 0.3f, 0.3f); y += 0.3f;
        g.quadViewport(ssao_blur_temp.tex(), -1.0f, y, 0.3f, 0.3f); y += 0.3f;
        g.quadViewport(ssao_blur.tex(), -1.0f, y, 0.3f, 0.3f); y += 0.3f;
        g.quadViewport(tex_pos_world, -1.0f, y, 0.3f, 0.3f); y += 0.3f;
        g.quadViewport(scene_data.tex(), -1.0f, y, 0.3f, 0.3f); y += 0.3f;
    }
}

bool ssao_app::onKeyDown(const Keyboard& k) {
    switch (k.key()) {
        case ' ': do_ssao = !do_ssao; break;
    }
    return true;
}

int main() {
    ssao_app app;
    app.dimensions(w, h);
    app.start();
}
