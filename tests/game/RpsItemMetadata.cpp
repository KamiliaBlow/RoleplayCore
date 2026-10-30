#include "RpsItemMetadata.h"
#include "catch2/catch_test_macros.hpp"

#include <limits>

namespace
{
RpsItemMetadata ExampleMetadata()
{
    RpsItemMetadata metadata;
    metadata.Revision = 7;
    metadata.Name = "Плащ странника";
    metadata.Description = "Первая строка\nВторая строка";
    metadata.Modifiers = { { "maxHealth", 5 }, { "armor", 1 }, { "mastery.knowledge", -1 } };
    return metadata;
}
}

TEST_CASE("RP metadata round trips whole modifiers and instance-local text", "[roleplay][items][metadata]")
{
    RpsItemMetadata original = ExampleMetadata();
    original.Revision = std::numeric_limits<uint32>::max();
    std::string error;
    std::string json = original.Serialize();
    auto parsed = RpsItemMetadata::Parse(json, &error);
    REQUIRE(parsed);
    REQUIRE(error.empty());
    REQUIRE(parsed->Name == original.Name);
    REQUIRE(parsed->Description == original.Description);
    REQUIRE(parsed->Revision == original.Revision);
    REQUIRE(parsed->Modifiers == original.Modifiers);
    REQUIRE(parsed->Serialize() == json);
}

TEST_CASE("RP metadata rejects malformed and ambiguous documents", "[roleplay][items][metadata]")
{
    for (std::string_view json : {
        "", "null", "[]", "{}", "{invalid}",
        R"({"version":2,"revision":1,"name":"","description":"","modifiers":{}})",
        R"({"version":1,"revision":0,"name":"","description":"","modifiers":{}})",
        R"({"version":1,"revision":1.0,"name":"","description":"","modifiers":{}})",
        R"({"version":1,"revision":4294967296,"name":"","description":"","modifiers":{}})",
        R"({"version":1,"revision":1,"name":"","name":"second","description":"","modifiers":{}})",
        R"({"version":1,"revision":1,"name":"","description":"","modifiers":{},"owner":123})",
        R"({"version":1,"revision":1,"name":null,"description":"","modifiers":{}})",
        R"({"version":1,"revision":1,"name":"","description":"","modifiers":[]})",
        R"({"version":1,"revision":1,"name":"","description":"","modifiers":{"accuracy":1.5}})",
        R"({"version":1,"revision":1,"name":"","description":"","modifiers":{"accuracy":true}})",
        R"({"version":1,"revision":1,"name":"","description":"","modifiers":{"accuracy":1,"accuracy":2}})",
        R"({"version":1,"revision":1,"name":"","description":"","modifiers":{"nativeStrength":2}})",
        R"({"version":1,"revision":1,"name":"","description":"","modifiers":{"mastery":{"skill":1}}})",
        R"({"version":1,"revision":1,"name":"\u0000","description":"","modifiers":{}})",
        R"({"version":1,"revision":1,"name":"\ud800","description":"","modifiers":{}})" })
    {
        INFO(json);
        std::string error;
        REQUIRE_FALSE(RpsItemMetadata::Parse(json, &error));
        REQUIRE_FALSE(error.empty());
    }
    REQUIRE_FALSE(RpsItemMetadata::Parse(std::string(RpsItemMetadata::MaxJsonBytes + 1, ' ')));
}

TEST_CASE("RP metadata enforces Unicode text and integer input bounds", "[roleplay][items][metadata]")
{
    RpsItemMetadata metadata = ExampleMetadata();
    for (std::string key : { "maxHealth", "maxEnergy", "accuracy", "healing", "evasion", "resistance", "speed", "armor",
        "woundLimit", "healDifficulty", "energySpendLimit", "mastery.skill", "mastery.knowledge", "mastery.endurance", "mastery.craft" })
    {
        int32 limit = key == "maxHealth" || key == "maxEnergy" ? 1000 : 20;
        for (int32 sign : { -1, 1 })
        {
            metadata.Modifiers = { { key, sign * limit } };
            REQUIRE(RpsItemMetadata::Parse(metadata.Serialize()));
            metadata.Modifiers[key] = sign * (limit + 1);
            REQUIRE_FALSE(RpsItemMetadata::Parse(metadata.Serialize()));
        }
    }
    metadata.Modifiers.clear();
    metadata.Name.clear();
    for (unsigned i = 0; i < 80; ++i)
        metadata.Name += "Я";
    REQUIRE(RpsItemMetadata::Parse(metadata.Serialize()));
    metadata.Name += "Я";
    REQUIRE_FALSE(RpsItemMetadata::Parse(metadata.Serialize()));
    metadata.Name = "valid";
    metadata.Description = std::string(1024, 'x');
    REQUIRE(RpsItemMetadata::Parse(metadata.Serialize()));
    metadata.Description += "x";
    REQUIRE_FALSE(RpsItemMetadata::Parse(metadata.Serialize()));
    metadata.Description = "valid";
    metadata.Name = "bad\nname";
    REQUIRE_FALSE(RpsItemMetadata::Parse(metadata.Serialize()));
    metadata.Name = std::string("\xC0\xAF", 2); // overlong UTF-8 encoding
    REQUIRE_FALSE(RpsItemMetadata::Parse(metadata.Serialize()));
}

TEST_CASE("RP properties and rune overrides preserve signed values and empty removal", "[roleplay][items][metadata]")
{
    RpsItemMetadata original = ExampleMetadata();
    original.Properties = RpsItemMetadata::ModifierMap{{"accuracy", -3}, {"maxHealth", 8}};
    original.Runes = {{"1", {{"armor", -1}, {"mastery.skill", 2}}}, {"2", {}}};
    auto parsed = RpsItemMetadata::Parse(original.Serialize());
    REQUIRE(parsed);
    REQUIRE(parsed->Modifiers == original.Modifiers);
    REQUIRE(parsed->Properties == original.Properties);
    REQUIRE(parsed->Runes == original.Runes);
    REQUIRE(parsed->Runes.contains("2"));
    REQUIRE_FALSE(parsed->Runes.contains("3"));
    auto legacy = RpsItemMetadata::Parse(ExampleMetadata().Serialize());
    REQUIRE(legacy);
    REQUIRE_FALSE(legacy->Properties);
    REQUIRE(legacy->Runes.empty());
    original.Runes["4"] = {};
    REQUIRE_FALSE(RpsItemMetadata::Parse(original.Serialize()));
    original.Runes.erase("4");
    (*original.Properties)["accuracy"] = -21;
    REQUIRE_FALSE(RpsItemMetadata::Parse(original.Serialize()));
    original.Properties = RpsItemMetadata::ModifierMap{};
    REQUIRE(RpsItemMetadata::Parse(original.Serialize())->Properties.has_value());
    original.Runes["1"]["nativeStrength"] = 1;
    REQUIRE_FALSE(RpsItemMetadata::Parse(original.Serialize()));
}

TEST_CASE("RP modifier definitions are accepted at both bounds", "[roleplay][items][metadata]")
{
    for (auto const& definition : RpsItemMetadata::ModifierDefinitions())
    {
        REQUIRE_FALSE(definition.Label.empty());
        auto metadata = ExampleMetadata();
        metadata.Properties = RpsItemMetadata::ModifierMap{{std::string(definition.Key), definition.Limit}};
        metadata.Runes["1"] = {{std::string(definition.Key), -definition.Limit}};
        REQUIRE(RpsItemMetadata::Parse(metadata.Serialize()));
    }
}
