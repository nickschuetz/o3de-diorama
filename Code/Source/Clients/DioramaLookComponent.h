/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Diorama/DioramaLookBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>

namespace Diorama
{
    class DioramaLookConfig;

    //! Apply a look config to the PostProcessFeatureProcessor of the scene that owns
    //! entityId (per-entity post settings = a PostFxLayer). Returns false if the scene
    //! or feature processor is not available yet. Shared by the runtime component and
    //! the editor twin, so the editor previews the exact same result it will export.
    bool ApplyLookToScene(AZ::EntityId entityId, const class DioramaLookConfig& config);
    //! Remove entityId's post settings from its scene's feature processor.
    void RemoveLookFromScene(AZ::EntityId entityId);

    //! Configuration for the 2D post-processing "look". Tuned for a 2D/2.5D scene:
    //! the defaults give a gentle glow on bright/emissive sprites plus a soft edge
    //! vignette, the look you would otherwise hand-build with Atom's PostFxLayer +
    //! Bloom + Vignette.
    class DioramaLookConfig final : public AZ::ComponentConfig
    {
    public:
        AZ_RTTI(DioramaLookConfig, DioramaLookConfigTypeId, AZ::ComponentConfig);
        AZ_CLASS_ALLOCATOR(DioramaLookConfig, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);
        ~DioramaLookConfig() override = default;

        //! HDR bloom (glow). Keys on brightness above m_bloomThreshold.
        bool m_bloomEnabled = true;
        //! Pixels brighter than this bloom (HDR). 1.0 = only above-white pixels,
        //! which pairs with the sprite emissive channel.
        float m_bloomThreshold = 1.0f;
        //! Softness of the threshold edge, 0..1.
        float m_bloomKnee = 0.5f;
        //! Bloom strength.
        float m_bloomIntensity = 0.5f;

        //! Edge darkening.
        bool m_vignetteEnabled = true;
        //! Vignette strength, 0..1.
        float m_vignetteIntensity = 0.3f;
    };

    //! Runtime 2D post-processing look. Applies the config to Atom's stock
    //! PostProcessFeatureProcessor (per-entity post settings = a PostFxLayer), so it
    //! composes with any other post content in the scene and reuses the engine's
    //! bloom/vignette exactly. Removes its settings on deactivate (no leaked layer).
    //! Driven at runtime through DioramaLookRequestBus for AI/human parity.
    class DioramaLookComponent final
        : public AZ::Component
        , protected AZ::TickBus::Handler
        , protected DioramaLookRequestBus::Handler
    {
    public:
        AZ_COMPONENT(Diorama::DioramaLookComponent, DioramaLookComponentTypeId);

        static void Reflect(AZ::ReflectContext* context);

        DioramaLookComponent() = default;
        explicit DioramaLookComponent(const DioramaLookConfig& config);
        ~DioramaLookComponent() override = default;

    protected:
        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AZ::TickBus (only used to retry applying once the scene/FP is ready)
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // DioramaLookRequests
        void SetBloomEnabled(bool enabled) override;
        void SetBloomThreshold(float threshold) override;
        void SetBloomIntensity(float intensity) override;
        void SetVignetteEnabled(bool enabled) override;
        void SetVignetteIntensity(float intensity) override;

    private:
        //! Push the current config into the scene's PostProcessFeatureProcessor.
        //! Returns false if the feature processor is not available yet.
        bool ApplyLook();
        //! Remove this entity's post settings from the feature processor.
        void RemoveLook();

        DioramaLookConfig m_config;
        bool m_applied = false;
    };
} // namespace Diorama
