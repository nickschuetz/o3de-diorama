/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "DioramaModuleInterface.h"
#include <AzCore/Memory/Memory.h>

#include <Diorama/DioramaTypeIds.h>

#include <Clients/Collider2DComponent.h>
#include <Clients/Collision2DSystemComponent.h>
#include <Clients/DioramaCRTComponent.h>
#include <Clients/DioramaCamera2DComponent.h>
#include <Clients/DioramaLightComponent.h>
#include <Clients/DioramaLookComponent.h>
#include <Clients/DioramaParallaxComponent.h>
#include <Clients/DioramaSkeletalClipComponent.h>
#include <Clients/DioramaSystemComponent.h>
#include <Clients/DioramaUIComponent.h>
#include <Clients/ParticleEmitterComponent.h>
#include <Clients/SpriteComponent.h>
#include <Clients/TilemapComponent.h>

namespace Diorama
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(DioramaModuleInterface, "DioramaModuleInterface", DioramaModuleInterfaceTypeId);
    AZ_RTTI_NO_TYPE_INFO_IMPL(DioramaModuleInterface, AZ::Module);
    AZ_CLASS_ALLOCATOR_IMPL(DioramaModuleInterface, AZ::SystemAllocator);

    DioramaModuleInterface::DioramaModuleInterface()
    {
        // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
        // Add ALL components descriptors associated with this gem to m_descriptors.
        // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
        // This happens through the [MyComponent]::Reflect() function.
        m_descriptors.insert(
            m_descriptors.end(),
            {
                DioramaSystemComponent::CreateDescriptor(),
                Collision2DSystemComponent::CreateDescriptor(),
                SpriteComponent::CreateDescriptor(),
                TilemapComponent::CreateDescriptor(),
                Collider2DComponent::CreateDescriptor(),
                DioramaLightComponent::CreateDescriptor(),
                DioramaCamera2DComponent::CreateDescriptor(),
                ParticleEmitterComponent::CreateDescriptor(),
                DioramaParallaxComponent::CreateDescriptor(),
                DioramaUIComponent::CreateDescriptor(),
                DioramaCRTComponent::CreateDescriptor(),
                DioramaLookComponent::CreateDescriptor(),
                DioramaSkeletalClipComponent::CreateDescriptor(),
            });
    }

    AZ::ComponentTypeList DioramaModuleInterface::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<DioramaSystemComponent>(),
            azrtti_typeid<Collision2DSystemComponent>(),
        };
    }
} // namespace Diorama
