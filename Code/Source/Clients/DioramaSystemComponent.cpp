/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "DioramaSystemComponent.h"

#include <Clients/SpriteFeatureProcessor.h>
#include <Diorama/DioramaTypeIds.h>

#include <AzCore/Serialization/SerializeContext.h>

#include <Atom/RPI.Public/FeatureProcessorFactory.h>

namespace Diorama
{
    AZ_COMPONENT_IMPL(DioramaSystemComponent, "DioramaSystemComponent", DioramaSystemComponentTypeId);

    void DioramaSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        SpriteFeatureProcessor::Reflect(context);

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

        // Register the sprite feature processor so each render scene can enable
        // it. Scenes that have it enabled will create an instance and call its
        // Render() every frame.
        AZ::RPI::FeatureProcessorFactory::Get()->RegisterFeatureProcessor<SpriteFeatureProcessor>();
    }

    void DioramaSystemComponent::Deactivate()
    {
        AZ::RPI::FeatureProcessorFactory::Get()->UnregisterFeatureProcessor<SpriteFeatureProcessor>();

        DioramaRequestBus::Handler::BusDisconnect();
    }

} // namespace Diorama
