/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <Clients/AsepriteImport.h>
#include <Diorama/DioramaTypeIds.h>

#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/string/string.h>

// The pure Aseprite::Direction enum lives in the import core; its reflection type
// info is specialized here (the one place that reflects it). This small header is
// shared by the component and the sheet product asset so neither has to include the
// other (which would be circular).
namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(Diorama::Aseprite::Direction, Diorama::AsepriteDirectionTypeId);
}

namespace Diorama
{
    //! Inspector-facing mirror of Aseprite::FrameData (a frame's atlas rect + duration).
    //! Populated by the editor import (or the .aseprite AssetBuilder); serialized so the
    //! runtime needs no file IO.
    struct AsepriteFrameData final
    {
        AZ_TYPE_INFO(Diorama::AsepriteFrameData, AsepriteFrameDataTypeId);
        static void Reflect(AZ::ReflectContext* context);

        int m_x = 0;
        int m_y = 0;
        int m_w = 0;
        int m_h = 0;
        float m_durationSeconds = 0.1f;
    };

    //! Inspector-facing mirror of Aseprite::TagData (a named frame range + direction).
    struct AsepriteTagData final
    {
        AZ_TYPE_INFO(Diorama::AsepriteTagData, AsepriteTagDataTypeId);
        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_name;
        int m_from = 0;
        int m_to = 0;
        Aseprite::Direction m_direction = Aseprite::Direction::Forward;
    };
} // namespace Diorama
