#ifndef ATTRIB_H
#define ATTRIB_H

#include <UMath/UMath.h>
#include <string>

namespace Attrib
{

    typedef uint32_t CollectionKey;
    typedef uint32_t ClassKey;

    inline void *DefaultDataArea()
    {
        return ((void *(*)())0x6269B0)();
    }

    struct StringKey
    {
        uint64_t mHash64;
        uint32_t mHash32;
        const char* mString;
    };

    class Collection;

    struct RefSpec
    {
        uint32_t mClassKey;
        uint32_t mCollectionKey;
        Collection *mCollectionPtr;
    };

    inline void Mix32_1(uint32_t &a, uint32_t &b, uint32_t &c)
    {
        a = c >> 13 ^ (a - b - c);
        b = a << 8 ^ (b - c - a);
        c = b >> 13 ^ (c - a - b);
        a = c >> 12 ^ (a - b - c);
        b = a << 16 ^ (b - c - a);
        c = b >> 5 ^ (c - a - b);
        a = c >> 3 ^ (a - b - c);
        b = a << 10 ^ (b - c - a);
        c = b >> 15 ^ (c - a - b);
    }

    inline constexpr uint32_t Mix32_2(uint32_t a, uint32_t b, uint32_t c)
    {
        a = c >> 13 ^ (a - b - c);
        b = a << 8 ^ (b - c - a);
        c = b >> 13 ^ (c - a - b);
        a = c >> 12 ^ (a - b - c);
        b = a << 16 ^ (b - c - a);
        c = b >> 5 ^ (c - a - b);
        a = c >> 3 ^ (a - b - c);
        b = a << 10 ^ (b - c - a);
        return b >> 15 ^ (c - a - b);
    }

    extern uint32_t hash32(const char *str, size_t length, uint32_t initval);
    extern uint32_t StringHash32(const char *str, uint32_t bytes);
    extern uint32_t StringHash32(const char* str);

    constexpr uint32_t CTStringHash32(const char *str)
    {
        uint32_t a = 0x9E3779B9;
        uint32_t b = 0x9E3779B9;
        uint32_t c = 0xABCDEF00;
        size_t length = std::char_traits<char>::length(str);
        int v1 = 0;
        int v2 = length;

        while (v2 >= 12)
        {
            a += ((uint32_t *)str)[(v1 / sizeof(uint32_t)) + 0];
            b += ((uint32_t *)str)[(v1 / sizeof(uint32_t)) + 1];
            c += ((uint32_t *)str)[(v1 / sizeof(uint32_t)) + 2];
            Mix32_1(a, b, c);
            v1 += 12;
            v2 -= 12;
        }

        c += length;

        switch (v2)
        {
        case 11:
            c += str[10 + v1] << 24;
        case 10:
            c += str[9 + v1] << 16;
        case 9:
            c += str[8 + v1] << 8;
        case 8:
            b += str[7 + v1] << 24;
        case 7:
            b += str[6 + v1] << 16;
        case 6:
            b += str[5 + v1] << 8;
        case 5:
            b += str[4 + v1];
        case 4:
            a += str[3 + v1] << 24;
        case 3:
            a += str[2 + v1] << 16;
        case 2:
            a += str[1 + v1] << 8;
        case 1:
            a += str[v1];
            break;
        default:
            break;
        }

        return Mix32_2(a, b, c);
    }

    class Array
    {
    public:
        // Returns the base location of this array's data
        uint8_t* BasePointer() { return reinterpret_cast<uint8_t*>(&this[1]); }

        void* Data(uint32_t byteindex, uint8_t* base) { return &BasePointer()[byteindex + *base]; }
        bool IsReferences();
        uint32_t GetPad();
        void* GetData(uint32_t index);

    private:
        uint16_t mAlloc;
        uint16_t mCount;
        uint16_t mSize;
        uint16_t mEncodedTypePad;

        uint8_t mData[];
    };

    class Node
    {
    public:
        enum Flags
        {
            Flag_RequiresRelease = 1 << 0,
            Flag_IsArray = 1 << 1,
            Flag_IsInherited = 1 << 2,
            Flag_IsAccessor = 1 << 3,
            Flag_IsLaidOut = 1 << 4,
            Flag_IsByValue = 1 << 5,
            Flag_IsLocatable = 1 << 6,
        };

        bool GetFlag(uint32_t mask)
        {
            return mFlags & mask;
        }
        bool RequiresRelease()
        {
            return GetFlag(Flag_RequiresRelease);
        }
        bool IsArray()
        {
            return GetFlag(Flag_IsArray);
        }
        bool IsInherited()
        {
            return GetFlag(Flag_IsInherited);
        }
        bool IsAccessor()
        {
            return GetFlag(Flag_IsAccessor);
        }
        bool IsLaidOut()
        {
            return GetFlag(Flag_IsLaidOut);
        }
        bool IsByValue()
        {
            return GetFlag(Flag_IsByValue);
        }
        bool IsLocatable()
        {
            return GetFlag(Flag_IsLocatable);
        }
        bool IsValid()
        {
            return IsLaidOut() || mPtr != this;
        }
        void *GetPointer(void *layoutptr)
        {
            if (IsByValue())
                return &mValue;
            else if (IsLaidOut())
                return (void *)(uintptr_t(layoutptr) + uintptr_t(mPtr));
            else
                return mPtr;
        }
        class Array *GetArray(void *layoutptr)
        {
            if (IsLaidOut())
                return (Array *)(uintptr_t(layoutptr) + uintptr_t(mArray));
            else
                return mArray;
        }
        uint32_t GetValue()
        {
            return mValue;
        }
        uint32_t *GetRefValue()
        {
            return &mValue;
        }
        uint32_t GetKey()
        {
            return IsValid() ? mKey : 0;
        }
        uint32_t MaxSearch()
        {
            return mMax;
        }
        uint32_t GetCount(void* layoutptr)
        {
            void* ptr;
            if (!IsArray())
                return 1;

            ptr = GetPointer(layoutptr);

            return *(((uint16_t*)mPtr) + 1);
        }

    private:
        uint32_t mKey;
        union {
            void *mPtr;
            class Array *mArray;
            uint32_t mValue;
            uint32_t mOffset;
        };
        uint16_t mTypeIndex;
        uint8_t mMax;
        uint8_t mFlags;
    };

    // Rotates (v) by (amount) bits
    inline uint32_t RotateNTo32(uint32_t v, uint32_t amount)
    {
        return (v << amount) | (v >> (32 - amount));
    }

    class HashMap
    {
    public:
        bool ValidIndex(uint32_t index)
        {
            return index < mTableSize && mTable[index].IsValid();
        }
        uint32_t FindIndex(uint32_t key);
        class Node *Find(uint32_t key)
        {
            if (!key)
                return 0;

            uint32_t index = FindIndex(key);
            return ValidIndex(index) ? &mTable[index] : NULL;
        }

        struct HashMapTablePolicy
        {
            static uint32_t KeyIndex(uint32_t k, uint32_t tableSize, uint32_t keyShift)
            {
                return RotateNTo32(k, keyShift) % tableSize;
            }
        };

    private:
        class Node *mTable;
        uint32_t mTableSize;
        uint32_t mNumEntries;
        uint16_t mWorstCollision;
        uint16_t mKeyShift;
    };

    class Collection
    {
    public:
        //class Node *GetNode(uint32_t attributeKey, const Collection *&container);
        //void *GetData(uint32_t attributeKey, uint32_t index);

    private:
        HashMap mTable;
        const Collection *mParent;
        class Class *mClass;
        void *mLayout;
        uint32_t mRefCount;
        uint32_t mKey;
        class Vault *mSource;
        const char *mNamePtr;
    };

    inline Collection *__cdecl FindCollection(ClassKey classKey, CollectionKey collectionKey)
    {
        return ((Collection *(__cdecl *)(ClassKey, CollectionKey))0x00455FD0)(classKey, collectionKey);
    }

    class Instance
    {
    public: 
        void *GetLayoutPointer() const
        {
            return mLayoutPtr;
        }
        void *GetLayoutPointer()
        {
            return mLayoutPtr;
        }
        inline void* GetAttributePointer(uint32_t attribkey, uint32_t index)
        {
            return ((void *(__thiscall *)(Instance *, uint32_t, uint32_t))0x454810)(this, attribkey, index);
        }

        Instance(Collection *collection, uint32_t msgPort, void* owner)
        {
            ((void(__thiscall *)(Instance *, Collection *, uint32_t, void*))0x452380)(this, collection, msgPort, owner);
        }
        ~Instance()
        {
            ((void *(__thiscall *)(Instance *))0x45A430)(this);
        }

        void *mOwner = NULL;
        class Collection *const mCollection = NULL;
        void *mLayoutPtr = NULL;
        uint32_t mMsgPort = NULL;
        uint16_t mFlags = NULL;
        uint16_t mLocks = NULL;

        enum Flags
        {
            kDynamic = 1
        };
    };
    
    class Attribute
    {
        Attrib::Instance* mInstance = NULL;
        Attrib::Collection* mCollection = NULL;
        Attrib::Node* mInternal = NULL;
        void* mDataPointer = NULL;
    public:
        Attribute() {};

        Attribute(Attribute* src)
        {
            if (mInstance)
                mInstance->mLocks--;
            *this = *src;
            if (mInstance)
                mInstance->mLocks++;
        }

        Attribute(Instance *instance, Collection* collection, Node* node)
        {
            this->mCollection = collection;
            this->mInstance = instance;
            this->mInternal = node;
            this->mDataPointer = NULL;

            if (!node->IsArray())
                this->mDataPointer = node->GetPointer(mInstance->mLayoutPtr);

            if (instance)
                ++instance->mLocks;
        }

        ~Attribute()
        {
            if (mInstance)
                mInstance->mLocks--;
        }

        inline uint32_t GetLength()
        {
            if (mInternal)
                return mInternal->GetCount(mInstance->mLayoutPtr);
            else
                return 0;
        }
    };

	namespace Gen
	{

        #define DEF_INSTANCE(name, layout, values) \
        class name : public Instance \
        { \
            struct _LayoutStruct \
            { \
                layout \
            }; \
            \
            inline ClassKey ClassKey() \
            { \
                return CTStringHash32(#name); \
            } \
            \
            public: \
            name (CollectionKey collectionKey, uint32_t msgPort) \
                : Instance(FindCollection(ClassKey(), collectionKey), msgPort, NULL) {} \
            \
            public: \
            name (Collection* collection, uint32_t msgPort) \
                : Instance(collection, msgPort, NULL) {} \
            values \
        };

        #define DEF_INSTANCE_NO_LAYOUT(name, values) \
        class name : public Instance \
        { \
            \
            inline ClassKey ClassKey() \
            { \
                return CTStringHash32(#name); \
            } \
            \
            public: \
            name (CollectionKey collectionKey, uint32_t msgPort) \
                : Instance(FindCollection(ClassKey(), collectionKey), msgPort, NULL) {} \
            values \
        };

        #define VALUE(type, name) \
            inline type name() { return ((_LayoutStruct *)this->mLayoutPtr)->name; }

        #define OPTIONAL_VALUE(type, name) \
        type name() \
            { \
                void *data = NULL; \
                if (!(data = GetAttributePointer(CTStringHash32(#name), 0))) \
                    data = DefaultDataArea(); \
                return *(type *)data; \
            }

        #define OPTIONAL_ARRAY(type, name) \
        type* name(uint32_t index) \
            { \
                void *data = NULL; \
                if (!(data = GetAttributePointer(CTStringHash32(#name), index))) \
                    data = DefaultDataArea(); \
                return (type *)data; \
            }

        // clang-format off

        DEF_INSTANCE(
            visuallook
            ,
            UMath::Matrix4 DetailMapCurve;
            UMath::Matrix4 BlackBloomCurve;
            UMath::Matrix4 ColourBloomCurve;
            UMath::Vector4 ColourBloomTint;
            float ColourBloomIntensity;
            float Desaturation;
            float DetailMapIntensity;
            float BlackBloomIntensity;
            ,
            VALUE(UMath::Matrix4, DetailMapCurve);
            VALUE(UMath::Matrix4, BlackBloomCurve);
            VALUE(UMath::Matrix4, ColourBloomCurve);
            VALUE(UMath::Vector4, ColourBloomTint);
            VALUE(float, ColourBloomIntensity);
            VALUE(float, Desaturation);           
            VALUE(float, DetailMapIntensity);
            VALUE(float, BlackBloomIntensity);
        )

        DEF_INSTANCE(
            visuallookeffect // name
            , 
            UMath::Matrix4 graph; // layout
            float _testvalue;
            float length;
            float magnitude;
            float heattrigger;
            ,
            VALUE(UMath::Matrix4, graph);
            VALUE(float, _testvalue);
            VALUE(float, length);
            VALUE(float, magnitude);
            VALUE(float, heattrigger);
            OPTIONAL_VALUE(float, radialblur_scale);
            OPTIONAL_VALUE(float, radialblur_uvoffset);
        );

        DEF_INSTANCE(
            lightmaterials // name
            , 
            UMath::Vector4 GrassColour; // layout
            UMath::Vector4 diffuseColour;
            UMath::Vector4 specularColour;
            StringKey shader;
            float GrassGamma;
            float specularFacing;
            float LODRamp;
            float Scruff;
            float LowNoiseSpace;
            float NoiseMipMapBias;
            float LODStart;
            float diffuseFacing;
            float LowNoiseIntensity;
            float MaxShells;
            float specularPower;
            float DiffuseMipMapBias;
            float GrassHeight;
            float diffusePower;
            float DiffuseSpace;
            float specularGrazing;
            float diffuseGrazing;
            float parallaxHeight;
            float PunchThroughAlphaRef;
            ,
            VALUE(UMath::Vector4, GrassColour); // value getters
            VALUE(UMath::Vector4, diffuseColour);
            VALUE(UMath::Vector4, specularColour);
            VALUE(StringKey, shader);
            VALUE(float, GrassGamma);
            VALUE(float, specularFacing);
            VALUE(float, LODRamp);
            VALUE(float, Scruff);
            VALUE(float, LowNoiseSpace);
            VALUE(float, NoiseMipMapBias);
            VALUE(float, LODStart);
            VALUE(float, diffuseFacing);
            VALUE(float, LowNoiseIntensity);
            VALUE(float, MaxShells);
            VALUE(float, specularPower);
            VALUE(float, DiffuseMipMapBias);
            VALUE(float, GrassHeight);
            VALUE(float, diffusePower);
            VALUE(float, DiffuseSpace);
            VALUE(float, specularGrazing);
            VALUE(float, diffuseGrazing);
            VALUE(float, parallaxHeight);
            VALUE(float, PunchThroughAlphaRef);
            OPTIONAL_VALUE(StringKey, opacityMap);
            OPTIONAL_VALUE(StringKey, refractionMap);
            OPTIONAL_VALUE(StringKey, specularColourMap);
            OPTIONAL_VALUE(bool, useVertexColour);
            OPTIONAL_VALUE(float, NoiseSpace);
            OPTIONAL_VALUE(float, LightingCone);
            OPTIONAL_VALUE(float, Brightness);
            OPTIONAL_VALUE(float, Ambient);
            OPTIONAL_VALUE(StringKey, glossMap);
            OPTIONAL_VALUE(StringKey, reflectionMap);
            OPTIONAL_VALUE(float, Smoothness);
            OPTIONAL_VALUE(RefSpec, shaderspec);
        );

        /*DEF_INSTANCE(
            fuelcell_effect // name
            , 
            bool doTest; // layout
            ,
            VALUE(bool, doTest); // value getters
            //OPTIONAL_ARRAY(Attrib::RefSpec, NGEmitter);
        );

        DEF_INSTANCE(
            fuelcell_emitter // name
            ,
            UMath::Vector4 VolumeExtent; // layout
            UMath::Vector4 VolumeCenter;
            UMath::Vector4 VelocityStart;
            UMath::Vector4 VelocityInherit;
            UMath::Vector4 VelocityDelta;
            UMath::Vector4 Colour1;
            Attrib::RefSpec emitteruv;
            float NumParticlesVariance;
            float NumParticles;
            float LifeVariance;
            float Life;
            float LengthStart;
            float LengthDelta;
            float HeightStart;
            float GravityStart;
            float GravityDelta;
            float Elasticity;
            char zDebrisType;
            char zContrail;
            ,
            VALUE(UMath::Vector4, VolumeExtent); // value getters
            VALUE(UMath::Vector4, VolumeCenter);
            VALUE(UMath::Vector4, VelocityStart);
            VALUE(UMath::Vector4, VelocityInherit);
            VALUE(UMath::Vector4, VelocityDelta);
            VALUE(UMath::Vector4, Colour1);
            VALUE(Attrib::RefSpec, emitteruv);
            VALUE(float, NumParticlesVariance);
            VALUE(float, NumParticles);
            VALUE(float, LifeVariance);
            VALUE(float, Life);
            VALUE(float, LengthStart);
            VALUE(float, LengthDelta);
            VALUE(float, HeightStart);
            VALUE(float, GravityStart);
            VALUE(float, GravityDelta);
            VALUE(float, Elasticity);
            VALUE(char, zDebrisType);
            VALUE(char, zContrail);
            );

        DEF_INSTANCE(
            emitteruv // name
            , 
            float EndV; // layout
            float StartU;
            float EndU;
            float StartV;
            ,
            VALUE(float, EndV); // value getters
            VALUE(float, StartU);
            VALUE(float, EndU);
            VALUE(float, StartV);
        );*/

        DEF_INSTANCE_NO_LAYOUT(
            lightshaders // name
            ,
            OPTIONAL_VALUE(bool, useVertexColour);
            OPTIONAL_VALUE(StringKey, shadername);
            OPTIONAL_VALUE(StringKey, name);
        );
        // clang-format on
	}
}

#undef VALUE
#undef VALUE_OPTIONAL

#endif
