#include "hud.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <glad/gl.h>

#define STB_EASY_FONT_IMPLEMENTATION
#include <stb_easy_font.h>

namespace cinerig::examples
{
namespace
{

constexpr const char* kUiVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;
out vec4 vColor;
uniform vec2 uViewport;
void main()
{
    vec2 ndc = vec2(
        aPosition.x / uViewport.x * 2.0 - 1.0,
        1.0 - aPosition.y / uViewport.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vColor = aColor;
}
)";

constexpr const char* kUiFragmentShader = R"(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main()
{
    FragColor = vColor;
}
)";

struct UiVertex
{
    float x = 0.0f;
    float y = 0.0f;
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
    unsigned char a = 255;
};

struct RawFontVertex
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
    unsigned char a = 255;
};

static_assert(sizeof(RawFontVertex) == 16, "stb_easy_font vertex layout mismatch");

GLuint compileShader(GLenum type, const char* source, std::string* error)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE)
    {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(std::max(logLength, 1)), '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        if (error != nullptr)
            *error = log;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint createProgram(const char* vertexSource, const char* fragmentSource, std::string* error)
{
    const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, error);
    if (vertexShader == 0)
        return 0;
    const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, error);
    if (fragmentShader == 0)
    {
        glDeleteShader(vertexShader);
        return 0;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE)
    {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(std::max(logLength, 1)), '\0');
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        if (error != nullptr)
            *error = log;
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

}

struct Hud::Impl
{
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ibo = 0;
    GLint viewportLocation = -1;
    std::size_t vboCapacity = 0;
    std::size_t iboCapacity = 0;
    bool initialized = false;
    std::vector<UiVertex> vertices;
    std::vector<std::uint32_t> indices;
};

Hud::Hud()
    : impl_(std::make_unique<Impl>())
{
}

Hud::~Hud()
{
    shutdown();
}

bool Hud::initialize(std::string* error)
{
    shutdown();

    impl_->program = createProgram(kUiVertexShader, kUiFragmentShader, error);
    if (impl_->program == 0)
        return false;

    impl_->viewportLocation = glGetUniformLocation(impl_->program, "uViewport");

    glGenVertexArrays(1, &impl_->vao);
    glGenBuffers(1, &impl_->vbo);
    glGenBuffers(1, &impl_->ibo);

    glBindVertexArray(impl_->vao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, impl_->ibo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        4,
        GL_UNSIGNED_BYTE,
        GL_TRUE,
        sizeof(UiVertex),
        reinterpret_cast<void*>(offsetof(UiVertex, r)));
    glBindVertexArray(0);

    impl_->initialized = true;
    return true;
}

void Hud::shutdown()
{
    if (impl_ == nullptr)
        return;

    if (impl_->ibo != 0)
        glDeleteBuffers(1, &impl_->ibo);
    if (impl_->vbo != 0)
        glDeleteBuffers(1, &impl_->vbo);
    if (impl_->vao != 0)
        glDeleteVertexArrays(1, &impl_->vao);
    if (impl_->program != 0)
        glDeleteProgram(impl_->program);

    impl_->ibo = 0;
    impl_->vbo = 0;
    impl_->vao = 0;
    impl_->program = 0;
    impl_->vboCapacity = 0;
    impl_->iboCapacity = 0;
    impl_->initialized = false;
}

bool Hud::isReady() const noexcept
{
    return impl_ != nullptr && impl_->initialized;
}

void Hud::begin()
{
    lines_.clear();
}

void Hud::addLine(std::string line)
{
    lines_.push_back(std::move(line));
}

void Hud::addLine(std::string_view label, std::string_view value)
{
    std::string line;
    line.reserve(label.size() + value.size() + 2);
    line.append(label);
    line.append(": ");
    line.append(value);
    lines_.push_back(std::move(line));
}

const std::vector<std::string>& Hud::lines() const noexcept
{
    return lines_;
}

void Hud::setTextScale(float scale)
{
    textScale_ = std::max(1.0f, scale);
}

float Hud::textScale() const noexcept
{
    return textScale_;
}

void Hud::setColor(math::Vec3 color)
{
    color_ = math::clamp(color, math::Vec3::zero, math::Vec3::one);
}

void Hud::render(int framebufferWidth, int framebufferHeight)
{
    if (!isReady() || framebufferWidth <= 0 || framebufferHeight <= 0)
        return;

    impl_->vertices.clear();
    impl_->indices.clear();

    const float scale = textScale_;
    const float margin = 10.0f * scale;
    const float lineSpacing = 17.0f * scale;
    const float panelHeight = lines_.empty()
        ? 0.0f
        : margin * 2.0f + lineSpacing * static_cast<float>(lines_.size());

    float maxTextWidth = 0.0f;
    std::vector<std::string> mutableLines = lines_;
    for (const std::string& line : mutableLines)
    {
        maxTextWidth = std::max(
            maxTextWidth,
            static_cast<float>(stb_easy_font_width(const_cast<char*>(line.data()))));
    }

    const float panelWidth = std::min(
        std::max(1.0f, static_cast<float>(framebufferWidth) - 8.0f),
        std::max(180.0f * scale, margin * 2.0f + maxTextWidth * scale));

    auto appendQuad = [this](
                          float x,
                          float y,
                          float width,
                          float height,
                          unsigned char r,
                          unsigned char g,
                          unsigned char b,
                          unsigned char a) {
        const std::uint32_t base = static_cast<std::uint32_t>(impl_->vertices.size());
        impl_->vertices.push_back({x, y, r, g, b, a});
        impl_->vertices.push_back({x + width, y, r, g, b, a});
        impl_->vertices.push_back({x, y + height, r, g, b, a});
        impl_->vertices.push_back({x + width, y + height, r, g, b, a});
        impl_->indices.insert(impl_->indices.end(), {base, base + 1, base + 2, base + 2, base + 1, base + 3});
    };

    if (!lines_.empty())
        appendQuad(8.0f, 8.0f, panelWidth, panelHeight, 0, 0, 0, 145);

    for (std::size_t index = 0; index < mutableLines.size(); ++index)
    {
        const std::string& line = mutableLines[index];
        if (line.empty())
            continue;

        std::vector<unsigned char> fontBuffer(line.size() * 300 + 1024);
        unsigned char textColor[4] = {
            static_cast<unsigned char>(color_.x * 255.0f),
            static_cast<unsigned char>(color_.y * 255.0f),
            static_cast<unsigned char>(color_.z * 255.0f),
            255,
        };
        const int quadCount = stb_easy_font_print(
            0.0f,
            0.0f,
            const_cast<char*>(line.data()),
            textColor,
            fontBuffer.data(),
            static_cast<int>(fontBuffer.size()));
        const auto* rawVertices = reinterpret_cast<const RawFontVertex*>(fontBuffer.data());
        const float originX = 8.0f + margin;
        const float originY = 8.0f + margin + lineSpacing * static_cast<float>(index);

        for (int quad = 0; quad < quadCount; ++quad)
        {
            const std::uint32_t base = static_cast<std::uint32_t>(impl_->vertices.size());
            for (int vertex = 0; vertex < 4; ++vertex)
            {
                const RawFontVertex& raw = rawVertices[quad * 4 + vertex];
                impl_->vertices.push_back({
                    originX + raw.x * scale,
                    originY + raw.y * scale,
                    raw.r,
                    raw.g,
                    raw.b,
                    raw.a,
                });
            }
            impl_->indices.insert(impl_->indices.end(), {base, base + 1, base + 2, base + 2, base + 1, base + 3});
        }
    }

    if (impl_->vertices.empty() || impl_->indices.empty())
        return;

    glBindVertexArray(impl_->vao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->vbo);
    const std::size_t vertexBytes = impl_->vertices.size() * sizeof(UiVertex);
    if (vertexBytes > impl_->vboCapacity)
    {
        impl_->vboCapacity = vertexBytes * 2;
        glBufferData(
            GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(impl_->vboCapacity), nullptr, GL_DYNAMIC_DRAW);
    }
    glBufferSubData(
        GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(vertexBytes), impl_->vertices.data());

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, impl_->ibo);
    const std::size_t indexBytes = impl_->indices.size() * sizeof(std::uint32_t);
    if (indexBytes > impl_->iboCapacity)
    {
        impl_->iboCapacity = indexBytes * 2;
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(impl_->iboCapacity),
            nullptr,
            GL_DYNAMIC_DRAW);
    }
    glBufferSubData(
        GL_ELEMENT_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(indexBytes), impl_->indices.data());

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(impl_->program);
    glUniform2f(
        impl_->viewportLocation,
        static_cast<float>(framebufferWidth),
        static_cast<float>(framebufferHeight));
    glDrawElements(
        GL_TRIANGLES,
        static_cast<GLsizei>(impl_->indices.size()),
        GL_UNSIGNED_INT,
        nullptr);
    glUseProgram(0);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

}
