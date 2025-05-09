#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>

// quad vertices
static const float quadVerts[] = {
    -1.0f, -1.0f, 0.0f, 0.0f,
     1.0f, -1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 1.0f,
     1.0f,  1.0f, 1.0f, 1.0f,
};

static const char* vertSrc = R"glsl(
#version 330 core
layout(location=0) in vec2 pos;
layout(location=1) in vec2 uv;
out vec2 vUV;
uniform float uTime;
uniform float zoomDuration;
uniform float stretchDuration;
uniform float uStartSkew;

void main() {
    vUV = uv;

    float scaleT = clamp(uTime / zoomDuration, 0.0, 1.0);
    float stretchT = clamp((uTime - zoomDuration) / stretchDuration, 0.0, 1.0);

    float scale = mix(0.0, 1.0, scaleT);
    float skew = mix(uStartSkew, 0.0, scaleT);

    mat2 skewMat = mat2(1.0, skew, 0.0, 1.0);
    vec2 transformed = skewMat * pos * scale;


    float maxStretchY = 1.0;
    float stretchY = mix(1.0, maxStretchY, stretchT);
    transformed.y *= stretchY;

    // Horizontal mirror
    transformed.x = -transformed.x;

    gl_Position = vec4(transformed, 0.0, 1.0);
}
)glsl";

static const char* fragSrc = R"glsl(
#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D tex;
void main() {
    fragColor = texture(tex, vUV);
}
)glsl";

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetShaderInfoLog(s, 512, nullptr, log);
        std::cerr << "Shader error:\n" << log << "\n";
    }
    return s;
}

GLuint linkProgram(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetProgramInfoLog(p, 512, nullptr, log);
        std::cerr << "Link error:\n" << log << "\n";
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* win = glfwCreateWindow(800, 600, "Rhombus Zoom and Stretch", nullptr, nullptr);
    if (!win) return -1;
    glfwMakeContextCurrent(win);
    glewInit();

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    GLuint prog = linkProgram(
        compileShader(GL_VERTEX_SHADER, vertSrc),
        compileShader(GL_FRAGMENT_SHADER, fragSrc)
    );

    // checkerboard texture
    const int TW = 256, TH = 256;
    unsigned char texData[TW * TH * 3];
    for (int y = 0; y < TH; ++y) {
        for (int x = 0; x < TW; ++x) {
            bool b = ((x / 32) % 2) ^ ((y / 32) % 2);
            unsigned char v = b ? 255 : 80;
            texData[3 * (y * TW + x) + 0] = v;
            texData[3 * (y * TW + x) + 1] = v;
            texData[3 * (y * TW + x) + 2] = v;
        }
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TW, TH, 0, GL_RGB, GL_UNSIGNED_BYTE, texData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLint locTime = glGetUniformLocation(prog, "uTime");
    GLint locZoomDuration = glGetUniformLocation(prog, "zoomDuration");
    GLint locStretchDuration = glGetUniformLocation(prog, "stretchDuration");
    GLint locStartSkew = glGetUniformLocation(prog, "uStartSkew");

    float startTime = (float)glfwGetTime();
    const float ZOOM_DURATION = 3.0f;
    const float STRETCH_DURATION = 1.5f;
    const float START_SKEW = tanf(45.0f * 3.14159f / 180.0f);

    while (!glfwWindowShouldClose(win)) {
        float now = (float)glfwGetTime();
        float t = now - startTime;

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(glGetUniformLocation(prog, "tex"), 0);
        glUniform1f(locTime, t);
        glUniform1f(locZoomDuration, ZOOM_DURATION);
        glUniform1f(locStretchDuration, STRETCH_DURATION);
        glUniform1f(locStartSkew, START_SKEW);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
