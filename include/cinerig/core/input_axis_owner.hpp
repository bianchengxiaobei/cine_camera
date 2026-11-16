#pragma once

#include <string>
#include <vector>

#include <cinerig/core/input_axis.hpp>

namespace cinerig
{

enum class InputAxisHint
{
    Default,
    X,
    Y,
};

struct InputAxisDescriptor
{
    std::string name;
    InputAxis* axis = nullptr;
    InputAxisHint hint = InputAxisHint::Default;
};

class IInputAxisOwner
{
public:
    virtual ~IInputAxisOwner() = default;
    virtual void getInputAxes(std::vector<InputAxisDescriptor>& axes) = 0;
};

class IInputAxisOwnerProvider
{
public:
    virtual ~IInputAxisOwnerProvider() = default;
    virtual void getInputAxisOwners(std::vector<IInputAxisOwner*>& owners) const = 0;
};

} // namespace cinerig
