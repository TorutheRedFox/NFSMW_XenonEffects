
#include "xSprites.hpp"

XSpriteManager NGSpriteManager;

void XSpriteManager::Init()
{
    sparkList.Init(MAX_NGPARTICLES);
    sparkList.mTexture = GetTextureInfo(bStringHash("MAIN"), 0, 0);
}

void XSpriteManager::Reset()
{
    sparkList.Reset();
}

void XSpriteManager::Flip()
{
    sparkList.mNumViews = 0;
    //sparkList.mSprintListView[0].mCurrentBuffer = (sparkList.mSprintListView[0].mCurrentBuffer + 1) % 3;
    //sparkList.mSprintListView[1].mCurrentBuffer = (sparkList.mSprintListView[1].mCurrentBuffer + 1) % 3;
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
            XSpark* spark = &sparkList.mSprintListView[sparkList.mCurrViewBuffer].mLockedVB[i];

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

void XSpriteManager::RenderAll(eView* view)
{
    *(eEffect**)CURRENTSHADER_OBJ_ADDR = *(eEffect**)WORLDPRELITSHADER_OBJ_ADDR;

    eEffect& effect = **(eEffect**)CURRENTSHADER_OBJ_ADDR;

    effect.Start();

    effect.mEffectHandlePlat->Begin(NULL, 0);
    ParticleSetTransform((UMath::Matrix4*)0x987AB0, view->EVIEW_ID);
    effect.mEffectHandlePlat->BeginPass(0);

    g_D3DDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

    sparkList.Draw(view->EVIEW_ID, 0, effect, NULL);

    g_D3DDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);

    effect.mEffectHandlePlat->EndPass();
    effect.mEffectHandlePlat->End();

    effect.End();

    *(eEffect**)CURRENTSHADER_OBJ_ADDR = NULL;
}
