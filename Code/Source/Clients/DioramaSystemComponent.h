/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/DioramaAsepriteSheetAsset.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzFramework/Asset/GenericAssetHandler.h>
#include <Diorama/DioramaAudioBus.h>
#include <Diorama/DioramaBus.h>

namespace Diorama
{
    //! System component for the Diorama runtime. Its sole runtime job is to
    //! register the SpriteFeatureProcessor with Atom's feature processor factory
    //! so each render scene can enable it; per-sprite drawing then lives entirely
    //! in the feature processor. It holds no renderer state of its own.
    class DioramaSystemComponent
        : public AZ::Component
        , protected DioramaRequestBus::Handler
        , protected DioramaAudioRequestBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(DioramaSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        DioramaSystemComponent();
        ~DioramaSystemComponent();

    protected:
        // AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;

        // DioramaAudioRequests
        void PlayOneShot(const AZStd::string& productPath, float volume) override;
        void SetMasterVolume(float volume) override;

    private:
        //! Lazily build the fire-and-forget voice pool (entities each carrying a
        //! MiniAudio Playback component) on first use, and tear it down on Deactivate.
        void EnsureAudioPool();
        void DestroyAudioPool();

        AZStd::vector<AZ::Entity*> m_audioVoices; //!< owned voice-pool entities
        size_t m_nextVoice = 0; //!< round-robin index into m_audioVoices

        //! Loads the .dioramasheet product the .aseprite builder emits.
        AZStd::unique_ptr<AzFramework::GenericAssetHandler<DioramaAsepriteSheetAsset>> m_asepriteSheetHandler;
    };

} // namespace Diorama
