/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* ScriptData
Name: shop_commandscript
%Complete: 100
Comment: In-game Shop (BattlePay) catalog administration commands
Category: commandscripts
EndScriptData */

#include "ScriptMgr.h"
#include "BattlePayMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "DatabaseEnv.h"
#include "Optional.h"
#include "RBAC.h"
#include "StringFormat.h"
#include <cstdlib>
#include <sstream>

using namespace Trinity::ChatCommands;

class shop_commandscript : public CommandScript
{
public:
    shop_commandscript() : CommandScript("shop_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable shopCommandTable =
        {
            { "list",    HandleShopListCommand,    rbac::RBAC_PERM_COMMAND_SHOP, Console::Yes },
            { "preview", HandleShopListCommand,    rbac::RBAC_PERM_COMMAND_SHOP, Console::Yes },
            { "enable",  HandleShopEnableCommand,  rbac::RBAC_PERM_COMMAND_SHOP, Console::Yes },
            { "disable", HandleShopDisableCommand, rbac::RBAC_PERM_COMMAND_SHOP, Console::Yes },
            { "price",   HandleShopPriceCommand,   rbac::RBAC_PERM_COMMAND_SHOP, Console::Yes },
            { "window",  HandleShopWindowCommand,  rbac::RBAC_PERM_COMMAND_SHOP, Console::Yes },
            { "feature", HandleShopFeatureCommand, rbac::RBAC_PERM_COMMAND_SHOP, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "shop", shopCommandTable },
        };
        return commandTable;
    }

    // Validates the product exists, runs a synchronous UPDATE (so the row is committed before the reload
    // reads it), rebuilds the catalog, and reports. setClause is core-built from integers only.
    static bool MutateAndReload(ChatHandler* handler, uint32 productId, std::string const& setClause)
    {
        if (!sBattlePayMgr->GetProduct(productId))
        {
            handler->SendSysMessage(Trinity::StringFormat("No shop product with productId {}.", productId));
            handler->SetSentErrorMessage(true);
            return false;
        }

        WorldDatabase.DirectExecute(Trinity::StringFormat("UPDATE `shop_product` SET {} WHERE `productId`={}",
            setClause, productId).c_str());
        sBattlePayMgr->Reload();
        handler->SendSysMessage(Trinity::StringFormat("Shop product {} updated; catalog rebuilt (generation {}).",
            productId, sBattlePayMgr->GetCatalogGeneration()));
        return true;
    }

    static bool HandleShopListCommand(ChatHandler* handler)
    {
        std::string const report = sBattlePayMgr->BuildStatusReport();
        std::istringstream stream(report);
        std::string line;
        while (std::getline(stream, line))
            if (!line.empty())
                handler->SendSysMessage(line);
        return true;
    }

    static bool HandleShopEnableCommand(ChatHandler* handler, uint32 productId)
    {
        return MutateAndReload(handler, productId, "`enabled`=1");
    }

    static bool HandleShopDisableCommand(ChatHandler* handler, uint32 productId)
    {
        return MutateAndReload(handler, productId, "`enabled`=0");
    }

    static bool HandleShopPriceCommand(ChatHandler* handler, uint32 productId, uint64 amount, Optional<uint8> currency)
    {
        std::string setClause = Trinity::StringFormat("`price`={}", amount);
        if (currency)
            setClause += Trinity::StringFormat(", `currency`={}", uint32(*currency));
        return MutateAndReload(handler, productId, setClause);
    }

    // from/until are unix epoch seconds, or "-" to clear (NULL = always available on that side).
    static bool HandleShopWindowCommand(ChatHandler* handler, uint32 productId, std::string from, std::string until)
    {
        auto boundary = [](std::string const& token) -> std::string
        {
            if (token == "-")
                return "NULL";
            return Trinity::StringFormat("FROM_UNIXTIME({})", strtoull(token.c_str(), nullptr, 10));
        };
        std::string const setClause = Trinity::StringFormat("`availableFrom`={}, `availableUntil`={}",
            boundary(from), boundary(until));
        return MutateAndReload(handler, productId, setClause);
    }

    static bool HandleShopFeatureCommand(ChatHandler* handler, uint32 productId, uint8 featured)
    {
        return MutateAndReload(handler, productId, Trinity::StringFormat("`featured`={}", featured ? 1 : 0));
    }
};

void AddSC_shop_commandscript()
{
    new shop_commandscript();
}
