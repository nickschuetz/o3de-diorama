/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "DioramaSystemComponent.h"

#include <Clients/SpriteFeatureProcessor.h>
#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <AzFramework/Components/TransformComponent.h>

#include <Atom/RPI.Public/FeatureProcessorFactory.h>

#include <MiniAudio/MiniAudioBus.h>
#include <MiniAudio/MiniAudioConstants.h>
#include <MiniAudio/MiniAudioPlaybackBus.h>
#include <MiniAudio/SoundAsset.h>

namespace Diorama
{
    AZ_COMPONENT_IMPL(DioramaSystemComponent, "DioramaSystemComponent", DioramaSystemComponentTypeId);

    void DioramaSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        SpriteFeatureProcessor::Reflect(context);
        ReflectAudioBuses(context);
        DioramaAsepriteSheetAsset::Reflect(context);

        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaSystemComponent, AZ::Component>()->Version(0);
        }
    }

    void DioramaSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("DioramaService"));
    }

    void DioramaSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("DioramaService"));
    }

    void DioramaSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        // Activate after the Atom RPI system so FeatureProcessorFactory::Get() is
        // valid when we register the sprite feature processor. Without this the
        // editor happens to order us correctly, but standalone runtimes (the
        // GameLauncher) activate us first and Get() returns null, crashing on
        // RegisterFeatureProcessor. This mirrors how every other feature-processor
        // gem (CoreLights, Common, SkinnedMesh, ...) declares the dependency.
        required.push_back(AZ_CRC_CE("RPISystem"));
    }

    void DioramaSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    DioramaSystemComponent::DioramaSystemComponent()
    {
        if (DioramaInterface::Get() == nullptr)
        {
            DioramaInterface::Register(this);
        }
    }

    DioramaSystemComponent::~DioramaSystemComponent()
    {
        if (DioramaInterface::Get() == this)
        {
            DioramaInterface::Unregister(this);
        }
    }

    void DioramaSystemComponent::Init()
    {
    }

    void DioramaSystemComponent::Activate()
    {
        DioramaRequestBus::Handler::BusConnect();
        DioramaAudioRequestBus::Handler::BusConnect();

        // Register the sprite feature processor so each render scene can enable
        // it. Scenes that have it enabled will create an instance and call its
        // Render() every frame.
        AZ::RPI::FeatureProcessorFactory::Get()->RegisterFeatureProcessor<SpriteFeatureProcessor>();

        // Register the loader for the .dioramasheet products the .aseprite builder emits.
        m_asepriteSheetHandler = AZStd::make_unique<AzFramework::GenericAssetHandler<DioramaAsepriteSheetAsset>>(
            "Diorama Aseprite Sheet", "Diorama", "dioramasheet");
        m_asepriteSheetHandler->Register();
    }

    void DioramaSystemComponent::Deactivate()
    {
        if (m_asepriteSheetHandler)
        {
            m_asepriteSheetHandler->Unregister();
            m_asepriteSheetHandler.reset();
        }

        AZ::RPI::FeatureProcessorFactory::Get()->UnregisterFeatureProcessor<SpriteFeatureProcessor>();

        DestroyAudioPool();
        DioramaAudioRequestBus::Handler::BusDisconnect();
        DioramaRequestBus::Handler::BusDisconnect();
    }

    void DioramaSystemComponent::EnsureAudioPool()
    {
        if (!m_audioVoices.empty())
        {
            return;
        }
        // A small pool of voices, so overlapping one-shots do not cut each other off.
        constexpr int VoiceCount = 8;
        const AZ::Uuid playbackType = AZ::Uuid::CreateString(MiniAudio::MiniAudioPlaybackComponentTypeId);
        for (int i = 0; i < VoiceCount; ++i)
        {
            AZ::Entity* voice = aznew AZ::Entity(AZStd::string::format("DioramaSfxVoice_%d", i));
            voice->CreateComponent(azrtti_typeid<AzFramework::TransformComponent>());
            voice->CreateComponent(playbackType);
            voice->Init();
            voice->Activate();
            m_audioVoices.push_back(voice);
        }
    }

    void DioramaSystemComponent::DestroyAudioPool()
    {
        for (AZ::Entity* voice : m_audioVoices)
        {
            if (voice != nullptr)
            {
                if (voice->GetState() == AZ::Entity::State::Active)
                {
                    voice->Deactivate();
                }
                delete voice;
            }
        }
        m_audioVoices.clear();
        m_nextVoice = 0;
    }

    void DioramaSystemComponent::PlayOneShot(const AZStd::string& productPath, float volume)
    {
        EnsureAudioPool();
        if (m_audioVoices.empty())
        {
            return;
        }

        // Resolve the sound product path to a SoundAsset and block until it is ready,
        // so the very first play of a sound is not silent.
        const AZ::TypeId soundType = azrtti_typeid<MiniAudio::SoundAsset>();
        AZ::Data::AssetId assetId;
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(
            assetId, &AZ::Data::AssetCatalogRequests::GetAssetIdByPath, productPath.c_str(), soundType, false);
        if (!assetId.IsValid())
        {
            // Be forgiving about the path: the sound product is registered as
            // "<source>.miniaudio", so accept either the source name or the product.
            const AZStd::string withExt = productPath + "." + MiniAudio::SoundAsset::FileExtension;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(
                assetId, &AZ::Data::AssetCatalogRequests::GetAssetIdByPath, withExt.c_str(), soundType, false);
        }
        if (!assetId.IsValid())
        {
            AZ_Warning("Diorama", false, "PlayOneShot: sound '%s' not found", productPath.c_str());
            return;
        }
        auto asset = AZ::Data::AssetManager::Instance().GetAsset<MiniAudio::SoundAsset>(assetId, AZ::Data::AssetLoadBehavior::PreLoad);
        asset.BlockUntilLoadComplete();

        const AZ::EntityId voice = m_audioVoices[m_nextVoice]->GetId();
        m_nextVoice = (m_nextVoice + 1) % m_audioVoices.size();

        const float vol = AZ::GetClamp(volume, 0.0f, 1.0f);
        MiniAudio::MiniAudioPlaybackRequestBus::Event(voice, &MiniAudio::MiniAudioPlaybackRequests::Stop);
        MiniAudio::MiniAudioPlaybackRequestBus::Event(voice, &MiniAudio::MiniAudioPlaybackRequests::SetSoundAsset, asset);
        MiniAudio::MiniAudioPlaybackRequestBus::Event(voice, &MiniAudio::MiniAudioPlaybackRequests::SetLooping, false);
        MiniAudio::MiniAudioPlaybackRequestBus::Event(voice, &MiniAudio::MiniAudioPlaybackRequests::SetVolumePercentage, vol);
        MiniAudio::MiniAudioPlaybackRequestBus::Event(voice, &MiniAudio::MiniAudioPlaybackRequests::Play);
    }

    void DioramaSystemComponent::SetMasterVolume(float volume)
    {
        MiniAudio::MiniAudioRequestBus::Broadcast(&MiniAudio::MiniAudioRequests::SetGlobalVolume, AZ::GetClamp(volume, 0.0f, 1.0f));
    }

} // namespace Diorama
