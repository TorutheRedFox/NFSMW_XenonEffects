
#include "xSpark.hpp"
#include <Speed/src/GameDefs.h>
#include <Render/xSprites.hpp>

XenonEffectList gNGEffectList;
ParticleList gParticleList;

uint32_t MAX_NGPARTICLES = 2048;
uint32_t MAX_NGEMITTERS = 100;

float gElapsedSparkTime = 0;
float gElapsedContrailTime = 0;

unsigned int randomSeed = 0xDEADBEEF;

// xenon bounce physics

void CalcCollisiontime(NGParticle* particle)
{
    UMath::Vector4 ray[2]{};

    ray[1].x = -(particle->life * particle->vel.y + particle->initialPos.y);
    ray[1].y = particle->life * particle->vel.z + particle->initialPos.z + particle->life * particle->life * particle->gravity;
    ray[1].z = (particle->life * particle->vel.x) + particle->initialPos.x;
    ray[1].w = 1.0f;

    ray[0].x = -particle->initialPos.y;
    ray[0].y = particle->initialPos.z;
    ray[0].z = particle->initialPos.x;
    ray[0].w = 1.0f;

    WCollisionMgr::WorldCollisionInfo collisionInfo;
    WCollisionMgr collisionMgr(0, 0);

    if (collisionMgr.CheckHitWorld(ray, &collisionInfo, 3u))
    {
        float newLife = 0.0f;
        float dist = ((particle->vel.z * particle->vel.z) - (((particle->initialPos.z - collisionInfo.fCollidePt.y) * particle->gravity) * 4.0f));
        if (dist > 0.0f)
        {
            dist = sqrt(dist);
            newLife = ((dist - particle->vel.z) / (particle->gravity * 2.0f));
            if (newLife < 0.0f)
                newLife = ((-particle->vel.z - dist) / (particle->gravity * 2.0f));
        }
        particle->life = (uint16_t)(newLife * 8191.0f);
        particle->flags |= NGParticle::Flags::SPAWN;
        particle->impactNormal.x = collisionInfo.fNormal.z;
        particle->impactNormal.y = -collisionInfo.fNormal.x;
        particle->impactNormal.z = collisionInfo.fNormal.y;
    }
}

void BounceParticle(NGParticle* particle)
{
    float life;
    float gravity;
    float gravityOverTime;
    float velocityMagnitude;
    UMath::Vector3 newVelocity = particle->vel;

    life = particle->life / 8191.0f;
    gravity = particle->gravity;

    gravityOverTime = particle->gravity * life;

    particle->initialPos += newVelocity * life;
    particle->initialPos.z += gravityOverTime * life;

    newVelocity.z += gravityOverTime;

    velocityMagnitude = UMath::Length(newVelocity);

    newVelocity = UMath::Normalize(newVelocity);
    
    particle->life = 8191;
    particle->age = 0.0f;
    particle->flags = NGParticle::Flags::BOUNCED;

    float bounceCos = UMath::Dot(newVelocity, particle->impactNormal);

    particle->vel = (newVelocity - (particle->impactNormal * bounceCos * 2.0f)) * velocityMagnitude;
}

// ParticleList

void ParticleList::AgeParticles(float dt)
{
    size_t aliveCount = 0;

    for (size_t i = 0; i < mNumParticles; i++)
    {
        NGParticle& particle = mParticles[i];
        if ((particle.age + dt) <= particle.life / 8191.0f)
        {
            memcpy(&mParticles[aliveCount], &mParticles[i], sizeof(NGParticle));
            mParticles[aliveCount].age += dt;
            aliveCount++;
        }
        else if (particle.flags & NGParticle::Flags::SPAWN)
        {
            BounceParticle(&particle);
            aliveCount++;
        }
    }
    mNumParticles = aliveCount;
}

void ParticleList::GeneratePolys()
{
    if (mNumParticles)
        NGSpriteManager.AddSpark(mParticles, mNumParticles);
}

NGParticle* ParticleList::GetNextParticle()
{
    if (mNumParticles >= MAX_NGPARTICLES)
        return NULL;
    return &mParticles[mNumParticles++];
}

// CGEmitter

CGEmitter::CGEmitter(Attrib::Collection* spec, XenonEffectDef* eDef) :
    mEmitterDef(spec, 0),
    mTextureUVs(mEmitterDef.emitteruv(), 0)
{
    mLocalWorld = eDef->mat;
    mVel = eDef->vel;
};

void CGEmitter::SpawnParticles(float dt, float intensity, bool isContrail)
{
    if (intensity <= 0.0f)
        return;

    // Local variables
    struct UMath::Matrix4 local_world; // r1+0x8
    struct UMath::Matrix4 local_orientation; // r1+0x48
    unsigned int random_seed = randomSeed; // r1+0xC8
    float life_variance = mEmitterDef.Life() * mEmitterDef.LifeVariance(); // f0
    float life = mEmitterDef.Life() - life_variance; // f23
    int r = (int)(mEmitterDef.Colour1().x * 255.0f); // r7
    int g = (int)(mEmitterDef.Colour1().y * 255.0f); // r8
    int b = (int)(mEmitterDef.Colour1().z * 255.0f); // r10
    int a = (int)(mEmitterDef.Colour1().w * 255.0f); // r9

    // begin next gen code
    if (!bCGIntensityBehavior)
    {
        if (intensity != 1.0f)
        {
            a = (int)std::fminf(42.0f, intensity * 42.0f);
        }

        intensity = std::fmaxf(1.0f, intensity);
    }
    // end next gen code

    unsigned int particleColor = (a << 24) | (r << 16) | (g << 8) | b; // r26
    float num_particles_variance = (intensity * mEmitterDef.NumParticles()) * mEmitterDef.NumParticlesVariance() * 100.0f; // f0
    float num_particles = (intensity * mEmitterDef.NumParticles()) - num_particles_variance; // f29
    float particle_age_factor = dt / num_particles; // f22
    float current_particle_age = 0.0f; // f27

    local_world = this->mLocalWorld;
    local_orientation = this->mLocalWorld;

    while (num_particles > 0.0f)
    {
        NGParticle* particle;
        float sparkLength; // f30
        float ld;
        struct UMath::Vector4 pvel; // r1+0x88
        struct UMath::Vector4 rand; // r1+0x98
        struct UMath::Vector4 rotatedVel; // r1+0xA8
        float gravity; // f30
        struct UMath::Vector4 ppos; // r1+0xB8

        num_particles--;

        if (!(particle = gParticleList.GetNextParticle())) // get next particle in list
            break;

        sparkLength = mEmitterDef.LengthStart() + bRandom_Float_Int(mEmitterDef.LengthDelta(), &random_seed);

        if (sparkLength < 0.0f)
            break;
        else if (sparkLength >= 255.0f)
            sparkLength = 255.0f;

        rand.x = 1.0f - (mEmitterDef.VelocityDelta().x - (2 * bRandom_Float_Int(mEmitterDef.VelocityDelta().x, &random_seed)));
        rand.y = 1.0f - (mEmitterDef.VelocityDelta().y - (2 * bRandom_Float_Int(mEmitterDef.VelocityDelta().y, &random_seed)));
        rand.z = 1.0f - (mEmitterDef.VelocityDelta().z - (2 * bRandom_Float_Int(mEmitterDef.VelocityDelta().z, &random_seed)));

        pvel = mEmitterDef.VelocityInherit();

        UMath::fpu::Scalexyz(pvel, mVel, pvel);
        UMath::fpu::Rotate(mEmitterDef.VelocityStart(), local_orientation, rotatedVel);
        UMath::fpu::Add(pvel, rotatedVel, pvel);
        UMath::fpu::Scalexyz(pvel, rand, pvel);

        particle->vel.x = pvel.x;
        particle->vel.y = pvel.y;
        particle->vel.z = pvel.z;

        rand.x = bRandom_Float_Int(mEmitterDef.VolumeExtent().x, &random_seed);
        rand.y = bRandom_Float_Int(mEmitterDef.VolumeExtent().y, &random_seed);
        rand.z = bRandom_Float_Int(mEmitterDef.VolumeExtent().z, &random_seed);

        ppos.x = rand.x - mEmitterDef.VolumeExtent().x * 0.5f + mEmitterDef.VolumeCenter().x;
        ppos.y = rand.y - mEmitterDef.VolumeExtent().y * 0.5f + mEmitterDef.VolumeCenter().y;
        ppos.z = rand.z - mEmitterDef.VolumeExtent().z * 0.5f + mEmitterDef.VolumeCenter().z;
        ppos.w = 1.0f;

        UMath::fpu::RotateTranslate(ppos, local_world, ppos);
        UMath::fpu::ScaleAdd(*(UMath::Vector3*)&pvel, current_particle_age, *(UMath::Vector3*)&ppos, particle->initialPos);

        particle->age = current_particle_age;

        gravity = (bRandom_Float_Int(mEmitterDef.GravityDelta(), &random_seed) * 2) + mEmitterDef.GravityStart() - mEmitterDef.GravityDelta();
        particle->gravity = gravity;

        particle->initialPos.z += gravity * current_particle_age * current_particle_age; // fall

        particle->life = (uint16_t)(life * 8191);
        particle->width = (uint8_t)mEmitterDef.HeightStart();
        particle->length = (uint8_t)sparkLength;

        particle->color = particleColor;

        particle->uv[0] = (uint8_t)(mTextureUVs.StartU() * 255.0f);
        particle->uv[1] = (uint8_t)(mTextureUVs.StartV() * 255.0f);
        particle->uv[2] = (uint8_t)(mTextureUVs.EndU() * 255.0f);
        particle->uv[3] = (uint8_t)(mTextureUVs.EndV() * 255.0f);

        current_particle_age += particle_age_factor;

        // begin next gen code
        particle->flags = (!mEmitterDef.zSprite() ? NGParticle::Flags::DEBRIS : NULL);
        particle->spin = mEmitterDef.Spin();
        if ((particle->flags & NGParticle::Flags::BOUNCED) == 0 && !isContrail && bBounceParticles)
        {
            CalcCollisiontime(particle);
        }
        // end next gen code
    }

    randomSeed = random_seed;
}

// NGEffect

NGEffect::NGEffect(XenonEffectDef* eDef, float dt) :
    mEffectDef(eDef->spec, 0)
{
    int numEmitters;
    //((void(__thiscall*)(NGEffect*, XenonEffectDef*, float)) & NGEffect_NGEffect)(this, eDef, dt);
    if (mEffectDef.mCollection)
    {
        numEmitters = mEffectDef.Num_NGEmitter();
        for (int i = 0; i < numEmitters; i++)
        {
            float intensity = SparkIntensity;

            if (!eDef->piggyback_effect && !bUseCGStyle)
            {
                float carspeed = (UMath::Length(*(UMath::Vector3*)&eDef->vel) - ContrailSpeed) / 30.0f;
                intensity = UMath::Lerp(ContrailMinIntensity, ContrailMaxIntensity, carspeed);
                if (intensity > ContrailMaxIntensity)
                    intensity = ContrailMaxIntensity;
                else if (ContrailMinIntensity > intensity)
                    intensity = 0.1f;
            }

            Attrib::Collection* emspec = mEffectDef.NGEmitter(i).GetCollection();
            CGEmitter anEmitter{ emspec, eDef };
            anEmitter.SpawnParticles(dt, intensity, !eDef->piggyback_effect);
        }
    }
}

// external interface

void AddXenonEffect(
    struct EmitterGroup* piggyback_fx,
    Attrib::Collection* spec,
    UMath::Matrix4* mat,
    UMath::Vector4* vel)
{
    XenonEffectDef newEffect;

    if (bLimitSparkRate && piggyback_fx)
    {
        if (gElapsedSparkTime < 1000.0f / SparkTargetFPS)
            return;
    }
    else if (bLimitContrailRate && !piggyback_fx)
    {
        if (gElapsedContrailTime < 1000.0f / ContrailTargetFPS)
            return;
    }

    if (gNGEffectList.size() < gNGEffectList.capacity())
    {
        newEffect.mat.v3 = mat->v3;
        newEffect.spec = spec;
        newEffect.vel = *vel;
        newEffect.piggyback_effect = piggyback_fx;
        gNGEffectList.push_back(newEffect);
    }
}

void UpdateXenonEmitters(float dt)
{
    gParticleList.AgeParticles(dt);

    // spawn emitters from all emitterdefs
    for (size_t i = 0; i < gNGEffectList.size(); i++)
    {
        XenonEffectDef& effectDef = gNGEffectList[i];
        if (!effectDef.piggyback_effect || (effectDef.piggyback_effect->mFlags & 0x10) != 0)
        {
            NGEffect effect{ &effectDef, dt }; // create NGEffect from effect def
        }
    }

    // clear list of emitterdefs
    gNGEffectList.clear();

    // generate mesh for rendering
    gParticleList.GeneratePolys();
}
