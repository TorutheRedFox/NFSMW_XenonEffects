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
#define WORLDPRELITSHADER_OBJ_ADDR 0x0093DEBC
#define CURRENTSHADER_OBJ_ADDR 0x00982C80

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
float (__cdecl* bRandom_Float_Int)(float range, int unk) = (float (__cdecl*)(float, int))0x0045D9E0;
int(__cdecl* bRandom_Int_Int)(int range, uint32_t* unk) = (int(__cdecl*)(int, uint32_t*))0x0045D9A0;
unsigned int(__cdecl* bStringHash)(char* str) = (unsigned int(__cdecl*)(char*))0x00460BF0;
TextureInfo*(__cdecl* GetTextureInfo)(unsigned int name_hash, int return_default_texture_if_not_found, int include_unloaded_textures) = (TextureInfo*(__cdecl*)(unsigned int, int, int))0x00503400;
void (__thiscall* EmitterSystem_UpdateParticles)(void* EmitterSystem, float dt) = (void (__thiscall*)(void*, float))0x00508C30;
void(__thiscall* EmitterSystem_Render)(void* EmitterSystem, void* eView) = (void(__thiscall*)(void*, void*))0x00503D00;
void(__stdcall* sub_7286D0)() = (void(__stdcall*)())0x007286D0;
void* (__cdecl* FastMem_CoreAlloc)(uint32_t size, char* debug_line) = (void* (__cdecl*)(uint32_t, char*))0x00465A70;
void(__stdcall* sub_739600)() = (void(__stdcall*)())0x739600;
void(__thiscall* CarRenderConn_UpdateEngineAnimation)(void* CarRenderConn, float param1, void* PktCarService) = (void(__thiscall*)(void*, float, void*))0x00745F20;
void(__stdcall* sub_6CFCE0)() = (void(__stdcall*)())0x6CFCE0;
void(__cdecl* ParticleSetTransform)(D3DXMATRIX* worldmatrix, uint32_t EVIEW_ID) = (void(__cdecl*)(D3DXMATRIX*, uint32_t))0x6C8000;
bool(__thiscall* WCollisionMgr_CheckHitWorld)(void* WCollisionMgr, UMath::Matrix4* inputSeg, void* cInfo, uint32_t primMask) = (bool(__thiscall*)(void*, UMath::Matrix4*, void*, uint32_t))0x007854B0;
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
    /*Attrib::Collection*/ void* spec;
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

    struct UMath::Vector3 initialPos;
    unsigned int color;
    struct UMath::Vector3 vel;
    float gravity;
    struct UMath::Vector3 impactNormal;
    float remainingLife;
    float life;
    float age;
    unsigned char elasticity;
    unsigned char pad[3];
    Flags flags;
    unsigned char rotX;
    unsigned char rotY;
    unsigned char rotZ;
    unsigned char size;
    unsigned char startX;
    unsigned char startY;
    unsigned char startZ;
    unsigned char uv[4];
};

inline constexpr NGParticle::Flags operator|(NGParticle::Flags a, NGParticle::Flags b)
{
    return static_cast<NGParticle::Flags>(static_cast<int>(a) | static_cast<int>(b));
}

inline constexpr NGParticle::Flags operator&(NGParticle::Flags a, NGParticle::Flags b)
{
    return static_cast<NGParticle::Flags>(static_cast<int>(a) & static_cast<int>(b));
}

class ParticleList
{
public:
    NGParticle* mParticles;// [1000] ;
    unsigned int mNumParticles;
    void AgeParticles(float dt);
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
namespace Attrib
{
    namespace Gen
    {
        struct HashMap
        {
            void* mTable;
            unsigned __int32 mTableSize;
            unsigned __int32 mNumEntries;
            unsigned __int16 mWorstCollision;
            unsigned __int16 mKeyShift;
        };

        struct Collection
        {
            Attrib::HashMap mTable;
            void* mParent;
            void* mClass;
            void* mLayout;
            unsigned int mRefCount;
            unsigned int mKey;
            void* mSource;
            const char* mNamePtr;
        };

        class CarbonInstance
        {
        public:
            Attrib::Collection* mCollection;
            void* mLayoutPtr;
            unsigned int mMsgPort;
            unsigned int mFlags;
        
            ~CarbonInstance()
            {
                Attrib_Instance_Dtor_Shim(this);
            }
        };

        struct RefSpec
        {
            uint32_t mClassKey;
            uint32_t mCollectionKey;
            Attrib::Collection* mCollectionPtr;
        };

        class fuelcell_effect : CarbonInstance
        {
        public:
            struct _LayoutStruct
            {
                bool doTest;
            };
        };

        class fuelcell_emitter : CarbonInstance
        {
        public:
            struct _LayoutStruct
            { // TODO - carbon layout, replace with MW's
                UMath::Vector4 VolumeExtent;
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
            };
        };

        class emitteruv : CarbonInstance
        {
        public:
            struct _LayoutStruct
            {
                float StartV;
                float StartU;
                float EndV;
                float EndU;
            };
        };
    }
}

struct eView
{
    void* PlatInfo;
    uint32_t EVIEW_ID;
};

template <typename T, typename U>
struct SpriteBuffer
{
    IDirect3DVertexBuffer9* vertex_buffer;
    IDirect3DIndexBuffer9* index_buffer;
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

template <typename Sprite, typename SpriteVert, int BufferCount>
struct XSpriteList
{
    SpriteBuffer<Sprite, SpriteVert> mSprintListView[BufferCount];
    int mNumViews;
    int mCurrViewBuffer;
    unsigned int mMaxSprites;
    TextureInfo* mTexture;
};

class __declspec(align(4)) XSpriteManager
{
public:
    XSpriteList<XSpark, XSparkVert, 1> sparkList;
    void* debris;
    bool bBatching;
    void DrawBatch(eView* view);
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
float EmitterDeltaTime = 0.0f;

LPDIRECT3DDEVICE9 g_D3DDevice;

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

//char NGSpriteManager_ClassData[128];

XSpriteManager NGSpriteManager;

uint32_t NGSpriteManager_thing[32] = { (uint32_t)(&NGSpriteManager), 0};

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
    /*Attrib::Collection*/void* spec,
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

void __declspec(naked) CalcCollisiontime(NGParticle* particle)
{
    _asm
    {
        sub     esp, 9Ch
        fld     flt_A6C230
        push    esi
        mov     esi, [esp + 0A4h]
        fadd    dword ptr[esi + 8]
        mov     eax, [esi + 30h]
        mov[esp + 4], eax
        mov     ecx, [esi]
        fst     dword ptr[esi + 8]
        mov     edx, ecx
        fld     dword ptr[esp + 4]
        mov[esp + 24h], ecx
        fmul    dword ptr[esi + 10h]
        mov     dword ptr[esp + 18h], 3F800000h
        mov[esp + 14h], edx
        mov[esp + 30h], edx
        fadd    dword ptr[esi]
        fld     dword ptr[esp + 4]
        fmul    dword ptr[esi + 14h]
        fadd    dword ptr[esi + 4]
        fstp    dword ptr[esp + 20h]
        fld     dword ptr[esi + 30h]
        fld     dword ptr[esp + 4]
        fmul    dword ptr[esi + 18h]
        fadd    st, st(3)
        fld     st(1)
        fmul    st, st(2)
        fmul    dword ptr[esi + 1Ch]
        faddp   st(1), st
        fstp    st(1)
        fld     dword ptr[esp + 20h]
        fchs
        fld     dword ptr[esi + 4]
        fchs
        fstp    dword ptr[esp + 0Ch]
        mov     eax, [esp + 0Ch]
        fxch    st(3)
        mov[esp + 28h], eax
        mov     eax, [esp + 18h]
        fstp    dword ptr[esp + 10h]
        mov     ecx, [esp + 10h]
        fxch    st(2)
        fstp    dword ptr[esp + 0Ch]
        mov[esp + 2Ch], ecx
        mov     ecx, [esp + 0Ch]
        fxch    st(1)
        fstp    dword ptr[esp + 10h]
        mov     edx, [esp + 10h]
        mov[esp + 38h], ecx
        mov     dword ptr[esp + 18h], 3F800000h
        fstp    dword ptr[esp + 14h]
        mov     ecx, [esp + 18h]
        mov[esp + 34h], eax
        mov     eax, [esp + 14h]
        mov[esp + 44h], ecx
        lea     ecx, [esp + 48h]
        mov[esp + 3Ch], edx
        mov[esp + 40h], eax
        call    sub_404A20
        fld     flt_A6C230
        fadd    dword ptr[esp + 2Ch]
        push    3
        lea     edx, [esp + 4Ch]
        push    edx
        lea     eax, [esp + 30h]
        fstp    dword ptr[esp + 34h]
        push    eax
        lea     ecx, [esp + 28h]
        mov     dword ptr[esp + 28h], 0
        mov     dword ptr[esp + 2Ch], 3
        call    WCollisionMgr_CheckHitWorld
        test    eax, eax
        jz      loc_73F30F
        mov     ecx, [esi + 18h]
        mov[esp + 4], ecx
        fld     dword ptr[esp + 4]
        fmul    dword ptr[esp + 4]
        fld     dword ptr[esi + 8]
        fsub    dword ptr[esp + 4Ch]
        fmul    dword ptr[esi + 1Ch]
        fmul    ds : flt_9C2A3C
        fsubp   st(1), st
        fst     dword ptr[esp + 8]
        fcomp   ds : flt_9C248C
        fnstsw  ax
        test    ah, 41h
        jp      loc_73F2AA
        fld     ds : flt_9C248C
        jmp     loc_73F2ED
        ; -------------------------------------------------------------------------- -

        loc_73F2AA:; CODE XREF : CalcCollisiontime + 140↑j
        mov     edx, [esp + 8]
        push    edx; float
        call    sub_6016B0
        fstp    dword ptr[esp + 0Ch]
        fld     dword ptr[esi + 1Ch]
        add     esp, 4
        fadd    st, st
        fstp    dword ptr[esp + 1Ch]
        fld     dword ptr[esp + 8]
        fsub    dword ptr[esp + 4]
        fdiv    dword ptr[esp + 1Ch]
        fcom    ds : flt_9C248C
        fnstsw  ax
        test    ah, 5
        jp      loc_73F2ED
        fstp    st
        fld     dword ptr[esp + 4]
        fchs
        fsub    dword ptr[esp + 8]
        fdiv    dword ptr[esp + 1Ch]

        loc_73F2ED:; CODE XREF : CalcCollisiontime + 148↑j
        ; CalcCollisiontime + 17B↑j
        mov     al, [esi + 3Ch]
        fstp    dword ptr[esi + 30h]
        fld     dword ptr[esp + 58h]
        mov     ecx, [esp + 5Ch]
        or al, 2
        fchs
        mov[esi + 3Ch], al
        fstp    dword ptr[esi + 24h]
        mov     eax, [esp + 60h]
        mov[esi + 20h], eax
        mov[esi + 28h], ecx

        loc_73F30F : ; CODE XREF : CalcCollisiontime + 10A↑j
        pop     esi
        add     esp, 9Ch
        retn
    }
}

void(*CalcCollisiontime_Abstract)(NGParticle* particle) = (void(*)(NGParticle*))&CalcCollisiontime;

bool BounceParticle(NGParticle* particle)
{
    if (!bBounceParticles)
        return true;

    float life;
    float gravity;
    float gravityOverTime;
    float velocityMagnitude;
    UMath::Vector3 newVelocity = particle->vel;
    
    life = particle->life; // / 8191.0f; // MW multiplies life by 8191 when spawning the particle, but Carbon does not as it uses a float
    gravity = particle->gravity;
    
    gravityOverTime = particle->gravity * life;
    
    particle->initialPos += newVelocity * life;
    particle->initialPos.z += gravityOverTime * life;
    
    if (bCarbonBounceBehavior) // carbon modification
        gravityOverTime *= 2.0f;

    newVelocity.z += gravityOverTime;
    
    velocityMagnitude = UMath::Length(newVelocity);
    
    newVelocity = UMath::Normalize(newVelocity);
    
    if (bCarbonBounceBehavior)
        particle->life = particle->remainingLife;
    else
        particle->life = 1.0f; // should be 8191
    particle->age = 0.0f;
    particle->flags = particle->flags & NGParticle::Flags::DEBRIS | NGParticle::Flags::BOUNCED;
    
    float bounceCos = UMath::Dot(newVelocity, particle->impactNormal);
    
    if (bCarbonBounceBehavior) // MW didn't have elasticity
        velocityMagnitude *= particle->elasticity / 255.0f;
    
    particle->vel = (newVelocity - (particle->impactNormal * bounceCos * 2.0f)) * velocityMagnitude;
    
    if (bCarbonBounceBehavior) // MW doesn't call this here
        CalcCollisiontime_Abstract(particle);
    
    return true;
}

void ParticleList::AgeParticles(float dt)
{
    size_t aliveCount = 0;
    for (size_t i = 0; i <mNumParticles; i++)
    {
        NGParticle& particle = mParticles[i];
        if ((particle.age + dt) <= particle.life)
        {
            memcpy(&mParticles[aliveCount], &mParticles[i], sizeof(NGParticle));
            mParticles[aliveCount].age += dt;
            aliveCount++;
        }
        else if (particle.flags & NGParticle::Flags::SPAWN)
        {
            particle.remainingLife -= particle.age + dt;
            if (particle.remainingLife > dt && BounceParticle(&particle))
            {
                aliveCount++;
            }
        }
    }
    mNumParticles = aliveCount;
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

void __declspec(naked) CGEmitter_SpawnParticles()
{
    _asm
    {
                fld     dword ptr [esp+8]
                sub     esp, 0ACh
                fcomp   ds:flt_9C248C
                push    ebp
                mov     ebp, ecx
                fnstsw  ax
                test    ah, 41h
                jnp     loc_73F920
                fld     dword ptr [esp+0B4h]
                fcomp   ds:flt_9C248C
                fnstsw  ax
                test    ah, 41h
                jnp     loc_73F920
                mov     eax, randomSeed
                push    ebx
                push    esi
                push    edi
                lea     esi, [ebp+30h]
                mov     ecx, 10h
                lea     edi, [esp+7Ch]
                rep movsd
                mov     esi, [ebp+4]
                mov     [esp+10h], eax
                fld     dword ptr [esi+6Ch]
                fld     st
                fmul    dword ptr [esi+8Ch]
                fsubr   st, st(1)
                fstp    dword ptr [esp+3Ch]
                fstp    st
                fld     dword ptr [esi+50h]
                fmul    ds:flt_9C92F0
                call    __ftol2
                fld     dword ptr [esi+54h]
                fmul    ds:flt_9C92F0
                mov     edi, eax
                call    __ftol2
                fld     dword ptr [esi+58h]
                fmul    ds:flt_9C92F0
                mov     ebx, eax
                call    __ftol2
                fld     dword ptr [esi+5Ch]
                fmul    ds:flt_9C92F0
                mov     [esp+24h], eax
                call    __ftol2
                fld     ds:flt_9C2478
                fld     dword ptr [esp+0C4h]
                mov     ecx, eax
                fucompp
                fnstsw  ax
                test    ah, 44h
                jnp     short loc_73F409
                fld     dword ptr [esp+0C4h]
                fmul    ds:flt_9EA540
                fld     ds:flt_9EA540
                fcomp   st(1)
                fnstsw  ax
                test    ah, 5
                jp      short loc_73F402
                fstp    st
                fld     ds:flt_9EA540

loc_73F402:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+D8↑j
                call    __ftol2
                mov     ecx, eax

loc_73F409:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+BC↑j
                fld     dword ptr [esp+0C4h]
                mov     eax, [esp+24h]
                fcomp   ds:flt_9C2478
                shl     ecx, 8
                or      ecx, edi
                shl     ecx, 8
                or      ecx, ebx
                shl     ecx, 8
                or      ecx, eax
                fnstsw  ax
                mov     [esp+50h], ecx
                test    ah, 41h
                jnz     short loc_73F43D
                fld     dword ptr [esp+0C4h]
                jmp     short loc_73F443
; ---------------------------------------------------------------------------

loc_73F43D:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+112↑j
                fld     ds:flt_9C2478

loc_73F443:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+11B↑j
                mov     eax, [ebp+4]
                fmul    dword ptr [eax+90h]
                fmul    dword ptr [eax+70h]
                fld     dword ptr [esp+0C4h]
                fcomp   ds:flt_9C2478
                fnstsw  ax
                test    ah, 41h
                jnz     short loc_73F46C
                fld     dword ptr [esp+0C4h]
                jmp     short loc_73F472
; ---------------------------------------------------------------------------

loc_73F46C:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+141↑j
                fld     ds:flt_9C2478

loc_73F472:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+14A↑j
                mov     ecx, [ebp+4]
                fmul    dword ptr [ecx+90h]
                xor     ebx, ebx
                fxch    st(1)
                push    ebx
                fmul    ds:flt_9C2C44
                push    28638D89h
                mov     ecx, ebp
                //sub ecx, 4
                fsubp   st(1), st
                fstp    dword ptr [esp+28h]
                call    Attrib_Instance_GetAttributePointer_Shim; Attrib::Instance::GetAttributePointer(const(ulong,uint))
                test    eax, eax
                jnz     short loc_73F4A6
                push    1
                call    Attrib_DefaultDataArea; Attrib::DefaultDataArea((uint))
                add     esp, 4

loc_73F4A6:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+17A↑j
                mov     dl, [eax]
                push    0
                push    0D2603865h
                mov     ecx, ebp
                //sub ecx, 4
                mov     [esp+22h], dl
                call    Attrib_Instance_GetAttributePointer_Shim; Attrib::Instance::GetAttributePointer(const(ulong,uint))
                test    eax, eax
                jnz     short loc_73F4C8
                push    1
                call    Attrib_DefaultDataArea; Attrib::DefaultDataArea((uint))
                add     esp, 4

loc_73F4C8:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+19C↑j
                mov     al, [eax]
                push    0
                push    0E2CC8106h
                mov     ecx, ebp
                //sub ecx, 4
                mov     [esp+21h], al
                call    Attrib_Instance_GetAttributePointer_Shim; Attrib::Instance::GetAttributePointer(const(ulong,uint))
                test    eax, eax
                jnz     short loc_73F4EA
                push    1
                call    Attrib_DefaultDataArea ; Attrib::DefaultDataArea((uint))
                add     esp, 4

loc_73F4EA:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+1BE↑j
                mov     cl, [eax]
                mov     edx, [ebp+4]
                movzx   eax, byte ptr [edx+94h]
                test    eax, eax
                mov     [esp+1Bh], cl
                mov     [esp+24h], eax
                jz      short loc_73F50C
                dec     eax
                mov     ebx, 1
                mov     [esp+24h], eax

loc_73F50C:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+1E0↑j
                fld     dword ptr [esp+0C0h]
                mov     dword ptr [esp+14h], 0
                fdiv    dword ptr [esp+20h]
                fstp    dword ptr [esp+58h]
                fld     ds:flt_9C248C
                fld     dword ptr [esp+20h]
                fucompp
                fnstsw  ax
                test    ah, 44h
                jnp     loc_73F913
                lea     ebx, [ebx+0]

loc_73F540:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+5ED↓j
                fld     dword ptr [esp+20h]
                mov     eax, dword ptr[gParticleList.mNumParticles]
                cmp     eax, MaxParticles
                fsub    ds:flt_9C2478
                fstp    dword ptr [esp+20h]
                jnb     loc_73F913
                lea     esi, [eax+eax*8]
                mov ecx, dword ptr[gParticleList.mParticles]
                lea     esi, [ecx+esi*8] ; ParticleList gParticleList
                inc     eax
                test    esi, esi
                mov     dword ptr[gParticleList.mNumParticles], eax
                jz      loc_73F913
                mov     eax, [ebp+4]
                mov     ecx, [eax+84h]
                mov     edx, [eax+88h]
                lea     eax, [esp+10h]
                mov     [esp+1Ch], ecx
                push    eax             ; int
                mov     ecx, edx
                push    ecx             ; float
                mov     [esp+5Ch], edx
                call    bRandom_Float_Int; bRandom(float,uint *)
                fadd    dword ptr [esp+24h]
                add     esp, 8
                fst     dword ptr [esp+1Ch]
                fcomp   ds:flt_9C248C
                fnstsw  ax
                test    ah, 5
                jnp     loc_73F913
                fld     dword ptr [esp+1Ch]
                fcomp   ds:flt_9C92F0
                fnstsw  ax
                test    ah, 5
                jnp     short loc_73F5CF
                mov     dword ptr [esp+1Ch], 437F0000h

loc_73F5CF:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+2A5↑j
                mov     edi, [ebp+4]
                mov     eax, [edi+10h]
                lea     edx, [esp+10h]
                push    edx             ; int
                push    eax             ; float
                call    bRandom_Float_Int; bRandom(float,uint *)
                fadd    st, st
                lea     ecx, [esp+18h]
                push    ecx             ; int
                fsubr   dword ptr [edi+10h]
                mov     edi, [ebp+4]
                mov     edx, [edi+14h]
                push    edx             ; float
                fsubr   ds:flt_9C2478
                fstp    dword ptr [esp+6Ch]
                call    bRandom_Float_Int; bRandom(float,uint *)
                fadd    st, st
                lea     eax, [esp+20h]
                push    eax             ; int
                fsubr   dword ptr [edi+14h]
                mov     edi, [ebp+4]
                mov     ecx, [edi+18h]
                push    ecx             ; float
                fsubr   ds:flt_9C2478
                fstp    dword ptr [esp+78h]
                call    bRandom_Float_Int  ; bRandom(float,uint *)
                mov     eax, [ebp+4]
                fadd    st, st
                lea     edx, [esp+84h]
                push    edx
                fsubr   dword ptr [edi+18h]
                lea     ecx, [ebp+30h]
                push    ecx
                add     eax, 40h ; '@'
                fsubr   ds:flt_9C2478
                push    eax
                fstp    dword ptr [esp+88h]
                fld     dword ptr [eax-10h]
                fmul    dword ptr [ebp+20h]
                fstp    dword ptr [esp+4Ch]
                fld     dword ptr [eax-0Ch]
                fmul    dword ptr [ebp+24h]
                fstp    dword ptr [esp+50h]
                fld     dword ptr [eax-8]
                fmul    dword ptr [ebp+28h]
                fstp    dword ptr [esp+54h]
                call    sub_478200
                fld     dword ptr [esp+90h]
                mov     edi, [ebp+4]
                fadd    dword ptr [esp+4Ch]
                mov     ecx, [edi+7Ch]
                fld     dword ptr [esp+94h]
                lea     eax, [esp+34h]
                fadd    dword ptr [esp+50h]
                push    eax             ; int
                fld     dword ptr [esp+9Ch]
                push    ecx             ; float
                fadd    dword ptr [esp+5Ch]
                fstp    dword ptr [esp+5Ch]
                fld     dword ptr [esp+0A4h]
                fadd    dword ptr [esp+60h]
                fstp    dword ptr [esp+60h]
                fxch    st(1)
                fmul    dword ptr [esp+88h]
                fstp    dword ptr [esp+54h]
                fmul    dword ptr [esp+8Ch]
                fstp    dword ptr [esp+58h]
                fld     dword ptr [esp+5Ch]
                fmul    dword ptr [esp+90h]
                fstp    dword ptr [esp+5Ch]
                call    bRandom_Float_Int; bRandom(float,uint *)
                fadd    st, st
                lea     edx, [esp+3Ch]
                fld     dword ptr [edi+74h]
                fsub    dword ptr [edi+7Ch]
                mov     edi, [ebp+4]
                faddp   st(1), st
                fstp    dword ptr [esp+64h]
                mov     eax, [edi+20h]
                push    edx             ; int
                push    eax             ; float
                call    bRandom_Float_Int; bRandom(float,uint *)
                fld     dword ptr [edi+20h]
                fmul    ds:flt_9C2888
                lea     ecx, [esp+44h]
                push    ecx             ; int
                fsubp   st(1), st
                fadd    dword ptr [edi]
                mov     edi, [ebp+4]
                mov     edx, [edi+24h]
                push    edx             ; float
                fstp    dword ptr [esp+7Ch]
                call    bRandom_Float_Int; bRandom(float,uint *)
                fld     dword ptr [edi+24h]
                fmul    ds:flt_9C2888
                lea     eax, [esp+4Ch]
                push    eax             ; int
                fsubp   st(1), st
                fadd    dword ptr [edi+4]
                mov     edi, [ebp+4]
                mov     ecx, [edi+28h]
                push    ecx             ; float
                fstp    dword ptr [esp+88h]
                call    bRandom_Float_Int  ; bRandom(float,uint *)
                fld     dword ptr [edi+28h]
                fmul    ds:flt_9C2888
                add     esp, 44h
                lea     edx, [esp+40h]
                push    edx
                fsubp   st(1), st
                lea     eax, [esp+80h]
                push    eax
                fadd    dword ptr [edi+8]
                mov     ecx, edx
                push    ecx
                fstp    dword ptr [esp+54h]
                mov     dword ptr [esp+58h], 3F800000h
                call    sub_6012B0
                fld     dword ptr [esp+34h]
                fmul    dword ptr [esp+20h]
                mov     edx, [esp+34h]
                mov     eax, [esp+38h]
                mov     ecx, [esp+3Ch]
                fadd    dword ptr [esp+4Ch]
                mov     [esi+10h], edx
                mov     edx, [esp+48h]
                fstp    dword ptr [esi]
                mov     [esi+14h], eax
                fld     dword ptr [esp+38h]
                mov     eax, edx
                fmul    dword ptr [esp+20h]
                mov     [esi+18h], ecx
                mov     ecx, [esp+20h]
                mov     [esi+2Ch], edx
                mov     edx, [esp+44h]
                fadd    dword ptr [esp+50h]
                mov     [esi+30h], eax
                mov     [esi+34h], ecx
                fstp    dword ptr [esi+4]
                mov     [esi+1Ch], edx
                fld     dword ptr [esp+44h]
                add     esp, 0Ch
                fmul    dword ptr [esp+14h]
                fmul    dword ptr [esp+14h]
                fld     dword ptr [esp+30h]
                fmul    dword ptr [esp+14h]
                faddp   st(1), st
                fadd    dword ptr [esp+48h]
                fstp    dword ptr [esi+8]
                mov     eax, [ebp+4]
                fld     dword ptr [eax+80h]
                call    __ftol2
                mov     [esi+38h], al
                mov     ecx, [ebp+4]
                fld     dword ptr [ecx+78h]
                call    __ftol2
                test    bl, 1
                mov     edx, [esp+50h]
                mov     [esi+40h], al
                mov     [esi+0Ch], edx
                mov     [esi+3Ch], bl
                jz      short loc_73F87A
                mov     al, [esp+24h]
                mov     [esi+44h], al
                mov     ecx, [ebp+14h]
                fld     dword ptr [ecx+4] // emitteruv StartU
                call    __ftol2
                mov     [esi+45h], al
                mov     edx, [ebp+14h]
                fld     dword ptr [edx+0Ch] // emitteruv StartV
                call    __ftol2
                mov     [esi+46h], al
                lea     eax, [esp+10h]
                push    eax
                push    0FFh
                call    bRandom_Int_Int; bRandom(int,uint *)
                lea     ecx, [esp+18h]
                push    ecx
                push    0FFh
                mov     [esi+41h], al
                call    bRandom_Int_Int; bRandom(int,uint *)
                lea     edx, [esp+20h]
                push    edx
                push    0FFh
                mov     [esi+42h], al
                call    bRandom_Int_Int  ; bRandom(int,uint *)
                mov     cl, [esp+31h]
                mov     dl, [esp+33h]
                mov     [esi+43h], al
                mov     al, [esp+32h]
                add     esp, 18h
                mov     [esi+3Dh], al
                mov     [esi+3Eh], cl
                mov     [esi+3Fh], dl
                jmp     short loc_73F8D5
; ---------------------------------------------------------------------------

loc_73F87A:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+4E5↑j
                mov     eax, [ebp+14h]
                fld     dword ptr [eax+4] // emitteruv StartU
                fmul    ds:flt_9C92F0
                call    __ftol2
                mov     [esi+44h], al
                mov     ecx, [ebp+14h]
                fld     dword ptr [ecx+0Ch] // emitteruv StartV
                fmul    ds:flt_9C92F0
                call    __ftol2
                mov     [esi+45h], al
                mov     edx, [ebp+14h]
                fld     dword ptr [edx+8] // emitteruv EndU
                fmul    ds:flt_9C92F0
                call    __ftol2
                mov     [esi+46h], al
                mov     eax, [ebp+14h]
                fld     dword ptr [eax] // emitteruv EndV
                fmul    ds:flt_9C92F0
                call    __ftol2
                fld     dword ptr [esp+1Ch]
                mov     [esi+47h], al
                call    __ftol2
                mov     [esi+41h], al

loc_73F8D5:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+558↑j
                fld     dword ptr [esp+14h]
                mov     al, [esi+3Ch]
                test    al, 4
                fadd    dword ptr [esp+58h]
                fstp    dword ptr [esp+14h]
                jnz     short loc_73F8FC
                mov     al, [esp+0C8h]
                test    al, al
                jnz     short loc_73F8FC
                push    esi
                call    CalcCollisiontime ; CalcCollisiontime(NGParticle *)
                add     esp, 4

loc_73F8FC:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+5C6↑j
                                        ; CGEmitter::SpawnParticles(float,float)+5D1↑j
                fld     ds:flt_9C248C
                fld     dword ptr [esp+20h]
                fucompp
                fnstsw  ax
                test    ah, 44h
                jp      loc_73F540

loc_73F913:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+214↑j
                                        ; CGEmitter::SpawnParticles(float,float)+238↑j ...
                mov     ecx, [esp+10h]
                pop     edi
                pop     esi
                mov     randomSeed, ecx
                pop     ebx

loc_73F920:                             ; CODE XREF: CGEmitter::SpawnParticles(float,float)+18↑j
                                        ; CGEmitter::SpawnParticles(float,float)+30↑j
                pop     ebp
                add     esp, 0ACh
                retn    0Ch
    }
}

struct CGEmitter
{
    Attrib::Gen::fuelcell_emitter mEmitterDef;
    Attrib::Gen::emitteruv mTextureUVs;
    UMath::Vector4 mVel;
    UMath::Matrix4 mLocalWorld;
};


//void __fastcall CGEmitter_SpawnParticles(CGEmitter* _this, int dummy, float dt, float intensity, bool isContrail)
//{
//    Attrib::Gen::fuelcell_emitter::_LayoutStruct* mLayoutPtr; // esi
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
//    int zDebrisType; // eax
//    NGParticle* v21; // esi
//    Attrib::Gen::fuelcell_emitter::_LayoutStruct* v22; // eax
//    float LengthDelta; // edx
//    double v24; // st7
//    double v25; // st7
//    Attrib::Gen::fuelcell_emitter::_LayoutStruct* v26; // edi
//    double v27; // st7
//    double v28; // st7
//    Attrib::Gen::fuelcell_emitter::_LayoutStruct* v29; // edi
//    double v30; // st7
//    double v31; // st7
//    Attrib::Gen::fuelcell_emitter::_LayoutStruct* v32; // edi
//    double v33; // st7
//    Attrib::Gen::fuelcell_emitter::_LayoutStruct* v34; // edi
//    double v35; // st7
//    double v36; // st6
//    Attrib::Gen::fuelcell_emitter::_LayoutStruct* v37; // edi
//    double v38; // st7
//    Attrib::Gen::fuelcell_emitter::_LayoutStruct* v39; // edi
//    double v40; // st7
//    Attrib::Gen::fuelcell_emitter::_LayoutStruct* v41; // edi
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
//    float GravityDelta; // [esp-3Ch] [ebp-E8h]
//    UMath::Vector4* p_VelocityDelta; // [esp-34h] [ebp-E0h]
//    float z; // [esp-28h] [ebp-D4h]
//    float y; // [esp-20h] [ebp-CCh]
//    unsigned int v65; // [esp+0h] [ebp-ACh] BYREF
//    float v66; // [esp+4h] [ebp-A8h]
//    char v67; // [esp+9h] [ebp-A3h]
//    unsigned __int8 v68; // [esp+Ah] [ebp-A2h]
//    char v69; // [esp+Bh] [ebp-A1h]
//    float LengthStart; // [esp+Ch] [ebp-A0h]
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
//    int v82; // [esp+3Ch] [ebp-70h]
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
//        memcpy(v90, &_this->mVel.w, sizeof(v90));
//        mLayoutPtr = _this->mEmitterDef.mLayoutPtr;
//        v65 = randomSeed;
//        v78 = mLayoutPtr->Life - mLayoutPtr->Life * mLayoutPtr->LifeVariance;
//        v6 = (int)(mLayoutPtr->Colour1.x * flt_9C92F0);
//        v7 =  (int)(mLayoutPtr->Colour1.y * flt_9C92F0);
//        v8 =  (int)(mLayoutPtr->Colour1.z * flt_9C92F0);
//        v9 = mLayoutPtr->Colour1.w * flt_9C92F0;
//        v72 = v8;
//        v10 = (int)(v9);
//        if (intensity != flt_9C2478)
//        {
//            v11 = intensity * flt_9EA540;
//            if (flt_9EA540 < v11)
//                v11 = flt_9EA540;
//            v10 = (int)v11;
//        }
//        v83 = v72 | ((v7 | ((v6 | (v10 << 8)) << 8)) << 8);
//        if (intensity <= (double)flt_9C2478)
//            v12 = flt_9C2478;
//        else
//            v12 = intensity;
//        v13 = v12 * _this->mEmitterDef.mLayoutPtr->NumParticles * _this->mEmitterDef.mLayoutPtr->NumParticlesVariance;
//        if (intensity <= (double)flt_9C2478)
//            v14 = flt_9C2478;
//        else
//            v14 = intensity;
//        v15 = 0;
//        v71 = v14 * _this->mEmitterDef.mLayoutPtr->NumParticles - v13 * flt_9C2C44;
//        AttributePointer_Shim = (char*)Attrib_Instance_GetAttributePointer_Shim(0x28638D89u, 0);
//        if (!AttributePointer_Shim)
//            AttributePointer_Shim = (char*)Attrib_DefaultDataArea(1u);
//        v68 = *AttributePointer_Shim;
//        v17 = (char*)Attrib_Instance_GetAttributePointer_Shim(0xD2603865, 0);
//        if (!v17)
//            v17 = (char*)Attrib_DefaultDataArea(1u);
//        v67 = *v17;
//        v18 = (char*)Attrib_Instance_GetAttributePointer_Shim(0xE2CC8106, 0);
//        if (!v18)
//            v18 = (char*)Attrib_DefaultDataArea(1u);
//        v19 = *v18;
//        zDebrisType = _this->mEmitterDef.mLayoutPtr->zDebrisType;
//        v69 = v19;
//        v72 = zDebrisType;
//        if (zDebrisType)
//        {
//            v15 = 1;
//            v72 = zDebrisType - 1;
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
//            v22 = _this->mEmitterDef.mLayoutPtr;
//            LengthDelta = v22->LengthDelta;
//            LengthStart = v22->LengthStart;
//            v84 = LengthDelta;
//            v24 = bRandom_Float_Int(LengthDelta, (int)&v65);
//            v25 = v24 + LengthStart;
//            LengthStart = v25;
//            if (v25 < flt_9C248C)
//                break;
//            if (LengthStart >= (double)flt_9C92F0)
//                LengthStart = 255.0;
//            v26 = _this->mEmitterDef.mLayoutPtr;
//            v27 = bRandom_Float_Int(v26->VolumeCenter.x, (int)&v65);
//            v28 = v26->VolumeCenter.x - (v27 + v27);
//            v29 = _this->mEmitterDef.mLayoutPtr;
//            y = v29->VolumeCenter.y;
//            v86 = flt_9C2478 - v28;
//            v30 = bRandom_Float_Int(y, (int)&v65);
//            v31 = v29->VolumeCenter.y - (v30 + v30);
//            v32 = _this->mEmitterDef.mLayoutPtr;
//            z = v32->VolumeCenter.z;
//            v87 = flt_9C2478 - v31;
//            v33 = bRandom_Float_Int(z, (int)&v65);
//            p_VelocityDelta = &_this->mEmitterDef.mLayoutPtr->VelocityDelta;
//            v88 = flt_9C2478 - (v32->VolumeCenter.z - (v33 + v33));
//            v73 = p_VelocityDelta[-1].x * *(float*)&_this->mTextureUVs.mFlags;
//            v74 = p_VelocityDelta[-1].y * _this->mVel.x;
//            v75 = p_VelocityDelta[-1].z * _this->mVel.y;
//            ((void(__cdecl*)(UMath::Vector4*, float*, float*))sub_478200)(p_VelocityDelta, &_this->mVel.w, v89);
//            v34 = _this->mEmitterDef.mLayoutPtr;
//            GravityDelta = v34->GravityDelta;
//            v75 = v89[2] + v75;
//            v76 = v89[3] + v76;
//            v73 = (v89[0] + v73) * v86;
//            v74 = (v89[1] + v74) * v87;
//            v75 = v75 * v88;
//            v35 = bRandom_Float_Int(GravityDelta, (int)&v65);
//            v36 = v34->GravityStart - v34->GravityDelta;
//            v37 = _this->mEmitterDef.mLayoutPtr;
//            v77 = v35 + v35 + v36;
//            v38 = bRandom_Float_Int(v37->VelocityStart.x, (int)&v65) - v37->VelocityStart.x * flt_9C2888 + v37->VolumeExtent.x;
//            v39 = _this->mEmitterDef.mLayoutPtr;
//            v60 = v39->VelocityStart.y;
//            v79 = v38;
//            v40 = bRandom_Float_Int(v60, (int)&v65) - v39->VelocityStart.y * flt_9C2888 + v39->VolumeExtent.y;
//            v41 = _this->mEmitterDef.mLayoutPtr;
//            v59 = v41->VelocityStart.z;
//            v80 = v40;
//            v81 = bRandom_Float_Int(v59, (int)&v65) - v41->VelocityStart.z * flt_9C2888 + v41->VolumeExtent.z;
//            v82 = 1065353216;
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
//            v21->elasticity = (int)((Attrib::Gen::fuelcell_emitter::_LayoutStruct*)_this->mEmitterDef.mLayoutPtr)->Elasticity;
//            v51 = (int)((Attrib::Gen::fuelcell_emitter::_LayoutStruct*)_this->mEmitterDef.mLayoutPtr)->HeightStart;
//            v52 = v83;
//            v21->size = v51;
//            v21->color = v52;
//            v21->flags = v15;
//            if ((v15 & 1) != 0)
//            {
//                v21->uv[0] = v72;
//                v21->uv[1] = ((int(__usercall*)@<eax>(double@<st0>))__ftol2)(*(float*)&this->mTextureUVs.mCollection->mTable.mTableSize);
//                v21->uv[2] = ((int(__usercall*)@<eax>(double@<st0>))__ftol2)(*(float*)&this->mTextureUVs.mCollection->mTable.mWorstCollision);
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
//                v21->uv[0] = ((int(__usercall*)@<eax>(double@<st0>))__ftol2)(*(float*)&this->mTextureUVs.mCollection->mTable.mTableSize * flt_9C92F0);
//                v21->uv[1] = ((int(__usercall*)@<eax>(double@<st0>))__ftol2)(*(float*)&this->mTextureUVs.mCollection->mTable.mWorstCollision * flt_9C92F0);
//                v21->uv[2] = ((int(__usercall*)@<eax>(double@<st0>))__ftol2)(*(float*)&this->mTextureUVs.mCollection->mTable.mNumEntries * flt_9C92F0);
//                v56 = ((int(__usercall*)@<eax>(double@<st0>))__ftol2)(*(float*)&this->mTextureUVs.mCollection->mTable.mTable * flt_9C92F0);
//                v57 = LengthStart;
//                v21->uv[3] = v56;
//                v21->startX = ((int(__usercall*)@<eax>(double@<st0>))__ftol2)(v57);
//            }
//            flags = v21->flags;
//            v66 = v66 + v85;
//            if ((flags & 4) == 0 && !isContrail)
//                CalcCollisiontime_abstract(v21);
//        }
//        randomSeed = v65;
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

void __declspec(naked) CGEmitter_CGEmitter()
{
    _asm
    {
          sub esp, 10h
          mov     eax, [esp+14h]
          push    ebx
          push    esi
          push    edi
          push    0
          mov     ebx, ecx
          push    eax
          mov     [esp+14h], ebx
          call    Attrib_Gen_fuelcell_emitter_constructor_shim; Attrib::Gen::fuelcell_emitter::fuelcell_emitter(Attrib::Collection const *,uint)
          mov     ecx, [ebx+4]
          add     ecx, 60h ; '`'
          push    0
          push    ecx
          lea     ecx, [ebx+10h]
          mov     dword ptr [esp+20h], 0
          call    sub_737610_shim
          mov     eax, [esp+24h]
          lea     esi, [eax+14h]
          add     eax, 4
          lea     edi, [ebx+30h]
          mov     ecx, 10h
          rep movsd
          mov     ecx, [eax]
          lea     edx, [ebx+20h]
          mov     [edx], ecx
          mov     ecx, [eax+4]
          mov     [edx+4], ecx
          mov     ecx, [eax+8]
          mov     [edx+8], ecx
          mov     eax, [eax+0Ch]
          pop     edi
          mov     [edx+0Ch], eax
          pop     esi
          mov     eax, ebx
          pop     ebx
          add     esp, 10h
          retn    8
    }
}


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

void __declspec(naked) NGEffect_NGEffect()
{
    _asm
    {
        sub     esp, 84h
        push    ebp
        push    edi
        mov     edi, [esp + 90h]
        mov     eax, [edi + 54h]
        push    0
        mov     ebp, ecx
        push    eax
        mov[esp + 14h], ebp
        call    Attrib_Gen_fuelcell_effect_constructor; Attrib::Gen::fuelcell_effect::fuelcell_effect(Attrib::Collection const*, uint)
        cmp     dword ptr[ebp + 4], 0
        mov     dword ptr[esp + 88h], 0
        jz      loc_74A36E
        push    esi
        mov     ecx, ebp
        call    Attrib_Gen_fuelcell_effect_Num_NGEmitter; Attrib::Gen::fuelcell_effect::Num_NGEmitter(void)
        xor esi, esi
        test    eax, eax
        mov[esp + 0Ch], eax
        jle     loc_74A352
        lea     ecx, [ecx + 0]

loc_74A2C0: ; CODE XREF : NGEffect::NGEffect(XenonEffectDef const&, float) + EC↓j
        push    esi
        push    0B0D98A89h
        mov     ecx, ebp
        call    Attrib_Instance_GetAttributePointer_Shim
        test    eax, eax
        jnz     loc_74A2DB
        push    0Ch
        call    Attrib_DefaultDataArea
        add     esp, 4

loc_74A2DB: ; CODE XREF : NGEffect::NGEffect(XenonEffectDef const&, float) + 6F↑j
        mov     ecx, eax
        call    Attrib_RefSpec_GetCollection
        push    edi
        push    eax
        lea     ecx, [esp + 1Ch]
        call    CGEmitter_CGEmitter; CGEmitter::CGEmitter(Attrib::Collection const*, XenonEffectDef const&)
        lea     ecx, [esp + 14h]
        call fuelcell_emitter_bridge
        mov     eax, [edi + 58h]
        test    eax, eax
        mov     byte ptr[esp + 8Ch], 1
        jnz     loc_74A30B // jnz
        mov     ecx, [edi]
        mov     edx, [esp + 98h]
        push    1
        push    ecx
        push    edx
        jmp     loc_74A31A
; -------------------------------------------------------------------------- -

loc_74A30B: ; CODE XREF : NGEffect::NGEffect(XenonEffectDef const&, float) + 9A↑j
        mov     eax, [esp + 98h]
        push    0; char
        push    3F800000h; float
        push    eax; float

loc_74A31A: ; CODE XREF : NGEffect::NGEffect(XenonEffectDef const&, float) + A9↑j
        lea     ecx, [esp + 20h]
        call    CGEmitter_SpawnParticles; CGEmitter::SpawnParticles(float, float)
        call fuelcell_emitter_bridge_restore
        lea     ecx, [esp + 24h]
        mov     byte ptr[esp + 8Ch], 2
        call    Attrib_Instance_Dtor_Shim; Attrib::Instance::~Instance((void))
        lea     ecx, [esp + 14h]
        mov     byte ptr[esp + 8Ch], 0
        call    Attrib_Instance_Dtor_Shim; Attrib::Instance::~Instance((void))
        mov     eax, [esp + 0Ch]
        inc     esi
        cmp     esi, eax
        jl      loc_74A2C0

loc_74A352: ; CODE XREF : NGEffect::NGEffect(XenonEffectDef const&, float) + 57↑j
        mov     eax, ebp
        pop     esi

loc_74A355: ; CODE XREF : NGEffect::NGEffect(XenonEffectDef const&, float) + 110↓j
        pop     edi
        pop     ebp
        add     esp, 84h

        retn    8
loc_74A36E:
        mov     eax, ebp
        jmp     short loc_74A355
    }
}

class NGEffect
{
public:
    NGEffect(XenonEffectDef* eDef, float dt) {
        ((void(__thiscall*)(NGEffect*, XenonEffectDef*, float)) & NGEffect_NGEffect)(this, eDef, dt);
    }
    Attrib::Gen::fuelcell_effect mEffectDef;
};

void __declspec(naked) XSpriteManager_AddParticle()
{
    _asm
    {
        sub     esp, 1Ch
        push    ebx
        push    ebp
        mov     ebp, ecx
        push    esi
        mov     esi, [ebp + 0]
        push    2000h // Flags D3DLOCK_DISCARD
        lea     ecx, [esi + 1Ch]
        push    ecx // **ppbData
        mov     bl, 1
        mov[ebp + 4], bl
        mov     ecx, [esi + 0Ch]
        mov     eax, [esi]
        mov     edx, [eax]
        lea     ecx, [ecx + ecx * 2]
        shl     ecx, 5
        push    ecx // SizeToLock
        push    0 // OffsetToLock
        push    eax
        mov[esp + 20h], ebp
        call    dword ptr[edx + 2Ch]; DriverVertexBuffer::Lock
        mov     ecx, [esp + 34h]
        test    ecx, ecx
        mov     dword ptr[esi + 10h], 0
        mov[esi + 18h], bl
        jbe     loc_74EDF3
        push    edi
        mov     edi, [esp + 34h]
        lea     eax, [edi + 18h]
        mov[esp + 38h], ecx

        loc_74EC13 : ; CODE XREF : XSpriteManager::AddParticle(eView*, NGParticle const*, uint) + 22C↓j
        fld     dword ptr[eax + 1Ch]
        mov     edx, [ebp + 0]
        fld     st
        mov     esi, [edx + 10h]
        fmul    dword ptr[eax - 8]
        mov     ecx, [edx + 0Ch]
        cmp     esi, ecx
        fadd    dword ptr[edi]
        fstp    dword ptr[esp + 14h]
        fld     st
        fmul    dword ptr[eax - 4]
        fadd    dword ptr[eax - 14h]
        fstp    dword ptr[esp + 18h]
        fld     dword ptr[eax + 1Ch]
        fxch    st(1)
        fmul    dword ptr[eax]
        fadd    dword ptr[eax - 10h]
        fld     st(1)
        fmul    st, st(2)
        fmul    dword ptr[eax + 4]
        faddp   st(1), st
        fstp    dword ptr[esp + 1Ch]
        fstp    st
        jnb     loc_74EDDD
        mov     ebx, [edx + 1Ch]
        lea     ecx, [esi + esi * 2]
        shl     ecx, 5
        add     ecx, ebx
        lea     esi, [esi + 1]
        mov[edx + 10h], esi
        jz      loc_74EDDD
        mov     edx, [esp + 14h]
        mov     esi, [esp + 18h]
        mov     ebx, [esp + 1Ch]
        mov     ebp, ecx
        mov[ebp + 0], edx
        mov[ebp + 4], esi
        mov[ebp + 8], ebx
        mov     ebp, [eax - 0Ch]
        mov[ecx + 0Ch], ebp
        movzx   ebp, byte ptr[eax + 2Ch]
        mov[esp + 34h], ebp
        fild    dword ptr[esp + 34h]
        fmul    ds : flt_9C77C8
        fstp    dword ptr[ecx + 10h]
        movzx   ebp, byte ptr[eax + 2Dh]
        mov[esp + 34h], ebp
        fild    dword ptr[esp + 34h]
        fmul    ds : flt_9C77C8
        fstp    dword ptr[ecx + 14h]
        movzx   ebp, byte ptr[eax + 28h]
        mov[esp + 34h], ebp
        lea     ebp, [ecx + 18h]
        mov[ebp + 0], edx
        fild    dword ptr[esp + 34h]
        mov[ebp + 4], esi
        mov[ebp + 8], ebx
        fmul    ds : flt_9EAFFC
        fld     st
        fadd    dword ptr[ecx + 20h]
        fstp    dword ptr[ecx + 20h]
        mov     edx, [eax - 0Ch]
        mov[ecx + 24h], edx
        movzx   edx, byte ptr[eax + 2Eh]
        mov[esp + 34h], edx
        fild    dword ptr[esp + 34h]
        fmul    ds : flt_9C77C8
        fstp    dword ptr[ecx + 28h]
        movzx   edx, byte ptr[eax + 2Dh]
        mov[esp + 34h], edx
        fild    dword ptr[esp + 34h]
        fmul    ds : flt_9C77C8
        fstp    dword ptr[ecx + 2Ch]
        movzx   edx, byte ptr[eax + 29h]
        mov[esp + 34h], edx
        lea     edx, [ecx + 30h]
        mov     ebp, edx
        fild    dword ptr[esp + 34h]
        fmul    ds : flt_9EAFFC
        fadd    dword ptr[eax + 1Ch]
        fld     st
        fmul    dword ptr[eax - 8]
        fadd    dword ptr[edi]
        fstp    dword ptr[esp + 20h]
        mov     edx, [esp + 20h]
        fld     st
        fmul    dword ptr[eax - 4]
        fadd    dword ptr[eax - 14h]
        fstp    dword ptr[esp + 24h]
        mov     esi, [esp + 24h]
        fld     st
        fmul    dword ptr[eax]
        fadd    dword ptr[eax - 10h]
        fld     st(1)
        fmul    dword ptr[eax + 4]
        mov[ebp + 0], edx
        mov[ebp + 4], esi
        fmul    st, st(2)
        faddp   st(1), st
        fstp    dword ptr[esp + 28h]
        mov     ebx, [esp + 28h]
        mov[ebp + 8], ebx
        fstp    st
        fadd    dword ptr[ecx + 38h]
        fstp    dword ptr[ecx + 38h]
        mov     ebp, [eax - 0Ch]
        mov[ecx + 3Ch], ebp
        movzx   ebp, byte ptr[eax + 2Eh]
        mov[esp + 34h], ebp
        fild    dword ptr[esp + 34h]
        fmul    ds : flt_9C77C8
        fstp    dword ptr[ecx + 40h]
        movzx   ebp, byte ptr[eax + 2Fh]
        mov[esp + 34h], ebp
        lea     ebp, [ecx + 48h]
        mov[ebp + 0], edx
        fild    dword ptr[esp + 34h]
        mov[ebp + 4], esi
        mov[ebp + 8], ebx
        mov     ebp, [esp + 10h]
        fmul    ds : flt_9C77C8
        fstp    dword ptr[ecx + 44h]
        mov     edx, [eax - 0Ch]
        mov[ecx + 54h], edx
        movzx   edx, byte ptr[eax + 2Ch]
        mov[esp + 34h], edx
        fild    dword ptr[esp + 34h]
        fmul    ds : flt_9C77C8
        fstp    dword ptr[ecx + 58h]
        movzx   edx, byte ptr[eax + 2Fh]
        mov[esp + 34h], edx
        fild    dword ptr[esp + 34h]
        fmul    ds : flt_9C77C8
        fstp    dword ptr[ecx + 5Ch]

        loc_74EDDD : ; CODE XREF : XSpriteManager::AddParticle(eView*, NGParticle const*, uint) + 91↑j
        ; XSpriteManager::AddParticle(eView*, NGParticle const*, uint) + A8↑j
        mov     ecx, [esp + 38h]
        add     edi, 48h; 'H'
        add     eax, 48h; 'H'
        dec     ecx
        mov[esp + 38h], ecx
        jnz     loc_74EC13
        pop     edi

        loc_74EDF3 : ; CODE XREF : XSpriteManager::AddParticle(eView*, NGParticle const*, uint) + 41↑j
        mov     byte ptr[ebp + 4], 0
        mov     ebp, [ebp + 0]
        mov     al, [ebp + 18h]
        test    al, al
        jz      short loc_74EE0B
        cmp     dword ptr[ebp + 0], 0
        jz      short loc_74EE0B
        mov     byte ptr[ebp + 18h], 0

        loc_74EE0B:; CODE XREF : XSpriteManager::AddParticle(eView*, NGParticle const*, uint) + 23F↑j
        ; XSpriteManager::AddParticle(eView*, NGParticle const*, uint) + 245↑j
        mov     eax, [ebp + 0]
        mov     ecx, [eax]
        push    eax
        call    dword ptr[ecx + 30h]
        pop     esi
        mov     dword ptr[ebp + 1Ch], 0
        pop     ebp
        pop     ebx
        add     esp, 1Ch
        retn    0Ch
    }
}

void (__thiscall* XSpriteManager_AddParticle_Abstract)(void* _this,
        eView *view,
        NGParticle *particleList,
        const unsigned int numParticles) = (void (__thiscall*)(void*, eView*, NGParticle*, const unsigned int))&XSpriteManager_AddParticle;

//void __declspec(naked) DrawXenonEmitters(void* eView)
//{
//    _asm
//    {
//        push    ebp
//        mov     ebp, esp
//        and esp, 0FFFFFFF8h
//        sub     esp, 74h
//        mov     eax, EmitterDeltaTime
//        push    ebx
//        push    esi
//        mov     ecx, eax
//        push    edi
//        push    ecx; float
//        mov     ecx, offset gParticleList
//        mov[esp + 10h], eax
//        mov     EmitterDeltaTime, 0
//        call    ParticleList::AgeParticles
//        mov     ebx, dword ptr gNGEffectList
//        mov     edx, dword ptr gNGEffectList[4]
//        cmp     ebx, edx
//        jz      loc_754C78
//        lea     esp, [esp + 0]
//
//        loc_754C30:
//        mov     ecx, 17h
//        mov     esi, ebx
//        lea     edi, [esp + 20h]
//        rep movsd
//        mov     eax, [esp + 78h]
//        test    eax, eax
//        jz      loc_754C4F
//        mov     eax, [eax + 18h]
//        shr     eax, 4
//        test    al, 1
//        jz      loc_754C71
//
//        loc_754C4F:
//        mov     ecx, [esp + 0Ch]
//        push    ecx
//        lea     edx, [esp + 24h]
//        push    edx
//        lea     ecx, [esp + 18h]
//        call    NGEffect_NGEffect
//        lea     ecx, [esp + 10h]
//        call    Attrib_Instance_Dtor_Shim
//        mov     edx, dword ptr gNGEffectList[4]
//
//        loc_754C71:
//        add     ebx, 5Ch; '\'
//        cmp     ebx, edx
//        jnz     loc_754C30
//
//        loc_754C78 : ; CODE XREF : DrawXenonEmitters(eView*) + 3A↑j
//        mov     eax, dword ptr gNGEffectList
//        push    edx
//        push    eax
//        mov     ecx, offset gNGEffectList
//        call    eastl_vector_erase_XenonEffectDef
//        mov     eax, dword ptr [gParticleList.mNumParticles]
//        test    eax, eax
//        jz      loc_754CA6
//        mov     ecx, [ebp + 8]
//        push    eax
//        push    gParticleList.mParticles
//        push    ecx
//        mov     ecx, offset NGSpriteManager
//        call    XSpriteManager_AddParticle
//
//        loc_754CA6:; CODE XREF : DrawXenonEmitters(eView*) + A0↑j
//        pop     edi
//        pop     esi
//        pop     ebx
//        mov     esp, ebp
//        pop     ebp
//        retn
//    }
//}

void DrawXenonEmitters(eView *view)
{
    XenonEffectDef* mpBegin; // ebx
    XenonEffectDef* i; // edx
    //NGEffect effect; // [esp-4h] [ebp-84h]
    XenonEffectDef effectDef; // [esp+20h] [ebp-60h] BYREF

    gParticleList.AgeParticles(EmitterDeltaTime);
    mpBegin = gNGEffectList.mpBegin;
    for (i = gNGEffectList.mpEnd; mpBegin != i; ++mpBegin)
    {
        memcpy(&effectDef, mpBegin, sizeof(effectDef));
        if (!effectDef.piggyback_effect || (*((uint32_t*)effectDef.piggyback_effect + 6) & 0x10) != 0)
        {
            NGEffect effect{ &effectDef, EmitterDeltaTime };
            i = gNGEffectList.mpEnd;
        }
    }
    EmitterDeltaTime = 0.0f;
    eastl_vector_erase_XenonEffectDef_Abstract(&gNGEffectList, gNGEffectList.mpBegin, gNGEffectList.mpEnd);
    if (gParticleList.mNumParticles)
        XSpriteManager_AddParticle_Abstract(NGSpriteManager_thing, view, gParticleList.mParticles, gParticleList.mNumParticles);
}

// RENDERER STUFF START
void __declspec(naked) sub_743DF0()
{
    _asm
    {
        push    ebx
        push    ebp
        mov     ebp, ds:[GLOBAL_D3DDEVICE]
        push    esi
        mov     esi, ecx
        push    edi
        mov     edi, [esp + 14h]
        xor eax, eax
        push    eax // pSharedHandle
        mov[esi + 10h], eax
        mov[esi + 14h], eax
        mov[esi + 18h], al
        mov     ecx, [edi]
        push    esi // ppVertexBuffer
        push    eax // Pool
        shl     ecx, 2
        mov[esi + 8], ecx
        mov     edx, [edi]
        push    eax // FVF
        mov[esi + 0Ch], edx
        mov     eax, [edi]
        mov     ecx, [ebp + 0]
        add     eax, 3
        lea     edx, [eax + eax * 2]
        push    208h // Usage D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY
        shl     edx, 5
        push    edx // Length
        push    ebp
        call    dword ptr[ecx + 68h] // CreateVertexBuffer
        mov     eax, [edi]
        mov     ecx, [ebp + 0]
        push    0 // pSharedHandle
        lea     ebx, [esi + 4]
        push    ebx // ppIndexBuffer
        push    1 // Pool
        push    65h // Format = D3DFMT_INDEX16
        lea     edx, [eax + eax * 2]
        push    8 // Usage = D3DUSAGE_WRITEONLY
        shl     edx, 2
        push    edx // Length
        push    ebp
        call    dword ptr[ecx + 6Ch] // CreateIndexBuffer
        mov     ebx, [ebx]
        mov     eax, [ebx]
        xor ebp, ebp
        push    ebp
        lea     ecx, [esp + 18h]
        push    ecx
        push    ebp
        push    ebp
        push    ebx
        call    dword ptr[eax + 2Ch]
        test    eax, eax
        jnz     loc_743ED3
        cmp[edi], ebp
        jbe     loc_743EBC
        xor ecx, ecx
        mov     eax, 6

        loc_743E71:; CODE XREF : sub_743DF0 + CA↓j
        mov     edx, [esp + 14h]
        mov[eax + edx - 6], cx
        mov     ebx, [esp + 14h]
        lea     edx, [ecx + 1]
        mov[eax + ebx - 4], dx
        mov     ebx, [esp + 14h]
        lea     edx, [ecx + 2]
        mov[eax + ebx - 2], dx
        mov     ebx, [esp + 14h]
        mov[eax + ebx], cx
        mov     ebx, [esp + 14h]
        mov[eax + ebx + 2], dx
        mov     ebx, [esp + 14h]
        lea     edx, [ecx + 3]
        mov[eax + ebx + 4], dx
        mov     edx, [edi]
        inc     ebp
        add     eax, 0Ch
        add     ecx, 4
        cmp     ebp, edx
        jb      loc_743E71

        loc_743EBC : ; CODE XREF : sub_743DF0 + 78↑j
        mov     eax, [esi + 4]
        mov     ecx, [eax]
        push    eax
        call    dword ptr[ecx + 30h]
        pop     edi
        mov     dword ptr[esi + 1Ch], 0
        pop     esi
        pop     ebp
        pop     ebx
        retn    4

        loc_743ED3:; CODE XREF : sub_743DF0 + 74↑j
        pop     edi
        mov[esi + 1Ch], ebp
        pop     esi
        pop     ebp
        pop     ebx
        retn    4
    }
}

void __declspec(naked) InitializeRenderObj()
{
    _asm
    {
        sub esp, 4
        push    ecx
        lea     eax, [esp]
        push    eax
        mov     ecx, offset NGSpriteManager
        mov eax, MaxParticles
        mov     dword ptr[esp + 4], eax
        call    sub_743DF0
        push    0
        push    0
        push    TextureName
        call    bStringHash
        add     esp, 4
        push    eax
        call    GetTextureInfo
        mov     ecx, offset NGSpriteManager
        add     ecx, 0x14
        mov     [ecx], eax
        mov ecx, ds:[GLOBAL_D3DDEVICE]
        mov g_D3DDevice, ecx
        xor al, al
        add     esp, 14h
        retn
    }
}

//void __cdecl InitializeRenderObj()
//{
//    unsigned int v0; // eax
//
//    sub_743DF0();
//    v0 = bStringHash((char*)TextureName);
//    *(TextureInfo**)&NGSpriteManager_ClassData[20] = GetTextureInfo(v0, 0, 0);
//    g_D3DDevice = *(LPDIRECT3DDEVICE9*)GLOBAL_D3DDEVICE;
//}

void __stdcall ReleaseRenderObj()
{
    //SpriteManager* sm = (SpriteManager*)NGSpriteManager_ClassData;

    if (NGSpriteManager.sparkList.mSprintListView[0].vertex_buffer)
        NGSpriteManager.sparkList.mSprintListView[0].vertex_buffer->Release();
    if (NGSpriteManager.sparkList.mSprintListView[0].vertex_buffer)
        NGSpriteManager.sparkList.mSprintListView[0].vertex_buffer->Release();
}

void XSpriteManager::DrawBatch(eView* view)
{
    //SpriteManager* sm = (SpriteManager*)NGSpriteManager_ClassData;
    // init shader stuff here...
    uint32_t CurrentShaderObj = CURRENTSHADER_OBJ_ADDR;
    if (!bPassShadowMap)
        *(uint32_t*)CURRENTSHADER_OBJ_ADDR = *(uint32_t*)WORLDPRELITSHADER_OBJ_ADDR;
    LPD3DXEFFECT effect = *(LPD3DXEFFECT*)(*(uint32_t*)CurrentShaderObj + 0x48);

    effect->Begin(NULL, 0);
    ParticleSetTransform((D3DXMATRIX*)0x00987AB0, view->EVIEW_ID);
    effect->BeginPass(0);

    g_D3DDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    if (sparkList.mMaxSprites && gParticleList.mNumParticles)
    {
        g_D3DDevice->SetStreamSource(0, sparkList.mSprintListView[0].vertex_buffer, 0, 0x18);
        g_D3DDevice->SetIndices(sparkList.mSprintListView[0].index_buffer);

        if (sparkList.mTexture)
        {
            if (bUseD3DDeviceTexture)
            {
                LPDIRECT3DBASETEXTURE9 texMain = sparkList.mTexture->PlatInfo->pD3DTexture;
                g_D3DDevice->SetTexture(0, texMain);
                g_D3DDevice->SetRenderState(D3DRS_ALPHAREF, (DWORD)0x000000B0);
                g_D3DDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
                g_D3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                g_D3DDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
                g_D3DDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
                g_D3DDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
                g_D3DDevice->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
            }
            else
                GameSetTexture(sparkList.mTexture, 0);
        }

        effect->CommitChanges();

        g_D3DDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4 * sparkList.mMaxSprites, 0, 2 * sparkList.mMaxSprites);
    }
    g_D3DDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);

    effect->EndPass();
    effect->End();
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
    //printf("VertexBuffer: 0x%X\n", vertex_buffer);
}

// D3D Reset Hook
void __stdcall sub_6CFCE0_hook()
{
    sub_6CFCE0();
    InitializeRenderObj();
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
    InitializeRenderObj();
    return sub_6DFAF0();
}

uint32_t SparkFC = 0;
void AddXenonEffect_Spark_Hook(void* piggyback_fx, void* spec, UMath::Matrix4* mat, UMath::Vector4* vel, float intensity)
{
    if (!bLimitSparkRate)
        return AddXenonEffect(piggyback_fx, spec, mat, vel, SparkIntensity);

    if ((SparkFC + SparkFrameDelay) <= eFrameCounter)
    {
        if (SparkFC != eFrameCounter)
        {
            SparkFC = eFrameCounter;
            AddXenonEffect(piggyback_fx, spec, mat, vel, SparkIntensity);
        }
    }
}

uint32_t ContrailFC = 0;
void AddXenonEffect_Contrail_Hook(void* piggyback_fx, void* spec, UMath::Matrix4* mat, UMath::Vector4* vel, float intensity)
{
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
        float carspeed = ((sqrtf((*vel).x * (*vel).x + (*vel).y * (*vel).y + (*vel).z * (*vel).z) - ContrailSpeed)) / ContrailSpeed;
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


void __stdcall EmitterSystem_Update_Hook(float dt)
{
    uint32_t that;
    _asm mov that, ecx
    EmitterSystem_UpdateParticles((void*)that, dt);
    EmitterDeltaTime = dt;
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
    float* v3 = (float*)*((uint32_t*)CarRenderConn + 0xE);
    float v4 = sqrt(*v3 * *v3 + v3[1] * v3[1] + v3[2] * v3[2]);

    *((bool*)CarRenderConn + 0x400) = (
        (v3 = (float*)*((uint32_t*)CarRenderConn + 0xE),
        sqrt(*v3 * *v3 + v3[1] * v3[1] + v3[2] * v3[2]) >= ContrailSpeed)
        );

    if (*((bool*)PktCarService + 0x71) && bUseCGStyle)
        *((bool*)CarRenderConn + 0x400) = true;

    if (*(uint32_t*)NISINSTANCE_ADDR && !bNISContrails)
        *((bool*)CarRenderConn + 0x400) = false;

    if (*(bool*)((uint32_t)(CarRenderConn)+0x400))
        *(float*)((uint32_t)(CarRenderConn) + 0x404) = *(float*)((uint32_t)(PktCarService)+0x6C);
    else
        *(float*)((uint32_t)(CarRenderConn)+0x404) = 0;
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
