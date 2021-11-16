#pragma once
struct Vec3 {
    Vec3(double x_ = 0, double y_ = 0, double z_ = 0) :x(x_), y(y_), z(z_) {}
    double x;
    double y;
    double z;
};

// x forward
// y up
// z right
class Vec3d {
public:
    double x;
    double y;
    double z;
    Vec3d() :x(0), y(0), z(0) {}
    Vec3d(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    double dot(Vec3d b) {
        return (this->x*b.x + this->y*b.y + this->z*b.z);
    }
    Vec3d cross(Vec3d b) {
        return Vec3d(this->y * b.z - this->z * b.y,
            this->z * b.x - this->x * b.z,
            this->x * b.y - this->y * b.x);
    }
    double len() {
        return sqrt(this->x*this->x + this->y*this->y + this->z*this->z);
    }
    Vec3d operator*(double scalar) {
        return Vec3d(this->x*scalar, this->y*scalar, this->z*scalar);
    }
    Vec3d operator/(double scalar) {
        return Vec3d(this->x / scalar, this->y / scalar, this->z / scalar);
    }
    Vec3d operator+(Vec3d b) {
        return Vec3d(this->x + b.x, this->y + b.y, this->z + b.z);
    }
    Vec3d operator-(Vec3d b) {
        return Vec3d(this->x - b.x, this->y - b.y, this->z - b.z);
    }
};

inline Vec3 cross(const Vec3 & a, const Vec3 & b) {
    return Vec3(a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x);
}

inline double dot(const Vec3 & a, const Vec3 &b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

struct Matrix33 {
    Vec3 x;
    Vec3 y;
    Vec3 z;
};

class Quaternion {
public:
    double x;
    double y;
    double z;
    double w;
    Quaternion(double w_ = 1, double x_ = 0, double y_ = 0, double z_ = 0) :x(x_), y(y_), z(z_), w(w_) {}
    Quaternion operator*(Quaternion & b) {
        return Quaternion(
            this->w*b.w - this->x*b.x - this->y*b.y - this->z*b.z,
            this->x*b.w + this->w*b.x - this->z*b.y + this->y*b.z,
            this->y*b.w + this->z*b.x + this->w*b.y - this->x*b.z,
            this->z*b.w - this->y*b.x + this->x*b.y + this->w*b.z);
    }
    Quaternion conj() {
        return Quaternion(this->w, -this->x, -this->y, -this->z);
    }
    double norm() {
        return sqrt(this->w*this->w + this->x*this->x + this->y*this->y + this->z*this->z);
    }
    void normalize() {
        double norm = this->norm();
        this->w /= norm;
        this->x /= norm;
        this->y /= norm;
        this->z /= norm;
    }
};

inline Quaternion vector_2_pureQ(Vec3d &a) {
    return Quaternion(0, a.x, a.y, a.z);
}

inline Vec3d pureQ_2_vector(Quaternion &q) {
    return Vec3d(q.x, q.y, q.z);
}

inline Matrix33 quaternion_to_matrix(const Quaternion & v) {
    Matrix33 mtrx;
    double wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;
    x2 = v.x + v.x;
    y2 = v.y + v.y;
    z2 = v.z + v.z;
    xx = v.x * x2;   xy = v.x * y2;   xz = v.x * z2;
    yy = v.y * y2;   yz = v.y * z2;   zz = v.z * z2;
    wx = v.w * x2;   wy = v.w * y2;   wz = v.w * z2;


    mtrx.x.x = 1.0 - (yy + zz);
    mtrx.x.y = xy + wz;
    mtrx.x.z = xz - wy;

    mtrx.y.x = xy - wz;
    mtrx.y.y = 1.0 - (xx + zz);
    mtrx.y.z = yz + wx;

    mtrx.z.x = xz + wy;
    mtrx.z.y = yz - wx;
    mtrx.z.z = 1.0 - (xx + yy);

    return mtrx;
}

inline double limit(double v, double min, double max) {
    if (max<min) {
        double t = min;
        min = max;
        max = t;
    }
    if (v<min) {
        return min;
    }
    else if (v>max) {
        return max;
    }
    return v;
}

inline double rands() {
    double x = 0;
    double denom = RAND_MAX + 1.;
    double need;

    for (need = 2.82e14; need > 1; need /= (RAND_MAX + 1.)) {
        x += rand() / denom;
        denom *= RAND_MAX + 1.;
    }

    return x;
}

inline double lerp(double * x, double * f, unsigned sz, double t) {
    for (unsigned i = 0; i < sz; i++)
    {
        if (t <= x[i])
        {
            if (i > 0)
            {
                return ((f[i] - f[i - 1]) / (x[i] - x[i - 1]) * t +
                    (x[i] * f[i - 1] - x[i - 1] * f[i]) / (x[i] - x[i - 1]));
            }
            return f[0];
        }
    }
    return f[sz - 1];
}

inline double fRand(double fMin, double fMax) {
    double f = (double)rand() / RAND_MAX;
    return fMin + f * (fMax - fMin);
}

