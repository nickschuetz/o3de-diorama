/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Builders/DioramaAsepriteBuilder.h>
#include <Builders/DioramaTilemapBuilder.h>

#include <AzCore/Component/Component.h>

namespace Diorama
{
    //! System component that registers Diorama's asset builders. It has no editor or
    //! runtime service dependencies, so it activates in the AssetProcessor's builder
    //! application (unlike the editor system component, which needs editor services).
    //! Listed in the editor module's required system components.
    class DioramaBuilderComponent final : public AZ::Component
    {
    public:
        AZ_COMPONENT(Diorama::DioramaBuilderComponent, "{3F1B6E8A-2C4D-4E9F-A1B2-C3D4E5F60718}");

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

    protected:
        void Activate() override;
        void Deactivate() override;

    private:
        DioramaAsepriteBuilder m_asepriteBuilder;
        DioramaTilemapBuilder m_tilemapBuilder;
    };
} // namespace Diorama
