/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/SpriteComponent.h>
#include <Diorama/SpriteBus.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/SerializeContext.h>

namespace Diorama
{
    SpriteComponent::SpriteComponent(const SpriteComponentConfig& config)
        : m_config(config)
    {
    }

    void SpriteComponent::Reflect(AZ::ReflectContext* context)
    {
        SpriteComponentConfig::Reflect(context);
        SpriteInfo::Reflect(context);
        ReflectSpriteBuses(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<SpriteComponent, AZ::Component>()->Version(1)->Field("Config", &SpriteComponent::m_config);

            if (auto* editContext = serializeContext->GetEditContext())
            {
                // No AppearsInAddComponentMenu: this lightweight runtime component is
                // built from EditorSpriteComponent via BuildGameEntity, never added
                // directly. Listing it in the Add Component menu collides with the
                // editor component's identical "Sprite" name, so a name lookup
                // (FindComponentTypeIdsByEntityType) could resolve this preview-less,
                // request-bus-less runtime component instead of the editor one. The
                // EditContext stays so a built game entity's config is still
                // inspectable.
                editContext->Class<SpriteComponent>("Sprite", "Draws a world-space 2D sprite through Atom")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Diorama")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &SpriteComponent::m_config, "Config", "Sprite configuration")
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly);
            }
        }
    }

    void SpriteComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("DioramaSpriteService"));
    }

    void SpriteComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("DioramaSpriteService"));
    }

    void SpriteComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void SpriteComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        AZ_UNUSED(dependent);
    }

    void SpriteComponent::Activate()
    {
        m_presenter.Connect(GetEntityId(), m_config);

        // Expose the AI request API. A verb edits m_config in place, then the
        // callback pushes it to the presenter (the same live-update path the
        // editor uses), so agent-driven changes apply immediately.
        m_requestHandler.Connect(
            GetEntityId(),
            m_config,
            m_presenter,
            [this]()
            {
                m_presenter.SetConfig(m_config);
            });

        DioramaSimStateParticipantBus::Handler::BusConnect(GetEntityId());
    }

    void SpriteComponent::Deactivate()
    {
        DioramaSimStateParticipantBus::Handler::BusDisconnect();
        m_requestHandler.Disconnect();
        m_presenter.Disconnect();
    }

    // ---- Snapshot / restore -----------------------------------------------------------
    // Chunk payload: frame (s64), frame timer (f32), finished (u8). The playback
    // position is the only mutable runtime state; the clip itself is config.

    void SpriteComponent::SaveSimState(SimState::Writer& writer)
    {
        const SpriteAnimation::FrameState state = m_presenter.GetFrameState();
        const size_t sizePos = writer.BeginChunk(SpriteChunkTag);
        writer.S64(state.m_frame);
        writer.F32(state.m_timer);
        writer.U8(state.m_finished ? 1 : 0);
        writer.EndChunk(sizePos);
    }

    bool SpriteComponent::TryRestoreChunk(AZ::u32 tag, SimState::Reader& payload)
    {
        if (tag != SpriteChunkTag)
        {
            return false;
        }
        AZ::s64 frame = 0;
        float timer = 0.0f;
        AZ::u8 finished = 0;
        if (!payload.S64(frame) || !payload.F32(timer) || !payload.U8(finished))
        {
            return false;
        }
        SpriteAnimation::FrameState state;
        state.m_frame = static_cast<int>(frame);
        state.m_timer = timer;
        state.m_finished = finished != 0;
        m_presenter.SetFrameState(state);
        return true;
    }
} // namespace Diorama
