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
//bool bCarbonBounceBehavior = false;
bool bFadeOutParticles = false;
bool bCGIntensityBehavior = false;
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
void(__stdcall* sub_6CFCE0_2)() = (void(__stdcall*)())0x6CFCE0;
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
    UMath::Vector4  vel;
    UMath::Matrix4  mat;
    Attrib::Collection* spec;
    struct EmitterGroup* piggyback_effect;
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


class XenonFXVec : public std::vector<XenonEffectDef>//public eastl::vector<XenonEffectDef, bstl::allocator>
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
    void GeneratePolys();
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

    unsigned int mVertexCount = 0;
    unsigned int mMaxSprites = 0;
    unsigned int mNumPolys = 0;

    bool mbLocked = false;
    T* mLockedVB = NULL;
    LPDIRECT3DVERTEXBUFFER9 mpVB = NULL;
    LPDIRECT3DINDEXBUFFER9 mpIB = NULL;

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

        // force disable mipmaps
        g_D3DDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        g_D3DDevice->SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        g_D3DDevice->SetSamplerState(2, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        g_D3DDevice->SetSamplerState(3, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

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

    void Lock()
    {
        if (mpVB)
        {
            mpVB->Lock(
                0,
                sizeof(T) * mMaxSprites,
                (void**)&mLockedVB,
                D3DLOCK_DISCARD);

            mNumPolys = 0;
            mbLocked = true;
        }
    }

    void Unlock()
    {
        if (mbLocked && mpVB)
        {
            mpVB->Unlock();
            mLockedVB = NULL;
            mbLocked = false;
        }
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
    size_t mNumViews = 0;
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

        for (size_t i = 0; i < NumViews; i++)
        {
            mSprintListView[i].Init(spriteCount);
        }
    }

    void Reset()
    {
        for (size_t i = 0; i < NumViews; i++)
        {
            mSprintListView[i].Reset();
        }
    }

    void Lock()
    {
        mCurrViewBuffer = mNumViews;
        mNumViews = (mNumViews + 1) % NumViews;
        mSprintListView[mCurrViewBuffer].Lock();
    }

    void Unlock()
    {
        mSprintListView[mCurrViewBuffer].Unlock();
    }
};

class XSpriteManager
{
public:
    XSpriteList<XSpark, XSparkVert, 2> sparkList; 
    bool bBatching = false;

    void DrawBatch(eView* view);
    void AddSpark(NGParticle* particleList, unsigned int numParticles);
    void Init();
    void Reset();
    void Flip();
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

XSpriteManager NGSpriteManager;

float GetTargetFrametime()
{
    return **(float**)0x6612EC;
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

// note: unk_9D7880 == unk_8A3028

void __cdecl AddXenonEffect(
    struct EmitterGroup* piggyback_fx,
    Attrib::Collection* spec,
    UMath::Matrix4* mat,
    UMath::Vector4* vel)
{
    XenonEffectDef newEffect;

    if (gNGEffectList.size() < gNGEffectList.capacity())
    {
        newEffect.mat = *(UMath::Matrix4*)0x8A3028;
        newEffect.mat.v3 = mat->v3;
        newEffect.spec = spec;
        newEffect.vel = *vel;
        newEffect.piggyback_effect = piggyback_fx;
        gNGEffectList.push_back(newEffect);
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

void CalcCollisiontime(NGParticle* particle)
{
    UMath::Vector4 ray[2]; // [esp+28h] [ebp-78h] OVERLAPPED BYREF
    WCollisionMgr::WorldCollisionInfo collisionInfo; // [esp+48h] [ebp-58h] OVERLAPPED BYREF
    
    //particle->initialPos.z += 0.15f;

    ray[1].x = -(particle->life * particle->vel.y + particle->initialPos.y);
    ray[1].y = particle->life * particle->vel.z + particle->initialPos.z + particle->life * particle->life * particle->gravity;
    ray[1].z = (particle->life * particle->vel.x) + particle->initialPos.x;
    ray[1].w = 1.0f;

    ray[0].x = -particle->initialPos.y;
    ray[0].y = particle->initialPos.z;
    ray[0].z = particle->initialPos.x;
    ray[0].w = 1.0f;

    //point.y += 0.15f;

    ((void(__thiscall*)(WCollisionMgr::WorldCollisionInfo*))sub_404A20)(&collisionInfo);
    WCollisionMgr collisionMgr;
    collisionMgr.fSurfaceExclusionMask = 0;
    collisionMgr.fPrimitiveMask = 3;
    if (WCollisionMgr_CheckHitWorld(&collisionMgr, ray, &collisionInfo, 3u))
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
        //CalcCollisiontime(particle);
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
            float intensity = 1.0f;
            
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

void XSpriteManager::AddSpark(NGParticle* particleList, unsigned int numParticles)
{
    sparkList.Lock();

    bBatching = true;

    for (size_t i = 0; i < numParticles; i++)
    {
        NGParticle* particle = &particleList[i];
        
        UMath::Vector3 startPos;
        UMath::Vector3 endPos;
        float endAge;
        float width = particle->width / 2048.0f;

        if (i < sparkList.mSprintListView[sparkList.mCurrViewBuffer].mMaxSprites)
        {
            XSpark *spark = &sparkList.mSprintListView[sparkList.mCurrViewBuffer].mLockedVB[i];

            sparkList.mSprintListView[sparkList.mCurrViewBuffer].mNumPolys = i;

            if (spark)
            {
                uint32_t color = particle->color;

                UMath::fpu::ScaleAdd(particle->vel, particle->age, particle->initialPos, startPos);
                startPos.z += particle->age * particle->age * particle->gravity;

                endAge = (particle->length / 2048.0f) + particle->age;
                UMath::fpu::ScaleAdd(particle->vel, endAge, particle->initialPos, endPos);
                endPos.z += endAge * endAge * particle->gravity;

                // fade out particles if they have bounced or are contrails
                if (bFadeOutParticles && (particle->flags & NGParticle::Flags::SPAWN) == 0)
                {
                    uint8_t alpha = (color & 0xFF000000) >> 24;
                    alpha = (uint8_t)(alpha * 1 - std::powf(particle->age / (particle->life / 8191.0f), 3.0f));
                    color = (color & 0x00FFFFFF) + (alpha << 24); // QOL feature
                }

                spark->v[0].position = startPos;
                spark->v[0].color = color;
                spark->v[0].texcoord[0] = particle->uv[0] / 255.0f;
                spark->v[0].texcoord[1] = particle->uv[1] / 255.0f;

                spark->v[1].position = startPos;
                spark->v[1].position.z += width;
                spark->v[1].color = color;
                spark->v[1].texcoord[0] = particle->uv[2] / 255.0f;
                spark->v[1].texcoord[1] = particle->uv[1] / 255.0f;
                
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
    
    if (bBatching && sparkList.mSprintListView[sparkList.mCurrViewBuffer].mpVB)
        bBatching = false;

    sparkList.Unlock();
}

void ParticleList::GeneratePolys()
{
    if (mNumParticles)
        NGSpriteManager.AddSpark(mParticles, mNumParticles);
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

void XSpriteManager::Flip()
{
    sparkList.mNumViews = 0;
    //sparkList.mSprintListView[0].mCurrentBuffer = (sparkList.mSprintListView[0].mCurrentBuffer + 1) % 3;
    //sparkList.mSprintListView[1].mCurrentBuffer = (sparkList.mSprintListView[1].mCurrentBuffer + 1) % 3;
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
    gNGEffectList.reserve(NGEffectListSize);
    return false;
}

uint32_t sub_6DFAF0_hook()
{
    NGSpriteManager.Init();//InitializeRenderObj();
    return sub_6DFAF0();
}

uint32_t SparkFC = 0;
void AddXenonEffect_Spark_Hook(struct EmitterGroup* piggyback_fx, Attrib::Collection* spec, UMath::Matrix4* mat, UMath::Vector4* vel, float intensity)
{
    if (!bLimitSparkRate)
        return AddXenonEffect(piggyback_fx, spec, mat, vel);

    if ((SparkFC + SparkFrameDelay) <= eFrameCounter)
    {
        if (SparkFC != eFrameCounter)
        {
            SparkFC = eFrameCounter;
            AddXenonEffect(piggyback_fx, spec, mat, vel);
        }
    }
}

uint32_t ContrailFC = 0;
void AddXenonEffect_Contrail_Hook(struct EmitterGroup* piggyback_fx, Attrib::Collection* spec, UMath::Matrix4* mat, UMath::Vector4* vel, float intensity)
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

    if (!bLimitContrailRate)
        return AddXenonEffect(piggyback_fx, spec, mat, vel);

    if ((ContrailFC + ContrailFrameDelay) <= eFrameCounter)
    {
        if (ContrailFC != eFrameCounter)
        {
            ContrailFC = eFrameCounter;
            AddXenonEffect(piggyback_fx, spec, mat, vel);
        }
    }
#endif
}

void UpdateXenonEmitters(float dt)
{
    gParticleList.AgeParticles(dt);

    for (int i = 0; i < gNGEffectList.size(); i++)
    {
        XenonEffectDef &effectDef = gNGEffectList[i];
        if (!effectDef.piggyback_effect || (*((uint32_t*)effectDef.piggyback_effect + 6) & 0x10) != 0)
        {
            NGEffect effect{ &effectDef, dt };
        }
    }

    gNGEffectList.clear();

    //if (dt > 0.0f)
    gParticleList.GeneratePolys();
}

void __stdcall EmitterSystem_Update_Hook(float dt)
{
    uint32_t that;
    _asm mov that, ecx
    EmitterSystem_UpdateParticles((void*)that, dt);
    if (*(uint32_t*)GAMEFLOWSTATUS_ADDR == 6)
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
        //if (ini["MAIN"].has("CarbonBounceBehavior"))
        //    bCarbonBounceBehavior = std::stol(ini["MAIN"]["CarbonBounceBehavior"]) != 0;
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
        if (ini["MAIN"].has("CGIntensityBehavior"))
            bCGIntensityBehavior = std::stol(ini["MAIN"]["CGIntensityBehavior"]) != 0;
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

    if (ContrailTargetFPS > 0)
    {
        static float fContrailFrameDelay = (fGameTargetFPS / ContrailTargetFPS);
        ContrailFrameDelay = (uint32_t)round(fContrailFrameDelay);
    }

    if (SparkTargetFPS > 0)
    {
        static float fSparkFrameDelay = (fGameTargetFPS / SparkTargetFPS);
        SparkFrameDelay = (uint32_t)round(fSparkFrameDelay);
    }

    // iterate through the Elasticity section
    //if (ini.has("Elasticity"))
    //{
    //    char* cursor = 0;
    //    char IDstr[16] = { 0 };
    //    
    //    auto const& section = ini["Elasticity"];
    //    for (auto const& it : section)
    //    {
    //        ElasticityPair ep = { 0 };
    //
    //        strcpy_s(IDstr, it.first.c_str());
    //        cursor = IDstr;
    //        if ((IDstr[0] == '0') && (IDstr[1] == 'x'))
    //        {
    //            cursor += 2;
    //            if (bValidateHexString(cursor))
    //                sscanf(cursor, "%x", &ep.emmitter_key);
    //        }
    //        else
    //            ep.emmitter_key = Attrib_StringHash32(it.first.c_str());
    //
    //        ep.Elasticity = stof(it.second);
    //
    //        //printf("it.first: %s\nit.second: %s\nkey: 0x%X\nel: %.2f\n", it.first.c_str(), it.second.c_str(), ep.emmitter_key, ep.Elasticity);
    //
    //        elasticityValues.push_back(ep);
    //    }
    //}
    //
    //// set Elasticity defaults
    //if (elasticityValues.size() == 0)
    //{
    //    ElasticityPair ep = { 0xF872A5B4, 160.0f };
    //    elasticityValues.push_back(ep);
    //    ep = { 0x525E0A0E, 120.0f };
    //    elasticityValues.push_back(ep);
    //}
    //else
    //{
    //    bool bSet_emsprk_line1 = false;
    //    bool bSet_emsprk_line2 = false;
    //
    //    for (size_t i = 0; i < elasticityValues.size(); i++)
    //    {
    //        if (elasticityValues.at(i).emmitter_key == 0xF872A5B4)
    //            bSet_emsprk_line1 = true;
    //        if (elasticityValues.at(i).emmitter_key == 0x525E0A0E)
    //            bSet_emsprk_line1 = true;
    //    }
    //
    //    if (!bSet_emsprk_line1)
    //    {
    //        ElasticityPair ep = { 0xF872A5B4, 160.0f };
    //        elasticityValues.push_back(ep);
    //    }
    //
    //    if (!bSet_emsprk_line2)
    //    {
    //        ElasticityPair ep = { 0x525E0A0E, 120.0f };
    //        elasticityValues.push_back(ep);
    //    }
    //}
}

void EarlyRenderHook()
{
    sub_6CFCE0_2();
    NGSpriteManager.Flip();
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

    // do some per-frame early initialization
    sub_6CFCE0_2 = injector::MakeCALL(0x6E73C3, EarlyRenderHook).get();

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
