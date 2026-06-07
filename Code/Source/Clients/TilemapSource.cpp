/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <Clients/TilemapSource.h>

#include <AzCore/JSON/document.h>
#include <AzCore/std/string/string.h>

namespace Diorama::TilemapSource
{
    namespace
    {
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

        float ReadFloatOr(const rapidjson::Value& obj, const char* key, float fallback)
        {
            auto it = obj.FindMember(key);
            if (it == obj.MemberEnd() || !it->value.IsNumber())
            {
                return fallback;
            }
            return it->value.GetFloat();
        }

        //! Read an [r,g,b,a] array into a color; leaves the default if absent/malformed.
        AZ::Color ReadColorOr(const rapidjson::Value& obj, const char* key, const AZ::Color& fallback)
        {
            auto it = obj.FindMember(key);
            if (it == obj.MemberEnd() || !it->value.IsArray() || it->value.Size() != 4)
            {
                return fallback;
            }
            AZ::Color c = fallback;
            for (rapidjson::SizeType i = 0; i < 4; ++i)
            {
                if (!it->value[i].IsNumber())
                {
                    return fallback;
                }
            }
            c.SetR(it->value[0].GetFloat());
            c.SetG(it->value[1].GetFloat());
            c.SetB(it->value[2].GetFloat());
            c.SetA(it->value[3].GetFloat());
            return c;
        }

        //! Read a "tiles" array of ints into outTiles. Returns false if the member is
        //! missing, not an array, exceeds maxCells, or holds a non-integer entry.
        bool ReadTiles(const rapidjson::Value& layerObj, AZ::s64 maxCells, AZStd::vector<AZ::s32>& outTiles, AZStd::string& outError)
        {
            auto it = layerObj.FindMember("tiles");
            if (it == layerObj.MemberEnd() || !it->value.IsArray())
            {
                outError = "layer is missing a 'tiles' array";
                return false;
            }
            const rapidjson::Value& arr = it->value;
            if (static_cast<AZ::s64>(arr.Size()) > maxCells)
            {
                outError = "'tiles' array exceeds the maximum cell count";
                return false;
            }
            outTiles.clear();
            outTiles.reserve(arr.Size());
            for (rapidjson::SizeType i = 0; i < arr.Size(); ++i)
            {
                if (!arr[i].IsInt())
                {
                    outError = "'tiles' contains a non-integer entry";
                    return false;
                }
                outTiles.push_back(static_cast<AZ::s32>(arr[i].GetInt()));
            }
            return true;
        }
    } // namespace

    bool Parse(AZStd::string_view json, DioramaTilemapAsset& outAsset, AZStd::string& outError)
    {
        rapidjson::Document doc;
        doc.Parse(json.data(), json.size());
        if (doc.HasParseError() || !doc.IsObject())
        {
            outError = "source is not valid JSON object";
            return false;
        }

        if (!ReadInt(doc, "columns", outAsset.m_columns) || !ReadInt(doc, "rows", outAsset.m_rows))
        {
            outError = "missing integer 'columns' / 'rows'";
            return false;
        }
        // atlasColumns/atlasRows default to 1 (a single-cell atlas) when omitted.
        if (!ReadInt(doc, "atlasColumns", outAsset.m_atlasColumns))
        {
            outAsset.m_atlasColumns = 1;
        }
        if (!ReadInt(doc, "atlasRows", outAsset.m_atlasRows))
        {
            outAsset.m_atlasRows = 1;
        }
        outAsset.m_tileWidth = ReadFloatOr(doc, "tileWidth", 1.0f);
        outAsset.m_tileHeight = ReadFloatOr(doc, "tileHeight", 1.0f);

        auto atlasIt = doc.FindMember("atlas");
        if (atlasIt != doc.MemberEnd() && atlasIt->value.IsString())
        {
            outAsset.m_atlasTexturePath.assign(atlasIt->value.GetString(), atlasIt->value.GetStringLength());
        }

        // Bound dimensions before computing the expected cell count, so the product
        // below can never request an oversized allocation from a hostile source.
        if (outAsset.m_columns < 1 || outAsset.m_columns > TilemapAssetLimits::MaxDimension || outAsset.m_rows < 1 ||
            outAsset.m_rows > TilemapAssetLimits::MaxDimension)
        {
            outError = "'columns' / 'rows' out of range";
            return false;
        }
        const AZ::s64 expectedCells = static_cast<AZ::s64>(outAsset.m_columns) * outAsset.m_rows;

        outAsset.m_layers.clear();
        auto layersIt = doc.FindMember("layers");
        if (layersIt != doc.MemberEnd() && layersIt->value.IsArray())
        {
            const rapidjson::Value& layers = layersIt->value;
            if (layers.Size() == 0 || layers.Size() > static_cast<rapidjson::SizeType>(TilemapAssetLimits::MaxLayers))
            {
                outError = "'layers' is empty or exceeds the layer limit";
                return false;
            }
            for (rapidjson::SizeType i = 0; i < layers.Size(); ++i)
            {
                if (!layers[i].IsObject())
                {
                    outError = "'layers' entry is not an object";
                    return false;
                }
                DioramaTilemapLayerData layer;
                auto nameIt = layers[i].FindMember("name");
                if (nameIt != layers[i].MemberEnd() && nameIt->value.IsString())
                {
                    layer.m_name.assign(nameIt->value.GetString(), nameIt->value.GetStringLength());
                }
                layer.m_sortOffset = ReadFloatOr(layers[i], "sortOffset", 0.0f);
                layer.m_tint = ReadColorOr(layers[i], "tint", AZ::Color(1.0f, 1.0f, 1.0f, 1.0f));
                if (!ReadTiles(layers[i], expectedCells, layer.m_tiles, outError))
                {
                    return false;
                }
                outAsset.m_layers.push_back(AZStd::move(layer));
            }
        }
        else
        {
            // Shorthand: a top-level "tiles" array is one layer named "main".
            DioramaTilemapLayerData layer;
            layer.m_name = "main";
            if (!ReadTiles(doc, expectedCells, layer.m_tiles, outError))
            {
                return false;
            }
            outAsset.m_layers.push_back(AZStd::move(layer));
        }

        // Final consistency + bounds gate (tile-array length matches the grid, every
        // index is in atlas range, etc.). Same check the runtime runs at load.
        if (!outAsset.IsValid())
        {
            outError = "tilemap failed validation (layer size mismatch or tile index out of range)";
            return false;
        }
        return true;
    }
} // namespace Diorama::TilemapSource
