#include "gl_renderer.hpp"

#include "sdl_camera_output.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <glad/gl.h>

namespace cinerig::examples
{
namespace
{

constexpr const char* kBasicVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
out vec3 vNormal;
out vec3 vColor;
void main()
{
    vec4 world = uModel * vec4(aPosition, 1.0);
    gl_Position = uProjection * uView * world;
    vNormal = mat3(uModel) * aNormal;
    vColor = aColor;
}
)";

constexpr const char* kBasicFragmentShader = R"(
#version 330 core
in vec3 vNormal;
in vec3 vColor;
uniform vec3 uTint;
out vec4 FragColor;
void main()
{
    vec3 normal = normalize(vNormal);
    vec3 lightDirection = normalize(vec3(0.35, 0.85, 0.35));
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float light = 0.35 + diffuse * 0.65;
    FragColor = vec4(vColor * uTint * light, 1.0);
}
)";

constexpr const char* kLineVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;
uniform mat4 uView;
uniform mat4 uProjection;
out vec3 vColor;
void main()
{
    gl_Position = uProjection * uView * vec4(aPosition, 1.0);
    vColor = aColor;
}
)";

constexpr const char* kLineFragmentShader = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main()
{
    FragColor = vec4(vColor, 1.0);
}
)";

constexpr const char* kScreenTextureVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
uniform vec2 uViewport;
uniform vec2 uCenter;
uniform vec2 uSize;
uniform float uRotation;
out vec2 vTexCoord;
void main()
{
    vec2 scaled = aPosition * uSize;
    float c = cos(uRotation);
    float s = sin(uRotation);
    vec2 rotated = vec2(
        scaled.x * c - scaled.y * s,
        scaled.x * s + scaled.y * c);
    vec2 screen = uCenter + rotated;
    vec2 ndc = vec2(
        screen.x / uViewport.x * 2.0 - 1.0,
        screen.y / uViewport.y * 2.0 - 1.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

constexpr const char* kScreenTextureFragmentShader = R"(
#version 330 core
in vec2 vTexCoord;
uniform sampler2D uTexture;
uniform vec3 uTint;
uniform float uAlpha;
out vec4 FragColor;
void main()
{
    vec4 color = texture(uTexture, vTexCoord);
    FragColor = vec4(color.rgb * uTint, color.a * uAlpha);
}
)";

struct BasicVertex
{
    math::Vec3 position;
    math::Vec3 normal;
    math::Vec3 color{1.0f, 1.0f, 1.0f};
};

struct LineVertex
{
    math::Vec3 position;
    math::Vec3 color;
};

struct ScreenVertex
{
    math::Vec2 position;
    math::Vec2 texCoord;
};

struct MeshData
{
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;
};

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

std::string readShaderSource(const char* fileName, const char* fallback)
{
#ifdef CINERIG_EXAMPLE_SHADER_DIR
    const std::string path = std::string(CINERIG_EXAMPLE_SHADER_DIR) + "/" + fileName;
    std::ifstream input(path);
    if (input)
    {
        std::string source(
            (std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (!source.empty())
            return source;
    }
#else
    (void)fileName;
#endif
    return fallback;
}

void addQuad(
    std::vector<BasicVertex>& vertices,
    std::vector<std::uint32_t>& indices,
    math::Vec3 a,
    math::Vec3 b,
    math::Vec3 c,
    math::Vec3 d,
    math::Vec3 normal)
{
    const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());
    vertices.push_back({a, normal, math::Vec3::one});
    vertices.push_back({b, normal, math::Vec3::one});
    vertices.push_back({c, normal, math::Vec3::one});
    vertices.push_back({d, normal, math::Vec3::one});
    indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

void createGpuMesh(
    const std::vector<BasicVertex>& vertices,
    const std::vector<std::uint32_t>& indices,
    MeshData& mesh)
{
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(BasicVertex)),
        vertices.data(),
        GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
        indices.data(),
        GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(BasicVertex),
        reinterpret_cast<void*>(offsetof(BasicVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(BasicVertex),
        reinterpret_cast<void*>(offsetof(BasicVertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(BasicVertex),
        reinterpret_cast<void*>(offsetof(BasicVertex, color)));

    glBindVertexArray(0);
    mesh.indexCount = static_cast<GLsizei>(indices.size());
}

}

struct GlRenderer::Impl
{
    struct Uniforms
    {
        GLint model = -1;
        GLint view = -1;
        GLint projection = -1;
        GLint tint = -1;
    };

    GLuint basicProgram = 0;
    GLuint lineProgram = 0;
    GLuint screenTextureProgram = 0;
    Uniforms basicUniforms;
    GLuint lineVao = 0;
    GLuint lineVbo = 0;
    GLint lineView = -1;
    GLint lineProjection = -1;
    GLuint screenTextureVao = 0;
    GLuint screenTextureVbo = 0;
    GLint screenTextureViewport = -1;
    GLint screenTextureCenter = -1;
    GLint screenTextureSize = -1;
    GLint screenTextureRotation = -1;
    GLint screenTextureTint = -1;
    GLint screenTextureAlpha = -1;
    GLint screenTextureSampler = -1;
    std::size_t lineCapacity = 0;
    std::vector<LineVertex> lines;
    std::vector<MeshData> meshes;
    std::vector<GLuint> textures;
    math::Mat4 view = math::Mat4::identity();
    math::Mat4 projection = math::Mat4::identity();
    bool initialized = false;
    float lineWidth = 1.0f;
};

GlRenderer::GlRenderer()
    : impl_(std::make_unique<Impl>())
{
}

GlRenderer::~GlRenderer()
{
    shutdown();
}

bool GlRenderer::initialize(std::string* error)
{
    shutdown();

    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)) == 0)
    {
        if (error != nullptr)
            *error = "failed to load OpenGL with glad";
        return false;
    }

    const std::string basicVertex = readShaderSource("basic.vert", kBasicVertexShader);
    const std::string basicFragment = readShaderSource("basic.frag", kBasicFragmentShader);
    const std::string lineVertex = readShaderSource("line.vert", kLineVertexShader);
    const std::string lineFragment = readShaderSource("line.frag", kLineFragmentShader);

    impl_->basicProgram = createProgram(basicVertex.c_str(), basicFragment.c_str(), error);
    if (impl_->basicProgram == 0)
        return false;
    impl_->lineProgram = createProgram(lineVertex.c_str(), lineFragment.c_str(), error);
    if (impl_->lineProgram == 0)
    {
        glDeleteProgram(impl_->basicProgram);
        impl_->basicProgram = 0;
        return false;
    }
    impl_->screenTextureProgram = createProgram(
        kScreenTextureVertexShader, kScreenTextureFragmentShader, error);
    if (impl_->screenTextureProgram == 0)
    {
        glDeleteProgram(impl_->lineProgram);
        glDeleteProgram(impl_->basicProgram);
        impl_->lineProgram = 0;
        impl_->basicProgram = 0;
        return false;
    }

    impl_->basicUniforms.model = glGetUniformLocation(impl_->basicProgram, "uModel");
    impl_->basicUniforms.view = glGetUniformLocation(impl_->basicProgram, "uView");
    impl_->basicUniforms.projection = glGetUniformLocation(impl_->basicProgram, "uProjection");
    impl_->basicUniforms.tint = glGetUniformLocation(impl_->basicProgram, "uTint");
    impl_->lineView = glGetUniformLocation(impl_->lineProgram, "uView");
    impl_->lineProjection = glGetUniformLocation(impl_->lineProgram, "uProjection");
    impl_->screenTextureViewport = glGetUniformLocation(
        impl_->screenTextureProgram, "uViewport");
    impl_->screenTextureCenter = glGetUniformLocation(
        impl_->screenTextureProgram, "uCenter");
    impl_->screenTextureSize = glGetUniformLocation(
        impl_->screenTextureProgram, "uSize");
    impl_->screenTextureRotation = glGetUniformLocation(
        impl_->screenTextureProgram, "uRotation");
    impl_->screenTextureTint = glGetUniformLocation(
        impl_->screenTextureProgram, "uTint");
    impl_->screenTextureAlpha = glGetUniformLocation(
        impl_->screenTextureProgram, "uAlpha");
    impl_->screenTextureSampler = glGetUniformLocation(
        impl_->screenTextureProgram, "uTexture");

    glGenVertexArrays(1, &impl_->lineVao);
    glGenBuffers(1, &impl_->lineVbo);
    glBindVertexArray(impl_->lineVao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->lineVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(LineVertex),
        reinterpret_cast<void*>(offsetof(LineVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(LineVertex),
        reinterpret_cast<void*>(offsetof(LineVertex, color)));
    glBindVertexArray(0);

    constexpr ScreenVertex screenQuad[] = {
        {{-0.5f, -0.5f}, {0.0f, 0.0f}},
        {{0.5f, -0.5f}, {1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 1.0f}},
        {{0.5f, 0.5f}, {1.0f, 1.0f}},
    };
    glGenVertexArrays(1, &impl_->screenTextureVao);
    glGenBuffers(1, &impl_->screenTextureVbo);
    glBindVertexArray(impl_->screenTextureVao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->screenTextureVbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(sizeof(screenQuad)),
        screenQuad,
        GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(ScreenVertex),
        reinterpret_cast<void*>(offsetof(ScreenVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(ScreenVertex),
        reinterpret_cast<void*>(offsetof(ScreenVertex, texCoord)));
    glBindVertexArray(0);

    impl_->initialized = true;
    return true;
}

void GlRenderer::shutdown()
{
    if (impl_ == nullptr)
        return;

    for (const MeshData& mesh : impl_->meshes)
    {
        if (mesh.ebo != 0)
            glDeleteBuffers(1, &mesh.ebo);
        if (mesh.vbo != 0)
            glDeleteBuffers(1, &mesh.vbo);
        if (mesh.vao != 0)
            glDeleteVertexArrays(1, &mesh.vao);
    }
    impl_->meshes.clear();

    for (const GLuint texture : impl_->textures)
    {
        if (texture != 0)
            glDeleteTextures(1, &texture);
    }
    impl_->textures.clear();

    if (impl_->screenTextureVbo != 0)
        glDeleteBuffers(1, &impl_->screenTextureVbo);
    if (impl_->screenTextureVao != 0)
        glDeleteVertexArrays(1, &impl_->screenTextureVao);
    if (impl_->lineVbo != 0)
        glDeleteBuffers(1, &impl_->lineVbo);
    if (impl_->lineVao != 0)
        glDeleteVertexArrays(1, &impl_->lineVao);
    if (impl_->screenTextureProgram != 0)
        glDeleteProgram(impl_->screenTextureProgram);
    if (impl_->lineProgram != 0)
        glDeleteProgram(impl_->lineProgram);
    if (impl_->basicProgram != 0)
        glDeleteProgram(impl_->basicProgram);

    impl_->screenTextureVbo = 0;
    impl_->screenTextureVao = 0;
    impl_->lineVbo = 0;
    impl_->lineVao = 0;
    impl_->screenTextureProgram = 0;
    impl_->lineProgram = 0;
    impl_->basicProgram = 0;
    impl_->lineCapacity = 0;
    impl_->initialized = false;
}

bool GlRenderer::isReady() const noexcept
{
    return impl_ != nullptr && impl_->initialized;
}

GlRenderer::MeshHandle GlRenderer::createCube(float size)
{
    if (!isReady())
        return 0;

    std::vector<BasicVertex> vertices;
    std::vector<std::uint32_t> indices;
    const float half = std::max(0.001f, size) * 0.5f;
    const math::Vec3 p000{-half, -half, -half};
    const math::Vec3 p100{half, -half, -half};
    const math::Vec3 p010{-half, half, -half};
    const math::Vec3 p110{half, half, -half};
    const math::Vec3 p001{-half, -half, half};
    const math::Vec3 p101{half, -half, half};
    const math::Vec3 p011{-half, half, half};
    const math::Vec3 p111{half, half, half};
    addQuad(vertices, indices, p001, p101, p111, p011, math::Vec3::back);
    addQuad(vertices, indices, p100, p000, p010, p110, math::Vec3::forward);
    addQuad(vertices, indices, p000, p001, p011, p010, math::Vec3::left);
    addQuad(vertices, indices, p101, p100, p110, p111, math::Vec3::right);
    addQuad(vertices, indices, p010, p011, p111, p110, math::Vec3::up);
    addQuad(vertices, indices, p000, p100, p101, p001, math::Vec3::down);

    impl_->meshes.emplace_back();
    createGpuMesh(vertices, indices, impl_->meshes.back());
    return static_cast<MeshHandle>(impl_->meshes.size());
}

GlRenderer::MeshHandle GlRenderer::createSphere(float radius, int slices, int stacks)
{
    if (!isReady())
        return 0;

    const float safeRadius = std::max(0.001f, radius);
    const int safeSlices = std::max(3, slices);
    const int safeStacks = std::max(2, stacks);
    std::vector<BasicVertex> vertices;
    std::vector<std::uint32_t> indices;

    for (int stack = 0; stack <= safeStacks; ++stack)
    {
        const float v = static_cast<float>(stack) / static_cast<float>(safeStacks);
        const float phi = v * math::Pi;
        const float y = std::cos(phi);
        const float ring = std::sin(phi);
        for (int slice = 0; slice <= safeSlices; ++slice)
        {
            const float u = static_cast<float>(slice) / static_cast<float>(safeSlices);
            const float theta = u * math::TwoPi;
            const math::Vec3 normal{ring * std::cos(theta), y, ring * std::sin(theta)};
            vertices.push_back({normal * safeRadius, normal, math::Vec3::one});
        }
    }

    for (int stack = 0; stack < safeStacks; ++stack)
    {
        for (int slice = 0; slice < safeSlices; ++slice)
        {
            const std::uint32_t a = static_cast<std::uint32_t>(stack * (safeSlices + 1) + slice);
            const std::uint32_t b = a + static_cast<std::uint32_t>(safeSlices + 1);
            indices.insert(indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }

    impl_->meshes.emplace_back();
    createGpuMesh(vertices, indices, impl_->meshes.back());
    return static_cast<MeshHandle>(impl_->meshes.size());
}

GlRenderer::MeshHandle GlRenderer::createCylinder(float radius, float height, int slices)
{
    if (!isReady())
        return 0;

    const float safeRadius = std::max(0.001f, radius);
    const float safeHeight = std::max(0.001f, height);
    const int safeSlices = std::max(3, slices);
    const float halfHeight = safeHeight * 0.5f;
    std::vector<BasicVertex> vertices;
    std::vector<std::uint32_t> indices;

    for (int slice = 0; slice <= safeSlices; ++slice)
    {
        const float angle = static_cast<float>(slice) / static_cast<float>(safeSlices) * math::TwoPi;
        const math::Vec3 radial{std::cos(angle), 0.0f, std::sin(angle)};
        vertices.push_back({radial * safeRadius + math::Vec3{0.0f, -halfHeight, 0.0f}, radial, math::Vec3::one});
        vertices.push_back({radial * safeRadius + math::Vec3{0.0f, halfHeight, 0.0f}, radial, math::Vec3::one});
    }

    for (int slice = 0; slice < safeSlices; ++slice)
    {
        const std::uint32_t base = static_cast<std::uint32_t>(slice * 2);
        indices.insert(indices.end(), {base, base + 2, base + 1, base + 1, base + 2, base + 3});
    }

    const std::uint32_t topCenter = static_cast<std::uint32_t>(vertices.size());
    vertices.push_back({{0.0f, halfHeight, 0.0f}, math::Vec3::up, math::Vec3::one});
    const std::uint32_t topRing = static_cast<std::uint32_t>(vertices.size());
    for (int slice = 0; slice < safeSlices; ++slice)
    {
        const float angle = static_cast<float>(slice) / static_cast<float>(safeSlices) * math::TwoPi;
        const math::Vec3 radial{std::cos(angle), 0.0f, std::sin(angle)};
        vertices.push_back({radial * safeRadius + math::Vec3{0.0f, halfHeight, 0.0f}, math::Vec3::up, math::Vec3::one});
    }
    for (int slice = 0; slice < safeSlices; ++slice)
    {
        const std::uint32_t next = static_cast<std::uint32_t>((slice + 1) % safeSlices);
        indices.insert(indices.end(), {topCenter, topRing + static_cast<std::uint32_t>(slice), topRing + next});
    }

    const std::uint32_t bottomCenter = static_cast<std::uint32_t>(vertices.size());
    vertices.push_back({{0.0f, -halfHeight, 0.0f}, math::Vec3::down, math::Vec3::one});
    const std::uint32_t bottomRing = static_cast<std::uint32_t>(vertices.size());
    for (int slice = 0; slice < safeSlices; ++slice)
    {
        const float angle = static_cast<float>(slice) / static_cast<float>(safeSlices) * math::TwoPi;
        const math::Vec3 radial{std::cos(angle), 0.0f, std::sin(angle)};
        vertices.push_back({radial * safeRadius + math::Vec3{0.0f, -halfHeight, 0.0f}, math::Vec3::down, math::Vec3::one});
    }
    for (int slice = 0; slice < safeSlices; ++slice)
    {
        const std::uint32_t next = static_cast<std::uint32_t>((slice + 1) % safeSlices);
        indices.insert(indices.end(), {bottomCenter, bottomRing + next, bottomRing + static_cast<std::uint32_t>(slice)});
    }

    impl_->meshes.emplace_back();
    createGpuMesh(vertices, indices, impl_->meshes.back());
    return static_cast<MeshHandle>(impl_->meshes.size());
}

GlRenderer::MeshHandle GlRenderer::createPlane(float halfSize)
{
    if (!isReady())
        return 0;

    const float size = std::max(0.001f, halfSize);
    std::vector<BasicVertex> vertices;
    std::vector<std::uint32_t> indices;
    addQuad(
        vertices,
        indices,
        {-size, 0.0f, -size},
        {-size, 0.0f, size},
        {size, 0.0f, size},
        {size, 0.0f, -size},
        math::Vec3::up);

    impl_->meshes.emplace_back();
    createGpuMesh(vertices, indices, impl_->meshes.back());
    return static_cast<MeshHandle>(impl_->meshes.size());
}

GlRenderer::TextureHandle GlRenderer::createTexture(
    int width,
    int height,
    const unsigned char* rgba)
{
    if (!isReady() || width <= 0 || height <= 0 || rgba == nullptr)
        return 0;

    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0)
        return 0;

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        rgba);
    glBindTexture(GL_TEXTURE_2D, 0);

    impl_->textures.push_back(texture);
    return static_cast<TextureHandle>(impl_->textures.size());
}

void GlRenderer::beginFrame(const SdlCameraOutput& camera, math::Vec3 clearColor)
{
    if (!isReady())
        return;

    impl_->view = camera.viewMatrix();
    impl_->projection = camera.projectionMatrix();
    impl_->lines.clear();

    glClearColor(clearColor.x, clearColor.y, clearColor.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_BLEND);
}

void GlRenderer::endFrame()
{
    if (!isReady())
        return;

    if (impl_->lines.empty())
        return;

    glBindVertexArray(impl_->lineVao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->lineVbo);
    const std::size_t bytes = impl_->lines.size() * sizeof(LineVertex);
    if (bytes > impl_->lineCapacity)
    {
        impl_->lineCapacity = bytes * 2;
        glBufferData(
            GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(impl_->lineCapacity), nullptr, GL_DYNAMIC_DRAW);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(bytes), impl_->lines.data());

    glLineWidth(impl_->lineWidth);
    glUseProgram(impl_->lineProgram);
    glUniformMatrix4fv(impl_->lineView, 1, GL_FALSE, impl_->view.m.data());
    glUniformMatrix4fv(impl_->lineProjection, 1, GL_FALSE, impl_->projection.m.data());
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(impl_->lines.size()));
    glUseProgram(0);
    glBindVertexArray(0);
    impl_->lines.clear();
}

void GlRenderer::drawMesh(MeshHandle mesh, const Transform& transform, math::Vec3 tint)
{
    if (!isReady() || mesh == 0 || mesh > impl_->meshes.size())
        return;

    const MeshData& data = impl_->meshes[mesh - 1];
    const math::Mat4 model = math::Mat4::translation(transform.position)
        * math::Mat4::rotation(transform.rotation)
        * math::Mat4::scale(transform.scale);

    glUseProgram(impl_->basicProgram);
    glUniformMatrix4fv(impl_->basicUniforms.model, 1, GL_FALSE, model.m.data());
    glUniformMatrix4fv(impl_->basicUniforms.view, 1, GL_FALSE, impl_->view.m.data());
    glUniformMatrix4fv(impl_->basicUniforms.projection, 1, GL_FALSE, impl_->projection.m.data());
    glUniform3f(impl_->basicUniforms.tint, tint.x, tint.y, tint.z);
    glBindVertexArray(data.vao);
    glDrawElements(GL_TRIANGLES, data.indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    glUseProgram(0);
}

void GlRenderer::drawGrid(float halfSize, float spacing, math::Vec3 color)
{
    if (!isReady())
        return;

    const float size = std::max(spacing, halfSize);
    const float step = std::max(0.01f, spacing);
    const int count = static_cast<int>(size / step);
    for (int index = -count; index <= count; ++index)
    {
        const float coordinate = static_cast<float>(index) * step;
        drawLine({-size, 0.0f, coordinate}, {size, 0.0f, coordinate}, color);
        drawLine({coordinate, 0.0f, -size}, {coordinate, 0.0f, size}, color);
    }
}

void GlRenderer::drawAxes(const Transform& transform, float length)
{
    const math::Vec3 origin = transform.position;
    const math::Vec3 right = transform.rotation * math::Vec3::right;
    const math::Vec3 up = transform.rotation * math::Vec3::up;
    const math::Vec3 forward = transform.rotation * math::Vec3::forward;
    drawLine(origin, origin + right * length, {0.95f, 0.25f, 0.25f});
    drawLine(origin, origin + up * length, {0.25f, 0.95f, 0.25f});
    drawLine(origin, origin + forward * length, {0.25f, 0.45f, 0.95f});
}

void GlRenderer::drawLine(math::Vec3 from, math::Vec3 to, math::Vec3 color)
{
    if (!isReady())
        return;
    impl_->lines.push_back({from, color});
    impl_->lines.push_back({to, color});
}

void GlRenderer::drawRay(math::Vec3 origin, math::Vec3 direction, float length, math::Vec3 color)
{
    const math::Vec3 normalized = math::normalize(direction);
    if (math::almostZero(normalized))
        return;
    drawLine(origin, origin + normalized * std::max(0.0f, length), color);
}

void GlRenderer::drawTargetMarker(math::Vec3 position, float size, math::Vec3 color)
{
    const float s = std::max(0.01f, size);
    drawLine(position - math::Vec3::right * s, position + math::Vec3::right * s, color);
    drawLine(position - math::Vec3::up * s, position + math::Vec3::up * s, color);
    drawLine(position - math::Vec3::forward * s, position + math::Vec3::forward * s, color);
}

void GlRenderer::drawCameraFrustum(const SdlCameraOutput& camera, math::Vec3 color)
{
    if (!camera.hasCamera())
        return;
    drawFrustum(camera.position(), camera.orientation(), camera.state().lens, camera.aspect(), color);
}

void GlRenderer::drawFrustum(
    math::Vec3 position,
    math::Quat orientation,
    const LensSettings& lens,
    float aspect,
    math::Vec3 color)
{
    const float safeAspect = aspect > math::Epsilon ? aspect : 1.0f;
    const float nearDistance = std::max(0.01f, lens.nearClipPlane);
    const float farDistance = std::max(
        nearDistance + 0.01f,
        std::min(lens.farClipPlane, 50.0f));
    const bool orthographic =
        lens.modeOverride == LensSettings::OverrideMode::Orthographic
        || (lens.modeOverride == LensSettings::OverrideMode::None && lens.orthographic);

    auto corners = [&](float distance) {
        const float halfHeight = orthographic
            ? lens.orthographicSize
            : std::tan(math::clamp(lens.fieldOfView, 0.01f, 179.0f) * math::DegToRad * 0.5f) * distance;
        const float halfWidth = halfHeight * safeAspect;
        return std::array<math::Vec3, 4>{
            position + orientation * math::Vec3{-halfWidth, -halfHeight, -distance},
            position + orientation * math::Vec3{halfWidth, -halfHeight, -distance},
            position + orientation * math::Vec3{halfWidth, halfHeight, -distance},
            position + orientation * math::Vec3{-halfWidth, halfHeight, -distance},
        };
    };

    const auto nearCorners = corners(nearDistance);
    const auto farCorners = corners(farDistance);
    for (std::size_t index = 0; index < 4; ++index)
    {
        const std::size_t next = (index + 1) % 4;
        drawLine(nearCorners[index], nearCorners[next], color);
        drawLine(farCorners[index], farCorners[next], color);
        drawLine(nearCorners[index], farCorners[index], color);
    }
}

void GlRenderer::drawScreenTexture(
    TextureHandle texture,
    math::Vec2 screenSize,
    math::Vec2 center,
    math::Vec2 size,
    float rotationDegrees,
    float alpha,
    math::Vec2 clipCenter,
    math::Vec2 clipSize,
    math::Vec3 tint)
{
    if (!isReady()
        || texture == 0
        || texture > impl_->textures.size()
        || screenSize.x <= 0.0f
        || screenSize.y <= 0.0f
        || clipSize.x <= 0.0f
        || clipSize.y <= 0.0f
        || !std::isfinite(alpha)
        || alpha <= 0.0f
        || size.x == 0.0f
        || size.y == 0.0f)
    {
        return;
    }

    const float clipLeft = clipCenter.x - clipSize.x * 0.5f;
    const float clipBottom = clipCenter.y - clipSize.y * 0.5f;
    const float clipRight = clipCenter.x + clipSize.x * 0.5f;
    const float clipTop = clipCenter.y + clipSize.y * 0.5f;
    const float clampedLeft = math::clamp(clipLeft, 0.0f, screenSize.x);
    const float clampedBottom = math::clamp(clipBottom, 0.0f, screenSize.y);
    const float clampedRight = math::clamp(clipRight, 0.0f, screenSize.x);
    const float clampedTop = math::clamp(clipTop, 0.0f, screenSize.y);
    const int scissorX = static_cast<int>(std::floor(clampedLeft));
    const int scissorY = static_cast<int>(std::floor(clampedBottom));
    const int scissorWidth =
        static_cast<int>(std::ceil(clampedRight - clampedLeft));
    const int scissorHeight =
        static_cast<int>(std::ceil(clampedTop - clampedBottom));
    if (scissorWidth <= 0 || scissorHeight <= 0)
        return;

    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GLint previousProgram = 0;
    GLint previousVao = 0;
    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousTexture = 0;
    GLint previousScissor[4]{};
    GLint previousBlendSrcRgb = GL_ONE;
    GLint previousBlendDstRgb = GL_ZERO;
    GLint previousBlendSrcAlpha = GL_ONE;
    GLint previousBlendDstAlpha = GL_ZERO;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_SCISSOR_BOX, previousScissor);
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_SCISSOR_TEST);
    glScissor(scissorX, scissorY, scissorWidth, scissorHeight);

    glUseProgram(impl_->screenTextureProgram);
    glBindVertexArray(impl_->screenTextureVao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, impl_->textures[texture - 1]);
    glUniform1i(impl_->screenTextureSampler, 0);
    glUniform2f(impl_->screenTextureViewport, screenSize.x, screenSize.y);
    glUniform2f(impl_->screenTextureCenter, center.x, center.y);
    glUniform2f(impl_->screenTextureSize, size.x, size.y);
    glUniform1f(
        impl_->screenTextureRotation,
        std::isfinite(rotationDegrees) ? rotationDegrees * math::DegToRad : 0.0f);
    glUniform3f(impl_->screenTextureTint, tint.x, tint.y, tint.z);
    glUniform1f(impl_->screenTextureAlpha, math::clamp01(alpha));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glUseProgram(static_cast<GLuint>(previousProgram));
    glScissor(
        previousScissor[0],
        previousScissor[1],
        previousScissor[2],
        previousScissor[3]);
    glBlendFuncSeparate(
        static_cast<GLenum>(previousBlendSrcRgb),
        static_cast<GLenum>(previousBlendDstRgb),
        static_cast<GLenum>(previousBlendSrcAlpha),
        static_cast<GLenum>(previousBlendDstAlpha));
    if (scissorEnabled == GL_FALSE)
        glDisable(GL_SCISSOR_TEST);
    if (blendEnabled == GL_FALSE)
        glDisable(GL_BLEND);
    if (depthEnabled == GL_TRUE)
        glEnable(GL_DEPTH_TEST);
}

void GlRenderer::setLineWidth(float width)
{
    impl_->lineWidth = std::max(1.0f, width);
}

}
