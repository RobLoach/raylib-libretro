/*
 * test_opengl_libretro.c — minimal OpenGL 3.3 Core test core for raylib-libretro.
 *
 * Draws a spinning RGB triangle into the hardware-rendered FBO each frame.
 * Uses a procedural vertex shader (gl_VertexID) so no VBO/vertex-attrib
 * setup is needed — avoids macOS Core Profile glVertexAttribPointer issues
 * when loading GL procs via glfwGetProcAddress.
 *
 * Requires no content (supports_no_game = true). Load it with no ROM.
 */

#include "libretro.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* --------------------------------------------------------------------------
 * Minimal GL type / constant definitions — no GL headers required.
 * -------------------------------------------------------------------------- */
typedef unsigned int  GLenum;
typedef unsigned int  GLuint;
typedef int           GLint;
typedef float         GLfloat;
typedef unsigned char GLboolean;
typedef char          GLchar;

#define GL_FALSE              0
#define GL_TRUE               1
#define GL_COLOR_BUFFER_BIT   0x00004000
#define GL_TRIANGLES          0x0004
#define GL_VERTEX_SHADER      0x8B31
#define GL_FRAGMENT_SHADER    0x8B30
#define GL_COMPILE_STATUS     0x8B81
#define GL_LINK_STATUS        0x8B82
#define GL_INFO_LOG_LENGTH    0x8B84

/* --------------------------------------------------------------------------
 * GL function pointer table — only what we actually use.
 * No VBO/VAO/vertex-attrib functions needed for procedural draw.
 * -------------------------------------------------------------------------- */
#define GL_FUNC_LIST \
    X(void,    Viewport,          (GLint x, GLint y, GLint w, GLint h)) \
    X(void,    ClearColor,        (GLfloat r, GLfloat g, GLfloat b, GLfloat a)) \
    X(void,    Clear,             (GLuint mask)) \
    X(GLuint,  CreateShader,      (GLenum type)) \
    X(void,    ShaderSource,      (GLuint s, GLint n, const GLchar **src, const GLint *len)) \
    X(void,    CompileShader,     (GLuint s)) \
    X(void,    GetShaderiv,       (GLuint s, GLenum p, GLint *v)) \
    X(void,    GetShaderInfoLog,  (GLuint s, GLint max, GLint *len, GLchar *buf)) \
    X(GLuint,  CreateProgram,     (void)) \
    X(void,    AttachShader,      (GLuint prog, GLuint s)) \
    X(void,    LinkProgram,       (GLuint prog)) \
    X(void,    GetProgramiv,      (GLuint prog, GLenum p, GLint *v)) \
    X(void,    GetProgramInfoLog, (GLuint prog, GLint max, GLint *len, GLchar *buf)) \
    X(void,    UseProgram,        (GLuint prog)) \
    X(void,    DeleteShader,      (GLuint s)) \
    X(void,    GenVertexArrays,   (GLint n, GLuint *a)) \
    X(void,    BindVertexArray,   (GLuint a)) \
    X(void,    DrawArrays,        (GLenum mode, GLint first, GLint count)) \
    X(GLint,   GetUniformLocation,(GLuint prog, const GLchar *name)) \
    X(void,    Uniform1f,         (GLint loc, GLfloat v))

#define X(ret, name, args) typedef ret (*PFNgl##name##PROC) args;
GL_FUNC_LIST
#undef X

#define X(ret, name, args) static PFNgl##name##PROC pgl##name = NULL;
GL_FUNC_LIST
#undef X

static retro_hw_get_proc_address_t s_get_proc_address = NULL;

static void gl_load_procs(void) {
#define X(ret, name, args) pgl##name = (PFNgl##name##PROC)s_get_proc_address("gl"#name);
    GL_FUNC_LIST
#undef X
}

/* --------------------------------------------------------------------------
 * Core output geometry
 * -------------------------------------------------------------------------- */
#define CORE_WIDTH  320
#define CORE_HEIGHT 240
#define CORE_FPS    60.0

/* --------------------------------------------------------------------------
 * Frontend-provided callbacks
 * -------------------------------------------------------------------------- */
static retro_video_refresh_t       s_video_refresh   = NULL;
static retro_audio_sample_t        s_audio_sample    = NULL;
static retro_audio_sample_batch_t  s_audio_batch     = NULL;
static retro_input_poll_t          s_input_poll      = NULL;
static retro_input_state_t         s_input_state     = NULL;
static retro_environment_t         s_environ         = NULL;

/* --------------------------------------------------------------------------
 * HW render state
 * -------------------------------------------------------------------------- */
static struct retro_hw_render_callback s_hw = {0};
static GLuint s_prog    = 0;
static GLuint s_vao     = 0; /* dummy empty VAO — required by Core Profile */
static GLint  s_loc_time = -1;
static float  s_time    = 0.0f;
static bool   s_gl_ready = false;

/* --------------------------------------------------------------------------
 * Procedural spinning triangle — no vertex attributes.
 * Positions and colours are computed from gl_VertexID and u_time.
 * -------------------------------------------------------------------------- */
static const char *s_vert_src =
    "#version 330 core\n"
    "uniform float u_time;\n"
    "out vec3 v_color;\n"
    "void main() {\n"
    "    const vec2 pos[3] = vec2[3](\n"
    "        vec2( 0.00,  0.75),\n"
    "        vec2(-0.65, -0.50),\n"
    "        vec2( 0.65, -0.50)\n"
    "    );\n"
    "    const vec3 col[3] = vec3[3](\n"
    "        vec3(1.0, 0.0, 0.0),\n"
    "        vec3(0.0, 1.0, 0.0),\n"
    "        vec3(0.0, 0.0, 1.0)\n"
    "    );\n"
    "    float c = cos(u_time), s = sin(u_time);\n"
    "    vec2 p = vec2(c*pos[gl_VertexID].x - s*pos[gl_VertexID].y,\n"
    "                  s*pos[gl_VertexID].x + c*pos[gl_VertexID].y);\n"
    "    gl_Position = vec4(p * 0.8, 0.0, 1.0);\n"
    "    v_color = col[gl_VertexID];\n"
    "}\n";

static const char *s_frag_src =
    "#version 330 core\n"
    "in vec3 v_color;\n"
    "out vec4 frag_color;\n"
    "void main() { frag_color = vec4(v_color, 1.0); }\n";

/* --------------------------------------------------------------------------
 * HW render callbacks
 * -------------------------------------------------------------------------- */
static void context_reset(void) {
    gl_load_procs();

    /* Verify critical pointers loaded. */
    if (!pglCreateShader || !pglDrawArrays || !pglGenVertexArrays || !pglBindVertexArray) {
        s_environ(RETRO_ENVIRONMENT_SET_MESSAGE,
            &(struct retro_message){"test_opengl: GL proc load failed", 300});
        return;
    }

    char log[512];
    GLint ok;

    GLuint vs = pglCreateShader(GL_VERTEX_SHADER);
    pglShaderSource(vs, 1, &s_vert_src, NULL);
    pglCompileShader(vs);
    pglGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        pglGetShaderInfoLog(vs, sizeof(log), NULL, log);
        s_environ(RETRO_ENVIRONMENT_SET_MESSAGE,
            &(struct retro_message){"test_opengl: vert shader error", 300});
        return;
    }

    GLuint fs = pglCreateShader(GL_FRAGMENT_SHADER);
    pglShaderSource(fs, 1, &s_frag_src, NULL);
    pglCompileShader(fs);
    pglGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        pglDeleteShader(vs);
        s_environ(RETRO_ENVIRONMENT_SET_MESSAGE,
            &(struct retro_message){"test_opengl: frag shader error", 300});
        return;
    }

    s_prog = pglCreateProgram();
    pglAttachShader(s_prog, vs);
    pglAttachShader(s_prog, fs);
    pglLinkProgram(s_prog);
    pglDeleteShader(vs);
    pglDeleteShader(fs);
    pglGetProgramiv(s_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        pglGetProgramInfoLog(s_prog, sizeof(log), NULL, log);
        s_environ(RETRO_ENVIRONMENT_SET_MESSAGE,
            &(struct retro_message){"test_opengl: link error", 300});
        s_prog = 0;
        return;
    }

    s_loc_time = pglGetUniformLocation(s_prog, "u_time");

    /* Core Profile requires a non-zero VAO bound for glDrawArrays,
     * even when drawing without vertex attributes. */
    pglGenVertexArrays(1, &s_vao);
    if (s_vao == 0) {
        s_environ(RETRO_ENVIRONMENT_SET_MESSAGE,
            &(struct retro_message){"test_opengl: GenVertexArrays returned 0", 300});
        return;
    }

    s_gl_ready = true;
}

static void context_destroy(void) {
    s_gl_ready = false;
    s_prog = 0;
    s_vao  = 0;
}

/* --------------------------------------------------------------------------
 * libretro API
 * -------------------------------------------------------------------------- */
void retro_set_video_refresh(retro_video_refresh_t cb)           { s_video_refresh = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb)             { s_audio_sample  = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { s_audio_batch   = cb; }
void retro_set_input_poll(retro_input_poll_t cb)                 { s_input_poll    = cb; }
void retro_set_input_state(retro_input_state_t cb)               { s_input_state   = cb; }

void retro_set_environment(retro_environment_t cb) {
    s_environ = cb;

    bool no_game = true;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);

    s_hw.context_type    = RETRO_HW_CONTEXT_OPENGL_CORE;
    s_hw.version_major   = 3;
    s_hw.version_minor   = 3;
    s_hw.context_reset   = context_reset;
    s_hw.context_destroy = context_destroy;
    s_hw.depth           = false;
    s_hw.stencil         = false;
    s_hw.bottom_left_origin = true;

    if (!cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &s_hw)) {
        s_hw.context_type  = RETRO_HW_CONTEXT_OPENGL;
        s_hw.version_major = 0;
        s_hw.version_minor = 0;
        cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &s_hw);
    }
}

void retro_init(void)   {}
void retro_deinit(void) {}

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info *info) {
    memset(info, 0, sizeof(*info));
    info->library_name     = "test_opengl";
    info->library_version  = "1.0";
    info->valid_extensions = "";
    info->need_fullpath    = false;
    info->block_extract    = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info) {
    memset(info, 0, sizeof(*info));
    info->geometry.base_width   = CORE_WIDTH;
    info->geometry.base_height  = CORE_HEIGHT;
    info->geometry.max_width    = CORE_WIDTH;
    info->geometry.max_height   = CORE_HEIGHT;
    info->geometry.aspect_ratio = (float)CORE_WIDTH / (float)CORE_HEIGHT;
    info->timing.fps            = CORE_FPS;
    info->timing.sample_rate    = 44100.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }
void retro_reset(void) { s_time = 0.0f; }

void retro_run(void) {
    s_input_poll();

    if (!s_gl_ready) {
        s_video_refresh(RETRO_HW_FRAME_BUFFER_VALID, CORE_WIDTH, CORE_HEIGHT, 0);
        return;
    }

    s_time += (float)(1.0 / CORE_FPS);

    pglViewport(0, 0, CORE_WIDTH, CORE_HEIGHT);
    pglClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    pglClear(GL_COLOR_BUFFER_BIT);

    pglUseProgram(s_prog);
    if (s_loc_time >= 0)
        pglUniform1f(s_loc_time, s_time);

    pglBindVertexArray(s_vao);
    pglDrawArrays(GL_TRIANGLES, 0, 3);
    pglBindVertexArray(0);
    pglUseProgram(0);

    s_video_refresh(RETRO_HW_FRAME_BUFFER_VALID, CORE_WIDTH, CORE_HEIGHT, 0);
}

bool retro_load_game(const struct retro_game_info *game) {
    (void)game;
    s_get_proc_address = s_hw.get_proc_address;
    return true;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num) {
    (void)type; (void)info; (void)num;
    return false;
}

void retro_unload_game(void) {}

size_t retro_serialize_size(void)                            { return 0; }
bool   retro_serialize(void *data, size_t size)              { (void)data; (void)size; return false; }
bool   retro_unserialize(const void *data, size_t size)      { (void)data; (void)size; return false; }
void   retro_cheat_reset(void)                               {}
void   retro_cheat_set(unsigned i, bool e, const char *c)    { (void)i; (void)e; (void)c; }
unsigned retro_get_region(void)                              { return RETRO_REGION_NTSC; }
void  *retro_get_memory_data(unsigned id)                    { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id)                    { (void)id; return 0; }
