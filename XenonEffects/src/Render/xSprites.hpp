
#ifndef XSPRITES_HPP
#define XSPRITES_HPP

#include <EABase/eabase.h>

#if EA_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <d3d9.h>
#include <d3dx9.h>
#include <Speed\src\GameDefs.h>
#include <xSpark.hpp>
#include <cmath>
#include <vector>

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

    void Draw(eEffect& effect, TextureInfo* pTexture)
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
                v7 = 0;
                do
                {
                    idxBuf[v7] = v6;
                    idxBuf[v7 + 1] = v6 + 1;
                    idxBuf[v7 + 2] = v6 + 2;
                    idxBuf[v7 + 3] = v6;
                    idxBuf[v7 + 4] = v6 + 2;
                    idxBuf[v7 + 5] = v6 + 3;
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
        //mNumViews = (mNumViews + 1) % NumViews;
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

    void RenderAll(eView* view);
    void AddSpark(NGParticle* particleList, unsigned int numParticles);
    void Init();
    void Reset();
    void Flip();
};

extern XSpriteManager NGSpriteManager;

#endif // XSPRITES_HPP
