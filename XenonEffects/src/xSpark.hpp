
#ifndef XSPARK_HPP
#define XSPARK_HPP

// config stuff
extern bool bContrails;
extern bool bLimitContrailRate;
extern bool bLimitSparkRate;
extern bool bNISContrails;
extern bool bUseCGStyle;
extern bool bUseD3DDeviceTexture;
extern bool bBounceParticles;
//extern bool bCarbonBounceBehavior;
extern bool bFadeOutParticles;
extern bool bCGIntensityBehavior;
extern float ContrailTargetFPS;
extern float SparkTargetFPS;
extern float ContrailSpeed;
extern float ContrailMinIntensity;
extern float ContrailMaxIntensity;
extern float SparkIntensity;
// end config stuff

#include <UMath/UMath.h>
#include <Attrib/Attrib.h>
#include <cmath>
#include <vector>

struct XenonEffectDef
{
    UMath::Vector4 vel = { 0, 0, 0, 0 };
    UMath::Matrix4 mat = *(UMath::Matrix4*)0x8A3028;
    Attrib::Collection* spec = NULL;
    struct EmitterGroup* piggyback_effect = NULL;
};

class XenonFXVec : public std::vector<XenonEffectDef>//public eastl::vector<XenonEffectDef, bstl::allocator>
{
};

class XenonEffectList : public XenonFXVec
{
};

class NGParticle
{
public:

    enum Flags
    {
        SPAWN = 1 << 0,
        DEBRIS = 1 << 1,
        BOUNCED = 1 << 2,
    };

    UMath::Vector3 initialPos;
    unsigned int color;
    UMath::Vector3 vel;
    float gravity;
    UMath::Vector3 impactNormal;
    __declspec(align(16)) uint16_t flags;
    uint16_t spin;
    uint16_t life;
    uint8_t length;
    uint8_t width;
    uint8_t uv[4];
    float age;
};

class ParticleList
{
public:
    NGParticle* mParticles;//[2048];
    unsigned int mNumParticles;
    void AgeParticles(float dt);
    void GeneratePolys();
    NGParticle* GetNextParticle();
};

struct CGEmitter
{
    Attrib::Gen::fuelcell_emitter mEmitterDef;
    Attrib::Gen::emitteruv mTextureUVs;
    UMath::Vector4 mVel;
    UMath::Matrix4 mLocalWorld;
    CGEmitter(Attrib::Collection* spec, XenonEffectDef* eDef);
    void SpawnParticles(float dt, float intensity, bool isContrail);
};

class NGEffect
{
public:
    NGEffect(XenonEffectDef* eDef, float dt);
    Attrib::Gen::fuelcell_effect mEffectDef;
};

extern uint32_t MAX_NGPARTICLES;
extern uint32_t MAX_NGEMITTERS;
extern float gElapsedSparkTime;
extern float gElapsedContrailTime;
extern XenonEffectList gNGEffectList;
extern ParticleList gParticleList;

// external interfaces to work with the particle system
extern void AddXenonEffect(
    struct EmitterGroup* piggyback_fx,
    Attrib::Collection* spec,
    UMath::Matrix4* mat,
    UMath::Vector4* vel);

extern void UpdateXenonEmitters(float dt);

#endif