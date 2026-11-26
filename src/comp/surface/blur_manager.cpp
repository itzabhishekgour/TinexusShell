// ============================================================================
// blur_manager.cpp — Wayland Blur Protocol + GLES2 Dual-Kawase Shader Engine
// ============================================================================
#include "comp/surface/blur_manager.hpp"
#include "common/logger.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

extern "C" {
#ifndef EGL_NO_X11
#define EGL_NO_X11
#endif
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#define static
#include <wlr/render/egl.h>
#include <wlr/render/gles2.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#undef static
#include "org-kde-kwin-blur-protocol.h"
}

namespace tinexus::comp {

namespace {

// ── GLSL ES 1.00 Shaders (100% Portable Across Mesa GLES2 Drivers) ──────────
constexpr const char* kVertexShaderSrc = R"GLSL(
#version 100
attribute vec2 a_pos;
attribute vec2 a_uv;
attribute vec2 a_local_px;
varying vec2 v_uv;
varying vec2 v_local_px;
void main() {
    v_uv = a_uv;
    v_local_px = a_local_px;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)GLSL";

constexpr const char* kDownsampleFrag2DSrc = R"GLSL(
#version 100
precision highp float;
varying vec2 v_uv;
uniform vec2 u_halfpixel;
uniform float u_offset;
uniform sampler2D u_tex;
void main() {
    vec2 uv = v_uv;
    vec2 hp = u_halfpixel * u_offset;
    vec4 sum = texture2D(u_tex, uv) * 4.0;
    sum += texture2D(u_tex, clamp(uv - hp, vec2(0.0), vec2(1.0)));
    sum += texture2D(u_tex, clamp(uv + hp, vec2(0.0), vec2(1.0)));
    sum += texture2D(u_tex, clamp(uv + vec2(hp.x, -hp.y), vec2(0.0), vec2(1.0)));
    sum += texture2D(u_tex, clamp(uv - vec2(hp.x, -hp.y), vec2(0.0), vec2(1.0)));
    gl_FragColor = vec4(sum.rgb * 0.125, 1.0);
}
)GLSL";

constexpr const char* kDownsampleFragOesSrc = R"GLSL(
#extension GL_OES_EGL_image_external : require
precision highp float;
varying vec2 v_uv;
uniform vec2 u_halfpixel;
uniform float u_offset;
uniform samplerExternalOES u_tex;
void main() {
    vec2 uv = v_uv;
    vec2 hp = u_halfpixel * u_offset;
    vec4 sum = texture2D(u_tex, uv) * 4.0;
    sum += texture2D(u_tex, clamp(uv - hp, vec2(0.0), vec2(1.0)));
    sum += texture2D(u_tex, clamp(uv + hp, vec2(0.0), vec2(1.0)));
    sum += texture2D(u_tex, clamp(uv + vec2(hp.x, -hp.y), vec2(0.0), vec2(1.0)));
    sum += texture2D(u_tex, clamp(uv - vec2(hp.x, -hp.y), vec2(0.0), vec2(1.0)));
    gl_FragColor = vec4(sum.rgb * 0.125, 1.0);
}
)GLSL";

constexpr const char* kUpsampleFragSrc = R"GLSL(
#version 100
precision highp float;
varying vec2 v_uv;
uniform vec2 u_halfpixel;
uniform float u_offset;
uniform sampler2D u_tex;
void main() {
    vec2 uv = v_uv;
    vec2 hp = u_halfpixel * u_offset;
    vec4 sum = vec4(0.0);
    sum += texture2D(u_tex, clamp(uv + vec2(-hp.x * 2.0, 0.0), vec2(0.0), vec2(1.0)));
    sum += texture2D(u_tex, clamp(uv + vec2(-hp.x, hp.y), vec2(0.0), vec2(1.0))) * 2.0;
    sum += texture2D(u_tex, clamp(uv + vec2(0.0, hp.y * 2.0), vec2(0.0), vec2(1.0)));
    sum += texture2D(u_tex, clamp(uv + vec2(hp.x, hp.y), vec2(0.0), vec2(1.0))) * 2.0;
    sum += texture2D(u_tex, clamp(uv + vec2(hp.x * 2.0, 0.0), vec2(0.0), vec2(1.0)));
    sum += texture2D(u_tex, clamp(uv + vec2(hp.x, -hp.y), vec2(0.0), vec2(1.0))) * 2.0;
    sum += texture2D(u_tex, clamp(uv + vec2(0.0, -hp.y * 2.0), vec2(0.0), vec2(1.0)));
    sum += texture2D(u_tex, clamp(uv + vec2(-hp.x, -hp.y), vec2(0.0), vec2(1.0))) * 2.0;
    gl_FragColor = vec4(sum.rgb / 12.0, 1.0);
}
)GLSL";

constexpr const char* kCompositeFragSrc = R"GLSL(
#version 100
precision highp float;
varying vec2 v_uv;
varying vec2 v_local_px;
uniform vec2 u_halfpixel;
uniform float u_offset;
uniform vec2 u_rect_size;
uniform float u_corner_radius;
uniform vec4 u_tint;
uniform float u_vibrancy;
uniform float u_opacity;
uniform float u_border_alpha;
uniform vec3 u_border_color;
uniform sampler2D u_tex;
void main() {
    vec2 uv = v_uv;
    vec2 hp = u_halfpixel * u_offset;
    vec4 sum = vec4(0.0);
    sum += texture2D(u_tex, clamp(uv + vec2(-hp.x * 2.0, 0.0), vec2(0.0), vec2(1.0)));
    sum += texture2D(u_tex, clamp(uv + vec2(-hp.x, hp.y), vec2(0.0), vec2(1.0))) * 2.0;
    sum += texture2D(u_tex, clamp(uv + vec2(0.0, hp.y * 2.0), vec2(0.0), vec2(1.0)));
    sum += texture2D(u_tex, clamp(uv + vec2(hp.x, hp.y), vec2(0.0), vec2(1.0))) * 2.0;
    sum += texture2D(u_tex, clamp(uv + vec2(hp.x * 2.0, 0.0), vec2(0.0), vec2(1.0)));
    sum += texture2D(u_tex, clamp(uv + vec2(hp.x, -hp.y), vec2(0.0), vec2(1.0))) * 2.0;
    sum += texture2D(u_tex, clamp(uv + vec2(0.0, -hp.y * 2.0), vec2(0.0), vec2(1.0)));
    sum += texture2D(u_tex, clamp(uv + vec2(-hp.x, -hp.y), vec2(0.0), vec2(1.0))) * 2.0;
    vec3 rgb = sum.rgb / 12.0;

    // Vibrancy / saturation boost (~1.25x around perceptual Rec.709 luminance)
    float luma = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
    rgb = clamp(mix(vec3(luma), rgb, u_vibrancy), 0.0, 1.0);

    // Glass tint blend
    rgb = mix(rgb, u_tint.rgb, u_tint.a);

    // 1.5% interleaved gradient noise dithering to suppress 8-bit dark gradient banding
    float ign = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    rgb = clamp(rgb + (ign - 0.5) * 0.015, 0.0, 1.0);

    // Antialiased rounded-corner SDF clipping in surface-local buffer coordinates
    float r = clamp(u_corner_radius, 0.0, min(u_rect_size.x, u_rect_size.y) * 0.5);
    vec2 half_sz = u_rect_size * 0.5;
    vec2 p = abs(v_local_px - half_sz) - (half_sz - vec2(r));
    float sdf = length(max(p, 0.0)) + min(max(p.x, p.y), 0.0) - r;

    float mask = clamp(0.75 - sdf, 0.0, 1.0) * u_opacity;
    if (u_border_alpha > 0.001) {
        float inner_rim = clamp(sdf + 1.5, 0.0, 1.0);
        rgb = mix(rgb, u_border_color, inner_rim * u_border_alpha);
    }
    gl_FragColor = vec4(rgb * mask, mask);
}
)GLSL";

struct ScopedEglContext {
    EGLDisplay prev_dpy{EGL_NO_DISPLAY};
    EGLSurface prev_draw{EGL_NO_SURFACE};
    EGLSurface prev_read{EGL_NO_SURFACE};
    EGLContext prev_ctx{EGL_NO_CONTEXT};
    EGLDisplay target_dpy{EGL_NO_DISPLAY};
    bool switched{false};
    bool valid{false};

    ScopedEglContext(EGLDisplay dpy, EGLContext ctx) {
        if (dpy == EGL_NO_DISPLAY || ctx == EGL_NO_CONTEXT) return;
        prev_dpy = eglGetCurrentDisplay();
        prev_ctx = eglGetCurrentContext();
        prev_draw = eglGetCurrentSurface(EGL_DRAW);
        prev_read = eglGetCurrentSurface(EGL_READ);
        target_dpy = dpy;

        if (prev_dpy == dpy && prev_ctx == ctx) {
            valid = true;
            return;
        }
        if (eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx) == EGL_TRUE) {
            switched = true;
            valid = true;
        }
    }

    ~ScopedEglContext() {
        if (switched) {
            if (prev_dpy != EGL_NO_DISPLAY && prev_ctx != EGL_NO_CONTEXT) {
                eglMakeCurrent(prev_dpy, prev_draw, prev_read, prev_ctx);
            } else {
                eglMakeCurrent(target_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            }
        }
    }

    ScopedEglContext(const ScopedEglContext&) = delete;
    ScopedEglContext& operator=(const ScopedEglContext&) = delete;
};

// Saves and restores active GLES2 state mutated by Dual-Kawase blur passes so wlroots'
// subsequent render passes never observe stale FBO, viewport, scissor, blend, or program bindings.
struct ScopedGles2State {
    GLint prev_fbo{0};
    GLint prev_program{0};
    GLint prev_viewport[4]{0, 0, 0, 0};
    GLint prev_scissor_box[4]{0, 0, 0, 0};
    GLint prev_active_tex{GL_TEXTURE0};
    GLint prev_tex_2d{0};
    GLint prev_array_buf{0};
    GLint prev_blend_src_rgb{GL_ONE};
    GLint prev_blend_dst_rgb{GL_ZERO};
    GLint prev_blend_src_alpha{GL_ONE};
    GLint prev_blend_dst_alpha{GL_ZERO};
    GLboolean prev_blend{GL_FALSE};
    GLboolean prev_scissor{GL_FALSE};

    ScopedGles2State() noexcept {
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
        glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
        glGetIntegerv(GL_VIEWPORT, prev_viewport);
        glGetIntegerv(GL_SCISSOR_BOX, prev_scissor_box);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active_tex);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex_2d);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_array_buf);
        glGetIntegerv(GL_BLEND_SRC_RGB, &prev_blend_src_rgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &prev_blend_dst_rgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &prev_blend_src_alpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &prev_blend_dst_alpha);
        prev_blend = glIsEnabled(GL_BLEND);
        prev_scissor = glIsEnabled(GL_SCISSOR_TEST);
    }

    ~ScopedGles2State() {
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(prev_array_buf));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_tex_2d));
        glActiveTexture(static_cast<GLenum>(prev_active_tex));
        glUseProgram(static_cast<GLuint>(prev_program));
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prev_fbo));
        glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
        glScissor(prev_scissor_box[0], prev_scissor_box[1], prev_scissor_box[2], prev_scissor_box[3]);
        if (prev_scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
        if (prev_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        glBlendFuncSeparate(static_cast<GLenum>(prev_blend_src_rgb),
                            static_cast<GLenum>(prev_blend_dst_rgb),
                            static_cast<GLenum>(prev_blend_src_alpha),
                            static_cast<GLenum>(prev_blend_dst_alpha));
    }

    ScopedGles2State(const ScopedGles2State&) = delete;
    ScopedGles2State& operator=(const ScopedGles2State&) = delete;
};

GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    if (!s) return 0;
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char msg[512]{};
        glGetShaderInfoLog(s, sizeof(msg), nullptr, msg);
        log::error("[BlurManager] GLES2 shader compile error: {}", msg);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

struct ShaderProgram {
    GLuint id{0};
    GLint a_pos{-1};
    GLint a_uv{-1};
    GLint a_local_px{-1};
    GLint u_tex{-1};
    GLint u_halfpixel{-1};
    GLint u_offset{-1};
    GLint u_rect_size{-1};
    GLint u_corner_radius{-1};
    GLint u_tint{-1};
    GLint u_vibrancy{-1};
    GLint u_opacity{-1};
    GLint u_border_alpha{-1};
    GLint u_border_color{-1};

    bool build(const char* vert_src, const char* frag_src) {
        destroy();
        GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
        if (!vs || !fs) {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            return false;
        }
        GLuint p = glCreateProgram();
        glAttachShader(p, vs);
        glAttachShader(p, fs);
        glBindAttribLocation(p, 0, "a_pos");
        glBindAttribLocation(p, 1, "a_uv");
        glBindAttribLocation(p, 2, "a_local_px");
        glLinkProgram(p);
        glDeleteShader(vs);
        glDeleteShader(fs);

        GLint linked = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &linked);
        if (!linked) {
            char msg[512]{};
            glGetProgramInfoLog(p, sizeof(msg), nullptr, msg);
            log::error("[BlurManager] GLES2 program link error: {}", msg);
            glDeleteProgram(p);
            return false;
        }

        id = p;
        a_pos = glGetAttribLocation(p, "a_pos");
        a_uv = glGetAttribLocation(p, "a_uv");
        a_local_px = glGetAttribLocation(p, "a_local_px");
        u_tex = glGetUniformLocation(p, "u_tex");
        u_halfpixel = glGetUniformLocation(p, "u_halfpixel");
        u_offset = glGetUniformLocation(p, "u_offset");
        u_rect_size = glGetUniformLocation(p, "u_rect_size");
        u_corner_radius = glGetUniformLocation(p, "u_corner_radius");
        u_tint = glGetUniformLocation(p, "u_tint");
        u_vibrancy = glGetUniformLocation(p, "u_vibrancy");
        u_opacity = glGetUniformLocation(p, "u_opacity");
        u_border_alpha = glGetUniformLocation(p, "u_border_alpha");
        u_border_color = glGetUniformLocation(p, "u_border_color");
        return true;
    }

    void destroy() {
        if (id != 0) {
            glDeleteProgram(id);
            id = 0;
        }
    }
};

struct BlurLevelFbo {
    GLuint fbo{0};
    GLuint tex{0};
    int32_t width{0};
    int32_t height{0};
};

struct OutputBlurPyramid {
    static constexpr int kLevels = 4;
    std::array<BlurLevelFbo, kLevels> levels{};
    int32_t base_width{0};
    int32_t base_height{0};
    int32_t last_radius{0};
    bool valid{false};
};

void destroy_output_pyramid(OutputBlurPyramid& pyr) {
    for (auto& lvl : pyr.levels) {
        if (lvl.fbo != 0) {
            glDeleteFramebuffers(1, &lvl.fbo);
            lvl.fbo = 0;
        }
        if (lvl.tex != 0) {
            glDeleteTextures(1, &lvl.tex);
            lvl.tex = 0;
        }
        lvl.width = 0;
        lvl.height = 0;
    }
    pyr.base_width = 0;
    pyr.base_height = 0;
    pyr.valid = false;
}

bool ensure_output_pyramid(OutputBlurPyramid& pyr, int32_t buf_w, int32_t buf_h) {
    if (buf_w <= 0 || buf_h <= 0) return false;
    if (pyr.base_width == buf_w && pyr.base_height == buf_h && pyr.levels[0].fbo != 0) {
        return true;
    }

    destroy_output_pyramid(pyr);

    int32_t w = buf_w;
    int32_t h = buf_h;
    for (int i = 0; i < OutputBlurPyramid::kLevels; ++i) {
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);

        GLuint tex = 0, fbo = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            glDeleteFramebuffers(1, &fbo);
            glDeleteTextures(1, &tex);
            destroy_output_pyramid(pyr);
            glBindTexture(GL_TEXTURE_2D, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }
        pyr.levels[i] = {fbo, tex, w, h};
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    pyr.base_width = buf_w;
    pyr.base_height = buf_h;
    pyr.valid = false;
    return true;
}

void draw_fullscreen_quad(const ShaderProgram& prog) {
    static constexpr GLfloat kQuadVerts[] = {
        // ndc.x, ndc.y, u, v, local.x, local.y
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    };
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), kQuadVerts);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), kQuadVerts + 2);
    if (prog.a_local_px >= 0) {
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), kQuadVerts + 4);
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    if (prog.a_local_px >= 0) {
        glDisableVertexAttribArray(2);
    }
}

void draw_rect_quad(const ShaderProgram& prog,
                    float rx, float ry, float rw, float rh,
                    float fb_w, float fb_h) {
    const float x0 = 2.0f * (rx / fb_w) - 1.0f;
    const float y0 = 2.0f * (ry / fb_h) - 1.0f;
    const float x1 = 2.0f * ((rx + rw) / fb_w) - 1.0f;
    const float y1 = 2.0f * ((ry + rh) / fb_h) - 1.0f;

    const float u0 = rx / fb_w;
    const float v0 = ry / fb_h;
    const float u1 = (rx + rw) / fb_w;
    const float v1 = (ry + rh) / fb_h;

    const GLfloat verts[] = {
        x0, y0, u0, v0, 0.0f, 0.0f,
        x1, y0, u1, v0,   rw, 0.0f,
        x0, y1, u0, v1, 0.0f,   rh,
        x1, y1, u1, v1,   rw,   rh,
    };
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), verts);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), verts + 2);
    if (prog.a_local_px >= 0) {
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), verts + 4);
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    if (prog.a_local_px >= 0) {
        glDisableVertexAttribArray(2);
    }
}

void blur_handle_commit(struct wl_client* /*client*/, struct wl_resource* resource) {
    auto* state = static_cast<BlurSurfaceState*>(wl_resource_get_user_data(resource));
    if (state && state->surface) {
        BlurManager::instance().commit_surface_blur(state->surface);
    }
}

void blur_handle_set_region(struct wl_client* /*client*/, struct wl_resource* resource,
                            struct wl_resource* region_resource) {
    auto* state = static_cast<BlurSurfaceState*>(wl_resource_get_user_data(resource));
    if (!state || !state->surface) return;

    if (region_resource) {
        const pixman_region32_t* reg = wlr_region_from_resource(region_resource);
        if (reg) {
            pixman_region32_copy(&state->region, reg);
            state->has_custom_region = true;
        }
    } else {
        pixman_region32_clear(&state->region);
        state->has_custom_region = false;
    }
}

void blur_handle_release(struct wl_client* /*client*/, struct wl_resource* resource) {
    wl_resource_destroy(resource);
}

const struct org_kde_kwin_blur_interface blur_implementation = {
    .commit = blur_handle_commit,
    .set_region = blur_handle_set_region,
    .release = blur_handle_release,
};

void blur_resource_destroy(struct wl_resource* resource) {
    (void)resource;
}

void blur_manager_handle_create(struct wl_client* client, struct wl_resource* /*resource*/,
                                uint32_t id, struct wl_resource* surface_resource) {
    struct wlr_surface* surface = wlr_surface_from_resource(surface_resource);
    if (!surface) return;

    struct wl_resource* blur_res = wl_resource_create(client, &org_kde_kwin_blur_interface, 1, id);
    if (!blur_res) {
        wl_client_post_no_memory(client);
        return;
    }

    BlurManager::instance().register_surface_blur(surface, blur_res);
}

void blur_manager_handle_unset(struct wl_client* /*client*/, struct wl_resource* /*resource*/,
                               struct wl_resource* surface_resource) {
    struct wlr_surface* surface = wlr_surface_from_resource(surface_resource);
    if (!surface) return;

    BlurManager::instance().unregister_surface_blur(surface);
}

const struct org_kde_kwin_blur_manager_interface blur_manager_implementation = {
    .create = blur_manager_handle_create,
    .unset = blur_manager_handle_unset,
};

void blur_manager_bind(struct wl_client* client, void* data, uint32_t version, uint32_t id) {
    (void)data;
    struct wl_resource* resource = wl_resource_create(client, &org_kde_kwin_blur_manager_interface,
                                                      std::min(version, 1u), id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &blur_manager_implementation, nullptr, nullptr);
}

} // anonymous namespace

struct BlurManager::GLImpl {
    struct wlr_renderer* renderer{nullptr};
    EGLDisplay egl_dpy{EGL_NO_DISPLAY};
    EGLContext egl_ctx{EGL_NO_CONTEXT};
    bool ready{false};
    bool has_oes_ext{false};

    ShaderProgram prog_down_2d;
    ShaderProgram prog_down_oes;
    ShaderProgram prog_up;
    ShaderProgram prog_composite;

    std::unordered_map<struct wlr_output*, OutputBlurPyramid> outputs;
    struct wl_listener renderer_destroy{};

    static void on_renderer_destroy(struct wl_listener* l, void* /*data*/) {
        GLImpl* self = wl_container_of(l, self, renderer_destroy);
        self->release_all();
    }

    void release_all() {
        if (renderer_destroy.link.next) {
            wl_list_remove(&renderer_destroy.link);
            renderer_destroy.link.next = nullptr;
            renderer_destroy.link.prev = nullptr;
        }
        if (ready && egl_dpy != EGL_NO_DISPLAY && egl_ctx != EGL_NO_CONTEXT) {
            ScopedEglContext guard(egl_dpy, egl_ctx);
            if (guard.valid) {
                for (auto& [out, pyr] : outputs) {
                    (void)out;
                    destroy_output_pyramid(pyr);
                }
                prog_down_2d.destroy();
                prog_down_oes.destroy();
                prog_up.destroy();
                prog_composite.destroy();
            }
        }
        outputs.clear();
        renderer = nullptr;
        egl_dpy = EGL_NO_DISPLAY;
        egl_ctx = EGL_NO_CONTEXT;
        ready = false;
    }
};

BlurConfig BlurConfig::load_from_config(const std::string& path) {
    BlurConfig cfg;
    std::string config_file = path;
    if (config_file.empty()) {
        const char* home = getenv("HOME");
        if (home && *home) {
            std::string user_cfg = std::string(home) + "/.config/tinexus/compositor.toml";
            if (std::filesystem::exists(user_cfg)) {
                config_file = user_cfg;
            }
        }
        if (config_file.empty()) {
            std::string sys_cfg = "/etc/tinexus/defaults/compositor.toml";
            if (std::filesystem::exists(sys_cfg)) {
                config_file = sys_cfg;
            }
        }
    }

    if (config_file.empty() || !std::filesystem::exists(config_file)) {
        return cfg;
    }

    std::ifstream f(config_file);
    if (!f.is_open()) return cfg;

    std::string line;
    bool in_blur_section = false;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.erase(line.begin());
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) line.pop_back();

        if (line.starts_with("[")) {
            in_blur_section = (line == "[blur]" || line == "[appearance]");
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.erase(key.begin());
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t' || val.back() == '"' || val.back() == '\'')) val.pop_back();
        while (!val.empty() && (val.front() == ' ' || val.front() == '\t' || val.front() == '"' || val.front() == '\'')) val.erase(val.begin());

        if (in_blur_section) {
            if (key == "radius" || key == "backdrop_blur_radius") {
                try { cfg.radius = std::stoi(val); } catch (...) {}
            } else if (key == "tint") {
                try {
                    if (val.starts_with("#")) val.erase(val.begin());
                    cfg.tint = static_cast<uint32_t>(std::stoul(val, nullptr, 16));
                } catch (...) {}
            } else if (key == "vibrancy" || key == "saturation") {
                try { cfg.vibrancy = std::clamp(std::stof(val), 0.5f, 2.0f); } catch (...) {}
            }
        }
    }
    return cfg;
}

BlurManager::BlurManager() : m_gl(std::make_unique<GLImpl>()) {}

BlurManager& BlurManager::instance() {
    static BlurManager s_instance;
    return s_instance;
}

BlurManager::~BlurManager() {
    shutdown();
}

bool BlurManager::initialize(struct wl_display* display) {
    if (!display) return false;
    m_display = display;

    load_config();

    m_global = wl_global_create(display, &org_kde_kwin_blur_manager_interface, 1, this, blur_manager_bind);
    if (!m_global) {
        log::error("[BlurManager] Failed to create org_kde_kwin_blur_manager global");
        return false;
    }

    log::info("[BlurManager] Successfully registered org_kde_kwin_blur_manager Wayland protocol global (radius={}, tint=0x{:08X}, vibrancy={:.2f})",
              m_config.radius, m_config.tint, m_config.vibrancy);
    return true;
}

void BlurManager::set_config(const BlurConfig& config) {
    m_config = config;
    for (auto& [surf, state] : m_surfaces) {
        if (state) {
            state->radius = m_config.radius;
            state->tint = m_config.tint;
        }
    }
}

void BlurManager::load_config(const std::string& path) {
    set_config(BlurConfig::load_from_config(path));
}

void BlurManager::shutdown() {
    if (m_gl) {
        m_gl->release_all();
    }
    if (m_global) {
        wl_global_destroy(m_global);
        m_global = nullptr;
    }
    for (auto& [surf, state] : m_surfaces) {
        if (state) {
            if (state->destroy_listener.link.next) {
                wl_list_remove(&state->destroy_listener.link);
                state->destroy_listener.link.next = nullptr;
                state->destroy_listener.link.prev = nullptr;
            }
            pixman_region32_fini(&state->region);
        }
    }
    m_surfaces.clear();
}

void BlurManager::register_surface_blur(struct wlr_surface* surface, struct wl_resource* blur_resource) {
    if (!surface) return;

    auto it = m_surfaces.find(surface);
    if (it == m_surfaces.end()) {
        auto state = std::make_unique<BlurSurfaceState>();
        state->surface = surface;
        pixman_region32_init(&state->region);
        state->has_custom_region = false;
        state->radius = m_config.radius;
        state->tint = m_config.tint;

        state->destroy_listener.notify = [](struct wl_listener* l, void* /*data*/) {
            BlurSurfaceState* s = wl_container_of(l, s, destroy_listener);
            if (s && s->surface) {
                BlurManager::instance().unregister_surface_blur(s->surface);
            }
        };
        wl_signal_add(&surface->events.destroy, &state->destroy_listener);

        if (blur_resource) {
            wl_resource_set_implementation(blur_resource, &blur_implementation, state.get(), blur_resource_destroy);
        }
        m_surfaces[surface] = std::move(state);
    } else {
        if (blur_resource) {
            wl_resource_set_implementation(blur_resource, &blur_implementation, it->second.get(), blur_resource_destroy);
        }
    }

    log::info("[BlurManager] Registered blur request for surface {:p}", static_cast<void*>(surface));
}

void BlurManager::set_surface_blur_region(struct wlr_surface* surface, struct wl_resource* region_resource) {
    if (!surface) return;
    auto it = m_surfaces.find(surface);
    if (it == m_surfaces.end() || !it->second) return;
    if (region_resource) {
        const pixman_region32_t* reg = wlr_region_from_resource(region_resource);
        if (reg) {
            pixman_region32_copy(&it->second->region, reg);
            it->second->has_custom_region = true;
        }
    } else {
        pixman_region32_clear(&it->second->region);
        it->second->has_custom_region = false;
    }
}

void BlurManager::set_surface_corner_radius(struct wlr_surface* surface, float corner_radius) {
    if (!surface) return;
    auto it = m_surfaces.find(surface);
    if (it != m_surfaces.end() && it->second) {
        it->second->corner_radius = std::max(0.0f, corner_radius);
    }
}

void BlurManager::commit_surface_blur(struct wlr_surface* surface) {
    if (!surface) return;
    auto it = m_surfaces.find(surface);
    if (it != m_surfaces.end()) {
        log::debug("[BlurManager] Committed blur for surface {:p} (custom_region={})",
                   static_cast<void*>(surface), it->second->has_custom_region);
    }
}

void BlurManager::unregister_surface_blur(struct wlr_surface* surface) {
    if (!surface) return;
    auto it = m_surfaces.find(surface);
    if (it != m_surfaces.end()) {
        if (it->second->destroy_listener.link.next) {
            wl_list_remove(&it->second->destroy_listener.link);
            it->second->destroy_listener.link.next = nullptr;
            it->second->destroy_listener.link.prev = nullptr;
        }
        pixman_region32_fini(&it->second->region);
        m_surfaces.erase(it);
        log::info("[BlurManager] Unregistered blur for surface {:p}", static_cast<void*>(surface));
    }
}

bool BlurManager::is_surface_blurred(struct wlr_surface* surface) const {
    if (!surface) return false;
    return m_surfaces.find(surface) != m_surfaces.end();
}

const BlurSurfaceState* BlurManager::get_surface_state(struct wlr_surface* surface) const {
    if (!surface) return nullptr;
    auto it = m_surfaces.find(surface);
    return (it != m_surfaces.end()) ? it->second.get() : nullptr;
}

bool BlurManager::is_namespace_blurred(const std::string& ns) const {
    return m_blurred_namespaces.find(ns) != m_blurred_namespaces.end();
}

bool BlurManager::init_gl(struct wlr_renderer* renderer) {
    if (!renderer || !wlr_renderer_is_gles2(renderer)) {
        return false;
    }
    if (m_gl->ready && m_gl->renderer == renderer) {
        return true;
    }
    m_gl->release_all();

    struct wlr_egl* wlr_egl = wlr_gles2_renderer_get_egl(renderer);
    if (!wlr_egl) return false;
    EGLDisplay dpy = wlr_egl_get_display(wlr_egl);
    EGLContext ctx = wlr_egl_get_context(wlr_egl);
    if (dpy == EGL_NO_DISPLAY || ctx == EGL_NO_CONTEXT) return false;

    ScopedEglContext guard(dpy, ctx);
    if (!guard.valid) {
        log::warn("[BlurManager] Could not make wlroots EGL context current for GLES2 blur init");
        return false;
    }

    if (!m_gl->prog_down_2d.build(kVertexShaderSrc, kDownsampleFrag2DSrc) ||
        !m_gl->prog_up.build(kVertexShaderSrc, kUpsampleFragSrc) ||
        !m_gl->prog_composite.build(kVertexShaderSrc, kCompositeFragSrc)) {
        m_gl->prog_down_2d.destroy();
        m_gl->prog_up.destroy();
        m_gl->prog_composite.destroy();
        return false;
    }

    m_gl->has_oes_ext = wlr_gles2_renderer_check_ext(renderer, "GL_OES_EGL_image_external");
    if (m_gl->has_oes_ext) {
        if (!m_gl->prog_down_oes.build(kVertexShaderSrc, kDownsampleFragOesSrc)) {
            m_gl->has_oes_ext = false;
        }
    }

    m_gl->renderer = renderer;
    m_gl->egl_dpy = dpy;
    m_gl->egl_ctx = ctx;
    m_gl->ready = true;
    m_gl->renderer_destroy.notify = GLImpl::on_renderer_destroy;
    wl_signal_add(&renderer->events.destroy, &m_gl->renderer_destroy);

    log::info("[BlurManager] GLES2 Dual-Kawase pipeline ready (OES_external={})",
              m_gl->has_oes_ext ? "enabled" : "2d-only");
    return true;
}

bool BlurManager::is_gles2_ready() const noexcept {
    return m_gl && m_gl->ready;
}

size_t BlurManager::active_gpu_output_count() const noexcept {
    return m_gl ? m_gl->outputs.size() : 0;
}

void BlurManager::release_output_resources(struct wlr_output* output) {
    if (!m_gl || !output) return;
    auto it = m_gl->outputs.find(output);
    if (it == m_gl->outputs.end()) return;

    if (m_gl->ready && m_gl->egl_dpy != EGL_NO_DISPLAY && m_gl->egl_ctx != EGL_NO_CONTEXT) {
        ScopedEglContext guard(m_gl->egl_dpy, m_gl->egl_ctx);
        if (guard.valid) {
            destroy_output_pyramid(it->second);
        }
    }
    m_gl->outputs.erase(it);
    log::info("[BlurManager] Released GPU FBO/texture pyramid for output {:p}", static_cast<void*>(output));
}

void BlurManager::expand_damage_near_blur(const pixman_region32_t* damage,
                                          const std::vector<BlurTargetBox>& targets,
                                          int32_t blur_pad_px,
                                          pixman_region32_t* out_expanded_damage,
                                          bool* out_intersects_blur) {
    if (out_intersects_blur) *out_intersects_blur = false;
    if (!out_expanded_damage) return;

    pixman_region32_clear(out_expanded_damage);
    if (!damage || !pixman_region32_not_empty(damage) || targets.empty()) {
        return;
    }

    const int32_t pad = std::max(0, blur_pad_px);
    int nrects = 0;
    const pixman_box32_t* rects = pixman_region32_rectangles(damage, &nrects);

    pixman_region32_t dilated;
    pixman_region32_init(&dilated);
    for (int i = 0; i < nrects; ++i) {
        pixman_region32_union_rect(&dilated, &dilated,
                                   rects[i].x1 - pad, rects[i].y1 - pad,
                                   static_cast<unsigned int>((rects[i].x2 - rects[i].x1) + 2 * pad),
                                   static_cast<unsigned int>((rects[i].y2 - rects[i].y1) + 2 * pad));
    }

    for (const auto& t : targets) {
        if (t.width <= 0 || t.height <= 0 || t.opacity <= 0.001f) continue;
        pixman_box32_t t_box{t.x, t.y, t.x + t.width, t.y + t.height};
        pixman_region32_t t_reg;
        pixman_region32_init_rect(&t_reg, t.x, t.y,
                                  static_cast<unsigned int>(t.width),
                                  static_cast<unsigned int>(t.height));
        pixman_region32_t inter;
        pixman_region32_init(&inter);
        pixman_region32_intersect(&inter, &dilated, &t_reg);
        if (pixman_region32_not_empty(&inter)) {
            if (out_intersects_blur) *out_intersects_blur = true;
            // Expand touched blur region back out cleanly so Kawase tent taps stay coherent
            int n_inter = 0;
            const pixman_box32_t* ir = pixman_region32_rectangles(&inter, &n_inter);
            for (int k = 0; k < n_inter; ++k) {
                const int32_t x1 = std::max(t_box.x1, ir[k].x1 - pad);
                const int32_t y1 = std::max(t_box.y1, ir[k].y1 - pad);
                const int32_t x2 = std::min(t_box.x2, ir[k].x2 + pad);
                const int32_t y2 = std::min(t_box.y2, ir[k].y2 + pad);
                if (x2 > x1 && y2 > y1) {
                    pixman_region32_union_rect(out_expanded_damage, out_expanded_damage,
                                               x1, y1,
                                               static_cast<unsigned int>(x2 - x1),
                                               static_cast<unsigned int>(y2 - y1));
                }
            }
        }
        pixman_region32_fini(&inter);
        pixman_region32_fini(&t_reg);
    }
    pixman_region32_fini(&dilated);
}

bool BlurManager::prepare_output_blur(struct wlr_output* output,
                                      struct wlr_renderer* renderer,
                                      struct wlr_buffer* backdrop_buffer,
                                      bool backdrop_near_blur_dirty,
                                      const std::vector<BlurTargetBox>& targets) {
    if (!output || !renderer || !backdrop_buffer || targets.empty()) {
        return false;
    }
    if (!init_gl(renderer)) {
        return false;
    }

    int max_rad = m_config.radius;
    for (const auto& t : targets) {
        if (t.radius > max_rad) max_rad = t.radius;
    }
    const float offset_scale = std::clamp(static_cast<float>(max_rad) / 28.0f, 0.5f, 2.0f);

    ScopedEglContext guard(m_gl->egl_dpy, m_gl->egl_ctx);
    if (!guard.valid) return false;
    ScopedGles2State gl_guard;

    auto& pyr = m_gl->outputs[output];
    const int32_t bw = backdrop_buffer->width;
    const int32_t bh = backdrop_buffer->height;
    if (!ensure_output_pyramid(pyr, bw, bh)) {
        return false;
    }

    // Skip expensive GPU passes entirely if cached pyramid already matches backdrop & radius
    if (pyr.valid && !backdrop_near_blur_dirty && pyr.last_radius == max_rad) {
        return true;
    }

    struct wlr_texture* bd_tex = wlr_texture_from_buffer(renderer, backdrop_buffer);
    if (!bd_tex || !wlr_texture_is_gles2(bd_tex)) {
        if (bd_tex) wlr_texture_destroy(bd_tex);
        return false;
    }

    struct wlr_gles2_texture_attribs attribs{};
    wlr_gles2_texture_get_attribs(bd_tex, &attribs);

    const bool use_oes = (attribs.target == GL_TEXTURE_EXTERNAL_OES);
    if (use_oes && !m_gl->has_oes_ext) {
        wlr_texture_destroy(bd_tex);
        return false;
    }

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);

    // 1. Downsample Pass 0: Full-res backdrop texture -> Pyramid Level 0 (half res)
    const ShaderProgram& p0 = use_oes ? m_gl->prog_down_oes : m_gl->prog_down_2d;
    glBindFramebuffer(GL_FRAMEBUFFER, pyr.levels[0].fbo);
    glViewport(0, 0, pyr.levels[0].width, pyr.levels[0].height);
    glUseProgram(p0.id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(attribs.target, attribs.tex);
    GLint prev_min_filter = GL_LINEAR, prev_mag_filter = GL_LINEAR;
    GLint prev_wrap_s = GL_CLAMP_TO_EDGE, prev_wrap_t = GL_CLAMP_TO_EDGE;
    glGetTexParameteriv(attribs.target, GL_TEXTURE_MIN_FILTER, &prev_min_filter);
    glGetTexParameteriv(attribs.target, GL_TEXTURE_MAG_FILTER, &prev_mag_filter);
    glGetTexParameteriv(attribs.target, GL_TEXTURE_WRAP_S, &prev_wrap_s);
    glGetTexParameteriv(attribs.target, GL_TEXTURE_WRAP_T, &prev_wrap_t);
    glTexParameteri(attribs.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(attribs.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(attribs.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(attribs.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glUniform1i(p0.u_tex, 0);
    glUniform2f(p0.u_halfpixel, 0.5f / static_cast<float>(bw), 0.5f / static_cast<float>(bh));
    glUniform1f(p0.u_offset, offset_scale);
    draw_fullscreen_quad(p0);
    glTexParameteri(attribs.target, GL_TEXTURE_MIN_FILTER, prev_min_filter);
    glTexParameteri(attribs.target, GL_TEXTURE_MAG_FILTER, prev_mag_filter);
    glTexParameteri(attribs.target, GL_TEXTURE_WRAP_S, prev_wrap_s);
    glTexParameteri(attribs.target, GL_TEXTURE_WRAP_T, prev_wrap_t);
    glBindTexture(attribs.target, 0);
    wlr_texture_destroy(bd_tex);

    // 2. Remaining Downsample Passes (L0 -> L1 -> L2 -> L3)
    glUseProgram(m_gl->prog_down_2d.id);
    glUniform1i(m_gl->prog_down_2d.u_tex, 0);
    glUniform1f(m_gl->prog_down_2d.u_offset, offset_scale);
    for (int i = 1; i < OutputBlurPyramid::kLevels; ++i) {
        const auto& src = pyr.levels[i - 1];
        const auto& dst = pyr.levels[i];
        glBindFramebuffer(GL_FRAMEBUFFER, dst.fbo);
        glViewport(0, 0, dst.width, dst.height);
        glBindTexture(GL_TEXTURE_2D, src.tex);
        glUniform2f(m_gl->prog_down_2d.u_halfpixel,
                    0.5f / static_cast<float>(src.width),
                    0.5f / static_cast<float>(src.height));
        draw_fullscreen_quad(m_gl->prog_down_2d);
    }

    // 3. Dual-Kawase Upsample Pyramid (L3 -> L2 -> L1 -> L0)
    glUseProgram(m_gl->prog_up.id);
    glUniform1i(m_gl->prog_up.u_tex, 0);
    glUniform1f(m_gl->prog_up.u_offset, offset_scale);
    for (int i = OutputBlurPyramid::kLevels - 1; i >= 1; --i) {
        const auto& src = pyr.levels[i];
        const auto& dst = pyr.levels[i - 1];
        glBindFramebuffer(GL_FRAMEBUFFER, dst.fbo);
        glViewport(0, 0, dst.width, dst.height);
        glBindTexture(GL_TEXTURE_2D, src.tex);
        glUniform2f(m_gl->prog_up.u_halfpixel,
                    0.5f / static_cast<float>(src.width),
                    0.5f / static_cast<float>(src.height));
        draw_fullscreen_quad(m_gl->prog_up);
    }

    pyr.last_radius = max_rad;
    pyr.valid = true;
    return true;
}

bool BlurManager::composite_output_blur(struct wlr_output* output,
                                        struct wlr_renderer* renderer,
                                        struct wlr_buffer* dst_buffer,
                                        const std::vector<BlurTargetBox>& targets) {
    if (!m_gl->ready || !output || !renderer || !dst_buffer || targets.empty()) {
        return false;
    }
    auto it = m_gl->outputs.find(output);
    if (it == m_gl->outputs.end() || !it->second.valid) {
        return false;
    }

    const auto& pyr = it->second;
    const auto& src = pyr.levels[0];

    ScopedEglContext guard(m_gl->egl_dpy, m_gl->egl_ctx);
    if (!guard.valid) return false;
    ScopedGles2State gl_guard;

    const GLuint out_fbo = wlr_gles2_renderer_get_buffer_fbo(renderer, dst_buffer);
    if (out_fbo == 0) {
        return false;
    }

    const int32_t fb_w = dst_buffer->width;
    const int32_t fb_h = dst_buffer->height;

    glBindFramebuffer(GL_FRAMEBUFFER, out_fbo);
    glViewport(0, 0, fb_w, fb_h);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_SCISSOR_TEST);

    const ShaderProgram& comp = m_gl->prog_composite;
    glUseProgram(comp.id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src.tex);
    glUniform1i(comp.u_tex, 0);
    glUniform2f(comp.u_halfpixel, 0.5f / static_cast<float>(src.width), 0.5f / static_cast<float>(src.height));

    for (const auto& t : targets) {
        if (t.width <= 0 || t.height <= 0 || t.opacity <= 0.001f) continue;

        const int32_t sx0 = std::clamp(t.x, 0, fb_w);
        const int32_t sy0 = std::clamp(t.y, 0, fb_h);
        const int32_t sx1 = std::clamp(t.x + t.width, 0, fb_w);
        const int32_t sy1 = std::clamp(t.y + t.height, 0, fb_h);
        if (sx1 <= sx0 || sy1 <= sy0) continue;

        const float offset_scale = std::clamp(static_cast<float>(t.radius) / 28.0f, 0.5f, 2.0f);
        glUniform1f(comp.u_offset, offset_scale);
        glUniform2f(comp.u_rect_size, static_cast<float>(t.width), static_cast<float>(t.height));
        glUniform1f(comp.u_corner_radius, t.corner_radius);
        glUniform4f(comp.u_tint, t.tint_rgba[0], t.tint_rgba[1], t.tint_rgba[2], t.tint_rgba[3]);
        glUniform1f(comp.u_vibrancy, t.vibrancy > 0.0f ? t.vibrancy : m_config.vibrancy);
        glUniform1f(comp.u_opacity, t.opacity);
        glUniform1f(comp.u_border_alpha, t.is_snap_preview ? 0.52f : 0.0f);
        glUniform3f(comp.u_border_color, 0.85f, 0.93f, 1.0f);

        glScissor(sx0, sy0, sx1 - sx0, sy1 - sy0);
        draw_rect_quad(comp,
                       static_cast<float>(t.x), static_cast<float>(t.y),
                       static_cast<float>(t.width), static_cast<float>(t.height),
                       static_cast<float>(fb_w), static_cast<float>(fb_h));
    }

    return true;
}

} // namespace tinexus::comp
