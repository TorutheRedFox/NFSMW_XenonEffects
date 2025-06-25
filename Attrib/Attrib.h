#ifndef ATTRIB_H
#define ATTRIB_H

#include <UMath/UMath.h>
#include <string>
#include "AttribCore.h"
#include "AttribCollection.h"
#include "AttribArray.h"
#include "AttribHashMap.h"
#include "AttribInstance.h"
#include "AttribAttribute.h"

namespace Attrib
{
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
            public: \
            name (RefSpec refspec, uint32_t msgPort) \
                : Instance(refspec, msgPort, NULL) {} \
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
            type& name(uint32_t index) \
            { \
                type *resultptr = NULL; \
                if (!(resultptr = (type *)GetAttributePointer(CTStringHash32(#name), index))) \
                    resultptr = (type *)DefaultDataArea(); \
                return *resultptr; \
            } \
            bool name(type& result, uint32_t index) \
            { \
                type *resultptr = NULL; \
                if (!(resultptr = (type *)GetAttributePointer(CTStringHash32(#name), index))) \
                { \
                    result = *(type *)DefaultDataArea(); \
                    return false; \
                } \
                result = *resultptr; \
                return true; \
            } \
            uint32_t Num_##name() \
            { \
                char v4[16]; \
                return Get(v4, CTStringHash32(#name))->GetLength(); \
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

        //DEF_INSTANCE(
        //    fuelcell_effect // name
        //    , 
        //    bool doTest; // layout
        //    ,
        //    VALUE(bool, doTest); // value getters
        //    OPTIONAL_ARRAY(Attrib::RefSpec, NGEmitter);
        //);
        //
        //DEF_INSTANCE(
        //    fuelcell_emitter // name
        //    ,
        //    UMath::Vector4 VolumeCenter;
        //    UMath::Vector4 VelocityDelta;
        //    UMath::Vector4 VolumeExtent;
        //    UMath::Vector4 VelocityInherit;
        //    UMath::Vector4 VelocityStart;
        //    UMath::Vector4 Colour1;
        //    Attrib::RefSpec emitteruv;
        //    float Life;
        //    float NumParticlesVariance;
        //    float GravityStart;
        //    float HeightStart;
        //    float GravityDelta;
        //    float LengthStart;
        //    float LengthDelta;
        //    float LifeVariance;
        //    float NumParticles;
        //    int16_t Spin;
        //    int8_t zSprite;
        //    int8_t zContrail;
        //    ,
        //    VALUE(UMath::Vector4, VolumeCenter); // value getters
        //    VALUE(UMath::Vector4, VelocityDelta);
        //    VALUE(UMath::Vector4, VolumeExtent);
        //    VALUE(UMath::Vector4, VelocityInherit);
        //    VALUE(UMath::Vector4, VelocityStart);
        //    VALUE(UMath::Vector4, Colour1);
        //    VALUE(Attrib::RefSpec, emitteruv);
        //    VALUE(float, Life);
        //    VALUE(float, NumParticlesVariance);
        //    VALUE(float, GravityStart);
        //    VALUE(float, HeightStart);
        //    VALUE(float, GravityDelta);
        //    VALUE(float, LengthStart);
        //    VALUE(float, LengthDelta);
        //    VALUE(float, LifeVariance);
        //    VALUE(float, NumParticles);
        //    VALUE(int16_t, Spin);
        //    VALUE(int8_t, zSprite);
        //    VALUE(int8_t, zContrail);
        //);
        //
        //DEF_INSTANCE(
        //    emitteruv // name
        //    , 
        //    float EndV; // layout
        //    float StartU;
        //    float EndU;
        //    float StartV;
        //    ,
        //    VALUE(float, EndV); // value getters
        //    VALUE(float, StartU);
        //    VALUE(float, EndU);
        //    VALUE(float, StartV);
        //);

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
