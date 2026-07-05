module;
#include <utility>
#include <cmath>
#include <algorithm>

export module Math.Quaternion;

export namespace ArtifactCore {

// ============================================================
// Quaternion (Hamilton convention: w + xi + yj + zk)
// ============================================================

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f; // identity

    // --- Constructors ---
    constexpr Quaternion() = default;
    constexpr Quaternion(float x_, float y_, float z_, float w_)
        : x(x_), y(y_), z(z_), w(w_) {}

    static constexpr Quaternion identity() { return {0, 0, 0, 1}; }

    // From axis-angle (axis must be normalized, angle in radians)
    static Quaternion fromAxisAngle(float ax, float ay, float az, float angleRad) {
        float half = angleRad * 0.5f;
        float s = std::sin(half);
        return { ax * s, ay * s, az * s, std::cos(half) };
    }

    // From Euler angles (XYZ order, radians)
    static Quaternion fromEuler(float pitchX, float yawY, float rollZ) {
        float cx = std::cos(pitchX * 0.5f), sx = std::sin(pitchX * 0.5f);
        float cy = std::cos(yawY * 0.5f),   sy = std::sin(yawY * 0.5f);
        float cz = std::cos(rollZ * 0.5f),  sz = std::sin(rollZ * 0.5f);
        return {
            sx * cy * cz - cx * sy * sz,
            cx * sy * cz + sx * cy * sz,
            cx * cy * sz - sx * sy * cz,
            cx * cy * cz + sx * sy * sz
        };
    }

    // From Euler angles (degrees, XYZ order)
    static Quaternion fromEulerDeg(float pitchX, float yawY, float rollZ) {
        constexpr float deg2rad = 0.01745329251994329577f;
        return fromEuler(pitchX * deg2rad, yawY * deg2rad, rollZ * deg2rad);
    }

    // --- Arithmetic ---
    Quaternion operator*(const Quaternion& q) const {
        return {
            w * q.x + x * q.w + y * q.z - z * q.y,
            w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w,
            w * q.w - x * q.x - y * q.y - z * q.z
        };
    }

    Quaternion operator+(const Quaternion& q) const {
        return { x + q.x, y + q.y, z + q.z, w + q.w };
    }

    Quaternion operator*(float s) const {
        return { x * s, y * s, z * s, w * s };
    }

    Quaternion operator-() const {
        return { -x, -y, -z, -w };
    }

    // --- Properties ---
    float dot(const Quaternion& q) const {
        return x * q.x + y * q.y + z * q.z + w * q.w;
    }

    float lengthSq() const { return dot(*this); }
    float length() const { return std::sqrt(lengthSq()); }

    Quaternion normalized() const {
        float len = length();
        if (len < 1e-8f) return identity();
        float inv = 1.0f / len;
        return { x * inv, y * inv, z * inv, w * inv };
    }

    Quaternion conjugate() const { return { -x, -y, -z, w }; }

    Quaternion inverse() const {
        float d = dot(*this);
        if (d < 1e-8f) return identity();
        float inv = 1.0f / d;
        return { -x * inv, -y * inv, -z * inv, w * inv };
    }

    // Rotate a 3D point
    void rotatePoint(float px, float py, float pz,
                     float& rx, float& ry, float& rz) const {
        // q * p * q^-1
        float ix = w * px + y * pz - z * py;
        float iy = w * py + z * px - x * pz;
        float iz = w * pz + x * py - y * px;
        float iw = -x * px - y * py - z * pz;
        rx = ix * w + iw * (-x) + iy * (-z) - iz * (-y);
        ry = iy * w + iw * (-y) + iz * (-x) - ix * (-z);
        rz = iz * w + iw * (-z) + ix * (-y) - iy * (-x);
    }

    // --- Conversion ---
    // To Euler angles (XYZ order, radians)
    void toEuler(float& pitchX, float& yawY, float& rollZ) const {
        // Roll (X-axis rotation)
        float sinr_cosp = 2.0f * (w * x + y * z);
        float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
        pitchX = std::atan2(sinr_cosp, cosr_cosp);

        // Pitch (Y-axis rotation)
        float sinp = 2.0f * (w * y - z * x);
        sinp = std::clamp(sinp, -1.0f, 1.0f);
        yawY = std::asin(sinp);

        // Yaw (Z-axis rotation)
        float siny_cosp = 2.0f * (w * z + x * y);
        float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
        rollZ = std::atan2(siny_cosp, cosy_cosp);
    }

    // To axis-angle
    void toAxisAngle(float& ax, float& ay, float& az, float& angleRad) const {
        Quaternion q = normalized();
        angleRad = 2.0f * std::acos(std::clamp(q.w, -1.0f, 1.0f));
        float s = std::sqrt(1.0f - q.w * q.w);
        if (s < 1e-6f) {
            ax = q.x; ay = q.y; az = q.z;
        } else {
            ax = q.x / s; ay = q.y / s; az = q.z / s;
        }
    }

    // To forward vector (assuming Z-forward convention)
    void toForwardVector(float& fx, float& fy, float& fz) const {
        fx = 2.0f * (x * z + w * y);
        fy = 2.0f * (y * z - w * x);
        fz = 1.0f - 2.0f * (x * x + y * y);
    }
};

// ============================================================
// Interpolation
// ============================================================

// Linear interpolation (NLerp - normalized)
export inline Quaternion nlerp(const Quaternion& a, const Quaternion& b, float t) {
    // Ensure shortest path
    float dot = a.dot(b);
    Quaternion target = (dot < 0.0f) ? -b : b;
    Quaternion result = a * (1.0f - t) + target * t;
    return result.normalized();
}

// Spherical Linear Interpolation (SLERP)
export inline Quaternion slerp(const Quaternion& a, const Quaternion& b, float t) {
    float dot = a.dot(b);

    // Ensure shortest path
    Quaternion qb = (dot < 0.0f) ? -b : b;
    dot = std::abs(dot);

    // If very close, use NLERP to avoid division by zero
    if (dot > 0.9995f) {
        return nlerp(a, qb, t);
    }

    float theta = std::acos(std::clamp(dot, -1.0f, 1.0f));
    float sinTheta = std::sin(theta);

    float wa = std::sin((1.0f - t) * theta) / sinTheta;
    float wb = std::sin(t * theta) / sinTheta;

    return {
        wa * a.x + wb * qb.x,
        wa * a.y + wb * qb.y,
        wa * a.z + wb * qb.z,
        wa * a.w + wb * qb.w
    };
}

// Scaled SLERP (multiple keyframes)
export inline Quaternion squad(const Quaternion& q0, const Quaternion& q1,
                               const Quaternion& q2, const Quaternion& q3, float t) {
    Quaternion slerp01 = slerp(q0, q1, t);
    Quaternion slerp23 = slerp(q2, q3, t);
    return slerp(slerp01, slerp23, t);
}

// Angle between two quaternions (in radians)
export inline float angleBetween(const Quaternion& a, const Quaternion& b) {
    float d = std::clamp(a.dot(b), -1.0f, 1.0f);
    return 2.0f * std::acos(std::abs(d));
}

}
