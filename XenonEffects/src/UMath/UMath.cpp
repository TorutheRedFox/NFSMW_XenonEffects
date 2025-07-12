
#include "UMath.h"

namespace UMath
{
    namespace fpu
    {
        void RotateTranslate(const UMath::Vector4& v, const UMath::Matrix4& m, UMath::Vector4& result)
        {
            result.x = (m.v0.x * v.x) + ((m.v1.x * v.y) + ((m.v3.x * v.w) + (m.v2.x * v.z)));
            result.y = (m.v0.y * v.x) + ((m.v1.y * v.y) + ((m.v3.y * v.w) + (m.v2.y * v.z)));
            result.z = (m.v0.z * v.x) + ((m.v1.z * v.y) + ((m.v3.z * v.w) + (m.v2.z * v.z)));
            result.w = (m.v0.w * v.x) + ((m.v1.w * v.y) + ((m.v3.w * v.w) + (m.v2.w * v.z)));
        }

        void Scalexyz(const UMath::Vector4& a, const UMath::Vector4& b, UMath::Vector4& result)
        {
            result.x = a.x * b.x;
            result.y = a.y * b.y;
            result.z = a.z * b.z;
        }

        void Scalexyz(const UMath::Vector4& a, const float scaleby, UMath::Vector4& result)
        {
            result.x = a.x * scaleby;
            result.y = a.y * scaleby;
            result.z = a.z * scaleby;
        }

        void Rotate(const UMath::Vector4& v, const UMath::Matrix4& m, UMath::Vector4& result)
        {
            result.x = (m.v0.x * v.x) + (m.v1.x * v.y) + (m.v2.x * v.z);
            result.y = (m.v0.y * v.x) + (m.v1.y * v.y) + (m.v2.y * v.z);
            result.z = (m.v0.z * v.x) + (m.v1.z * v.y) + (m.v2.z * v.z);
            result.w = v.w;
        }

        void Add(const UMath::Vector4& a, const UMath::Vector4& b, UMath::Vector4& result)
        {
            result.x = a.x + b.x;
            result.y = a.y + b.y;
            result.z = a.z + b.z;
            result.w = a.w + b.w;
        }

        void Add(const UMath::Vector3& a, const UMath::Vector3& b, UMath::Vector4& result)
        {
            result.x = a.x + b.x;
            result.y = a.y + b.y;
            result.z = a.z + b.z;
        }

        void ScaleAdd(const UMath::Vector4* a, const float scaleby, const UMath::Vector4* b, UMath::Vector4* result)
        {
            result->x = (a->x * scaleby) + b->x;
            result->y = (a->y * scaleby) + b->y;
            result->z = (a->z * scaleby) + b->z;
            result->w = (a->w * scaleby) + b->w;
        }

        void ScaleAdd(const UMath::Vector3& a, const float scaleby, const UMath::Vector3& b, UMath::Vector3& result)
        {
            result.x = (a.x * scaleby) + b.x;
            result.y = (a.y * scaleby) + b.y;
            result.z = (a.z * scaleby) + b.z;
        }
    }
}
