#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <cinerig/core/input_axis.hpp>
#include <cinerig/core/input_axis_owner.hpp>
#include <cinerig/core/input_axis_source.hpp>

namespace cinerig
{

struct DefaultInputAxisDriver
{
    float accelTime = 0.2f;
    float decelTime = 0.2f;

    void validate();
    void processInput(InputAxis& axis, float inputValue, float deltaTime);
    void reset(InputAxis& axis);
    void cancelCurrentInput(InputAxis& axis);
    float currentSpeed() const noexcept { return currentSpeed_; }

private:
    float currentSpeed_ = 0.0f;
};

class InputAxisController
{
public:
    struct Binding
    {
        const IInputAxisOwner* owner = nullptr;
        std::string name;
        std::string inputAxis;
        InputAxis* axis = nullptr;
        InputAxisHint hint = InputAxisHint::Default;
        float gain = 1.0f;
        bool cancelDeltaTime = false;
        bool enabled = true;
        float inputValue = 0.0f;
        DefaultInputAxisDriver driver;
    };

    std::size_t synchronize(const IInputAxisOwnerProvider& provider);
    Binding* addAxis(
        std::string name,
        InputAxis& axis,
        InputAxisHint hint = InputAxisHint::Default);
    bool bind(std::string_view axisName, std::string_view inputAxisName);
    Binding* find(std::string_view axisName);
    const Binding* find(std::string_view axisName) const;

    bool update(const IInputAxisSource& source, float deltaTime);
    bool triggerRecentering(std::string_view axisName);
    void reset();
    void cancelInput();
    void clear();
    std::size_t size() const noexcept;
    const std::vector<Binding>& bindings() const noexcept;

private:
    std::vector<Binding> bindings_;
};

} // namespace cinerig

