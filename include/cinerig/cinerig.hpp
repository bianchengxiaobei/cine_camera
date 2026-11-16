#pragma once

#include <cinerig/core/bezier_spline.hpp>
#include <cinerig/core/blend.hpp>
#include <cinerig/core/blend_definition.hpp>
#include <cinerig/core/camera_interface.hpp>
#include <cinerig/core/camera_state.hpp>
#include <cinerig/core/camera_state_blend.hpp>
#include <cinerig/core/camera_target.hpp>
#include <cinerig/core/input_axis.hpp>
#include <cinerig/core/input_axis_controller.hpp>
#include <cinerig/core/input_axis_owner.hpp>
#include <cinerig/core/input_axis_source.hpp>
#include <cinerig/core/lookahead_settings.hpp>
#include <cinerig/core/noise_profile.hpp>
#include <cinerig/core/orbit_rig.hpp>
#include <cinerig/core/output_channels.hpp>
#include <cinerig/core/priority.hpp>
#include <cinerig/core/screen_composer_settings.hpp>
#include <cinerig/core/shot_quality.hpp>
#include <cinerig/core/signal_source.hpp>
#include <cinerig/core/spline.hpp>
#include <cinerig/core/spline_auto_dolly.hpp>
#include <cinerig/core/spline_helpers.hpp>
#include <cinerig/core/spline_settings.hpp>
#include <cinerig/core/tracker.hpp>
#include <cinerig/camera/camera_component.hpp>
#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/camera/camera_lens_cache_invalidator.hpp>
#include <cinerig/camera/pipeline.hpp>
#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/camera/virtual_camera_manager_base.hpp>
#include <cinerig/camera/external_camera.hpp>
#include <cinerig/components/auto_focus.hpp>
#include <cinerig/components/basic_multi_channel_perlin.hpp>
#include <cinerig/components/confiner3d.hpp>
#include <cinerig/components/confiner_oven.hpp>
#include <cinerig/components/confiner2d.hpp>
#include <cinerig/components/decollider.hpp>
#include <cinerig/components/deoccluder.hpp>
#include <cinerig/components/shot_quality_evaluator.hpp>
#include <cinerig/components/follow.hpp>
#include <cinerig/components/spline_cart.hpp>
#include <cinerig/components/spline_dolly.hpp>
#include <cinerig/components/spline_dolly_look_at_targets.hpp>
#include <cinerig/components/spline_roll.hpp>
#include <cinerig/components/spline_smoother.hpp>
#include <cinerig/components/hard_lock_to_target.hpp>
#include <cinerig/components/hard_look_at.hpp>
#include <cinerig/components/camera_offset.hpp>
#include <cinerig/components/recomposer.hpp>
#include <cinerig/components/free_look_modifier.hpp>
#include <cinerig/components/follow_zoom.hpp>
#include <cinerig/components/rotate_with_follow_target.hpp>
#include <cinerig/components/orbital_follow.hpp>
#include <cinerig/components/pan_tilt.hpp>
#include <cinerig/components/third_person_aim.hpp>
#include <cinerig/components/third_person_follow.hpp>
#include <cinerig/components/post_processing.hpp>
#include <cinerig/components/storyboard.hpp>
#include <cinerig/components/pixel_perfect.hpp>
#include <cinerig/components/position_composer.hpp>
#include <cinerig/components/rotation_composer.hpp>
#include <cinerig/impulse/collision_impulse_source.hpp>
#include <cinerig/impulse/external_impulse_listener.hpp>
#include <cinerig/impulse/fixed_signal.hpp>
#include <cinerig/impulse/impulse_definition.hpp>
#include <cinerig/impulse/impulse_listener.hpp>
#include <cinerig/impulse/impulse_manager.hpp>
#include <cinerig/impulse/impulse_source.hpp>
#include <cinerig/managers/clear_shot.hpp>
#include <cinerig/managers/mixing_camera.hpp>
#include <cinerig/managers/sequencer_camera.hpp>
#include <cinerig/managers/state_driven_camera.hpp>
#include <cinerig/brain/blend_lookup.hpp>
#include <cinerig/brain/camera_blend_stack.hpp>
#include <cinerig/brain/camera_override_stack.hpp>
#include <cinerig/brain/blend_manager.hpp>
#include <cinerig/brain/camera_brain.hpp>
#include <cinerig/brain/brain_events.hpp>
#include <cinerig/brain/timeline_mixer.hpp>
#include <cinerig/targets/target_group.hpp>
#include <cinerig/targets/group_framing.hpp>
#include <cinerig/runtime/camera_system.hpp>
#include <cinerig/runtime/update_manager.hpp>
#include <cinerig/runtime/update_tracker.hpp>
#include <cinerig/runtime/virtual_camera_registry.hpp>
#include <cinerig/engine/animation_state.hpp>
#include <cinerig/engine/camera_output.hpp>
#include <cinerig/engine/entity_world.hpp>
#include <cinerig/engine/input_source.hpp>
#include <cinerig/engine/physics_world.hpp>
#include <cinerig/engine/post_processor.hpp>
#include <cinerig/engine/spline.hpp>
#include <cinerig/engine/null_adapters.hpp>
#include <cinerig/math/math.hpp>

namespace cinerig
{

inline constexpr int versionMajor = 0;
inline constexpr int versionMinor = 1;
inline constexpr int versionPatch = 0;

const char* versionString() noexcept;

}



















