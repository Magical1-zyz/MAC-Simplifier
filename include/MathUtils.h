#pragma once
#include <cmath>
#include <algorithm>
#include <vector>
#include <Eigen/Dense>

// 使用 Eigen 的 Vector3d 作为基础向量
using Vec3 = Eigen::Vector3d;
using Mat4 = Eigen::Matrix4d;

// --- 二次误差矩阵类 (Quadric Matrix) ---
// 使用 Eigen::Matrix4d 存储 4x4 对称矩阵
// Q = | A  b |
//     | bT c |
struct Quadric {
    Eigen::Matrix4d A;

    Quadric();

    void setZero();

    // 运算符重载
    Quadric operator+(const Quadric& b) const;
    Quadric& operator+=(const Quadric& b);
    Quadric operator*(double s) const;

    // 根据平面构建: ax+by+cz+d=0
    static Quadric FromPlane(double a, double b, double c, double d);

    // 属性惩罚项 (用于正则化)
    static Quadric AttributePenalty(double w);

    // 计算 v^T * Q * v (误差值)
    double evaluate(const Vec3& v) const;

    // 求解最佳位置: 求解线性方程组来找到误差最小的点
    bool optimize(Vec3& result) const;
};