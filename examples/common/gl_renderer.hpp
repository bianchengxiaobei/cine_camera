#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <cinerig/core/lens_settings.hpp>
#include <cinerig/engine/entity_world.hpp>
#include <cinerig/math/matrix.hpp>

namespace cinerig::examples
{

class SdlCameraOutput;

// OpenGL 3.3 renderer for the example scenes. Meshes are program-generated and
// owned by the renderer; a handle is valid until shutdown().
class GlRenderer
{
public:
    using MeshHandle = std::uint32_t;
    using TextureHandle = std::uint32_t;

    GlRenderer();
    ~GlRenderer();

    GlRenderer(const GlRenderer&) = delete;
    GlRenderer& operator=(const GlRenderer&) = delete;

    bool initialize(std::string* error = nullptr);
    void shutdown();
    bool isReady() const noexcept;

    MeshHandle createCube(float size = 1.0f);
    MeshHandle createSphere(float radius = 1.0f, int slices = 24, int stacks = 16);
    MeshHandle createCylinder(float radius = 1.0f, float height = 2.0f, int slices = 24);
    MeshHandle createPlane(float halfSize = 20.0f);
    TextureHandle createTexture(int width, int height, const unsigned char* rgba);

    void beginFrame(
        const SdlCameraOutput& camera,
        math::Vec3 clearColor = {0.055f, 0.065f, 0.085f});
    void endFrame();

    void drawMesh(MeshHandle mesh, const Transform& transform, math::Vec3 tint = math::Vec3::one);
    void drawGrid(float halfSize = 20.0f, float spacing = 1.0f, math::Vec3 color = {0.25f, 0.28f, 0.34f});
    void drawAxes(const Transform& transform, float length = 1.0f);
    void drawLine(math::Vec3 from, math::Vec3 to, math::Vec3 color);
    void drawRay(math::Vec3 origin, math::Vec3 direction, float length, math::Vec3 color);
    void drawTargetMarker(math::Vec3 position, float size = 0.35f, math::Vec3 color = {1.0f, 0.8f, 0.25f});
    void drawCameraFrustum(const SdlCameraOutput& camera, math::Vec3 color = {0.9f, 0.9f, 0.9f});
    void drawFrustum(
        math::Vec3 position,
        math::Quat orientation,
        const LensSettings& lens,
        float aspect,
        math::Vec3 color = {0.9f, 0.9f, 0.9f});
    void drawScreenTexture(
        TextureHandle texture,
        math::Vec2 screenSize,
        math::Vec2 center,
        math::Vec2 size,
        float rotationDegrees,
        float alpha,
        math::Vec2 clipCenter,
        math::Vec2 clipSize,
        math::Vec3 tint = math::Vec3::one);
    void setLineWidth(float width);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
