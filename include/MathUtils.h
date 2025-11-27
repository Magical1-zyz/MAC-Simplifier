#pragma once
#include <cmath>
#include <algorithm>
#include <vector>

// --- 基础向量类 ---
struct Vec3 {
    double x, y, z;
    Vec3(double _x = 0, double _y = 0, double _z = 0) : x(_x), y(_y), z(_z) {}
    Vec3 operator+(const Vec3& b) const { return Vec3(x + b.x, y + b.y, z + b.z); }
    Vec3 operator-(const Vec3& b) const { return Vec3(x - b.x, y - b.y, z - b.z); }
    Vec3 operator*(double s) const { return Vec3(x * s, y * s, z * s); }
    double dot(const Vec3& b) const { return x * b.x + y * b.y + z * b.z; }
    Vec3 cross(const Vec3& b) const { return Vec3(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x); }
    double length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 normalize() const {
        double l = length();
        return (l > 1e-9) ? (*this) * (1.0 / l) : Vec3(0, 0, 0);
    }
};

// --- 二次误差矩阵类 (Quadric Matrix) ---
// 存储对称矩阵的上三角部分
// [ q11 q12 q13 q14 ]
// [ q12 q22 q23 q24 ]
// [ q13 q23 q33 q34 ]
// [ q14 q24 q34 q44 ]
struct Quadric {
    double a[10];

    Quadric();
    void setZero();
    Quadric operator+(const Quadric& b) const;
    Quadric operator*(double s) const;
    Quadric& operator+=(const Quadric& b);

    // 根据平面构建: ax+by+cz+d=0
    static Quadric FromPlane(double a, double b, double c, double d);

    // 属性惩罚项 (对应论文 1.1.1.2)
    static Quadric AttributePenalty(double w);

    // 计算 v^T * Q * v
    double evaluate(const Vec3& v) const;

    // 求解最佳位置 Ax = -b
    bool optimize(Vec3& result) const;
};