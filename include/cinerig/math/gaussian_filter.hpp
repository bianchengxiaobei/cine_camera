#pragma once

#include <cinerig/math/vec.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace cinerig::math
{

template <typename T>
class GaussianWindow1D
{
public:
    explicit GaussianWindow1D(float sigma, int maxKernelRadius = 10)
    {
        generateKernel(sigma, maxKernelRadius);
        data_.resize(static_cast<std::size_t>(kernelSize_));
        currentPosition_ = -1;
    }

    void reset() { currentPosition_ = -1; }
    bool isEmpty() const { return currentPosition_ < 0; }
    float sigma() const { return sigma_; }
    int kernelSize() const { return kernelSize_; }
    int bufferLength() const { return static_cast<int>(data_.size()); }

    void addValue(T value)
    {
        if (currentPosition_ < 0)
        {
            for (T& entry : data_)
                entry = value;
            currentPosition_ = kernelSize_ > 1 ? 1 : 0;
        }

        data_[static_cast<std::size_t>(currentPosition_)] = value;
        ++currentPosition_;
        if (currentPosition_ == kernelSize_)
            currentPosition_ = 0;
    }

    T filter(T value)
    {
        if (kernelSize_ < 3)
            return value;
        addValue(value);
        return valueAtCurrentPosition();
    }

    T value() const { return valueAtCurrentPosition(); }

    void setBufferValue(int index, T value)
    {
        data_[static_cast<std::size_t>(index)] = value;
    }

    T getBufferValue(int index) const
    {
        return data_[static_cast<std::size_t>(index)];
    }

private:
    void generateKernel(float sigma, int maxKernelRadius)
    {
        const int kernelRadius = std::min(
            maxKernelRadius,
            static_cast<int>(std::floor(std::abs(sigma) * 2.5f)));
        kernelSize_ = kernelRadius * 2 + 1;
        kernel_.resize(static_cast<std::size_t>(kernelSize_));

        if (kernelRadius == 0)
        {
            kernel_[0] = 1.0f;
        }
        else
        {
            const float denominator = 2.0f * sigma * sigma;
            float sum = 0.0f;
            for (int index = -kernelRadius; index <= kernelRadius; ++index)
            {
                const float value = std::exp(-(static_cast<float>(index * index)) / denominator)
                    / (TwoPi * sigma * sigma);
                kernel_[static_cast<std::size_t>(index + kernelRadius)] = value;
                sum += value;
            }
            for (float& value : kernel_)
                value /= sum;
        }
        sigma_ = sigma;
    }

    T valueAtCurrentPosition() const
    {
        T sum{};
        int windowPosition = currentPosition_;
        for (int index = 0; index < kernelSize_; ++index)
        {
            sum += data_[static_cast<std::size_t>(windowPosition)] * kernel_[static_cast<std::size_t>(index)];
            ++windowPosition;
            if (windowPosition == kernelSize_)
                windowPosition = 0;
        }
        return sum;
    }

    std::vector<T> data_;
    std::vector<float> kernel_;
    int kernelSize_ = 1;
    int currentPosition_ = -1;
    float sigma_ = 0.0f;
};

}
