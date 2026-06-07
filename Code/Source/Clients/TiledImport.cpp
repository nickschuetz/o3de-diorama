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
        // Tiled stores flip/rotation in the top GID bits; the cell index is the rest.
        constexpr AZ::u32 TiledFlipHorizontal = 0x80000000u;
        constexpr AZ::u32 TiledFlipVertical = 0x40000000u;
        constexpr AZ::u32 TiledGidIndexMask = 0x1FFFFFFFu; // strips H, V, and diagonal bits

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

        //! Read the atlas grid (columns x rows) and image from a tileset object,
        //! whether embedded in the map or loaded from an external .tsj. The tileset's
        //! own `columns` and `tilecount` describe the atlas; the image is the atlas
        //! texture. Returns false (with a reason) if `columns` is missing/invalid.
        bool ReadTilesetFields(const rapidjson::Value& tileset, DioramaTilemapAsset& outAsset, AZStd::string& outError)
        {
            if (!tileset.IsObject() || !ReadInt(tileset, "columns", outAsset.m_atlasColumns) || outAsset.m_atlasColumns < 1)
            {
                outError = "tileset is missing a positive 'columns'";
                return false;
            }
            int tileCount = 0;
            outAsset.m_atlasRows = (ReadInt(tileset, "tilecount", tileCount) && tileCount > 0)
                ? (tileCount + outAsset.m_atlasColumns - 1) / outAsset.m_atlasColumns
                : 1;
            auto imageIt = tileset.FindMember("image");
            if (imageIt != tileset.MemberEnd() && imageIt->value.IsString())
            {
                outAsset.m_atlasTexturePath.assign(imageIt->value.GetString(), imageIt->value.GetStringLength());
            }
            return true;
        }
    } // namespace

    bool Parse(AZStd::string_view json, DioramaTilemapAsset& outAsset, AZStd::string& outError, const ExternalTilesetResolver& resolver)
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

        // First tileset only. It may be embedded (full fields) or external (just
        // {firstgid, source} pointing at a .tsj resolved through the callback).
        auto tilesetsIt = doc.FindMember("tilesets");
        if (tilesetsIt == doc.MemberEnd() || !tilesetsIt->value.IsArray() || tilesetsIt->value.Empty())
        {
            outError = "no tilesets in the map";
            return false;
        }
        const rapidjson::Value& tileset = tilesetsIt->value[0];
        int firstGid = 1;
        ReadInt(tileset, "firstgid", firstGid);

        auto sourceIt = tileset.IsObject() ? tileset.FindMember("source") : tileset.MemberEnd();
        rapidjson::Document externalDoc; // kept alive while its fields are read
        if (sourceIt != tileset.MemberEnd() && sourceIt->value.IsString())
        {
            const AZStd::string_view source(sourceIt->value.GetString(), sourceIt->value.GetStringLength());
            if (source.ends_with(".tsx"))
            {
                outError = "XML external tilesets (.tsx) are not supported; export the tileset as .tsj";
                return false;
            }
            AZStd::string tilesetJson;
            if (!resolver || !resolver(source, tilesetJson))
            {
                outError = "could not resolve external tileset; embed it or provide a .tsj next to the map";
                return false;
            }
            externalDoc.Parse(tilesetJson.data(), tilesetJson.size());
            if (externalDoc.HasParseError() || !externalDoc.IsObject())
            {
                outError = "external tileset is not a valid JSON object";
                return false;
            }
            if (!ReadTilesetFields(externalDoc, outAsset, outError))
            {
                return false;
            }
        }
        else if (!ReadTilesetFields(tileset, outAsset, outError))
        {
            return false;
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
                const AZ::u32 rawGid = data[c].GetUint();
                const AZ::u32 gid = rawGid & TiledGidIndexMask; // cell part, flags removed
                AZ::s32 cell = TilemapComponentConfig::EmptyTile;
                if (gid != 0)
                {
                    const AZ::s32 candidate = static_cast<AZ::s32>(gid) - firstGid;
                    if (candidate >= 0 && candidate < atlasCells)
                    {
                        cell = candidate;
                        // Carry horizontal/vertical mirroring; the diagonal flag is a
                        // rotation the sprite path cannot express yet, so it is dropped.
                        if (rawGid & TiledFlipHorizontal)
                        {
                            cell |= TilemapTile::FlipHorizontal;
                        }
                        if (rawGid & TiledFlipVertical)
                        {
                            cell |= TilemapTile::FlipVertical;
                        }
                    }
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
