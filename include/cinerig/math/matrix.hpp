#pragma once

#include <cinerig/math/quat.hpp>

#include <array>
#include <cstddef>

namespace cinerig::math
{

struct Mat3
{
    std::array<float, 9> m{};

    static Mat3 identity();
    static Mat3 fromColumns(Vec3 xAxis, Vec3 yAxis, Vec3 zAxis);
    static Mat3 fromQuat(Quat q);

    float& at(int row, int column) { return m[static_cast<std::size_t>(column * 3 + row)]; }
    const float& at(int row, int column) const { return m[static_cast<std::size_t>(column * 3 + row)]; }

    Vec3 column(int index) const { return {at(0, index), at(1, index), at(2, index)}; }
    Vec3 row(int index) const { return {at(index, 0), at(index, 1), at(index, 2)}; }
    Mat3 transposed() const;
    float determinant() const;
};

inline Mat3 Mat3::identity()
{
    Mat3 result;
    result.at(0, 0) = 1.0f;
    result.at(1, 1) = 1.0f;
    result.at(2, 2) = 1.0f;
    return result;
}

inline Mat3 Mat3::fromColumns(Vec3 xAxis, Vec3 yAxis, Vec3 zAxis)
{
    Mat3 result;
    result.at(0, 0) = xAxis.x;
    result.at(1, 0) = xAxis.y;
    result.at(2, 0) = xAxis.z;
    result.at(0, 1) = yAxis.x;
    result.at(1, 1) = yAxis.y;
    result.at(2, 1) = yAxis.z;
    result.at(0, 2) = zAxis.x;
    result.at(1, 2) = zAxis.y;
    result.at(2, 2) = zAxis.z;
    return result;
}

inline Mat3 Mat3::fromQuat(Quat q)
{
    const Quat normalized = normalize(q);
    const float xx = normalized.x * normalized.x;
    const float yy = normalized.y * normalized.y;
    const float zz = normalized.z * normalized.z;
    const float xy = normalized.x * normalized.y;
    const float xz = normalized.x * normalized.z;
    const float yz = normalized.y * normalized.z;
    const float wx = normalized.w * normalized.x;
    const float wy = normalized.w * normalized.y;
    const float wz = normalized.w * normalized.z;

    Mat3 result;
    result.at(0, 0) = 1.0f - 2.0f * (yy + zz);
    result.at(0, 1) = 2.0f * (xy - wz);
    result.at(0, 2) = 2.0f * (xz + wy);
    result.at(1, 0) = 2.0f * (xy + wz);
    result.at(1, 1) = 1.0f - 2.0f * (xx + zz);
    result.at(1, 2) = 2.0f * (yz - wx);
    result.at(2, 0) = 2.0f * (xz - wy);
    result.at(2, 1) = 2.0f * (yz + wx);
    result.at(2, 2) = 1.0f - 2.0f * (xx + yy);
    return result;
}

inline Mat3 Mat3::transposed() const
{
    Mat3 result;
    for (int rowIndex = 0; rowIndex < 3; ++rowIndex)
    {
        for (int columnIndex = 0; columnIndex < 3; ++columnIndex)
            result.at(rowIndex, columnIndex) = at(columnIndex, rowIndex);
    }
    return result;
}

inline float Mat3::determinant() const
{
    return at(0, 0) * (at(1, 1) * at(2, 2) - at(1, 2) * at(2, 1))
        - at(0, 1) * (at(1, 0) * at(2, 2) - at(1, 2) * at(2, 0))
        + at(0, 2) * (at(1, 0) * at(2, 1) - at(1, 1) * at(2, 0));
}

inline Vec3 operator*(const Mat3& matrix, Vec3 value)
{
    return {
        matrix.at(0, 0) * value.x + matrix.at(0, 1) * value.y + matrix.at(0, 2) * value.z,
        matrix.at(1, 0) * value.x + matrix.at(1, 1) * value.y + matrix.at(1, 2) * value.z,
        matrix.at(2, 0) * value.x + matrix.at(2, 1) * value.y + matrix.at(2, 2) * value.z,
    };
}

inline Mat3 operator*(const Mat3& a, const Mat3& b)
{
    Mat3 result;
    for (int rowIndex = 0; rowIndex < 3; ++rowIndex)
    {
        for (int columnIndex = 0; columnIndex < 3; ++columnIndex)
        {
            float value = 0.0f;
            for (int inner = 0; inner < 3; ++inner)
                value += a.at(rowIndex, inner) * b.at(inner, columnIndex);
            result.at(rowIndex, columnIndex) = value;
        }
    }
    return result;
}

inline Mat3 inverse(const Mat3& matrix)
{
    const float determinant = matrix.determinant();
    if (std::abs(determinant) < Epsilon)
        return Mat3::identity();

    Mat3 result;
    result.at(0, 0) = (matrix.at(1, 1) * matrix.at(2, 2) - matrix.at(1, 2) * matrix.at(2, 1)) / determinant;
    result.at(0, 1) = (matrix.at(0, 2) * matrix.at(2, 1) - matrix.at(0, 1) * matrix.at(2, 2)) / determinant;
    result.at(0, 2) = (matrix.at(0, 1) * matrix.at(1, 2) - matrix.at(0, 2) * matrix.at(1, 1)) / determinant;
    result.at(1, 0) = (matrix.at(1, 2) * matrix.at(2, 0) - matrix.at(1, 0) * matrix.at(2, 2)) / determinant;
    result.at(1, 1) = (matrix.at(0, 0) * matrix.at(2, 2) - matrix.at(0, 2) * matrix.at(2, 0)) / determinant;
    result.at(1, 2) = (matrix.at(0, 2) * matrix.at(1, 0) - matrix.at(0, 0) * matrix.at(1, 2)) / determinant;
    result.at(2, 0) = (matrix.at(1, 0) * matrix.at(2, 1) - matrix.at(1, 1) * matrix.at(2, 0)) / determinant;
    result.at(2, 1) = (matrix.at(0, 1) * matrix.at(2, 0) - matrix.at(0, 0) * matrix.at(2, 1)) / determinant;
    result.at(2, 2) = (matrix.at(0, 0) * matrix.at(1, 1) - matrix.at(0, 1) * matrix.at(1, 0)) / determinant;
    return result;
}

struct Mat4
{
    std::array<float, 16> m{};

    static Mat4 identity();
    static Mat4 translation(Vec3 value);
    static Mat4 rotation(Quat value);
    static Mat4 scale(Vec3 value);
    static Mat4 fromMat3(const Mat3& value);

    float& at(int row, int column) { return m[static_cast<std::size_t>(column * 4 + row)]; }
    const float& at(int row, int column) const { return m[static_cast<std::size_t>(column * 4 + row)]; }

    Vec4 column(int index) const
    {
        return {at(0, index), at(1, index), at(2, index), at(3, index)};
    }

    Vec3 transformPoint(Vec3 value) const
    {
        const float w = at(3, 0) * value.x + at(3, 1) * value.y + at(3, 2) * value.z + at(3, 3);
        const float inverseW = std::abs(w) < Epsilon ? 1.0f : 1.0f / w;
        return {
            (at(0, 0) * value.x + at(0, 1) * value.y + at(0, 2) * value.z + at(0, 3)) * inverseW,
            (at(1, 0) * value.x + at(1, 1) * value.y + at(1, 2) * value.z + at(1, 3)) * inverseW,
            (at(2, 0) * value.x + at(2, 1) * value.y + at(2, 2) * value.z + at(2, 3)) * inverseW,
        };
    }

    Vec3 transformVector(Vec3 value) const
    {
        return {
            at(0, 0) * value.x + at(0, 1) * value.y + at(0, 2) * value.z,
            at(1, 0) * value.x + at(1, 1) * value.y + at(1, 2) * value.z,
            at(2, 0) * value.x + at(2, 1) * value.y + at(2, 2) * value.z,
        };
    }
};

inline Mat4 Mat4::identity()
{
    Mat4 result;
    result.at(0, 0) = 1.0f;
    result.at(1, 1) = 1.0f;
    result.at(2, 2) = 1.0f;
    result.at(3, 3) = 1.0f;
    return result;
}

inline Mat4 Mat4::translation(Vec3 value)
{
    Mat4 result = identity();
    result.at(0, 3) = value.x;
    result.at(1, 3) = value.y;
    result.at(2, 3) = value.z;
    return result;
}

inline Mat4 Mat4::rotation(Quat value)
{
    const Mat3 rotationMatrix = Mat3::fromQuat(value);
    Mat4 result = identity();
    for (int rowIndex = 0; rowIndex < 3; ++rowIndex)
    {
        for (int columnIndex = 0; columnIndex < 3; ++columnIndex)
            result.at(rowIndex, columnIndex) = rotationMatrix.at(rowIndex, columnIndex);
    }
    return result;
}

inline Mat4 Mat4::scale(Vec3 value)
{
    Mat4 result = identity();
    result.at(0, 0) = value.x;
    result.at(1, 1) = value.y;
    result.at(2, 2) = value.z;
    return result;
}

inline Mat4 Mat4::fromMat3(const Mat3& value)
{
    Mat4 result = identity();
    for (int rowIndex = 0; rowIndex < 3; ++rowIndex)
    {
        for (int columnIndex = 0; columnIndex < 3; ++columnIndex)
            result.at(rowIndex, columnIndex) = value.at(rowIndex, columnIndex);
    }
    return result;
}

inline Mat4 operator*(const Mat4& a, const Mat4& b)
{
    Mat4 result;
    for (int rowIndex = 0; rowIndex < 4; ++rowIndex)
    {
        for (int columnIndex = 0; columnIndex < 4; ++columnIndex)
        {
            float value = 0.0f;
            for (int inner = 0; inner < 4; ++inner)
                value += a.at(rowIndex, inner) * b.at(inner, columnIndex);
            result.at(rowIndex, columnIndex) = value;
        }
    }
    return result;
}

inline Mat4 inverseAffine(const Mat4& matrix)
{
    Mat3 rotationPart;
    for (int rowIndex = 0; rowIndex < 3; ++rowIndex)
    {
        for (int columnIndex = 0; columnIndex < 3; ++columnIndex)
            rotationPart.at(rowIndex, columnIndex) = matrix.at(rowIndex, columnIndex);
    }

    const Mat3 inverseRotation = inverse(rotationPart);
    const Vec3 translation{matrix.at(0, 3), matrix.at(1, 3), matrix.at(2, 3)};
    const Vec3 inverseTranslation = -(inverseRotation * translation);

    Mat4 result = Mat4::fromMat3(inverseRotation);
    result.at(0, 3) = inverseTranslation.x;
    result.at(1, 3) = inverseTranslation.y;
    result.at(2, 3) = inverseTranslation.z;
    return result;
}

}
