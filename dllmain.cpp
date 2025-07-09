// NFS Most Wanted - Xenon Effects implementation
// A port of the XenonEffect particle effect emitters from NFS Carbon
// by Xan/Tenjoin

// BUG LIST:
// - particles stay in the world after restart - MAKE A XENON EFFECT RESET
// - contrails get overwritten by sparks at high rates
// - texture filtering messes with drawing coordinates - if it's disabled, sparks are rendered off screen...
//

#include "stdio.h"
#include "includes\injector\injector.hpp"
#include "includes\mINI\src\mini\ini.h"
#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")
#include <d3dx9.h>
#include <cmath>
#include <vector>
#include <UMath/UMath.h>
#include <Attrib/Attrib.h>
using namespace std;

#pragma runtime_checks( "", off )

// uncomment to enable contrail test near the SkipFE start location in MW's map (next to car lot in College/Rosewood)
//#define CONTRAIL_TEST

bool bContrails = true;
bool bLimitContrailRate = true;
bool bLimitSparkRate = true;
bool bNISContrails = false;
bool bUseCGStyle = false;
bool bPassShadowMap = false;
bool bUseD3DDeviceTexture = false;
bool bBounceParticles = true;
bool bCarbonBounceBehavior = false;
bool bFadeOutParticles = false;
float ContrailTargetFPS = 30.0f;
float SparkTargetFPS = 60.0f;
float ContrailSpeed = 44.0f;
float ContrailMinIntensity = 0.1f;
float ContrailMaxIntensity = 0.75f;
float SparkIntensity = 1.0f;
char TPKfilename[128] = { "GLOBAL\\XenonEffects.tpk" };
uint32_t MaxParticles = 2048; // MW default
uint32_t NGEffectListSize = 100; // MW default

uint32_t ContrailFrameDelay = 1;
uint32_t SparkFrameDelay = 1;

#define GLOBAL_D3DDEVICE 0x00982BDC
#define GAMEFLOWSTATUS_ADDR 0x00925E90
#define FASTMEM_ADDR 0x00925B30
#define NISINSTANCE_ADDR 0x009885C8
#define WORLDPRELITSHADER_OBJ_ADDR 0x93DEBC
#define CURRENTSHADER_OBJ_ADDR 0x00982C80

// this is nowhere near complete, but it's just enough
class eEffect
{
public:
    virtual void vecDTor() = 0;
    virtual void Start() = 0;
    virtual void End() = 0;
private:
    uint8_t pad[0x44];
public:
    LPD3DXEFFECT mEffectHandlePlat;

    inline static void SetAlphaTestState(DWORD AlphaTestEnable, DWORD AlphaRef)
    {
        ((void(*)(DWORD, DWORD))0x6C67C0)(AlphaTestEnable, AlphaRef);
    };

    inline static void SetAlphaBlendState(DWORD AlphaBlendEnable, D3DBLEND SrcBlend, D3DBLEND DestBlend)
    {
        ((void(*)(DWORD, D3DBLEND, D3DBLEND))0x6C6810)(AlphaBlendEnable, SrcBlend, DestBlend);
    };

    inline static void SetAlphaTestState()
    {
        ((void(*)())0x6C83A0)();
    };
};

#define FRAMECOUNTER_ADDR 0x00982B78
#define eFrameCounter *(uint32_t*)FRAMECOUNTER_ADDR

class bNode {
    void* Prev;
    void* Next;
};

template <class T> class bTNode : bNode {};

struct RenderState {
    uint32_t _bf_0;
};

class TextureInfoPlatInfo : public bTNode<TextureInfoPlatInfo> {
public:
    RenderState mRenderState;
    uint32_t type;

private:
    uint16_t Pad0;

public:
    uint16_t PunchThruValue;
    uint32_t format;
    LPDIRECT3DBASETEXTURE9 pD3DTexture;
    void* pActiveBucket;
};

class TextureInfoPlatInterface {
public:
    TextureInfoPlatInfo* PlatInfo;
};

class TextureInfo : public TextureInfoPlatInterface, public bTNode<TextureInfo> {
public:
    char DebugName[24];
    uint32_t NameHash;
    uint32_t ClassNameHash;

private:
    uint32_t Padding0;

public:
    uint32_t ImagePlacement;
    uint32_t PalettePlacement;
    uint32_t ImageSize;
    uint32_t PaletteSize;
    uint32_t BaseImageSize;
    uint16_t Width;
    uint16_t Height;
    uint8_t ShiftWidth;
    uint8_t ShiftHeight;
    uint8_t ImageCompressionType;
    uint8_t PaletteCompressionType;
    uint16_t NumPaletteEntries;
    uint8_t NumMipMapLevels;
    uint8_t TilableUV;
    uint8_t BiasLevel;
    uint8_t RenderingOrder;
    uint8_t ScrollType;
    uint8_t UsedFlag;
    uint8_t ApplyAlphaSorting;
    uint8_t AlphaUsageType;
    uint8_t AlphaBlendType;
    uint8_t Flags;
    uint8_t MipmapBiasType;

private:
    uint8_t Padding1;

public:
    uint16_t ScrollTimeStep;
    uint16_t ScrollSpeedS;
    uint16_t ScrollSpeedT;
    uint16_t OffsetS;
    uint16_t OffsetT;
    uint16_t ScaleS;
    uint16_t ScaleT;
    void* pTexturePack;
    void* ImageData;
    void* PaletteData;

private:
    uint32_t Padding2[2];
};

void*(__thiscall* FastMem_Alloc)(void* FastMem, unsigned int bytes, char* kind) = (void*(__thiscall*)(void*, unsigned int, char*))0x005D29D0;
void* (__thiscall* FastMem_Free)(void* FastMem, void* ptr, unsigned int bytes, char* kind) = (void* (__thiscall*)(void*, void*, unsigned int, char*))0x005D0370;
void (__stdcall* __CxxThrowException)(int arg1, int arg2) = (void (__stdcall*)(int, int))0x007C56B0;
void* (__thiscall* Attrib_Instance_MW)(void* Attrib, void* AttribCollection, unsigned int unk, void* ucomlist) = (void* (__thiscall*)(void*, void*, unsigned int, void*))0x00452380;
void*(__cdecl* Attrib_DefaultDataArea)(unsigned int size) = (void*(__cdecl*)(unsigned int))0x006269B0;
void* (__thiscall* Attrib_Instance_Get)(void* AttribCollection, unsigned int unk, unsigned int hash) = (void* (__thiscall*)(void*, unsigned int, unsigned int))0x004546C0;
void* (__thiscall* Attrib_Attribute_GetLength)(void* AttribCollection) = (void* (__thiscall*)(void*))0x00452D40;
void* (__thiscall* Attrib_Dtor)(void* AttribCollection) = (void* (__thiscall*)(void*))0x00452BD0;
void* (__thiscall* Attrib_Instance_GetAttributePointer)(void* AttribCollection, unsigned int hash, unsigned int unk) = (void* (__thiscall*)(void*, unsigned int, unsigned int))0x00454810;
void* (__thiscall* Attrib_RefSpec_GetCollection)(void* Attrib) = (void* (__thiscall*)(void*))0x004560D0;
void* (__thiscall* Attrib_Instance_Dtor)(void* AttribInstance) = (void* (__thiscall*)(void*))0x0045A430;
void* (__thiscall* Attrib_Instance_Refspec)(void* AttribCollection, void* refspec, unsigned int unk, void* ucomlist) = (void* (__thiscall*)(void*, void*, unsigned int, void*))0x00456CB0;
void* (__cdecl* Attrib_FindCollection)(uint32_t param1, uint32_t param2) = (void* (__cdecl*)(uint32_t, uint32_t))0x00455FD0;
float (__cdecl* bRandom_Float_Int)(float range, unsigned int * seed) = (float (__cdecl*)(float, unsigned int*))0x0045D9E0;
int(__cdecl* bRandom_Int_Int)(int range, unsigned int* seed) = (int(__cdecl*)(int, unsigned int*))0x0045D9A0;
unsigned int(__cdecl* bStringHash)(const char* str) = (unsigned int(__cdecl*)(const char*))0x00460BF0;
TextureInfo*(__cdecl* GetTextureInfo)(unsigned int name_hash, int return_default_texture_if_not_found, int include_unloaded_textures) = (TextureInfo*(__cdecl*)(unsigned int, int, int))0x00503400;
void (__thiscall* EmitterSystem_UpdateParticles)(void* EmitterSystem, float dt) = (void (__thiscall*)(void*, float))0x00508C30;
void(__thiscall* EmitterSystem_Render)(void* EmitterSystem, void* eView) = (void(__thiscall*)(void*, void*))0x00503D00;
void(__stdcall* sub_7286D0)() = (void(__stdcall*)())0x007286D0;
void* (__cdecl* FastMem_CoreAlloc)(uint32_t size, char* debug_line) = (void* (__cdecl*)(uint32_t, char*))0x00465A70;
void(__stdcall* sub_739600)() = (void(__stdcall*)())0x739600;
void(__thiscall* CarRenderConn_UpdateEngineAnimation)(void* CarRenderConn, float param1, void* PktCarService) = (void(__thiscall*)(void*, float, void*))0x00745F20;
void(__stdcall* sub_6CFCE0)() = (void(__stdcall*)())0x6CFCE0;
void(__cdecl* ParticleSetTransform)(D3DXMATRIX* worldmatrix, uint32_t EVIEW_ID) = (void(__cdecl*)(D3DXMATRIX*, uint32_t))0x6C8000;
bool(__thiscall* WCollisionMgr_CheckHitWorld)(void* WCollisionMgr, UMath::Vector4* inputSeg, void* cInfo, uint32_t primMask) = (bool(__thiscall*)(void*, UMath::Vector4*, void*, uint32_t))0x007854B0;
void(__cdecl* GameSetTexture)(void* TextureInfo, uint32_t unk) = (void(__cdecl*)(void*, uint32_t))0x006C68B0;
void* (*CreateResourceFile)(char* filename, int ResFileType, int unk1, int unk2, int unk3) = (void* (*)(char*, int, int, int, int))0x0065FD30;
void(__thiscall* ResourceFile_BeginLoading)(void* ResourceFile, void* callback, void* unk) = (void(__thiscall*)(void*, void*, void*))0x006616F0;
void(*ServiceResourceLoading)() = (void(*)())0x006626B0;
uint32_t(__stdcall* sub_6DFAF0)() = (uint32_t(__stdcall*)())0x6DFAF0;
uint32_t(* Attrib_StringHash32)(const char* k) = (uint32_t(*)(const char*))0x004519D0;

void __stdcall LoadResourceFile(char* filename, int ResType, int unk1, void* unk2, void* unk3, int unk4, int unk5)
{
    ResourceFile_BeginLoading(CreateResourceFile(filename, ResType, unk1, unk4, unk5), unk2, unk3);
}

// bridge the difference between MW and Carbon
void* __stdcall Attrib_Instance(void* collection, uint32_t msgPort)
{
    uint32_t that;
    _asm mov that, ecx
    auto result = Attrib_Instance_MW((void*)that, collection, msgPort, NULL);

    return result;
}

// function maps from Carbon to MW
unsigned int __ftol2 = 0x007C4B80;
unsigned int sub_6016B0 = 0x5C5E80;
unsigned int sub_404A20 = 0x004048C0;
unsigned int rsqrt = 0x00410220;
unsigned int sub_478200 = 0x466520;
unsigned int sub_6012B0 = 0x4FA510;

struct XenonEffectDef
{
    float           intensity;
    UMath::Vector4  vel;
    UMath::Matrix4  mat;
    Attrib::Collection* spec;
    void* piggyback_effect;
};

namespace bstl
{
    class allocator
    {
        inline void* allocate(size_t n)
        {
            return FastMem_Alloc((void*)FASTMEM_ADDR, n, NULL);
        }

        inline void* deallocate(void* p, size_t n)
        {
            FastMem_Free((void*)FASTMEM_ADDR, p, n, NULL);
        }
    };
}

namespace eastl
{
    template <typename T, typename Allocator>
    class __declspec(align(4)) VectorBase
    {
    public:
        T* mpBegin = NULL;
        T* mpEnd = NULL;
        T* mpCapacity = NULL;
        Allocator mAllocator;
    };

    template <typename T, typename Allocator>
    class vector : public VectorBase<T, Allocator>{};
}


class XenonFXVec : public eastl::vector<XenonEffectDef, bstl::allocator>
{
};

class XenonEffectList : public XenonFXVec
{
};


//char gNGEffectList[64];
XenonEffectList gNGEffectList;

class NGParticle
{
public:

    enum Flags : uint8_t
    {
        DEBRIS = 1 << 0,
        SPAWN = 1 << 1,
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

inline constexpr NGParticle::Flags operator|(NGParticle::Flags a, NGParticle::Flags b)
{
    return static_cast<NGParticle::Flags>(static_cast<int>(a) | static_cast<int>(b));
}

inline constexpr NGParticle::Flags operator&(NGParticle::Flags a, NGParticle::Flags b)
{
    return static_cast<NGParticle::Flags>(static_cast<int>(a) & static_cast<int>(b));
}

struct eView
{
    void* PlatInfo;
    uint32_t EVIEW_ID;
};

class ParticleList
{
public:
    NGParticle* mParticles;// [1000] ;
    unsigned int mNumParticles;
    void AgeParticles(float dt);
    void GeneratePolys(eView* view);
    NGParticle* GetNextParticle();
};

ParticleList gParticleList;
//uint32_t NumParticles = 0;

//#define numParticles dword ptr gParticleList[PARTICLELIST_SIZE]

// copies the object back to MW's format to avoid crashing during Attrib garbage collection
char fuelcell_attrib_buffer4[20];
void __fastcall Attrib_Instance_Dtor_Shim(void* _this)
{
    memset(fuelcell_attrib_buffer4, 0, 20);
    memcpy(&(fuelcell_attrib_buffer4[4]), (void*)_this, 16);
    memcpy((void*)_this, fuelcell_attrib_buffer4, 20);

    Attrib_Instance_Dtor((void*)_this);
}

// TODO - replace this with proper Attrib bridge - TBD when fully decompiled
//namespace Attrib
//{
//    class CarbonInstance
//    {
//    public:
//        Attrib::Collection* mCollection;
//        void* mLayoutPtr;
//        unsigned int mMsgPort;
//        unsigned int mFlags;
//
//        ~CarbonInstance()
//        {
//            //Attrib_Instance_Dtor_Shim(this);
//        }
//    };
//
//    namespace Gen
//    {
//        struct RefSpec
//        {
//            uint32_t mClassKey;
//            uint32_t mCollectionKey;
//            Attrib::Collection* mCollectionPtr;
//        };
//
//        class fuelcell_effect : public CarbonInstance
//        {
//        public:
//            struct _LayoutStruct
//            {
//                bool doTest;
//            };
//        };
//
//        class fuelcell_emitter : public CarbonInstance
//        {
//        public:
//            struct _LayoutStruct
//            { // TODO - carbon layout, replace with MW's
//                UMath::Vector4 VolumeExtent;
//                UMath::Vector4 VolumeCenter;
//                UMath::Vector4 VelocityStart;
//                UMath::Vector4 VelocityInherit;
//                UMath::Vector4 VelocityDelta;
//                UMath::Vector4 Colour1;
//                Attrib::RefSpec emitteruv;
//                float NumParticlesVariance;
//                float NumParticles;
//                float LifeVariance;
//                float Life;
//                float LengthStart;
//                float LengthDelta;
//                float HeightStart;
//                float GravityStart;
//                float GravityDelta;
//                float Elasticity;
//                char zDebrisType;
//                char zContrail;
//            };
//        };
//
//        class emitteruv : public CarbonInstance
//        {
//        public:
//            struct _LayoutStruct
//            {
//                float StartV;
//                float StartU;
//                float EndV;
//                float EndU;
//            };
//        };
//    }
//}

LPDIRECT3DDEVICE9 &g_D3DDevice = *(LPDIRECT3DDEVICE9*)0x982BDC;

template <typename T, typename U>
struct SpriteBuffer
{
    LPDIRECT3DVERTEXBUFFER9 mpVB = NULL;
    LPDIRECT3DINDEXBUFFER9 mpIB = NULL;
    unsigned int mVertexCount = 0;
    unsigned int mMaxSprites = 0;
    unsigned int mNumPolys = 0;

    T* mLockedVB;

    void Draw(eEffect &effect, TextureInfo* pTexture)
    {
        g_D3DDevice->SetStreamSource(0, mpVB, 0, sizeof(U));
        g_D3DDevice->SetIndices(mpIB);

        if (pTexture)
        {
            if (bUseD3DDeviceTexture)
            {
                g_D3DDevice->SetTexture(0, pTexture->PlatInfo->pD3DTexture);
                g_D3DDevice->SetRenderState(D3DRS_ALPHAREF, (DWORD)0x000000B0);
                g_D3DDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
                g_D3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                g_D3DDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
                g_D3DDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
                g_D3DDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
                g_D3DDevice->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
            }
            else
                GameSetTexture(pTexture, 0);
        }

        effect.mEffectHandlePlat->CommitChanges();

        g_D3DDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);

        g_D3DDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4 * mNumPolys, 0, 2 * mNumPolys);
    }

    void Init(uint32_t spriteCount)
    {
        unsigned int v5; // ebp
        uint16_t v6; // ecx
        int v7; // eax

        uint16_t* idxBuf;

        mpVB = NULL;
        mNumPolys = 0;
        mVertexCount = 4 * spriteCount;
        mMaxSprites = spriteCount;
        g_D3DDevice->CreateVertexBuffer(sizeof(T) * (spriteCount + 3), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &mpVB, 0);
        g_D3DDevice->CreateIndexBuffer(
            12 * spriteCount,
            D3DUSAGE_WRITEONLY,
            D3DFMT_INDEX16,
            D3DPOOL_MANAGED,
            &mpIB,
            0);
        v5 = 0;
        if (mpIB->Lock(NULL, NULL, (void**)&idxBuf, NULL) != S_OK)
        {
            mpIB = NULL;
        }
        else
        {
            if (spriteCount)
            {
                v6 = 0;
                v7 = 3;
                do
                {
                    idxBuf[v7 - 3] = v6;
                    idxBuf[v7 - 2] = v6 + 1;
                    idxBuf[v7 - 1] = v6 + 2;
                    idxBuf[v7] = v6;
                    idxBuf[v7 + 1] = v6 + 2;
                    idxBuf[v7 + 2] = v6 + 3;
                    ++v5;
                    v7 += 6;
                    v6 += 4;
                } while (v5 < spriteCount);
            }
            mpIB->Unlock();
        }
    }

    void Reset()
    {
         if (mpVB)
             mpVB->Release();
         if (mpIB)
             mpIB->Release();
    }

    void Lock(size_t viewId)
    {
        mpVB->Lock(
            0,
            sizeof(T) * mMaxSprites,
            (void**)&mLockedVB,
            D3DLOCK_DISCARD);

        mNumPolys = 0;
    }

    void Unlock(size_t viewId)
    {
        mpVB->Unlock();
        mLockedVB = NULL;
    }
};

struct XSparkVert
{
    UMath::Vector3 position;
    unsigned int color;
    float texcoord[2];
};

struct XSpark
{
    XSparkVert v[4];
};

template <typename Sprite, typename SpriteVert, size_t NumViews>
class XSpriteList
{
public:
    SpriteBuffer<Sprite, SpriteVert> mSprintListView[NumViews];
    size_t mNumViews = NumViews;
    size_t mCurrViewBuffer = 0;
    TextureInfo* mTexture = NULL;

    void Draw(int viewId, int viewBuffer, eEffect &effect, TextureInfo* pOverrideTexture)
    {
        (void)viewId; // not using this atm

        if (viewBuffer >= 0 && mSprintListView[viewBuffer].mNumPolys)
        {
            mSprintListView[viewBuffer].Draw(effect, pOverrideTexture ? pOverrideTexture : this->mTexture);
        }
    }

    void Init(const uint32_t spriteCount)
    {
        this->mTexture = NULL;

        for (size_t i = 0; i < mNumViews; i++)
        {
            mSprintListView[i].Init(spriteCount);
        }
    }

    void Reset()
    {
        for (size_t i = 0; i < mNumViews; i++)
        {
            mSprintListView[i].Reset();
        }
    }

    void Lock(size_t viewId)
    {
        mSprintListView[viewId - 1].Lock(viewId);
    }

    void Unlock(size_t viewId)
    {
        mSprintListView[viewId - 1].Unlock(viewId);
    }
};

class XSpriteManager
{
public:
    XSpriteList<XSpark, XSparkVert, 2> sparkList; 
    bool bBatching = false;

    void DrawBatch(eView* view);
    void AddParticle(eView* view, NGParticle* particleList, unsigned int numParticles);
    void Init();
    void Reset();

    void Lock(size_t viewId)
    {
        sparkList.Lock(viewId);
    }

    void Unlock(size_t viewId)
    {
        sparkList.Unlock(viewId);
    }
};

float flt_9C92F0 = 255.0f;
float flt_9C2478 = 1.0f;
float flt_9EA540 = 42.0f;
float flt_9C2C44 = 100.0f;
float flt_9C77C8 = 0.0039215689f;
float flt_9EAFFC = 0.00048828125f;
float flt_9C248C = 0.0f;
float flt_A6C230 = 0.15000001f;
float flt_9C2A3C = 4.0f;
float flt_9C2888 = 0.5f;
unsigned int randomSeed = 0xDEADBEEF;
const char* TextureName = "MAIN";
float gFrameDT = 0.0f;

struct fuelcell_emitter_mw
{
    UMath::Vector4 VolumeCenter;
    UMath::Vector4 VelocityDelta;
    UMath::Vector4 VolumeExtent;
    UMath::Vector4 VelocityInherit;
    UMath::Vector4 VelocityStart;
    UMath::Vector4 Colour1;
    uint32_t emitteruv_classKey;
    uint32_t emitteruv_collectionKey;
    void* emitteruv_collection;
    float Life;
    float NumParticlesVariance;
    float GravityStart;
    float HeightStart;
    float GravityDelta;
    float LengthStart;
    float LengthDelta;
    float LifeVariance;
    float NumParticles;
    uint16_t Spin;
    uint8_t zSprite;
    uint8_t zContrail;
};

struct fuelcell_emitter_carbon
{
    UMath::Vector4 VolumeCenter;
    UMath::Vector4 VelocityDelta;
    UMath::Vector4 VolumeExtent;
    UMath::Vector4 VelocityInherit;
    UMath::Vector4 VelocityStart;
    UMath::Vector4 Colour1;
    uint32_t emitteruv_classKey;
    uint32_t emitteruv_collectionKey;
    void* emitteruv_collection;
    float Life;
    float NumParticlesVariance;
    float GravityStart;
    float HeightStart;
    float GravityDelta;
    float Elasticity;
    float LengthStart;
    float LengthDelta;
    float LifeVariance;
    float NumParticles;
    uint8_t zDebrisType;
    uint8_t zContrail;
}bridge_instance;

struct ElasticityPair
{
    uint32_t emmitter_key;
    float Elasticity;
};
vector<ElasticityPair> elasticityValues;

float GetElasticityValue(uint32_t key)
{
    for (size_t i = 0; i < elasticityValues.size(); i++)
    {
        if (elasticityValues.at(i).emmitter_key == key)
            return elasticityValues.at(i).Elasticity;
    }
    return 0.0f;
}

XSpriteManager NGSpriteManager;

float GetTargetFrametime()
{
    return **(float**)0x6612EC;
}

uint32_t bridge_oldaddr = 0;
uint32_t bridge_instance_addr = 0;
void __stdcall fuelcell_emitter_bridge()
{
    uint32_t that;
    _asm mov that, ecx
    bridge_instance_addr = that;

    uint32_t instance_pointer = *(uint32_t*)(that + 4);
    uint32_t key = *(uint32_t*)((*(uint32_t*)(that)) + 0x20);

    fuelcell_emitter_mw* mw_emitter = (fuelcell_emitter_mw*)instance_pointer;
    memset(&bridge_instance, 0, sizeof(fuelcell_emitter_carbon));
    // copy the matching data first (first 0x80 bytes)
    memcpy(&bridge_instance, mw_emitter, 0x80);
    // adapt
    bridge_instance.LengthStart = mw_emitter->LengthStart;
    bridge_instance.LengthDelta = mw_emitter->LengthDelta;
    bridge_instance.LifeVariance = mw_emitter->LifeVariance;
    bridge_instance.NumParticles = mw_emitter->NumParticles;
    bridge_instance.zContrail = mw_emitter->zContrail;
    bridge_instance.Elasticity = GetElasticityValue(key);
    
    //printf("key: 0x%X Elasticity: %.2f\n", key, bridge_instance.Elasticity);

    // no idea if this is right
    //bridge_instance.zDebrisType = (*mw_emitter).unk_0xD8782949;

    // save & write back the pointer
    bridge_oldaddr = instance_pointer;
    *(uint32_t*)(that + 4) = (uint32_t)&bridge_instance;
}

void __stdcall fuelcell_emitter_bridge_restore()
{
    *(uint32_t*)(bridge_instance_addr + 4) = bridge_oldaddr;
}

void* FastMem_CoreAlloc_Wrapper(uint32_t size)
{
    return FastMem_CoreAlloc(size, 0);
}

void __declspec(naked) sub_736EA0()
{
    _asm
    {
        sub     esp, 10h
        push    esi
        push    0FE40E637h
        lea     eax, [esp + 8]
        push    eax
        call    Attrib_Instance_Get ; Attrib::Instance::Get(const(ulong))
        mov     ecx, eax
        call    Attrib_Attribute_GetLength ; Attrib::Attribute::GetLength(const(void))
        pop     esi
        add     esp, 10h
        retn
    }
}

// EASTL List stuff Start
void __declspec(naked) eastl_vector_uninitialized_copy_impl_XenonEffectDef()
{
    _asm
    {
                mov     eax, [esp+8]
                mov     edx, [esp+10h]
                push    ebx
                mov     ebx, [esp+10h]
                cmp     eax, ebx
                jz      short loc_74C80E
                push    esi
                push    edi

loc_74C7F3:                             ; CODE XREF: eastl::uninitialized_copy_impl<eastl::generic_iterator<XenonEffectDef *,void>,eastl::generic_iterator<XenonEffectDef *,void>>(eastl::generic_iterator<XenonEffectDef *,void>,eastl::generic_iterator<XenonEffectDef *,void>,eastl::generic_iterator<XenonEffectDef *,void>,eastl::integral_constant<bool,0>)+2A↓j
                test    edx, edx
                jz      short loc_74C802
                mov     ecx, 17h
                mov     esi, eax
                mov     edi, edx
                rep movsd

loc_74C802:                             ; CODE XREF: eastl::uninitialized_copy_impl<eastl::generic_iterator<XenonEffectDef *,void>,eastl::generic_iterator<XenonEffectDef *,void>>(eastl::generic_iterator<XenonEffectDef *,void>,eastl::generic_iterator<XenonEffectDef *,void>,eastl::generic_iterator<XenonEffectDef *,void>,eastl::integral_constant<bool,0>)+15↑j
                add     eax, 5Ch ; '\'
                add     edx, 5Ch ; '\'
                cmp     eax, ebx
                jnz     short loc_74C7F3
                pop     edi
                pop     esi

loc_74C80E:                             ; CODE XREF: eastl::uninitialized_copy_impl<eastl::generic_iterator<XenonEffectDef *,void>,eastl::generic_iterator<XenonEffectDef *,void>>(eastl::generic_iterator<XenonEffectDef *,void>,eastl::generic_iterator<XenonEffectDef *,void>,eastl::generic_iterator<XenonEffectDef *,void>,eastl::integral_constant<bool,0>)+F↑j
                mov     eax, [esp+8]
                mov     [eax], edx
                pop     ebx
                retn
    }
}

void __declspec(naked) eastl_vector_erase_XenonEffectDef()
{
    _asm
    {
                push    ecx
                push    ebx
                mov     ebx, [ecx+4]
                push    ebp
                mov     ebp, [esp+14h]
                cmp     ebp, ebx
                push    esi
                mov     esi, [esp+14h]
                mov     [esp+0Ch], ecx
                mov     edx, esi
                mov     eax, ebp
                jz      short loc_752BFE
                push    edi
                lea     esp, [esp+0]

loc_752BE0:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::erase(XenonEffectDef *,XenonEffectDef *)+33↓j
                mov     esi, eax
                mov     edi, edx
                add     eax, 5Ch ; '\'
                mov     ecx, 17h
                add     edx, 5Ch ; '\'
                cmp     eax, ebx
                rep movsd
                jnz     short loc_752BE0
                mov     ecx, [esp+10h]
                mov     esi, [esp+18h]
                pop     edi

loc_752BFE:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::erase(XenonEffectDef *,XenonEffectDef *)+19↑j
                sub     ebp, esi
                mov     eax, 4DE9BD37h
                imul    ebp
                sub     edx, ebp
                sar     edx, 6
                mov     eax, edx
                shr     eax, 1Fh
                add     eax, edx
                mov     edx, [ecx+4]
                imul    eax, 5Ch ; '\'
                add     edx, eax
                mov     eax, esi
                pop     esi
                pop     ebp
                mov     [ecx+4], edx
                pop     ebx
                pop     ecx
                retn    8
    }
}

void __declspec(naked) eastl_vector_DoInsertValue_XenonEffectDef()
{
    _asm
    {
                sub     esp, 8
                push    ebx
                push    ebp
                push    esi
                mov     ebx, ecx
                mov     eax, [ebx+8]
                push    edi
                mov     edi, [ebx+4]
                cmp     edi, eax
                jz      short loc_752CAB
                mov     eax, [esp+20h]
                mov     ebp, [esp+1Ch]
                cmp     eax, ebp
                mov     [esp+20h], eax
                jb      short loc_752C5E
                cmp     eax, edi
                jnb     short loc_752C5E
                add     eax, 5Ch ; '\'
                mov     [esp+20h], eax

loc_752C5E:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::DoInsertValue(XenonEffectDef *,XenonEffectDef const &)+21↑j
                                        ; eastl::vector<XenonEffectDef,bstl::allocator>::DoInsertValue(XenonEffectDef *,XenonEffectDef const &)+25↑j
                test    edi, edi
                jz      short loc_752C6C
                lea     esi, [edi-5Ch]
                mov     ecx, 17h
                rep movsd

loc_752C6C:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::DoInsertValue(XenonEffectDef *,XenonEffectDef const &)+30↑j
                mov     edx, [ebx+4]
                lea     eax, [edx-5Ch]
                cmp     eax, ebp
                jz      short loc_752C8B

loc_752C76:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::DoInsertValue(XenonEffectDef *,XenonEffectDef const &)+59↓j
                sub     eax, 5Ch ; '\'
                sub     edx, 5Ch ; '\'
                cmp     eax, ebp
                mov     ecx, 17h
                mov     esi, eax
                mov     edi, edx
                rep movsd
                jnz     short loc_752C76

loc_752C8B:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::DoInsertValue(XenonEffectDef *,XenonEffectDef const &)+44↑j
                mov     esi, [esp+20h]
                mov     edi, ebp
                mov     ecx, 17h
                rep movsd
                mov     eax, [ebx+4]
                pop     edi
                pop     esi
                add     eax, 5Ch ; '\'
                pop     ebp
                mov     [ebx+4], eax
                pop     ebx
                add     esp, 8
                retn    8
; ---------------------------------------------------------------------------

loc_752CAB:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::DoInsertValue(XenonEffectDef *,XenonEffectDef const &)+11↑j
                sub     edi, [ebx]
                mov     eax, 0B21642C9h
                imul    edi
                add     edx, edi
                sar     edx, 6
                mov     eax, edx
                shr     eax, 1Fh
                add     eax, edx
                jz      short loc_752CE9
                lea     edi, [eax+eax]
                test    edi, edi
                mov     [esp+14h], edi
                jz      short loc_752CF7

loc_752CCD:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::DoInsertValue(XenonEffectDef *,XenonEffectDef const &)+C5↓j
                mov     eax, edi
                imul    eax, 5Ch ; '\'
                test    eax, eax
                jz      short loc_752CF7
                push    0
                push    eax
                mov     ecx, FASTMEM_ADDR
                call    FastMem_Alloc ; FastMem::Alloc((uint,char const *))
                mov     [esp+10h], eax
                jmp     short loc_752CFF
; ---------------------------------------------------------------------------

loc_752CE9:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::DoInsertValue(XenonEffectDef *,XenonEffectDef const &)+90↑j
                mov     dword ptr [esp+14h], 1
                mov     edi, [esp+14h]
                jmp     short loc_752CCD
; ---------------------------------------------------------------------------

loc_752CF7:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::DoInsertValue(XenonEffectDef *,XenonEffectDef const &)+9B↑j
                                        ; eastl::vector<XenonEffectDef,bstl::allocator>::DoInsertValue(XenonEffectDef *,XenonEffectDef const &)+A4↑j
                mov     dword ptr [esp+10h], 0

loc_752CFF:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::DoInsertValue(XenonEffectDef *,XenonEffectDef const &)+B7↑j
                mov     ecx, [esp+1Ch]
                mov     edx, [esp+10h]
                mov     ebp, [esp+1Ch]
                mov     eax, [ebx]
                push    ecx
                push    edx
                push    ebp
                push    eax
                lea     eax, [esp+2Ch]
                push    eax
                call    eastl_vector_uninitialized_copy_impl_XenonEffectDef
                mov     eax, [esp+30h]
                add     esp, 14h
                test    eax, eax
                jz      short loc_752D37
                mov     esi, [esp+20h]
                mov     ecx, 17h
                mov     edi, eax
                rep movsd
                mov     edi, [esp+14h]

loc_752D37:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::DoInsertValue(XenonEffectDef *,XenonEffectDef const &)+F4↑j
                mov     edx, [esp+1Ch]
                mov     ecx, [ebx+4]
                push    edx
                add     eax, 5Ch ; '\'
                push    eax
                push    ecx
                lea     eax, [esp+28h]
                push    ebp
                push    eax
                call    eastl_vector_uninitialized_copy_impl_XenonEffectDef
                mov     esi, [ebx]
                add     esp, 14h
                test    esi, esi
                jz      short loc_752D81
                mov     ecx, [ebx+8]
                sub     ecx, esi
                mov     eax, 0B21642C9h
                imul    ecx
                add     edx, ecx
                sar     edx, 6
                mov     ecx, edx
                shr     ecx, 1Fh
                add     ecx, edx
                imul    ecx, 5Ch ; '\'
                push    0
                push    ecx
                push    esi
                mov     ecx, FASTMEM_ADDR
                call    FastMem_Free ; FastMem::Free((void *,uint,char const *))

loc_752D81:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::DoInsertValue(XenonEffectDef *,XenonEffectDef const &)+126↑j
                mov     eax, [esp+10h]
                imul    edi, 5Ch ; '\'
                mov     edx, [esp+1Ch]
                add     edi, eax
                mov     [ebx+8], edi
                pop     edi
                pop     esi
                pop     ebp
                mov     [ebx], eax
                mov     [ebx+4], edx
                pop     ebx
                add     esp, 8
                retn    8
    }
}

void __declspec(naked) eastl_vector_reserve_XenonEffectDef()
{
    _asm
    {
                push    ebx
                mov     ebx, [esp+8]
                push    ebp
                push    esi
                mov     esi, ecx
                mov     ebp, [esi]
                mov     ecx, [esi+8]
                sub     ecx, ebp
                mov     eax, 0B21642C9h
                imul    ecx
                add     edx, ecx
                sar     edx, 6
                mov     eax, edx
                shr     eax, 1Fh
                add     eax, edx
                cmp     ebx, eax
                jbe     loc_752858
                test    ebx, ebx
                mov     ecx, [esi+4]
                push    edi
                mov     [esp+14h], ecx
                jz      short loc_7527E1
                mov     eax, ebx
                imul    eax, 5Ch ; '\'
                test    eax, eax
                jz      short loc_7527E1
                push    0
                push    eax
                mov     ecx, FASTMEM_ADDR
                call    FastMem_Alloc ; FastMem::Alloc((uint,char const *))
                mov     edi, eax
                jmp     short loc_7527E3
; ---------------------------------------------------------------------------

loc_7527E1:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::reserve(uint)+35↑j
                                        ; eastl::vector<XenonEffectDef,bstl::allocator>::reserve(uint)+3E↑j
                xor     edi, edi

loc_7527E3:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::reserve(uint)+4F↑j
                mov     edx, [esp+14h]
                mov     eax, [esp+14h]
                push    edx
                push    edi
                push    eax
                lea     ecx, [esp+20h]
                push    ebp
                push    ecx
                call    eastl_vector_uninitialized_copy_impl_XenonEffectDef ; eastl::uninitialized_copy_impl<eastl::generic_iterator<XenonEffectDef *,void>,eastl::generic_iterator<XenonEffectDef *,void>>(eastl::generic_iterator<XenonEffectDef *,void>,eastl::generic_iterator<XenonEffectDef *,void>,eastl::generic_iterator<XenonEffectDef *,void>,eastl::integral_constant<bool,0>)
                mov     ebp, [esi]
                add     esp, 14h
                test    ebp, ebp
                jz      short loc_75282B
                mov     ecx, [esi+8]
                sub     ecx, ebp
                mov     eax, 0B21642C9h
                imul    ecx
                add     edx, ecx
                sar     edx, 6
                mov     eax, edx
                shr     eax, 1Fh
                add     eax, edx
                imul    eax, 5Ch ; '\'
                push    0
                push    eax
                push    ebp
                mov     ecx, FASTMEM_ADDR
                call    FastMem_Free ; FastMem::Free((void *,uint,char const *))

loc_75282B:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::reserve(uint)+70↑j
                mov     edx, [esi]
                imul    ebx, 5Ch ; '\'
                mov     ecx, [esi+4]
                sub     ecx, edx
                mov     eax, 0B21642C9h
                imul    ecx
                add     edx, ecx
                sar     edx, 6
                mov     eax, edx
                shr     eax, 1Fh
                add     eax, edx
                imul    eax, 5Ch ; '\'
                add     eax, edi
                add     ebx, edi
                mov     [esi], edi
                mov     [esi+4], eax
                mov     [esi+8], ebx
                pop     edi

loc_752858:                             ; CODE XREF: eastl::vector<XenonEffectDef,bstl::allocator>::reserve(uint)+25↑j
                pop     esi
                pop     ebp
                pop     ebx
                retn    4
    }
}

void(__thiscall* eastl_vector_DoInsertValue_XenonEffectDef_Abstract)(eastl::vector<XenonEffectDef, bstl::allocator>* vector, XenonEffectDef* position, XenonEffectDef* value) = (void(__thiscall*)(eastl::vector<XenonEffectDef, bstl::allocator>*, XenonEffectDef*, XenonEffectDef*)) & eastl_vector_DoInsertValue_XenonEffectDef;
void(__thiscall* eastl_vector_reserve_XenonEffectDef_Abstract)(void* vector, uint32_t n) = (void(__thiscall*)(void*, uint32_t))&eastl_vector_reserve_XenonEffectDef;
void(__thiscall* eastl_vector_erase_XenonEffectDef_Abstract)(void* vector, void* first, void* last) = (void(__thiscall*)(void*, void*, void*))&eastl_vector_erase_XenonEffectDef;

void __stdcall XenonEffectList_Initialize()
{
    eastl_vector_reserve_XenonEffectDef_Abstract(&gNGEffectList, NGEffectListSize);
    //eastl_vector_erase_XenonEffectDef_Abstract(&gNGEffectList, gNGEffectList, (void*)((uint32_t)(gNGEffectList + 4)));
}

// EASTL List stuff End

// note: unk_9D7880 == unk_8A3028

void __cdecl AddXenonEffect(
    void* piggyback_fx,
    Attrib::Collection* spec,
    UMath::Matrix4* mat,
    UMath::Vector4* vel,
    float intensity)
{
    XenonEffectDef newEffect; // [esp+8h] [ebp-5Ch] BYREF
    XenonEffectDef* listPosition = gNGEffectList.mpEnd;

    if (((uint32_t)gNGEffectList.mpEnd - (uint32_t)gNGEffectList.mpBegin) / sizeof(XenonEffectDef) < NGEffectListSize)
    {
        memcpy(&newEffect.mat, (const void*)0x8A3028, sizeof(newEffect.mat));
        newEffect.mat.v3.x = mat->v3.x;
        newEffect.mat.v3.y = mat->v3.y;
        newEffect.mat.v3.z = mat->v3.z;
        newEffect.mat.v3.w = mat->v3.w;
        newEffect.spec = spec;
        newEffect.vel.x = vel->x;
        newEffect.vel.y = vel->y;
        newEffect.vel.z = vel->z;
        newEffect.vel.w = vel->w;
        newEffect.piggyback_effect = piggyback_fx;
        newEffect.intensity = intensity;
        if ((uint32_t)gNGEffectList.mpEnd >= (uint32_t)gNGEffectList.mpCapacity)
        {
            eastl_vector_DoInsertValue_XenonEffectDef_Abstract(&gNGEffectList, gNGEffectList.mpEnd, &newEffect);
        }
        else
        {
            ++gNGEffectList.mpEnd;
            if (listPosition)
                memcpy(listPosition, &newEffect, sizeof(XenonEffectDef));
        }
    }
}

struct WCollisionMgr
{
    unsigned int fSurfaceExclusionMask;
    unsigned int fPrimitiveMask;

    struct WorldCollisionInfo
    {
        UMath::Vector4 fCollidePt;
        UMath::Vector4 fNormal;
        int pad[16];
    };
};

void __cdecl CalcCollisiontime(NGParticle* particle)
{
    double v2; // st7
    float x; // ecx
    double v4; // st6
    double v5; // st6
    double v6; // st5
    int v7; // eax
    double v8; // st7
    double v9; // st7
    NGParticle::Flags flags; // al
    float y; // ecx
    double v12; // st7
    float life; // [esp+4h] [ebp-9Ch]
    float z; // [esp+4h] [ebp-9Ch]
    float v15; // [esp+8h] [ebp-98h]
    float v16; // [esp+8h] [ebp-98h]
    float v17; // [esp+Ch] [ebp-94h]
    float v18; // [esp+Ch] [ebp-94h]
    float v19; // [esp+10h] [ebp-90h]
    float v20; // [esp+10h] [ebp-90h]
    float v21; // [esp+14h] [ebp-8Ch]
    float v22; // [esp+1Ch] [ebp-84h] BYREF
    int v23; // [esp+20h] [ebp-80h]
    float v24; // [esp+24h] [ebp-7Ch]
    UMath::Vector4 point; // [esp+28h] [ebp-78h] OVERLAPPED BYREF
    WCollisionMgr::WorldCollisionInfo collisionInfo; // [esp+48h] [ebp-58h] OVERLAPPED BYREF

    v2 = 0.15f + particle->initialPos.z;
    life = particle->life;
    x = particle->initialPos.x;
    particle->initialPos.z = v2;
    v24 = x;
    v4 = life * particle->vel.x;
    point.z = x;
    v5 = v4 + particle->initialPos.x;
    *(float*)&v23 = life * particle->vel.y + particle->initialPos.y;
    v6 = life * particle->vel.z + v2 + particle->life * particle->life * particle->gravity;
    v17 = -particle->initialPos.y;
    point.x = v17;
    v19 = v2;
    v18 = -*(float*)&v23;
    point.y = v19;
    v20 = v6;
    v21 = v5;
    point.w = 1.0;
    ((void(__thiscall*)(WCollisionMgr::WorldCollisionInfo*))sub_404A20)(&collisionInfo);
    point.y = 0.15f + point.y;
    WCollisionMgr collisionMgr;
    collisionMgr.fSurfaceExclusionMask = 0;
    collisionMgr.fPrimitiveMask = 3;
    if (WCollisionMgr_CheckHitWorld(&collisionMgr, &point, &collisionInfo, 3u))
    {
        v8 = particle->vel.z * particle->vel.z - (particle->initialPos.z - collisionInfo.fCollidePt.y) * particle->gravity * flt_9C2A3C;
        if (v8 > 0.0f)
        {
            v16 = sqrt(v8);
            v22 = particle->gravity + particle->gravity;
            particle->life = (uint16_t)(((v16 - particle->vel.z) / (particle->gravity * 2.0)) * 8191.0f);
            if (particle->life < 0)
                particle->life = (uint16_t)(((-particle->vel.z - v16) / (particle->gravity * 2.0)) * 8191.0f);
        }
        else
        {
            particle->life = 0;
        }
        particle->flags |= NGParticle::Flags::SPAWN;
        particle->impactNormal.x = collisionInfo.fNormal.x;
        particle->impactNormal.y = collisionInfo.fNormal.y;
        particle->impactNormal.z = collisionInfo.fNormal.z;
    }
}

bool BounceParticle(NGParticle* particle)
{
    if (!bBounceParticles)
        return true;

    float life;
    float gravity;
    float gravityOverTime;
    float velocityMagnitude;
    UMath::Vector3 newVelocity = particle->vel;
    
    life = particle->life / 8191.0f; // MW multiplies life by 8191 when spawning the particle, but Carbon does not as it uses a float
    gravity = particle->gravity;
    
    gravityOverTime = particle->gravity * life;
    
    particle->initialPos += newVelocity * life;
    particle->initialPos.z += gravityOverTime * life;
    
    //if (bCarbonBounceBehavior) // carbon modification
    //    gravityOverTime *= 2.0f;

    newVelocity.z += gravityOverTime;
    
    velocityMagnitude = UMath::Length(newVelocity);
    
    newVelocity = UMath::Normalize(newVelocity);
    
    //if (bCarbonBounceBehavior)
    //    particle->life = particle->remainingLife;
    //else
    particle->life = 8191;
    particle->age = 0.0f;
    particle->flags = NGParticle::Flags::BOUNCED;
    
    float bounceCos = UMath::Dot(newVelocity, particle->impactNormal);
    
    //if (bCarbonBounceBehavior) // MW didn't have elasticity
    //    velocityMagnitude *= particle->elasticity / 255.0f;
    
    particle->vel = (newVelocity - (particle->impactNormal * bounceCos * 2.0f)) * velocityMagnitude;
    
    //if (bCarbonBounceBehavior) // MW doesn't call this here
    //    CalcCollisiontime(particle);
    
    return true;
}

void ParticleList::AgeParticles(float dt)
{
    size_t aliveCount = 0;

    for (size_t i = 0; i <mNumParticles; i++)
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

NGParticle* ParticleList::GetNextParticle()
{
    if (gParticleList.mNumParticles >= MaxParticles)
        return NULL;
    return &gParticleList.mParticles[gParticleList.mNumParticles++];
}

char fuelcell_attrib_buffer5[20];
void* __stdcall Attrib_Instance_GetAttributePointer_Shim(uint32_t hash, uint32_t unk)
{
    uint32_t that;
    _asm mov that, ecx

    memset(fuelcell_attrib_buffer5, 0, 20);
    memcpy(&(fuelcell_attrib_buffer5[4]), (void*)that, 16);

    return Attrib_Instance_GetAttributePointer((void*)fuelcell_attrib_buffer5, hash, unk);
}

struct CGEmitter
{
    Attrib::Gen::fuelcell_emitter mEmitterDef;
    Attrib::Gen::emitteruv mTextureUVs;
    UMath::Vector4 mVel;
    UMath::Matrix4 mLocalWorld;
    CGEmitter(Attrib::Collection* spec, XenonEffectDef* eDef);
    void SpawnParticles(float dt, float intensity, bool isContrail);
};

CGEmitter::CGEmitter(Attrib::Collection* spec, XenonEffectDef* eDef) :
    mEmitterDef(spec, 0),
    mTextureUVs(mEmitterDef.emitteruv(), 0)
{
    mLocalWorld = eDef->mat;
    mVel = eDef->vel;
};

// for reference
// class NGParticle
// {
// public:
// 
//     enum Flags : uint8_t
//     {
//         DEBRIS = 1 << 0,
//         SPAWN = 1 << 1,
//         BOUNCED = 1 << 2,
//     };
// 
//     UMath::Vector3 initialPos;
//     unsigned int color;
//     UMath::Vector3 vel;
//     float gravity;
//     UMath::Vector3 impactNormal;
//     __declspec(align(16)) uint16_t flags;
//     uint16_t spin;
//     uint16_t life;
//     char length;
//     char width;
//     char uv[4];
//     float age;
// };

namespace UMath
{
    namespace fpu
    {
        void RotateTranslate(UMath::Vector4* v, UMath::Matrix4* m, UMath::Vector4* result)
        {
            UMath::Vector4 vector;

            vector = *v;
            result->x = (m->v0.x * v->x) + ((m->v1.x * vector.y) + ((m->v3.x * vector.w) + (m->v2.x * vector.z)));
            result->y = (m->v0.y * vector.x) + ((m->v1.y * vector.y) + ((m->v3.y * vector.w) + (m->v2.y * vector.z)));
            result->z = (m->v0.z * vector.x) + ((m->v1.z * vector.y) + ((m->v3.z * vector.w) + (m->v2.z * vector.z)));
            result->w = (m->v0.w * vector.x) + ((m->v1.w * vector.y) + ((m->v3.w * vector.w) + (m->v2.w * vector.z)));
        }

        void Scalexyz(UMath::Vector4* a, UMath::Vector4* b, UMath::Vector4* result)
        {
            result->x = a->x * b->x;
            result->y = a->y * b->y;
            result->z = a->z * b->z;
        }

        void Scalexyz(UMath::Vector4* a, float scaleby, UMath::Vector4* result)
        {
            result->x = a->x * scaleby;
            result->y = a->y * scaleby;
            result->z = a->z * scaleby;
        }

        void Rotate(UMath::Vector4* v, UMath::Matrix4* m, UMath::Vector4* result)
        {
            result->x = (m->v0.x * v->x) + (m->v1.x * v->y) + (m->v2.x * v->z);
            result->y = (m->v0.y * v->x) + (m->v1.y * v->y) + (m->v2.y * v->z);
            result->z = (m->v0.z * v->x) + (m->v1.z * v->y) + (m->v2.z * v->z);
            result->w = v->w;
        }

        void Add(UMath::Vector4* a, UMath::Vector4* b, UMath::Vector4* result)
        {
            result->x = a->x + b->x;
            result->y = a->y + b->y;
            result->z = a->z + b->z;
            result->w = a->w + b->w;
        }

        void Add(UMath::Vector3* a, UMath::Vector3* b, UMath::Vector4* result)
        {
            result->x = a->x + b->x;
            result->y = a->y + b->y;
            result->z = a->z + b->z;
        }

        void ScaleAdd(UMath::Vector4* a, float scaleby, UMath::Vector4* b, UMath::Vector4* result)
        {
            result->x = (a->x * scaleby) + b->x;
            result->y = (a->y * scaleby) + b->y;
            result->z = (a->z * scaleby) + b->z;
            result->w = (a->w * scaleby) + b->w;
        }

        void ScaleAdd(UMath::Vector3* a, float scaleby, UMath::Vector3* b, UMath::Vector3* result)
        {
            result->x = (a->x * scaleby) + b->x;
            result->y = (a->y * scaleby) + b->y;
            result->z = (a->z * scaleby) + b->z;
        }
    }
}

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
    if (intensity != 1.0f)
    {
        a = (int)std::fminf(42.0f, intensity * 42.0f);
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

        UMath::fpu::Scalexyz(&pvel, &mVel, &pvel);
        UMath::fpu::Rotate(&mEmitterDef.VelocityStart(), &local_orientation, &rotatedVel);
        UMath::fpu::Add(&pvel, &rotatedVel, &pvel);
        UMath::fpu::Scalexyz(&pvel, &rand, &pvel);

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

        UMath::fpu::RotateTranslate(&ppos, &local_world, &ppos);
        UMath::fpu::ScaleAdd((UMath::Vector3*)&pvel, current_particle_age, (UMath::Vector3*)&ppos, &particle->initialPos);

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
        particle->flags = (!mEmitterDef.zContrail() ? NGParticle::Flags::SPAWN : (NGParticle::Flags)NULL);
        particle->spin = mEmitterDef.Spin();
        if ((particle->flags & NGParticle::Flags::BOUNCED) == 0 && !isContrail)
        {
            //CalcCollisiontime(particle);
        }
        // end next gen code
    }

    randomSeed = random_seed;
}

//void __fastcall CGEmitter_SpawnParticles(CGEmitter* _this, int dummy, float dt, float intensity, bool isContrail)
//{
//    float* mLayoutPtr; // esi
//    int v6; // edi
//    int v7; // ebx
//    int v8; // eax
//    double v9; // st7
//    int v10; // ecx
//    double v11; // st7
//    double v12; // st7
//    double v13; // st7
//    double v14; // st6
//    char v15; // bl
//    char* AttributePointer_Shim; // eax
//    char* v17; // eax
//    char* v18; // eax
//    char v19; // cl
//    int v20; // eax
//    NGParticle* v21; // esi
//    float* v22; // eax
//    float v23; // edx
//    double v24; // st7
//    double v25; // st7
//    float* v26; // edi
//    double v27; // st7
//    double v28; // st7
//    float* v29; // edi
//    double v30; // st7
//    double v31; // st7
//    float* v32; // edi
//    double v33; // st7
//    float* v34; // edi
//    double v35; // st7
//    double v36; // st6
//    float* v37; // edi
//    double v38; // st7
//    float* v39; // edi
//    double v40; // st7
//    float* v41; // edi
//    float v42; // eax
//    float v43; // ecx
//    double v44; // st7
//    float v45; // edx
//    float v46; // eax
//    double v47; // st7
//    float v48; // ecx
//    float v49; // edx
//    double v50; // st7
//    unsigned __int8 v51; // al
//    int v52; // edx
//    unsigned __int8 v53; // al
//    char v54; // cl
//    char v55; // dl
//    unsigned __int8 v56; // al
//    double v57; // st7
//    NGParticle::Flags flags; // al
//    float v59; // [esp-54h] [ebp-100h]
//    float v60; // [esp-4Ch] [ebp-F8h]
//    float v61; // [esp-3Ch] [ebp-E8h]
//    float* v62; // [esp-34h] [ebp-E0h]
//    float v63; // [esp-28h] [ebp-D4h]
//    float v64; // [esp-20h] [ebp-CCh]
//    unsigned int v65; // [esp+0h] [ebp-ACh] BYREF
//    float v66; // [esp+4h] [ebp-A8h]
//    char v67; // [esp+9h] [ebp-A3h]
//    unsigned __int8 v68; // [esp+Ah] [ebp-A2h]
//    char v69; // [esp+Bh] [ebp-A1h]
//    float v70; // [esp+Ch] [ebp-A0h]
//    float v71; // [esp+10h] [ebp-9Ch]
//    int v72; // [esp+14h] [ebp-98h]
//    float v73; // [esp+18h] [ebp-94h]
//    float v74; // [esp+1Ch] [ebp-90h]
//    float v75; // [esp+20h] [ebp-8Ch]
//    float v76; // [esp+24h] [ebp-88h]
//    float v77; // [esp+28h] [ebp-84h]
//    float v78; // [esp+2Ch] [ebp-80h]
//    float v79; // [esp+30h] [ebp-7Ch] BYREF
//    float v80; // [esp+34h] [ebp-78h]
//    float v81; // [esp+38h] [ebp-74h]
//    float v82; // [esp+3Ch] [ebp-70h]
//    int v83; // [esp+40h] [ebp-6Ch]
//    float v84; // [esp+44h] [ebp-68h]
//    float v85; // [esp+48h] [ebp-64h]
//    float v86; // [esp+4Ch] [ebp-60h]
//    float v87; // [esp+50h] [ebp-5Ch]
//    float v88; // [esp+54h] [ebp-58h]
//    float v89[4]; // [esp+5Ch] [ebp-50h] BYREF
//    char v90[64]; // [esp+6Ch] [ebp-40h] BYREF
//
//    if (intensity > (double)flt_9C248C && dt > (double)flt_9C248C)
//    {
//        memcpy(v90, &_this->mLocalWorld, sizeof(v90));
//        mLayoutPtr = (float*)_this->mEmitterDef.mLayoutPtr;
//        Attrib::Gen::fuelcell_emitter::_LayoutStruct* emitterThing = (Attrib::Gen::fuelcell_emitter::_LayoutStruct *)_this->mEmitterDef.mLayoutPtr;
//        v65 = randomSeed;
//        v78 = mLayoutPtr[27] - mLayoutPtr[27] * mLayoutPtr[35];
//        v6 = (int)(mLayoutPtr[20] * flt_9C92F0);
//        v7 = (int)(mLayoutPtr[21] * flt_9C92F0);
//        v8 = (int)(mLayoutPtr[22] * flt_9C92F0);
//        v9 = mLayoutPtr[23] * flt_9C92F0;
//        v72 = v8;
//        v10 = (int)(v9);
//        if (intensity != flt_9C2478)
//        {
//            v11 = intensity * flt_9EA540;
//            if (flt_9EA540 < v11)
//                v11 = flt_9EA540;
//            v10 = (int)(v10, v11);
//        }
//        v83 = v72 | ((v7 | ((v6 | (v10 << 8)) << 8)) << 8);
//        if (intensity <= (double)flt_9C2478)
//            v12 = flt_9C2478;
//        else
//            v12 = intensity;
//        v13 = v12 * *((float*)_this->mEmitterDef.mLayoutPtr + 36) * *((float*)_this->mEmitterDef.mLayoutPtr + 28);
//        if (intensity <= (double)flt_9C2478)
//            v14 = flt_9C2478;
//        else
//            v14 = intensity;
//        v15 = 0;
//        v71 = v14 * *((float*)_this->mEmitterDef.mLayoutPtr + 36) - v13 * flt_9C2C44;
//        AttributePointer_Shim = ((char* (__thiscall*)(Attrib::CarbonInstance*, int, int))Attrib_Instance_GetAttributePointer_Shim)(&_this->mEmitterDef, 0x28638D89u, 0);
//        if (!AttributePointer_Shim)
//            AttributePointer_Shim = (char*)Attrib::DefaultDataArea();
//        v68 = *AttributePointer_Shim;
//        v17 = ((char* (__thiscall*)(Attrib::CarbonInstance*, int, int))Attrib_Instance_GetAttributePointer_Shim)(&_this->mEmitterDef, 0xD2603865, 0);
//        if (!v17)
//            v17 = (char*)Attrib::DefaultDataArea();
//        v67 = *v17;
//        v18 = ((char*(__thiscall*)(Attrib::CarbonInstance*, int, int))Attrib_Instance_GetAttributePointer_Shim)(&_this->mEmitterDef, 0xE2CC8106, 0);
//        if (!v18)
//            v18 = (char*)Attrib::DefaultDataArea();
//        v19 = *v18;
//        v20 = *((unsigned __int8*)_this->mEmitterDef.mLayoutPtr + 148);
//        v69 = v19;
//        v72 = v20;
//        if (v20)
//        {
//            v15 = 1;
//            v72 = v20 - 1;
//        }
//        v66 = 0.0;
//        v85 = dt / v71;
//        while (v71 != flt_9C248C)
//        {
//            v71 = v71 - flt_9C2478;
//            if (gParticleList.mNumParticles >= MaxParticles)
//                break;
//            v21 = &gParticleList.mParticles[gParticleList.mNumParticles++];
//            if (!v21)
//                break;
//            v22 = (float*)_this->mEmitterDef.mLayoutPtr;
//            v23 = v22[34];
//            v70 = v22[33];
//            v84 = v23;
//            v24 = bRandom_Float_Int(v23, (int)&v65);
//            v25 = v24 + v70;
//            v70 = v25;
//            if (v25 < flt_9C248C)
//                break;
//            if (v70 >= (double)flt_9C92F0)
//                v70 = 255.0;
//            v26 = (float*)_this->mEmitterDef.mLayoutPtr;
//            v27 = bRandom_Float_Int(v26[4], (int)&v65);
//            v28 = v26[4] - (v27 + v27);
//            v29 = (float*)_this->mEmitterDef.mLayoutPtr;
//            v64 = v29[5];
//            v86 = flt_9C2478 - v28;
//            v30 = bRandom_Float_Int(v64, (int)&v65);
//            v31 = v29[5] - (v30 + v30);
//            v32 = (float*)_this->mEmitterDef.mLayoutPtr;
//            v63 = v32[6];
//            v87 = flt_9C2478 - v31;
//            v33 = bRandom_Float_Int(v63, (int)&v65);
//            v62 = (float*)((char*)_this->mEmitterDef.mLayoutPtr + 64);
//            v88 = flt_9C2478 - (v32[6] - (v33 + v33));
//            v73 = *(v62 - 4) * _this->mVel.x;
//            v74 = *(v62 - 3) * _this->mVel.y;
//            v75 = *(v62 - 2) * _this->mVel.z;
//            ((void(__cdecl*)(float*, UMath::Matrix4*, float*))sub_478200)(v62, &_this->mLocalWorld, v89);
//            v34 = (float*)_this->mEmitterDef.mLayoutPtr;
//            v61 = v34[31];
//            v75 = v89[2] + v75;
//            v76 = v89[3] + v76;
//            v73 = (v89[0] + v73) * v86;
//            v74 = (v89[1] + v74) * v87;
//            v75 = v75 * v88;
//            v35 = bRandom_Float_Int(v61, (int)&v65);
//            v36 = v34[29] - v34[31];
//            v37 = (float*)_this->mEmitterDef.mLayoutPtr;
//            v77 = v35 + v35 + v36;
//            v38 = bRandom_Float_Int(v37[8], (int)&v65) - v37[8] * flt_9C2888 + *v37;
//            v39 = (float*)_this->mEmitterDef.mLayoutPtr;
//            v60 = v39[9];
//            v79 = v38;
//            v40 = bRandom_Float_Int(v60, (int)&v65) - v39[9] * flt_9C2888 + v39[1];
//            v41 = (float*)_this->mEmitterDef.mLayoutPtr;
//            v59 = v41[10];
//            v80 = v40;
//            v81 = bRandom_Float_Int(v59, (int)&v65) - v41[10] * flt_9C2888 + v41[2];
//            v82 = 1.0;
//            ((void(__cdecl*)(float*, char*, float*))sub_6012B0)(&v79, v90, &v79);
//            v42 = v74;
//            v43 = v75;
//            v44 = v73 * v66 + v79;
//            v21->vel.x = v73;
//            v45 = v78;
//            v21->initialPos.x = v44;
//            v21->vel.y = v42;
//            v46 = v45;
//            v47 = v74 * v66;
//            v21->vel.z = v43;
//            v48 = v66;
//            v21->remainingLife = v45;
//            v49 = v77;
//            v50 = v47 + v80;
//            v21->life = v46;
//            v21->age = v48;
//            v21->initialPos.y = v50;
//            v21->gravity = v49;
//            v21->initialPos.z = v77 * v66 * v66 + v75 * v66 + v81;
//            v21->elasticity = (int)(*((float*)_this->mEmitterDef.mLayoutPtr + 32));
//            v51 = (int)(*((float*)_this->mEmitterDef.mLayoutPtr + 30));
//            v52 = v83;
//            v21->size = v51;
//            v21->color = v52;
//            v21->flags = (NGParticle::Flags)v15;
//            if ((v15 & 1) != 0)
//            {
//                v21->uv[0] = v72;
//                v21->uv[1] = (int)(*((float*)_this->mTextureUVs.mLayoutPtr + 1));
//                v21->uv[2] = (int)(*((float*)_this->mTextureUVs.mLayoutPtr + 3));
//                v21->startX = bRandom_Int_Int(255, &v65);
//                v21->startY = bRandom_Int_Int(255, &v65);
//                v53 = bRandom_Int_Int(255, &v65);
//                v54 = v67;
//                v55 = v69;
//                v21->startZ = v53;
//                v21->rotX = v68;
//                v21->rotY = v54;
//                v21->rotZ = v55;
//            }
//            else
//            {
//                v21->uv[0] = (int)(*((float*)_this->mTextureUVs.mLayoutPtr + 1) * flt_9C92F0);
//                v21->uv[1] = (int)(*((float*)_this->mTextureUVs.mLayoutPtr + 3) * flt_9C92F0);
//                v21->uv[2] = (int)(*((float*)_this->mTextureUVs.mLayoutPtr + 2) * flt_9C92F0);
//                v56 = (int)(*(float*)_this->mTextureUVs.mLayoutPtr * flt_9C92F0);
//                v57 = v70;
//                v21->uv[3] = v56;
//                v21->startX = (int)(v57);
//            }
//            flags = v21->flags;
//            v66 = v66 + v85;
//            //if ((flags & 4) == 0 && !isContrail)
//            //    CalcCollisiontime(v21);
//        }
//        randomSeed = v65;
//    }
//}

void __declspec(naked) Attrib_Gen_fuelcell_emitter_constructor()
{
    _asm
    {
                sub esp, 10h
                mov     eax, [esp+18h]
                push    esi
                mov     esi, ecx
                mov     ecx, [esp+18h]
                push    eax
                push    ecx
                mov     ecx, esi
                mov     [esp+0Ch], esi
                call    Attrib_Instance ; Attrib::Instance::Instance((Attrib::Collection const *,ulong))
                mov     eax, [esi+8]
                test    eax, eax
                mov     dword ptr [esp+10h], 0
                jnz     loc_73EEFD
                push    98h ; '˜'
                call    Attrib_DefaultDataArea ; Attrib::DefaultDataArea((uint))
                add     esp, 4
                mov     [esi+8], eax

loc_73EEFD:                             ; CODE XREF: Attrib::Gen::fuelcell_emitter::fuelcell_emitter(Attrib::Collection const *,uint)+3B↑j
                mov     eax, esi
                pop     esi
                add     esp, 10h
                retn    8
    }
}


char fuelcell_attrib_buffer[20];
void*(__thiscall* Attrib_Gen_fuelcell_emitter_constructor_Abstract)(void* AttribInstance, uint32_t unk1, uint32_t unk2) = (void*(__thiscall*)(void*, uint32_t, uint32_t)) & Attrib_Gen_fuelcell_emitter_constructor;
void* __stdcall Attrib_Gen_fuelcell_emitter_constructor_shim(uint32_t unk1, uint32_t unk2)
{
    uint32_t that;
    _asm mov that, ecx

    memset(fuelcell_attrib_buffer, 0, 20);
    auto result = Attrib_Gen_fuelcell_emitter_constructor_Abstract((void*)fuelcell_attrib_buffer, unk1, unk2);
    
    memcpy((void*)that, (void*)(&fuelcell_attrib_buffer[4]), 16);
    
    return result;
}

void __declspec(naked) sub_737610()
{
    _asm
    {
                sub esp, 10h
                mov     eax, [esp+18h]
                push    esi
                mov     esi, ecx
                mov     ecx, [esp+18h]
                push 0
                push    eax
                push    ecx
                mov     ecx, esi
                mov     [esp+0Ch], esi
                call    Attrib_Instance_Refspec; Attrib::Instance::Instance((Attrib::RefSpec const &,ulong))
                mov     eax, [esi+8]
                test    eax, eax
                mov     dword ptr [esp+10h], 0
                jnz     loc_73765A
                push    10h
                call    Attrib_DefaultDataArea ; Attrib::DefaultDataArea((uint))
                add     esp, 4
                mov     [esi+8], eax

loc_73765A:                             ; CODE XREF: sub_737610+3B↑j
                mov     eax, esi
                pop     esi
                add     esp, 10h
                retn    8
    }
}

char fuelcell_attrib_buffer2[20];

void* (__thiscall* sub_737610_Abstract)(void* AttribInstance, uint32_t unk1, uint32_t unk2) = (void* (__thiscall*)(void*, uint32_t, uint32_t)) & sub_737610;
void* __stdcall sub_737610_shim(uint32_t unk1, uint32_t unk2)
{
    uint32_t that;
    _asm mov that, ecx

    memset(fuelcell_attrib_buffer2, 0, 20);
    auto result = sub_737610_Abstract((void*)fuelcell_attrib_buffer2, unk1, unk2);

    memcpy((void*)that, (void*)(&fuelcell_attrib_buffer2[4]), 16);

    return result;
}

//void __declspec(naked) CGEmitter_CGEmitter()
//{
//    _asm
//    {
//          sub esp, 10h
//          mov     eax, [esp+14h]
//          push    ebx
//          push    esi
//          push    edi
//          push    0
//          mov     ebx, ecx
//          push    eax
//          mov     [esp+14h], ebx
//          call    Attrib_Gen_fuelcell_emitter_constructor_shim; Attrib::Gen::fuelcell_emitter::fuelcell_emitter(Attrib::Collection const *,uint)
//          mov     ecx, [ebx+4]
//          add     ecx, 60h ; '`'
//          push    0
//          push    ecx
//          lea     ecx, [ebx+10h]
//          mov     dword ptr [esp+20h], 0
//          call    sub_737610_shim
//          mov     eax, [esp+24h]
//          lea     esi, [eax+14h]
//          add     eax, 4
//          lea     edi, [ebx+30h]
//          mov     ecx, 10h
//          rep movsd
//          mov     ecx, [eax]
//          lea     edx, [ebx+20h]
//          mov     [edx], ecx
//          mov     ecx, [eax+4]
//          mov     [edx+4], ecx
//          mov     ecx, [eax+8]
//          mov     [edx+8], ecx
//          mov     eax, [eax+0Ch]
//          pop     edi
//          mov     [edx+0Ch], eax
//          pop     esi
//          mov     eax, ebx
//          pop     ebx
//          add     esp, 10h
//          retn    8
//    }
//}

char fuelcell_attrib_buffer3[20];
void __stdcall Attrib_Gen_fuelcell_effect_constructor(void* collection, unsigned int msgPort)
{
    uint32_t that;
    _asm mov that, ecx
    //_asm int 3

    memset(fuelcell_attrib_buffer3, 0, 20);

    Attrib_Instance_MW((void*)fuelcell_attrib_buffer3, collection, msgPort, NULL);

    memcpy((void*)that, (void*)(&fuelcell_attrib_buffer3[4]), 16);

    if (!*(uint32_t*)(that + 4))
        *(uint32_t*)(that + 4) = (uint32_t)Attrib_DefaultDataArea(1);
}

unsigned int __stdcall Attrib_Gen_fuelcell_effect_Num_NGEmitter()
{
    uint32_t that;
    _asm mov that, ecx

    Attrib::Attribute* v1; // eax
    uint32_t v2; // esi
    char v4[16]; // [esp+4h] [ebp-1Ch] BYREF

    memset(fuelcell_attrib_buffer3, 0, 20);
    memcpy(&(fuelcell_attrib_buffer3[4]), (void*)that, 16);

    v1 = (Attrib::Attribute*)Attrib_Instance_Get(fuelcell_attrib_buffer3, (unsigned int)v4, 0xB0D98A89);
    v2 = v1->GetLength();
    //v2 = (uint32_t)Attrib_Attribute_GetLength((void*)v1);


    return v2;
}

//void __declspec(naked) NGEffect_NGEffect()
//{
//    _asm
//    {
//        sub     esp, 84h
//        push    ebp
//        push    edi
//        mov     edi, [esp + 90h]
//        mov     eax, [edi + 54h]
//        push    0
//        mov     ebp, ecx
//        push    eax
//        mov[esp + 14h], ebp
//        call    Attrib_Gen_fuelcell_effect_constructor; Attrib::Gen::fuelcell_effect::fuelcell_effect(Attrib::Collection const*, uint)
//        cmp     dword ptr[ebp + 4], 0
//        mov     dword ptr[esp + 88h], 0
//        jz      loc_74A36E
//        push    esi
//        mov     ecx, ebp
//        call    Attrib_Gen_fuelcell_effect_Num_NGEmitter; Attrib::Gen::fuelcell_effect::Num_NGEmitter(void)
//        xor esi, esi
//        test    eax, eax
//        mov[esp + 0Ch], eax
//        jle     loc_74A352
//        lea     ecx, [ecx + 0]
//
//loc_74A2C0: ; CODE XREF : NGEffect::NGEffect(XenonEffectDef const&, float) + EC↓j
//        push    esi
//        push    0B0D98A89h
//        mov     ecx, ebp
//        call    Attrib_Instance_GetAttributePointer_Shim
//        test    eax, eax
//        jnz     loc_74A2DB
//        push    0Ch
//        call    Attrib_DefaultDataArea
//        add     esp, 4
//
//loc_74A2DB: ; CODE XREF : NGEffect::NGEffect(XenonEffectDef const&, float) + 6F↑j
//        mov     ecx, eax
//        call    Attrib_RefSpec_GetCollection
//        push    edi
//        push    eax
//        lea     ecx, [esp + 1Ch]
//        call    CGEmitter::CGEmitter; CGEmitter::CGEmitter(Attrib::Collection const*, XenonEffectDef const&)
//        lea     ecx, [esp + 14h]
//        call fuelcell_emitter_bridge
//        mov     eax, [edi + 58h]
//        test    eax, eax
//        mov     byte ptr[esp + 8Ch], 1
//        jnz     loc_74A30B // jnz
//        mov     ecx, [edi]
//        mov     edx, [esp + 98h]
//        push    1
//        push    ecx
//        push    edx
//        jmp     loc_74A31A
//; -------------------------------------------------------------------------- -
//
//loc_74A30B: ; CODE XREF : NGEffect::NGEffect(XenonEffectDef const&, float) + 9A↑j
//        mov     eax, [esp + 98h]
//        push    0; char
//        push    3F800000h; float
//        push    eax; float
//
//loc_74A31A: ; CODE XREF : NGEffect::NGEffect(XenonEffectDef const&, float) + A9↑j
//        lea     ecx, [esp + 20h]
//        call    CGEmitter::SpawnParticles; CGEmitter::SpawnParticles(float, float)
//        call fuelcell_emitter_bridge_restore
//        lea     ecx, [esp + 24h]
//        mov     byte ptr[esp + 8Ch], 2
//        call    Attrib_Instance_Dtor_Shim; Attrib::Instance::~Instance((void))
//        lea     ecx, [esp + 14h]
//        mov     byte ptr[esp + 8Ch], 0
//        call    Attrib_Instance_Dtor_Shim; Attrib::Instance::~Instance((void))
//        mov     eax, [esp + 0Ch]
//        inc     esi
//        cmp     esi, eax
//        jl      loc_74A2C0
//
//loc_74A352: ; CODE XREF : NGEffect::NGEffect(XenonEffectDef const&, float) + 57↑j
//        mov     eax, ebp
//        pop     esi
//
//loc_74A355: ; CODE XREF : NGEffect::NGEffect(XenonEffectDef const&, float) + 110↓j
//        pop     edi
//        pop     ebp
//        add     esp, 84h
//
//        retn    8
//loc_74A36E:
//        mov     eax, ebp
//        jmp     short loc_74A355
//    }
//}

class NGEffect
{
public:
    NGEffect(XenonEffectDef* eDef, float dt);
    Attrib::Gen::fuelcell_effect mEffectDef;
};

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
            Attrib::Collection* emspec = mEffectDef.NGEmitter(i).GetCollection();
            CGEmitter anEmitter{ emspec, eDef };
            anEmitter.SpawnParticles(dt, eDef->piggyback_effect ? 1.0f : eDef->intensity, false);
        }
    }
}

void XSpriteManager::AddParticle(eView* view, NGParticle* particleList, unsigned int numParticles)
{
    Lock(view->EVIEW_ID);

    bBatching = true;

    for (size_t i = 0; i < numParticles; i++)
    {
        NGParticle* particle = &particleList[i];
        
        //float x = (particle->age * particle->vel.x) + particle->initialPos.x;
        //float y = (particle->age * particle->vel.y) + particle->initialPos.y;
        //float z = (particle->age * particle->vel.z) + particle->initialPos.z + particle->age * particle->age * particle->gravity;

        UMath::Vector3 startPos;
        UMath::Vector3 endPos;
        float endAge;
        float width = particle->width / 2048.0f;

        if (i < sparkList.mSprintListView[view->EVIEW_ID - 1].mMaxSprites)
        {
            XSpark *spark = &sparkList.mSprintListView[view->EVIEW_ID - 1].mLockedVB[i];

            sparkList.mSprintListView[view->EVIEW_ID - 1].mNumPolys = i;

            if (spark)
            {
                uint32_t color = particle->color;

                UMath::fpu::ScaleAdd(&particle->vel, particle->age, &particle->initialPos, &startPos);
                startPos.z += particle->age * particle->age * particle->gravity;

                endAge = (particle->length / 2048.0f) + particle->age;
                UMath::fpu::ScaleAdd(&particle->vel, endAge, &particle->initialPos, &endPos);
                endPos.z += endAge * endAge * particle->gravity;

                //if (bFadeOutParticles)
                //    ((uint8_t*)&color)[3] = (uint8_t)(((uint8_t*)&color)[3] * (1 - (pow((particle->age / particle->life), 1.1f)))); // QOL feature

                spark->v[0].position = startPos;
                spark->v[0].color = color;
                spark->v[0].texcoord[0] = particle->uv[0] / 255.0f;
                spark->v[0].texcoord[1] = particle->uv[1] / 255.0f;

                spark->v[1].position = startPos;
                spark->v[1].position.z += width;
                spark->v[1].color = color;
                spark->v[1].texcoord[0] = particle->uv[2] / 255.0f;
                spark->v[1].texcoord[1] = particle->uv[1] / 255.0f;

                //float offsetAge = (particle->length / 2048.0f) + particle->age;
                //x = (offsetAge * particle->vel.x) + particle->initialPos.x;
                //y = (offsetAge * particle->vel.y) + particle->initialPos.y;
                //z = (offsetAge * particle->vel.z) + particle->initialPos.z;
                //z += offsetAge * particle->gravity * offsetAge;
                
                spark->v[2].position = endPos;
                spark->v[2].position.z += width;
                spark->v[2].color = color;
                spark->v[2].texcoord[0] = particle->uv[2] / 255.0f;
                spark->v[2].texcoord[1] = particle->uv[3] / 255.0f;

                spark->v[3].position = endPos;
                spark->v[3].color = color;
                spark->v[3].texcoord[0] = particle->uv[0] / 255.0f;
                spark->v[3].texcoord[1] = particle->uv[3] / 255.0f;
            }
        }
    }
    
    if (bBatching && sparkList.mSprintListView[view->EVIEW_ID - 1].mpVB)
        bBatching = false;

    Unlock(view->EVIEW_ID);

    (void)view; // silence
}

void ParticleList::GeneratePolys(eView* view)
{
    if (mNumParticles)
        NGSpriteManager.AddParticle(view, mParticles, mNumParticles);
}

void DrawXenonEmitters(eView *view)
{
    XenonEffectDef* mpBegin; // ebx
    XenonEffectDef* i; // edx
    //NGEffect effect; // [esp-4h] [ebp-84h]
    XenonEffectDef effectDef; // [esp+20h] [ebp-60h] BYREF

    gParticleList.AgeParticles(gFrameDT);
    mpBegin = gNGEffectList.mpBegin;
    for (i = gNGEffectList.mpEnd; mpBegin != i; ++mpBegin)
    {
        memcpy(&effectDef, mpBegin, sizeof(effectDef));
        if (!effectDef.piggyback_effect || (*((uint32_t*)effectDef.piggyback_effect + 6) & 0x10) != 0)
        {
            NGEffect effect{ &effectDef, gFrameDT };
            i = gNGEffectList.mpEnd;
        }
    }
    eastl_vector_erase_XenonEffectDef_Abstract(&gNGEffectList, gNGEffectList.mpBegin, gNGEffectList.mpEnd);
    
    if (gFrameDT > 0.0f)
        gParticleList.GeneratePolys(view);

    gFrameDT = 0.0f;
}

void XSpriteManager::Init()
{
    //g_D3DDevice = *(LPDIRECT3DDEVICE9*)0x982BDC;
    sparkList.Init(MaxParticles);
    NGSpriteManager.sparkList.mTexture = GetTextureInfo(bStringHash(TextureName), 0, 0);
}

void XSpriteManager::Reset()
{
    sparkList.Reset();
}

void ReleaseRenderObj()
{
    NGSpriteManager.Reset();
}

void XSpriteManager::DrawBatch(eView* view)
{
    *(eEffect**)CURRENTSHADER_OBJ_ADDR = *(eEffect**)WORLDPRELITSHADER_OBJ_ADDR;
    
    eEffect& effect = **(eEffect**)CURRENTSHADER_OBJ_ADDR;

    effect.Start();
    
    effect.mEffectHandlePlat->Begin(NULL, 0);
    ParticleSetTransform((D3DXMATRIX*)0x00987AB0, view->EVIEW_ID);
    effect.mEffectHandlePlat->BeginPass(0);
    
    g_D3DDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    
    sparkList.Draw(view->EVIEW_ID, 0, effect, NULL);
    
    g_D3DDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    
    effect.mEffectHandlePlat->EndPass();
    effect.mEffectHandlePlat->End();
    
    effect.End();

    *(eEffect**)CURRENTSHADER_OBJ_ADDR = NULL;
}

void __stdcall EmitterSystem_Render_Hook(eView* view)
{
    void* that;
    _asm mov that, ecx

    EmitterSystem_Render(that, view);
    if (*(uint32_t*)GAMEFLOWSTATUS_ADDR == 6)
    {
        DrawXenonEmitters(view);
        NGSpriteManager.DrawBatch(view);
    }
    //printf("VertexBuffer: 0x%X\n", mpVB);
}

// D3D Reset Hook
void __stdcall sub_6CFCE0_hook()
{
    sub_6CFCE0();
    NGSpriteManager.Init();
}

// RENDERER STUFF END

bool InitXenonEffects()
{
    LoadResourceFile(TPKfilename, 0, 0, NULL, 0, 0, 0);
    ServiceResourceLoading();
    XenonEffectList_Initialize();
    return false;
}

uint32_t sub_6DFAF0_hook()
{
    NGSpriteManager.Init();//InitializeRenderObj();
    return sub_6DFAF0();
}

uint32_t SparkFC = 0;
void AddXenonEffect_Spark_Hook(void* piggyback_fx, Attrib::Collection* spec, UMath::Matrix4* mat, UMath::Vector4* vel, float intensity)
{
    if (!bLimitSparkRate)
        return AddXenonEffect(piggyback_fx, spec, mat, vel, intensity * SparkIntensity);

    if ((SparkFC + SparkFrameDelay) <= eFrameCounter)
    {
        if (SparkFC != eFrameCounter)
        {
            SparkFC = eFrameCounter;
            AddXenonEffect(piggyback_fx, spec, mat, vel, intensity * SparkIntensity);
        }
    }
}

uint32_t ContrailFC = 0;
void AddXenonEffect_Contrail_Hook(void* piggyback_fx, Attrib::Collection* spec, UMath::Matrix4* mat, UMath::Vector4* vel, float intensity)
{
    (void)intensity; // not using this
#ifdef CONTRAIL_TEST
    // TEST CODE
    UMath::Vector4 newvel = { -40.6f, 29.3f, -2.3f, 0.0f };
    UMath::Matrix4 newmat;

    newmat.v0.x = 0.60f;
    newmat.v0.y = 0.80f;
    newmat.v0.z = -0.03f;
    newmat.v0.w = 0.00f;

    newmat.v1.x = -0.80f;
    newmat.v1.y = 0.60f;
    newmat.v1.z = 0.01f;
    newmat.v1.w = 0.00f;

    newmat.v2.x = 0.03f;
    newmat.v2.y = 0.02f;
    newmat.v2.z = 1.00f;
    newmat.v2.w = 0.00f;

    newmat.v3.x = 981.90f;
    newmat.v3.y = 2148.45f;
    newmat.v3.z = 153.05f;
    newmat.v3.w = 1.00f;

    if (!bLimitContrailRate)
        AddXenonEffect(piggyback_fx, spec, &newmat, &newvel, 1.0f);

    if ((ContrailFC + ContrailFrameDelay) <= eFrameCounter)
    {
        if (ContrailFC != eFrameCounter)
        {
            ContrailFC = eFrameCounter;
            AddXenonEffect(piggyback_fx, spec, &newmat, &newvel, 1.0f);
        }
    }
    // TEST CODE
#else

    float newintensity = ContrailMaxIntensity;

    if (!bUseCGStyle)
    {
        float carspeed = (UMath::Length(*(UMath::Vector3*)vel) - ContrailSpeed) / ContrailSpeed;
        newintensity = UMath::Lerp(ContrailMinIntensity, ContrailMaxIntensity, carspeed);
        if (newintensity > ContrailMaxIntensity)
            newintensity = ContrailMaxIntensity;
    }

    if (!bLimitContrailRate)
        return AddXenonEffect(piggyback_fx, spec, mat, vel, newintensity);

    if ((ContrailFC + ContrailFrameDelay) <= eFrameCounter)
    {
        if (ContrailFC != eFrameCounter)
        {
            ContrailFC = eFrameCounter;
            AddXenonEffect(piggyback_fx, spec, mat, vel, newintensity);
        }
    }
#endif
}

void UpdateXenonEmitters(float dt)
{
    gFrameDT = dt;
}

void __stdcall EmitterSystem_Update_Hook(float dt)
{
    uint32_t that;
    _asm mov that, ecx
    EmitterSystem_UpdateParticles((void*)that, dt);
    UpdateXenonEmitters(dt);
}

uint32_t RescueESI = 0;
void __declspec(naked) Emitter_SpawnParticles_Cave()
{
    _asm
    {
        //int 3
        mov ecx, [edi+0x7C]
        call sub_736EA0
        mov RescueESI, edi
        xor esi, esi
        test eax, eax
        mov ebx, eax
        jle loc_755CA6
    loc_D9D30:
        //int 3
        mov     ecx, [edi + 7Ch]
        push    esi
        push    0FE40E637h
        call    Attrib_Instance_GetAttributePointer
        test    eax, eax
        jnz     loc_D9D4C

        push    0Ch
        call    Attrib_DefaultDataArea
        add     esp, 4

    loc_D9D4C:
        mov     ecx, eax
        call    Attrib_RefSpec_GetCollection
        test    eax, eax
        jz      loc_D9D6F

        push    3F800000h
        lea     ecx, [edi+60h]
        push    ecx
        lea     edx, [edi+20h]
        push    edx
        push    eax
        mov     eax, [edi+8Ch]
        push    eax
        call    AddXenonEffect_Spark_Hook
        add     esp, 14h

    loc_D9D6F:
        //mov     eax, ebx
        inc     esi
        cmp     esi, ebx
        jl      loc_D9D30

    loc_755CA6:
        mov ecx, [esp+0x18]
        mov [edi+0x14], ecx
        pop edi
        pop esi
        pop ebx
        mov esp, ebp
        pop ebp
        mov esi, RescueESI
        retn 8
    }
}

// entry point: 0x750F48
// exit points: 0x750F6D & 0x750F4E

uint32_t loc_750F4E = 0x750F4E;
uint32_t loc_750F6D = 0x750F6D;
uint32_t loc_750FB0 = 0x750FB0;

void __declspec(naked) CarRenderConn_OnRender_Cave()
{
    _asm
    {
#ifndef CONTRAIL_TEST
                mov     al, [edi+400h]
                test    al, al
                jz      loc_7E13A5
#endif
                test    ebx, ebx
                jz      loc_7E140C
                mov     eax, [esi+8]
                cmp     eax, 1
                jz      short loc_7E1346
                cmp     eax, 2
                jnz     loc_7E13A5
; ---------------------------------------------------------------------------

loc_7E1346:                             ; CODE XREF: sub_7E1160+169↑j
                mov     edx, [ebx]
                mov     ecx, ebx
                call    dword ptr [edx+24h]
                test    al, al
                jz      short loc_7E13A5
                push    16AFDE7Bh
                push    6F5943F1h
                call    Attrib_FindCollection ; Attrib::FindCollection((ulong,ulong))
                add     esp, 8
                push    3F400000h       ; float
                mov     esi, eax
                mov     eax, [edi+38h]
                mov     ecx, [edi+34h]
                push    eax
                push    ecx

//loc_7E1397:                             ; CODE XREF: sub_7E1160+1DD↑j
                push    esi
                push    0
                call    AddXenonEffect_Contrail_Hook ; AddXenonEffect(AcidEffect *,Attrib::Collection const *,UMath::Matrix4 const *,UMath::Vector4 const *,float)
                mov     esi, [ebp+8]
                add     esp, 14h

loc_7E13A5:
                cmp dword ptr [esi+4], 3
                jnz exitpoint_nz

                jmp loc_750F4E

exitpoint_nz:
                jmp loc_750F6D

loc_7E140C:
                jmp loc_750FB0
    }
}

void __stdcall CarRenderConn_UpdateContrails(void* CarRenderConn, void* PktCarService, float param)
{
    UMath::Vector3 velocity = **((UMath::Vector3**)CarRenderConn + 0xE);
    float length = UMath::Length(velocity);

    bool &emitContrails = *((bool*)CarRenderConn + 0x400);

    emitContrails = length >= ContrailSpeed;

    if (*((bool*)PktCarService + 0x71) && bUseCGStyle)
        emitContrails = true;

    if (*(uint32_t*)NISINSTANCE_ADDR && !bNISContrails)
        emitContrails = false;

    if (*(bool*)((uint32_t)(CarRenderConn)+0x400))
        *(float*)((uint32_t)(CarRenderConn) + 0x404) = *(float*)((uint32_t)(PktCarService)+0x6C);
    else
        *(float*)((uint32_t)(CarRenderConn)+0x404) = 0;

    (void)param; // silence
}

void __stdcall CarRenderConn_UpdateEngineAnimation_Hook(float param, void* PktCarService)
{
    uint32_t that;
    _asm mov that, ecx

    CarRenderConn_UpdateEngineAnimation((void*)that, param, PktCarService);

    *(uint32_t*)(that + 0x400) = 0;
    *(uint32_t*)(that + 0x404) = 0;

    if ((*(char*)(that + 0x3F8) & 4) != 0)
    {
        CarRenderConn_UpdateContrails((void*)that, PktCarService, param);
    }
}

bool bValidateHexString(char* str)
{
    for (size_t i = 0; i < strlen(str); i++)
    {
        if ((!isdigit(str[i])) &&
            (toupper(str[i]) != 'A') &&
            (toupper(str[i]) != 'B') &&
            (toupper(str[i]) != 'C') &&
            (toupper(str[i]) != 'D') &&
            (toupper(str[i]) != 'E') &&
            (toupper(str[i]) != 'F')
            )
            return false;
    }
    return true;
}

void InitConfig()
{
    mINI::INIFile inifile("NFSMW_XenonEffects.ini");
    mINI::INIStructure ini;
    inifile.read(ini);

    if (ini.has("MAIN"))
    {
        if (ini["MAIN"].has("TexturePack"))
            strcpy_s(TPKfilename, ini["MAIN"]["TexturePack"].c_str());
        if (ini["MAIN"].has("UseD3DDeviceTexture"))
            bUseD3DDeviceTexture = std::stol(ini["MAIN"]["UseD3DDeviceTexture"]) != 0;
        if (ini["MAIN"].has("Contrails"))
            bContrails = std::stol(ini["MAIN"]["Contrails"]) != 0;
        if (ini["MAIN"].has("UseCGStyle"))
            bUseCGStyle = std::stol(ini["MAIN"]["UseCGStyle"]) != 0;
        if (ini["MAIN"].has("BounceParticles"))
            bBounceParticles = std::stol(ini["MAIN"]["BounceParticles"]) != 0;
        if (ini["MAIN"].has("CarbonBounceBehavior"))
            bCarbonBounceBehavior = std::stol(ini["MAIN"]["CarbonBounceBehavior"]) != 0;
        if (ini["MAIN"].has("NISContrails"))
            bNISContrails = std::stol(ini["MAIN"]["NISContrails"]) != 0;
        if (ini["MAIN"].has("PassShadowMap"))
            bPassShadowMap = std::stol(ini["MAIN"]["PassShadowMap"]) != 0;
        if (ini["MAIN"].has("ContrailSpeed"))
            ContrailSpeed = std::stof(ini["MAIN"]["ContrailSpeed"]);
        if (ini["MAIN"].has("LimitContrailRate"))
            bLimitContrailRate = std::stol(ini["MAIN"]["LimitContrailRate"]) != 0;
        if (ini["MAIN"].has("LimitSparkRate"))
            bLimitSparkRate = std::stol(ini["MAIN"]["LimitSparkRate"]) != 0;
        if (ini["MAIN"].has("FadeOutParticles"))
            bFadeOutParticles = std::stol(ini["MAIN"]["FadeOutParticles"]) != 0;
    }

    if (ini.has("Limits"))
    {
        if (ini["Limits"].has("ContrailTargetFPS"))
            ContrailTargetFPS = std::stof(ini["Limits"]["ContrailTargetFPS"]);
        if (ini["Limits"].has("SparkTargetFPS"))
            SparkTargetFPS = std::stof(ini["Limits"]["SparkTargetFPS"]);
        if (ini["Limits"].has("ContrailMinIntensity"))
            ContrailMinIntensity = std::stof(ini["Limits"]["ContrailMinIntensity"]);
        if (ini["Limits"].has("ContrailMaxIntensity"))
            ContrailMaxIntensity = std::stof(ini["Limits"]["ContrailMaxIntensity"]);
        if (ini["Limits"].has("SparkIntensity"))
            SparkIntensity = std::stof(ini["Limits"]["SparkIntensity"]);
        if (ini["Limits"].has("MaxParticles"))
            MaxParticles = std::stol(ini["Limits"]["MaxParticles"]);
        if (ini["Limits"].has("NGEffectListSize"))
            NGEffectListSize = std::stol(ini["Limits"]["NGEffectListSize"]);
    }


    static float fGameTargetFPS = 1.0f / GetTargetFrametime();

    static float fContrailFrameDelay = (fGameTargetFPS / ContrailTargetFPS);
    ContrailFrameDelay = (uint32_t)round(fContrailFrameDelay);

    static float fSparkFrameDelay = (fGameTargetFPS / SparkTargetFPS);
    SparkFrameDelay = (uint32_t)round(fSparkFrameDelay);

    // iterate through the Elasticity section
    if (ini.has("Elasticity"))
    {
        char* cursor = 0;
        char IDstr[16] = { 0 };
        
        auto const& section = ini["Elasticity"];
        for (auto const& it : section)
        {
            ElasticityPair ep = { 0 };

            strcpy_s(IDstr, it.first.c_str());
            cursor = IDstr;
            if ((IDstr[0] == '0') && (IDstr[1] == 'x'))
            {
                cursor += 2;
                if (bValidateHexString(cursor))
                    sscanf(cursor, "%x", &ep.emmitter_key);
            }
            else
                ep.emmitter_key = Attrib_StringHash32(it.first.c_str());

            ep.Elasticity = stof(it.second);

            //printf("it.first: %s\nit.second: %s\nkey: 0x%X\nel: %.2f\n", it.first.c_str(), it.second.c_str(), ep.emmitter_key, ep.Elasticity);

            elasticityValues.push_back(ep);
        }
    }

    // set Elasticity defaults
    if (elasticityValues.size() == 0)
    {
        ElasticityPair ep = { 0xF872A5B4, 160.0f };
        elasticityValues.push_back(ep);
        ep = { 0x525E0A0E, 120.0f };
        elasticityValues.push_back(ep);
    }
    else
    {
        bool bSet_emsprk_line1 = false;
        bool bSet_emsprk_line2 = false;

        for (size_t i = 0; i < elasticityValues.size(); i++)
        {
            if (elasticityValues.at(i).emmitter_key == 0xF872A5B4)
                bSet_emsprk_line1 = true;
            if (elasticityValues.at(i).emmitter_key == 0x525E0A0E)
                bSet_emsprk_line1 = true;
        }

        if (!bSet_emsprk_line1)
        {
            ElasticityPair ep = { 0xF872A5B4, 160.0f };
            elasticityValues.push_back(ep);
        }

        if (!bSet_emsprk_line2)
        {
            ElasticityPair ep = { 0x525E0A0E, 120.0f };
            elasticityValues.push_back(ep);
        }
    }
}

int Init()
{
    // allocate for effect list
    gParticleList.mParticles = (NGParticle*)calloc(MaxParticles, sizeof(NGParticle));
    
    // delta time stealer
    injector::MakeCALL(0x0050D43C, EmitterSystem_Update_Hook, true);

    // injection point in LoadGlobalChunks()
    injector::MakeCALL(0x006648BC, InitXenonEffects, true);
    injector::MakeCALL(0x0066493E, sub_6DFAF0_hook, true);

    // render objects release
    injector::MakeCALL(0x006BD622, ReleaseRenderObj, true);

    // D3D reset inject
    injector::MakeCALL(0x006DB293, sub_6CFCE0_hook, true);

    // render inject, should be hooked to EmitterSystem::Render or somewhere after it
    injector::MakeCALL(0x006DF4BB, EmitterSystem_Render_Hook, true);

    // xenon effect inject at loc_50A5D7 for Emitter::SpawnParticles
    injector::MakeJMP(0x50A5D7, Emitter_SpawnParticles_Cave, true);

    // xenon effect inject at CarRenderConn::OnRender for contrails
    if (bContrails)
    {
        injector::MakeJMP(0x750F48, CarRenderConn_OnRender_Cave, true); // ExOpts was the cause of the Debug Cam crash due to an ancient workaround.
        // update contrail inject
        injector::MakeCALL(0x00756629, CarRenderConn_UpdateEngineAnimation_Hook, true);
        // extend CarRenderConn by 8 bytes to accommodate a float and a bool
        injector::WriteMemory<uint32_t>(0x0075E6FC, 0x408, true);
        injector::WriteMemory<uint32_t>(0x0075E766, 0x408, true);
    }

    //freopen("CON", "w", stdout);
    //freopen("CON", "w", stderr);

	return 0;
}


BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
        InitConfig();
		Init();
	}
	return TRUE;
}
