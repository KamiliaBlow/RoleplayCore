#include "RpsItemMetadata.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <algorithm>
#include <array>

namespace
{
bool IsTextValid(std::string_view value, bool multiline)
{
    // UTF-8 validity is checked by the JSON parser. Hidden control bytes are not allowed.
    for (unsigned char character : value)
        if ((character < 0x20 && !(multiline && (character == '\n' || character == '\t'))) || character == 0x7F)
            return false;
    return true;
}

bool ReadModifiers(rapidjson::Value const& object, RpsItemMetadata::ModifierMap& output)
{
    if (!object.IsObject()) return false;
    for (auto const& member : object.GetObject())
    {
        std::string key(member.name.GetString(), member.name.GetStringLength());
        auto definitions = RpsItemMetadata::ModifierDefinitions();
        auto definition = std::ranges::find(definitions, key, &RpsItemMetadata::ModifierDefinition::Key);
        if (definition == definitions.end() || !member.value.IsInt()) return false;
        int32 value = member.value.GetInt();
        if (value < -definition->Limit || value > definition->Limit || !output.emplace(key, value).second) return false;
    }
    return true;
}
}

std::span<RpsItemMetadata::ModifierDefinition const> RpsItemMetadata::ModifierDefinitions()
{
    static constexpr ModifierDefinition definitions[] = {
        {"maxHealth", "ОЗ", 1000}, {"maxEnergy", "ОЭ", 1000}, {"accuracy", "Меткость", 20},
        {"healing", "Исцеление", 20}, {"evasion", "Уклонение", 20}, {"resistance", "Сопротивление", 20},
        {"speed", "Скорость", 20}, {"armor", "Броня", 20}, {"woundLimit", "Предел ран", 20},
        {"healDifficulty", "Сложность исцеления", 20}, {"energySpendLimit", "Лимит затрат энергии", 20},
        {"mastery.skill", "Навык", 20}, {"mastery.knowledge", "Знание", 20},
        {"mastery.endurance", "Упорство", 20}, {"mastery.craft", "Ремесло", 20}
    };
    return definitions;
}

std::optional<RpsItemMetadata> RpsItemMetadata::Parse(std::string_view json, std::string* error)
{
    auto fail = [error](char const* message) -> std::optional<RpsItemMetadata>
    {
        if (error)
            *error = message;
        return std::nullopt;
    };
    if (error)
        error->clear();

    if (json.empty() || json.size() > MaxJsonBytes || json.find('\0') != std::string_view::npos)
        return fail("invalid_payload_size");

    rapidjson::Document document;
    document.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag>(json.data(), json.size());
    if (document.HasParseError() || !document.IsObject())
        return fail("invalid_json");

    RpsItemMetadata metadata;
    uint8 seen = 0;
    for (auto const& member : document.GetObject())
    {
        std::string_view key(member.name.GetString(), member.name.GetStringLength());
        uint8 field = key == "version" ? 1 : key == "revision" ? 2 : key == "name" ? 4 : key == "description" ? 8 : key == "modifiers" ? 16 : key == "properties" ? 32 : key == "runes" ? 64 : 0;
        if (!field || (seen & field))
            return fail("unknown_or_duplicate_field");
        seen |= field;

        if (field == 1)
        {
            if (!member.value.IsUint() || member.value.GetUint() != CurrentVersion)
                return fail("unsupported_version");
        }
        else if (field == 2)
        {
            if (!member.value.IsUint() || !member.value.GetUint())
                return fail("invalid_revision");
            metadata.Revision = member.value.GetUint();
        }
        else if (field == 4 || field == 8)
        {
            if (!member.value.IsString())
                return fail("invalid_text");
            std::string_view value(member.value.GetString(), member.value.GetStringLength());
            if (!IsTextValid(value, field == 8))
                return fail("invalid_text");
            if (field == 4)
            {
                std::size_t characters = std::ranges::count_if(value, [](unsigned char character) { return (character & 0xC0) != 0x80; });
                if (characters > 80)
                    return fail("name_too_long");
                metadata.Name = value;
            }
            else
            {
                if (value.size() > 1024)
                    return fail("description_too_long");
                metadata.Description = value;
            }
        }
        else if (field == 16 || field == 32)
        {
            ModifierMap values;
            if (!ReadModifiers(member.value, values)) return fail("invalid_modifier");
            if (field == 16) metadata.Modifiers = std::move(values);
            else metadata.Properties = std::move(values);
        }
        else
        {
            if (!member.value.IsObject()) return fail("invalid_runes");
            for (auto const& rune : member.value.GetObject())
            {
                std::string socket(rune.name.GetString(), rune.name.GetStringLength());
                ModifierMap values;
                if ((socket != "1" && socket != "2" && socket != "3") || !ReadModifiers(rune.value, values)
                    || !metadata.Runes.emplace(socket, std::move(values)).second) return fail("invalid_runes");
            }
        }
    }
    if ((seen & 31) != 31)
        return fail("missing_field");
    return metadata;
}

std::string RpsItemMetadata::Serialize() const
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("version");
    writer.Uint(Version);
    writer.Key("revision");
    writer.Uint(Revision);
    writer.Key("name");
    writer.String(Name.data(), rapidjson::SizeType(Name.size()));
    writer.Key("description");
    writer.String(Description.data(), rapidjson::SizeType(Description.size()));
    auto writeModifiers = [&writer](ModifierMap const& modifiers)
    {
        writer.StartObject();
        for (auto const& [key, value] : modifiers)
        {
            writer.Key(key.data(), rapidjson::SizeType(key.size()));
            writer.Int(value);
        }
        writer.EndObject();
    };
    writer.Key("modifiers");
    writeModifiers(Modifiers);
    if (Properties)
    {
        writer.Key("properties");
        writeModifiers(*Properties);
    }
    if (!Runes.empty())
    {
        writer.Key("runes");
        writer.StartObject();
        for (auto const& [socket, values] : Runes)
        {
            writer.Key(socket.data(), rapidjson::SizeType(socket.size()));
            writeModifiers(values);
        }
        writer.EndObject();
    }
    writer.EndObject();
    return { buffer.GetString(), buffer.GetSize() };
}
