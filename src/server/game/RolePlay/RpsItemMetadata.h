#ifndef RpsItemMetadata_h__
#define RpsItemMetadata_h__

#include "Define.h"
#include <map>
#include <span>
#include <optional>
#include <string>
#include <string_view>

// Instance-local RP data stored in characters.rps_item_metadata by item GUID.
// Native item loading never reads it; NobleNextLua owns the cache and the CAS editor.
struct TC_GAME_API RpsItemMetadata
{
    static constexpr uint32 CurrentVersion = 1;
    static constexpr std::size_t MaxJsonBytes = 8192;

    uint32 Version = CurrentVersion;
    uint32 Revision = 1;
    std::string Name;
    std::string Description;
    using ModifierMap = std::map<std::string, int32>;
    struct ModifierDefinition { std::string_view Key; std::string_view Label; int32 Limit; };
    static std::span<ModifierDefinition const> ModifierDefinitions();
    ModifierMap Modifiers;
    std::optional<ModifierMap> Properties;
    std::map<std::string, ModifierMap> Runes;

    static std::optional<RpsItemMetadata> Parse(std::string_view json, std::string* error = nullptr);
    std::string Serialize() const;
};

#endif
