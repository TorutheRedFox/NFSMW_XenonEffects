#ifndef UMATH_H
#define UMATH_H

#include <corecrt_math.h>

namespace UMath
{
	struct Vector2
    {
        float x;
        float y;
    };
	
	struct Vector3
    {
        float x;
        float y;
        float z;

        inline Vector3& operator=(const Vector3& b)
        {
            this->x = b.x;
            this->y = b.y;
            this->z = b.z;

            return *this;
        }

        inline Vector3& operator*(const Vector3& b)
        {
            this->x *= b.x;
            this->y *= b.y;
            this->z *= b.z;
            return *this;
        }

        inline Vector3& operator+(const Vector3& b)
        {
            this->x += b.x;
            this->y += b.y;
            this->z += b.z;
            return *this;
        }

        inline Vector3& operator+=(const Vector3& b)
        {
            this->x += b.x;
            this->y += b.y;
            this->z += b.z;
            return *this;
        }

        inline Vector3& operator-(const Vector3& b)
        {
            this->x -= b.x;
            this->y -= b.y;
            this->z -= b.z;
            return *this;
        }

        inline Vector3& operator-=(const Vector3& b)
        {
            this->x -= b.x;
            this->y -= b.y;
            this->z -= b.z;
            return *this;
        }

        inline Vector3& operator*=(const Vector3& b)
        {
            this->x *= b.x;
            this->y *= b.y;
            this->z *= b.z;
            return *this;
        }

        inline Vector3& operator*(const float& f)
        {
            this->x *= f;
            this->y *= f;
            this->z *= f;
            return *this;
        }

        inline Vector3& operator*=(const float& f)
        {
            this->x *= f;
            this->y *= f;
            this->z *= f;
            return *this;
        }

        inline Vector3& operator/(const float& f)
        {
            this->x /= f;
            this->y /= f;
            this->z /= f;
            return *this;
        }

        inline Vector3& operator/=(const float& f)
        {
            this->x /= f;
            this->y /= f;
            this->z /= f;
            return *this;
        }
    };
	
    struct Vector4
    {
        float x;
        float y;
        float z;
        float w;

        inline Vector4& operator=(const Vector4& b)
        {
            this->x = b.x;
            this->y = b.y;
            this->z = b.z;
            this->w = b.w;

            return *this;
        }

        inline Vector4& operator*(const Vector4& b)
        {
            this->x *= b.x;
            this->y *= b.y;
            this->z *= b.z;
            this->w *= b.w;

            return *this;
        }

        inline Vector4& operator+(const Vector4& b)
        {
            this->x += b.x;
            this->y += b.y;
            this->z += b.z;
            this->w += b.w;

            return *this;
        }

        inline Vector4& operator+=(const Vector4& b)
        {
            this->x += b.x;
            this->y += b.y;
            this->z += b.z;
            this->w += b.w;

            return *this;
        }

        inline Vector4& operator-(const Vector4& b)
        {
            this->x -= b.x;
            this->y -= b.y;
            this->z -= b.z;
            this->w -= b.w;

            return *this;
        }

        inline Vector4& operator-=(const Vector4& b)
        {
            this->x -= b.x;
            this->y -= b.y;
            this->z -= b.z;
            this->w -= b.w;

            return *this;
        }

        inline Vector4& operator*=(const Vector4& b)
        {
            this->x *= b.x;
            this->y *= b.y;
            this->z *= b.z;
            this->w *= b.w;

            return *this;
        }

        inline Vector4& operator*(const float& f)
        {
            this->x *= f;
            this->y *= f;
            this->z *= f;
            this->w *= f;

            return *this;
        }

        inline Vector4& operator*=(const float& f)
        {
            this->x *= f;
            this->y *= f;
            this->z *= f;
            this->w *= f;

            return *this;
        }

        inline Vector4& operator/(const float& f)
        {
            this->x /= f;
            this->y /= f;
            this->z /= f;
            this->w /= f;

            return *this;
        }

        inline Vector4& operator/=(const float& f)
        {
            this->x /= f;
            this->y /= f;
            this->z /= f;
            this->w /= f;

            return *this;
        }
    };
    
    struct Matrix4
    {
        Vector4 v0;
        Vector4 v1;
        Vector4 v2;
        Vector4 v3;

        inline Matrix4 operator=(const Matrix4& b)
        {
            this->v0 = b.v0;
            this->v1 = b.v1;
            this->v2 = b.v2;
            this->v3 = b.v3;
        }
        // TODO - do matrix math
    };

    inline float Dot(const Vector3& a, const Vector3& b)
    {
        return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
    }

    inline float Dot(const Vector4& a, const Vector4& b)
    {
        return (a.x * b.x) + (a.y * b.y) + (a.z * b.z) + (a.w * b.w);
    }

    inline float Dotxyz(const Vector4& a, const Vector4& b)
    {
        return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
    }

    inline void Lerp(const Vector3& a, const Vector3& b, float t, Vector3& r)
    {
        r.x = ((b.x - a.x) * t) + a.x;
        r.y = ((b.y - a.y) * t) + a.y;
        r.z = ((b.z - a.z) * t) + a.z;
    }

    inline void Lerp(const Vector4& a, const Vector4& b, float t, Vector4& r)
    {
        r.x = ((b.x - a.x) * t) + a.x;
        r.y = ((b.y - a.y) * t) + a.y;
        r.z = ((b.z - a.z) * t) + a.z;
        r.w = ((b.w - a.w) * t) + a.w;
    }

    inline float Lerp(float a, float b, float t)
    {
        return (((b - a) * t) + a);
    }

    inline float Length(const Vector3& a)
    {
        return sqrtf(((a.z * a.z) + ((a.x * a.x) + (a.y * a.y))));
    }

    inline Vector3 Normalize(const Vector3& a)
    {
        float length = Length(a);

        Vector3 out = a;

        if (length != 0.0f)
        {
            out /= length;
        }

        return out;
    }
}

#endif // UMATH_H