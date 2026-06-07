/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/TiledImport.h>

#include <Diorama/TilemapComponentConfig.h> // TilemapComponentConfig::EmptyTile

#include <AzCore/JSON/document.h>

namespace Diorama::TiledImport
{
    namespace
    {
        //! Tiled stores flip/rotation in the top 3 GID bits; the cell index is the
        //! remaining 29 bits.
        constexpr AZ::u32 GidFlagMask = 0x1FFFFFFFu;

        bool ReadInt(const rapidjson::Value& obj, const char* key, int& out)
        {
            auto it = obj.FindMember(key);
            if (it == obj.MemberEnd() || !it->value.IsInt())
            {
                return false;
            }
            out = it->value.GetInt();
            return true;
        }
    } // namespace

    bool Parse(AZStd::string_view json, DioramaTilemapAsset& outAsset, AZStd::string& outError)
    {
        rapidjson::Document doc;
        doc.Parse(json.data(), json.size());
        if (doc.HasParseError() || !doc.IsObject())
        {
            outError = "source is not a valid JSON object";
            return false;
        }

        auto orientationIt = doc.FindMember("orientation");
        if (orientationIt != doc.MemberEnd() && orientationIt->value.IsString() &&
            AZStd::string_view(orientationIt->value.GetString(), orientationIt->value.GetStringLength()) != "orthogonal")
        {
            outError = "only orthogonal Tiled maps are supported";
            return false;
        }

        if (!ReadInt(doc, "width", outAsset.m_columns) || !ReadInt(doc, "height", outAsset.m_rows))
        {
            outError = "missing integer map 'width' / 'height'";
            return false;
        }
        if (outAsset.m_columns < 1 || outAsset.m_columns > TilemapAssetLimits::MaxDimension || outAsset.m_rows < 1 ||
            outAsset.m_rows > TilemapAssetLimits::MaxDimension)
        {
            outError = "map 'width' / 'height' out of range";
            return false;
        }
        // Tiled pixel tile sizes describe the atlas, not world units; default to 1.0.
        outAsset.m_tileWidth = 1.0f;
        outAsset.m_tileHeight = 1.0f;

        // First tileset only: read firstgid, the atlas grid (columns x rows), and image.
        auto tilesetsIt = doc.FindMember("tilesets");
        if (tilesetsIt == doc.MemberEnd() || !tilesetsIt->value.IsArray() || tilesetsIt->value.Empty())
        {
            outError = "no tilesets in the map";
            return false;
        }
        const rapidjson::Value& tileset = tilesetsIt->value[0];
        if (!tileset.IsObject() || tileset.HasMember("source"))
        {
            // An external tileset reference (.tsx/.tsj) has only {firstgid, source}.
            outError = "external tilesets (.tsx/.tsj) are not supported yet; embed the tileset in the map";
            return false;
        }
        int firstGid = 1;
        ReadInt(tileset, "firstgid", firstGid);
        if (!ReadInt(tileset, "columns", outAsset.m_atlasColumns) || outAsset.m_atlasColumns < 1)
        {
            outError = "tileset is missing a positive 'columns'";
            return false;
        }
        int tileCount = 0;
        if (ReadInt(tileset, "tilecount", tileCount) && tileCount > 0)
        {
            outAsset.m_atlasRows = (tileCount + outAsset.m_atlasColumns - 1) / outAsset.m_atlasColumns;
        }
        else
        {
            outAsset.m_atlasRows = 1;
        }
        auto imageIt = tileset.FindMember("image");
        if (imageIt != tileset.MemberEnd() && imageIt->value.IsString())
        {
            outAsset.m_atlasTexturePath.assign(imageIt->value.GetString(), imageIt->value.GetStringLength());
        }

        const AZ::s64 expectedCells = static_cast<AZ::s64>(outAsset.m_columns) * outAsset.m_rows;
        const int atlasCells = outAsset.m_atlasColumns * outAsset.m_atlasRows;

        auto layersIt = doc.FindMember("layers");
        if (layersIt == doc.MemberEnd() || !layersIt->value.IsArray())
        {
            outError = "map has no layers array";
            return false;
        }
        outAsset.m_layers.clear();
        const rapidjson::Value& layers = layersIt->value;
        for (rapidjson::SizeType i = 0; i < layers.Size(); ++i)
        {
            const rapidjson::Value& src = layers[i];
            if (!src.IsObject())
            {
                continue;
            }
            auto typeIt = src.FindMember("type");
            if (typeIt == src.MemberEnd() || !typeIt->value.IsString() ||
                AZStd::string_view(typeIt->value.GetString(), typeIt->value.GetStringLength()) != "tilelayer")
            {
                continue; // skip object/image/group layers
            }
            auto dataIt = src.FindMember("data");
            if (dataIt == src.MemberEnd() || !dataIt->value.IsArray())
            {
                outError = "a tile layer has no integer 'data' array (compressed/base64 layers are not supported)";
                return false;
            }
            const rapidjson::Value& data = dataIt->value;
            if (static_cast<AZ::s64>(data.Size()) != expectedCells)
            {
                outError = "a layer's 'data' length does not match map width * height";
                return false;
            }
            if (outAsset.m_layers.size() >= static_cast<size_t>(TilemapAssetLimits::MaxLayers))
            {
                outError = "too many tile layers";
                return false;
            }

            DioramaTilemapLayerData layer;
            auto nameIt = src.FindMember("name");
            if (nameIt != src.MemberEnd() && nameIt->value.IsString())
            {
                layer.m_name.assign(nameIt->value.GetString(), nameIt->value.GetStringLength());
            }
            layer.m_sortOffset = static_cast<float>(outAsset.m_layers.size());
            layer.m_tiles.reserve(data.Size());
            for (rapidjson::SizeType c = 0; c < data.Size(); ++c)
            {
                if (!data[c].IsUint() && !data[c].IsInt())
                {
                    outError = "a layer's 'data' contains a non-integer entry";
                    return false;
                }
                const AZ::u32 gid = data[c].GetUint() & GidFlagMask;
                // GID 0 is empty; otherwise it indexes the tileset from firstgid.
                AZ::s32 cell = TilemapComponentConfig::EmptyTile;
                if (gid != 0)
                {
                    const AZ::s32 candidate = static_cast<AZ::s32>(gid) - firstGid;
                    cell = (candidate >= 0 && candidate < atlasCells) ? candidate : TilemapComponentConfig::EmptyTile;
                }
                layer.m_tiles.push_back(cell);
            }
            outAsset.m_layers.push_back(AZStd::move(layer));
        }

        if (outAsset.m_layers.empty())
        {
            outError = "map has no tile layers";
            return false;
        }
        if (!outAsset.IsValid())
        {
            outError = "imported tilemap failed validation";
            return false;
        }
        return true;
    }
} // namespace Diorama::TiledImport
