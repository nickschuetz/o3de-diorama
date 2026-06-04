/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/DioramaLookComponent.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <Atom/Feature/PostProcess/Bloom/BloomSettingsInterface.h>
#include <Atom/Feature/PostProcess/PostProcessFeatureProcessorInterface.h>
#include <Atom/Feature/PostProcess/PostProcessSettingsInterface.h>
#include <Atom/Feature/PostProcess/Vignette/VignetteSettingsInterface.h>
#include <Atom/RPI.Public/Scene.h>

namespace Diorama
{
    namespace
    {
        float LookNonNeg(float v)
        {
            return v < 0.0f ? 0.0f : v;
        }
        float LookClamp01(float v)
        {
            return AZ::GetClamp(v, 0.0f, 1.0f);
        }
    } // namespace

    void DioramaLookConfig::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaLookConfig>()
                ->Version(1)
                ->Field("bloomEnabled", &DioramaLookConfig::m_bloomEnabled)
                ->Field("bloomThreshold", &DioramaLookConfig::m_bloomThreshold)
                ->Field("bloomKnee", &DioramaLookConfig::m_bloomKnee)
                ->Field("bloomIntensity", &DioramaLookConfig::m_bloomIntensity)
                ->Field("vignetteEnabled", &DioramaLookConfig::m_vignetteEnabled)
                ->Field("vignetteIntensity", &DioramaLookConfig::m_vignetteIntensity);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<DioramaLookConfig>("2D Look Config", "Tuned bloom + vignette for a 2D/2.5D scene")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Bloom (glow)")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox, &DioramaLookConfig::m_bloomEnabled, "Bloom", "Glow on bright/emissive pixels")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Default,
                        &DioramaLookConfig::m_bloomThreshold,
                        "Threshold",
                        "HDR brightness above which a pixel glows (1.0 pairs with sprite emissive)")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->DataElement(
                        AZ::Edit::UIHandlers::Slider, &DioramaLookConfig::m_bloomKnee, "Knee", "Softness of the threshold edge, 0..1")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 1.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &DioramaLookConfig::m_bloomIntensity, "Intensity", "Bloom strength")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Vignette")
                    ->DataElement(
                        AZ::Edit::UIHandlers::CheckBox, &DioramaLookConfig::m_vignetteEnabled, "Vignette", "Darken the screen edges")
                    ->DataElement(
                        AZ::Edit::UIHandlers::Slider, &DioramaLookConfig::m_vignetteIntensity, "Intensity", "Vignette strength, 0..1")
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 1.0f);
            }
        }
    }

    DioramaLookComponent::DioramaLookComponent(const DioramaLookConfig& config)
        : m_config(config)
    {
    }

    void DioramaLookComponent::Reflect(AZ::ReflectContext* context)
    {
        DioramaLookConfig::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DioramaLookComponent, AZ::Component>()->Version(1)->Field("Config", &DioramaLookComponent::m_config);
        }

        ReflectLookBuses(context);
    }

    void DioramaLookComponent::Activate()
    {
        DioramaLookRequestBus::Handler::BusConnect(GetEntityId());

        // The scene's feature processor may not exist yet at activation. Apply now if
        // it does; otherwise retry on tick until it succeeds, then stop ticking.
        if (!ApplyLook())
        {
            AZ::TickBus::Handler::BusConnect();
        }
    }

    void DioramaLookComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        DioramaLookRequestBus::Handler::BusDisconnect();
        RemoveLook();
    }

    void DioramaLookComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (ApplyLook())
        {
            AZ::TickBus::Handler::BusDisconnect();
        }
    }

    bool ApplyLookToScene(AZ::EntityId entityId, const DioramaLookConfig& config)
    {
        AZ::RPI::Scene* scene = AZ::RPI::Scene::GetSceneForEntityId(entityId);
        if (scene == nullptr)
        {
            return false;
        }
        auto* postProcess = scene->GetFeatureProcessor<AZ::Render::PostProcessFeatureProcessorInterface>();
        if (postProcess == nullptr)
        {
            return false;
        }

        AZ::Render::PostProcessSettingsInterface* settings = postProcess->GetOrCreateSettingsInterface(entityId);
        if (settings == nullptr)
        {
            return false;
        }

        if (AZ::Render::BloomSettingsInterface* bloom = settings->GetOrCreateBloomSettingsInterface())
        {
            bloom->SetEnabled(config.m_bloomEnabled);
            bloom->SetThreshold(LookNonNeg(config.m_bloomThreshold));
            bloom->SetKnee(LookClamp01(config.m_bloomKnee));
            bloom->SetIntensity(LookNonNeg(config.m_bloomIntensity));
            bloom->OnConfigChanged();
        }

        if (AZ::Render::VignetteSettingsInterface* vignette = settings->GetOrCreateVignetteSettingsInterface())
        {
            vignette->SetEnabled(config.m_vignetteEnabled);
            vignette->SetIntensity(LookClamp01(config.m_vignetteIntensity));
            vignette->OnConfigChanged();
        }

        settings->OnConfigChanged();
        return true;
    }

    void RemoveLookFromScene(AZ::EntityId entityId)
    {
        if (AZ::RPI::Scene* scene = AZ::RPI::Scene::GetSceneForEntityId(entityId))
        {
            if (auto* postProcess = scene->GetFeatureProcessor<AZ::Render::PostProcessFeatureProcessorInterface>())
            {
                postProcess->RemoveSettingsInterface(entityId);
            }
        }
    }

    bool DioramaLookComponent::ApplyLook()
    {
        m_applied = ApplyLookToScene(GetEntityId(), m_config);
        return m_applied;
    }

    void DioramaLookComponent::RemoveLook()
    {
        if (!m_applied)
        {
            return;
        }
        RemoveLookFromScene(GetEntityId());
        m_applied = false;
    }

    void DioramaLookComponent::SetBloomEnabled(bool enabled)
    {
        m_config.m_bloomEnabled = enabled;
        ApplyLook();
    }

    void DioramaLookComponent::SetBloomThreshold(float threshold)
    {
        m_config.m_bloomThreshold = LookNonNeg(threshold);
        ApplyLook();
    }

    void DioramaLookComponent::SetBloomIntensity(float intensity)
    {
        m_config.m_bloomIntensity = LookNonNeg(intensity);
        ApplyLook();
    }

    void DioramaLookComponent::SetVignetteEnabled(bool enabled)
    {
        m_config.m_vignetteEnabled = enabled;
        ApplyLook();
    }

    void DioramaLookComponent::SetVignetteIntensity(float intensity)
    {
        m_config.m_vignetteIntensity = LookClamp01(intensity);
        ApplyLook();
    }
} // namespace Diorama
