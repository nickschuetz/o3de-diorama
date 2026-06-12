/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include "DioramaEditorSystemComponent.h"
#include <Builders/DioramaBuilderComponent.h>
#include <Clients/Collision2DSystemComponent.h>
#include <Diorama/DioramaTypeIds.h>
#include <DioramaModuleInterface.h>
#include <Tools/EditorCollider2DComponent.h>
#include <Tools/EditorDioramaAnimStateMachineComponent.h>
#include <Tools/EditorDioramaAsepriteComponent.h>
#include <Tools/EditorDioramaBulletEmitterComponent.h>
#include <Tools/EditorDioramaCRTComponent.h>
#include <Tools/EditorDioramaCamera2DComponent.h>
#include <Tools/EditorDioramaDayNightComponent.h>
#include <Tools/EditorDioramaDepthBodyComponent.h>
#include <Tools/EditorDioramaHitboxComponent.h>
#include <Tools/EditorDioramaInputComponent.h>
#include <Tools/EditorDioramaLightComponent.h>
#include <Tools/EditorDioramaLookComponent.h>
#include <Tools/EditorDioramaParallaxComponent.h>
#include <Tools/EditorDioramaSkeletalClipComponent.h>
#include <Tools/EditorParticleEmitterComponent.h>
#include <Tools/EditorSpriteComponent.h>
#include <Tools/EditorTilemapComponent.h>

namespace Diorama
{
    class DioramaEditorModule : public DioramaModuleInterface
    {
    public:
        AZ_RTTI(DioramaEditorModule, DioramaEditorModuleTypeId, DioramaModuleInterface);
        AZ_CLASS_ALLOCATOR(DioramaEditorModule, AZ::SystemAllocator);

        DioramaEditorModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            // Add ALL components descriptors associated with this gem to m_descriptors.
            // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and
            // EditContext. This happens through the [MyComponent]::Reflect() function.
            m_descriptors.insert(
                m_descriptors.end(),
                {
                    DioramaEditorSystemComponent::CreateDescriptor(),
                    EditorSpriteComponent::CreateDescriptor(),
                    EditorTilemapComponent::CreateDescriptor(),
                    EditorCollider2DComponent::CreateDescriptor(),
                    EditorDioramaLightComponent::CreateDescriptor(),
                    EditorDioramaCamera2DComponent::CreateDescriptor(),
                    EditorParticleEmitterComponent::CreateDescriptor(),
                    EditorDioramaParallaxComponent::CreateDescriptor(),
                    EditorDioramaCRTComponent::CreateDescriptor(),
                    EditorDioramaLookComponent::CreateDescriptor(),
                    EditorDioramaSkeletalClipComponent::CreateDescriptor(),
                    EditorDioramaInputComponent::CreateDescriptor(),
                    EditorDioramaHitboxComponent::CreateDescriptor(),
                    EditorDioramaBulletEmitterComponent::CreateDescriptor(),
                    EditorDioramaDepthBodyComponent::CreateDescriptor(),
                    EditorDioramaDayNightComponent::CreateDescriptor(),
                    EditorDioramaAnimStateMachineComponent::CreateDescriptor(),
                    EditorDioramaAsepriteComponent::CreateDescriptor(),
                    DioramaBuilderComponent::CreateDescriptor(),
                });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         * Non-SystemComponents should not be added here
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<DioramaEditorSystemComponent>(),
                // Run the collision world in the editor too, so colliders simulate
                // in play-in-editor (the runtime module supplies it in launchers).
                azrtti_typeid<Collision2DSystemComponent>(),
                // Note: DioramaBuilderComponent is intentionally NOT required here. The
                // AssetProcessor's builder app activates it via its AssetBuilder
                // system-component tag (set in its Reflect); it only needs its descriptor
                // registered (in the descriptor list above).
            };
        }
    };
} // namespace Diorama

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor), Diorama::DioramaEditorModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_Diorama_Editor, Diorama::DioramaEditorModule)
#endif
