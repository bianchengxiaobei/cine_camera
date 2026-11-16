#include <cinerig/core/input_axis_controller.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

#include <cinerig/math/damp.hpp>

namespace cinerig
{

namespace
{

struct FoundAxis
{
    const IInputAxisOwner* owner = nullptr;
    InputAxisDescriptor descriptor;
};

bool sameOwnerAndName(
    const InputAxisController::Binding& binding,
    const FoundAxis& found)
{
    return binding.owner == found.owner && binding.name == found.descriptor.name;
}

} // namespace

void DefaultInputAxisDriver::validate()
{
    accelTime = std::max(0.0f, accelTime);
    decelTime = std::max(0.0f, decelTime);
}

void DefaultInputAxisDriver::processInput(
    InputAxis& axis,
    float inputValue,
    float deltaTime)
{
    const float dampTime =
        std::abs(inputValue) < std::abs(currentSpeed_) ? decelTime : accelTime;

    if (!hasInputAxisRestriction(axis.restrictions, InputAxisRestriction::Momentary))
    {
        if (deltaTime < 0.0f)
        {
            currentSpeed_ = 0.0f;
            return;
        }

        currentSpeed_ += math::standardDamp(
            inputValue - currentSpeed_,
            dampTime,
            deltaTime);

        if (!axis.wrap
            && decelTime > math::Epsilon
            && std::abs(currentSpeed_) > math::Epsilon)
        {
            const float current = axis.clampValue(axis.value);
            const float distance = currentSpeed_ > 0.0f
                ? axis.range.y - current
                : current - axis.range.x;
            const float maxSpeed = 0.1f + 4.0f * distance / decelTime;
            if (std::abs(currentSpeed_) > std::abs(maxSpeed))
                currentSpeed_ = std::copysign(maxSpeed, currentSpeed_);
        }

        axis.value = axis.clampValue(axis.value + currentSpeed_ * deltaTime);
        return;
    }

    if (deltaTime < 0.0f)
    {
        axis.value = axis.center;
        return;
    }

    const float desiredValue = axis.clampValue(inputValue + axis.center);
    axis.value += math::standardDamp(
        desiredValue - axis.value,
        dampTime,
        deltaTime);
}

void DefaultInputAxisDriver::reset(InputAxis& axis)
{
    currentSpeed_ = 0.0f;
    axis.reset();
}

void DefaultInputAxisDriver::cancelCurrentInput(InputAxis& axis)
{
    currentSpeed_ = 0.0f;
    axis.setValueAndLastValue(axis.value);
}

std::size_t InputAxisController::synchronize(const IInputAxisOwnerProvider& provider)
{
    std::vector<FoundAxis> found;

    std::vector<IInputAxisOwner*> owners;
    provider.getInputAxisOwners(owners);
    for (IInputAxisOwner* owner : owners)
    {
        if (owner == nullptr)
            continue;

        std::vector<InputAxisDescriptor> descriptors;
        owner->getInputAxes(descriptors);
        for (InputAxisDescriptor& descriptor : descriptors)
            found.push_back({owner, std::move(descriptor)});
    }

    std::vector<Binding> next;
    next.reserve(found.size() + bindings_.size());

    for (Binding& binding : bindings_)
    {
        if (binding.owner == nullptr)
            next.push_back(std::move(binding));
    }

    for (const FoundAxis& axis : found)
    {
        auto existing = std::find_if(
            bindings_.begin(),
            bindings_.end(),
            [&axis](const Binding& binding) {
                return sameOwnerAndName(binding, axis);
            });

        if (existing != bindings_.end())
        {
            Binding binding = std::move(*existing);
            binding.axis = axis.descriptor.axis;
            binding.hint = axis.descriptor.hint;
            next.push_back(std::move(binding));
            continue;
        }

        Binding binding;
        binding.owner = axis.owner;
        binding.name = axis.descriptor.name;
        binding.inputAxis = axis.descriptor.name;
        binding.axis = axis.descriptor.axis;
        binding.hint = axis.descriptor.hint;
        next.push_back(std::move(binding));
    }

    bindings_ = std::move(next);
    return found.size();
}

InputAxisController::Binding* InputAxisController::addAxis(
    std::string name,
    InputAxis& axis,
    InputAxisHint hint)
{
    Binding binding;
    binding.name = std::move(name);
    binding.inputAxis = binding.name;
    binding.axis = &axis;
    binding.hint = hint;
    bindings_.push_back(std::move(binding));
    return &bindings_.back();
}

bool InputAxisController::bind(
    std::string_view axisName,
    std::string_view inputAxisName)
{
    Binding* binding = find(axisName);
    if (binding == nullptr)
        return false;
    binding->inputAxis = inputAxisName;
    return true;
}

InputAxisController::Binding* InputAxisController::find(std::string_view axisName)
{
    for (Binding& binding : bindings_)
    {
        if (binding.name == axisName)
            return &binding;
    }
    return nullptr;
}

const InputAxisController::Binding* InputAxisController::find(
    std::string_view axisName) const
{
    for (const Binding& binding : bindings_)
    {
        if (binding.name == axisName)
            return &binding;
    }
    return nullptr;
}

bool InputAxisController::update(const IInputAxisSource& source, float deltaTime)
{
    bool updated = false;
    for (Binding& binding : bindings_)
    {
        if (!binding.enabled
            || binding.axis == nullptr
            || binding.inputAxis.empty())
        {
            continue;
        }

        float value = source.axis(binding.inputAxis) * binding.gain;
        if (binding.cancelDeltaTime && deltaTime > 0.0f)
            value /= deltaTime;
        binding.inputValue = value;
        binding.driver.processInput(*binding.axis, value, deltaTime);
        updated = true;
    }
    return updated;
}

bool InputAxisController::triggerRecentering(std::string_view axisName)
{
    Binding* binding = find(axisName);
    if (binding == nullptr || binding->axis == nullptr)
        return false;

    binding->driver.cancelCurrentInput(*binding->axis);
    binding->axis->triggerRecentering();
    return true;
}

void InputAxisController::reset()
{
    for (Binding& binding : bindings_)
    {
        if (binding.axis != nullptr)
            binding.driver.reset(*binding.axis);
    }
}

void InputAxisController::cancelInput()
{
    for (Binding& binding : bindings_)
    {
        if (binding.axis != nullptr)
            binding.driver.cancelCurrentInput(*binding.axis);
    }
}

void InputAxisController::clear()
{
    bindings_.clear();
}

std::size_t InputAxisController::size() const noexcept
{
    return bindings_.size();
}

const std::vector<InputAxisController::Binding>&
InputAxisController::bindings() const noexcept
{
    return bindings_;
}

} // namespace cinerig

