// NFS Most Wanted - Xenon Effects implementation
// A port of the XenonEffect particle effect emitters from NFS Carbon
// by Xan/Tenjoin

// BUG LIST:
// - particles stay in the world after restart - MAKE A XENON EFFECT RESET
//

#include "stdio.h"
#include <injector\injector.hpp>
#include <mINI\src\mini\ini.h>
#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")
#include <d3dx9.h>
#include <UMath/UMath.h>
#include <Attrib/Attrib.h>
#include <Speed/src/GameDefs.h>
#include <xSpark.hpp>
#include <Render/xSprites.hpp>
using namespace std;

#pragma runtime_checks( "", off )

// uncomment to enable contrail test near the SkipFE start location in MW's map (next to car lot in College/Rosewood)
//#define CONTRAIL_TEST

LPDIRECT3DDEVICE9& g_D3DDevice = *(LPDIRECT3DDEVICE9*)0x982BDC;

bool bContrails = true;
bool bLimitContrailRate = true;
bool bLimitSparkRate = true;
bool bNISContrails = false;
bool bUseCGStyle = false;
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

void __stdcall LoadResourceFile(char* filename, int ResType, int unk1, void* unk2, void* unk3, int unk4, int unk5)
{
    ResourceFile_BeginLoading(CreateResourceFile(filename, ResType, unk1, unk4, unk5), unk2, unk3);
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

#if _DEBUG
void* bMalloc(int size, const char* debug_text, int debug_line, int allocation_params) {
    return bWareMalloc(size, debug_text, debug_line, allocation_params);
}
#else
void* bMalloc(int size, int allocation_params) {
    return bWareMalloc(size, NULL, 0, allocation_params);
}
#endif

#if _DEBUG
#define bMALLOC(size, debug_text, debug_line, allocation_params) bMalloc((size), (debug_text), (debug_line), (allocation_params))
#else
#define bMALLOC(size, debug_text, debug_line, allocation_params) bMalloc((size), (allocation_params))
#endif

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* operator new(size_t size)
{
    return bMALLOC(size, NULL, 0, NULL);
}

void operator delete(void* ptr)
{
    bFree(ptr);
}

void* operator new[](size_t size, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return bMALLOC(size, pName, line, flags);
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName,
    int flags, unsigned debugFlags, const char* file, int line)
{
    return bMALLOC(size, pName, line, flags);
}

// note: unk_9D7880 == unk_8A3028

void ReleaseRenderObj()
{
    NGSpriteManager.Reset();
}

void __stdcall EmitterSystem_Render_Hook(eView* view)
{
    void* that;
    _asm mov that, ecx

    EmitterSystem_Render(that, view);
    if (*(uint32_t*)GAMEFLOWSTATUS_ADDR == 6)
    {
        NGSpriteManager.RenderAll(view);
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
    gNGEffectList.reserve(MAX_NGEMITTERS);
    return false;
}

uint32_t sub_6DFAF0_hook()
{
    NGSpriteManager.Init();
    return sub_6DFAF0();
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

        
        lea     ecx, [edi+60h]
        push    ecx
        lea     edx, [edi+20h]
        push    edx
        push    eax
        mov     eax, [edi+8Ch]
        push    eax
        call    AddXenonEffect
        add     esp, 10h

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

                mov     esi, eax
                mov     eax, [edi+38h]
                mov     ecx, [edi+34h]
                push    eax
                push    ecx

//loc_7E1397:                             ; CODE XREF: sub_7E1160+1DD↑j
                push    esi
                push    0
                call    AddXenonEffect; AddXenonEffect(AcidEffect *,Attrib::Collection const *,UMath::Matrix4 const *,UMath::Vector4 const *,float)
                mov     esi, [ebp+8]
                add     esp, 10h

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

    emitContrails = (*((bool*)PktCarService + 0x71) || length >= ContrailSpeed) && (!*(uint32_t*)NISINSTANCE_ADDR || bNISContrails);

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
            MAX_NGPARTICLES = std::stol(ini["Limits"]["MaxParticles"]);
        if (ini["Limits"].has("NGEffectListSize"))
            MAX_NGEMITTERS = std::stol(ini["Limits"]["NGEffectListSize"]);
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
    float dT = ((float(__cdecl*)(unsigned int, unsigned int))0x45CE70)(*(unsigned int*)0x925830, *(unsigned int*)0x925834);
    
    if (gElapsedSparkTime >= 1000.0f / SparkTargetFPS)
        gElapsedSparkTime = 0.0f;

    if (gElapsedContrailTime >= 1000.0f / ContrailTargetFPS)
        gElapsedContrailTime = 0.0f;

    gElapsedSparkTime += dT;
    gElapsedContrailTime += dT;
}

int Init()
{
    // allocate for effect list
    gParticleList.mParticles = (NGParticle*)calloc(MAX_NGPARTICLES, sizeof(NGParticle));
    
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

bool (*CheckMultipleInstance)(const char*, int);
static bool EarlyInitializeEverythingHook(const char* a, int b)
{
    bool result = CheckMultipleInstance(a, b);
    InitConfig();
    Init();
    return result;
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
        // defer mod init until the game itself has initialized so we can use bWare
        CheckMultipleInstance = injector::MakeCALL(0x666597, EarlyInitializeEverythingHook, true).get();
	}
	return TRUE;
}
