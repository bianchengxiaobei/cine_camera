#pragma once

#include <cinerig/core/camera_state.hpp>

namespace cinerig
{

CameraState lerp(const CameraState& a, const CameraState& b, float t);

}
