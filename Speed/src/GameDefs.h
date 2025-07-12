
// functions in the game and stuff

#ifndef GAMEDEFS_H
#define GAMEDEFS_H

#include <d3d9.h>
#include <cstdint>
#include <UMath/UMath.h>
#include <d3dx9.h>

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

class bList {
    bNode Head;
};

template <class T> class bTList : bList {};

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

struct eView
{
    void* PlatInfo;
    uint32_t EVIEW_ID;
};


extern LPDIRECT3DDEVICE9& g_D3DDevice;

inline void*(__thiscall* FastMem_Alloc)(void* FastMem, unsigned int bytes, char* kind) = (void*(__thiscall*)(void*, unsigned int, char*))0x005D29D0;
inline void* (__thiscall* FastMem_Free)(void* FastMem, void* ptr, unsigned int bytes, char* kind) = (void* (__thiscall*)(void*, void*, unsigned int, char*))0x005D0370;
inline void (__stdcall* __CxxThrowException)(int arg1, int arg2) = (void (__stdcall*)(int, int))0x007C56B0;
inline void* (__thiscall* Attrib_Instance_MW)(void* Attrib, void* AttribCollection, unsigned int unk, void* ucomlist) = (void* (__thiscall*)(void*, void*, unsigned int, void*))0x00452380;
inline void*(__cdecl* Attrib_DefaultDataArea)(unsigned int size) = (void*(__cdecl*)(unsigned int))0x006269B0;
inline void* (__thiscall* Attrib_Instance_Get)(void* AttribCollection, unsigned int unk, unsigned int hash) = (void* (__thiscall*)(void*, unsigned int, unsigned int))0x004546C0;
inline void* (__thiscall* Attrib_Attribute_GetLength)(void* AttribCollection) = (void* (__thiscall*)(void*))0x00452D40;
inline void* (__thiscall* Attrib_Dtor)(void* AttribCollection) = (void* (__thiscall*)(void*))0x00452BD0;
inline void* (__thiscall* Attrib_Instance_GetAttributePointer)(void* AttribCollection, unsigned int hash, unsigned int unk) = (void* (__thiscall*)(void*, unsigned int, unsigned int))0x00454810;
inline void* (__thiscall* Attrib_RefSpec_GetCollection)(void* Attrib) = (void* (__thiscall*)(void*))0x004560D0;
inline void* (__thiscall* Attrib_Instance_Dtor)(void* AttribInstance) = (void* (__thiscall*)(void*))0x0045A430;
inline void* (__thiscall* Attrib_Instance_Refspec)(void* AttribCollection, void* refspec, unsigned int unk, void* ucomlist) = (void* (__thiscall*)(void*, void*, unsigned int, void*))0x00456CB0;
inline void* (__cdecl* Attrib_FindCollection)(uint32_t param1, uint32_t param2) = (void* (__cdecl*)(uint32_t, uint32_t))0x00455FD0;
inline float (__cdecl* bRandom_Float_Int)(float range, unsigned int * seed) = (float (__cdecl*)(float, unsigned int*))0x0045D9E0;
inline int(__cdecl* bRandom_Int_Int)(int range, unsigned int* seed) = (int(__cdecl*)(int, unsigned int*))0x0045D9A0;
inline unsigned int(__cdecl* bStringHash)(const char* str) = (unsigned int(__cdecl*)(const char*))0x00460BF0;
inline TextureInfo*(__cdecl* GetTextureInfo)(unsigned int name_hash, int return_default_texture_if_not_found, int include_unloaded_textures) = (TextureInfo*(__cdecl*)(unsigned int, int, int))0x00503400;
inline void (__thiscall* EmitterSystem_UpdateParticles)(void* EmitterSystem, float dt) = (void (__thiscall*)(void*, float))0x00508C30;
inline void(__thiscall* EmitterSystem_Render)(void* EmitterSystem, void* eView) = (void(__thiscall*)(void*, void*))0x00503D00;
inline void(__stdcall* sub_7286D0)() = (void(__stdcall*)())0x007286D0;
inline void* (__cdecl* FastMem_CoreAlloc)(uint32_t size, char* debug_line) = (void* (__cdecl*)(uint32_t, char*))0x00465A70;
inline void(__stdcall* sub_739600)() = (void(__stdcall*)())0x739600;
inline void(__thiscall* CarRenderConn_UpdateEngineAnimation)(void* CarRenderConn, float param1, void* PktCarService) = (void(__thiscall*)(void*, float, void*))0x00745F20;
inline void(__stdcall* sub_6CFCE0)() = (void(__stdcall*)())0x6CFCE0;
inline void(__stdcall* sub_6CFCE0_2)() = (void(__stdcall*)())0x6CFCE0;
inline void(__cdecl* ParticleSetTransform)(UMath::Matrix4* worldmatrix, uint32_t EVIEW_ID) = (void(__cdecl*)(UMath::Matrix4*, uint32_t))0x6C8000;
inline bool(__thiscall* WCollisionMgr_CheckHitWorld)(void* WCollisionMgr, UMath::Vector4* inputSeg, void* cInfo, uint32_t primMask) = (bool(__thiscall*)(void*, UMath::Vector4*, void*, uint32_t))0x007854B0;
inline void(__cdecl* GameSetTexture)(void* TextureInfo, uint32_t unk) = (void(__cdecl*)(void*, uint32_t))0x006C68B0;
inline void* (*CreateResourceFile)(char* filename, int ResFileType, int unk1, int unk2, int unk3) = (void* (*)(char*, int, int, int, int))0x0065FD30;
inline void(__thiscall* ResourceFile_BeginLoading)(void* ResourceFile, void* callback, void* unk) = (void(__thiscall*)(void*, void*, void*))0x006616F0;
inline void(*ServiceResourceLoading)() = (void(*)())0x006626B0;
inline uint32_t(__stdcall* sub_6DFAF0)() = (uint32_t(__stdcall*)())0x6DFAF0;
inline uint32_t(* Attrib_StringHash32)(const char* k) = (uint32_t(*)(const char*))0x004519D0;

class WCollisionMgr
{
public:
    class WorldCollisionInfo
    {
    public:
        UMath::Vector4 fCollidePt;
        UMath::Vector4 fNormal;
        int pad[16];
        WorldCollisionInfo()
        {
            ((void(__thiscall*)(WCollisionMgr::WorldCollisionInfo*))0x4048C0)(this);
        }
    };

    unsigned int fSurfaceExclusionMask = 0;
    unsigned int fPrimitiveMask = 3;

    WCollisionMgr(unsigned int surfaceExclMask, unsigned int primitiveExclMask)
    {
        fSurfaceExclusionMask = surfaceExclMask;
        (void)primitiveExclMask;
    }

    bool CheckHitWorld(UMath::Vector4* inputSeg, WCollisionMgr::WorldCollisionInfo* cInfo, unsigned int primMask)
    {
        return WCollisionMgr_CheckHitWorld(this, inputSeg, cInfo, primMask);
    }
};

struct EmitterGroup : bTNode<EmitterGroup>
{
    class bList mEmitters;
    unsigned int mGroupKey;
    unsigned int Padding;
    unsigned int mFlags;
    unsigned __int16 mNumEmitters;
    unsigned __int16 mSectionNumber;
    struct UMath::Matrix4 mLocalWorld;
    void* mSubscriber;
    float mFarClip;
    float mIntensity;
    void(__cdecl* mDeleteCallback)(void*, struct EmitterGroup*);
    void* mDynamicData;
    unsigned int mNumZeroParticleFrames;
    unsigned int mCreationTimeStamp;
    unsigned int pad;
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

// TODO - replace this with real EASTL
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
    class vector : public VectorBase<T, Allocator> {};
}

#endif // GAMEDEFS_H