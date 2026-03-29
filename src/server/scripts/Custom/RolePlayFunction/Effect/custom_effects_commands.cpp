#include "custom_effects_handler.h"
#include "Chat.h"
#include "ChatCommand.h"

namespace Noblegarden
{
    using namespace Trinity::ChatCommands;
    class EffectsCommands : public CommandScript
    {
    public:
        EffectsCommands() : CommandScript("player_effects_commands") { }

        std::span<ChatCommandBuilder const> GetCommands() const override
        {
            static ChatCommandTable effectSelfCommandTable =
            {
                { "add",       HandleSelfAdd,       rbac::RBAC_ROLE_PLAYER,  Console::Yes },
                { "cast",      HandleSelfCast,      rbac::RBAC_ROLE_PLAYER,  Console::Yes },
                { "channel",   HandleSelfChannel,   rbac::RBAC_ROLE_PLAYER,  Console::Yes },
                { "toggle",    HandleSelfToggle,    rbac::RBAC_ROLE_PLAYER,  Console::Yes },
                { "remove",    HandleSelfRemove,    rbac::RBAC_ROLE_PLAYER,  Console::Yes },
                { "reset",     HandleSelfReset,     rbac::RBAC_ROLE_PLAYER,  Console::Yes },
            };

            static ChatCommandTable effectTargetCommandTable =
            {
                { "add",       HandleTargetAdd,     rbac::RBAC_ROLE_PLAYER,  Console::Yes },
                { "toggle",    HandleTargetToggle,  rbac::RBAC_ROLE_PLAYER,  Console::Yes },
                { "remove",    HandleTargetRemove,  rbac::RBAC_ROLE_PLAYER,  Console::Yes },
                { "reset",     HandleTargetReset,   rbac::RBAC_ROLE_PLAYER,  Console::Yes },
            };

            static ChatCommandTable effectCommandTable =
            {
                { "self",   effectSelfCommandTable},
                { "target", effectTargetCommandTable},
            };

            static ChatCommandTable commandTable =
            {
                { "effect", effectCommandTable},
            };

            return commandTable;
        }


        static bool HandleSelfAdd(ChatHandler* handler, uint32 id, Optional<uint32> mode)
        {
            auto player = handler->GetPlayer();

            EffectsHandler::GetInstance().Add(player, id, mode.value_or(2), handler);

            return true;
        }


        static bool HandleSelfCast(ChatHandler* handler, uint32 id, float duration)
        {
            auto player = handler->GetPlayer();
            auto select = player->GetSelectedUnit();
            auto target = select ? select : player;

            EffectsHandler::GetInstance().Cast(player, target, id, duration, handler);

            return true;
        }


        static bool HandleSelfChannel(ChatHandler* handler, uint32 id)
        {
            auto player = handler->GetPlayer();

            if (auto target = player->GetSelectedUnit())
            {
                EffectsHandler::GetInstance().Channel(player, target, id, handler);
            }

            return true;
        }


        static bool HandleSelfToggle(ChatHandler* handler, uint32 id, Optional<uint32> mode)
        {
            auto player = handler->GetPlayer();

            EffectsHandler::GetInstance().Toggle(player, id, mode.value_or(2), handler);

            return true;
        }


        static bool HandleSelfRemove(ChatHandler* handler, uint32 id)
        {
            auto player = handler->GetPlayer();

            EffectsHandler::GetInstance().Remove(player, id, handler);

            return true;
        }


        static bool HandleSelfReset(ChatHandler* handler)
        {
            auto player = handler->GetPlayer();

            EffectsHandler::GetInstance().Reset(player, handler);

            return true;
        }


        static bool HandleTargetAdd(ChatHandler* handler, uint32 id, Optional<uint32> mode)
        {
            auto player = handler->GetPlayer();

            if (auto target = player->GetSelectedUnit())
            {
                EffectsHandler::GetInstance().Add(target, id, mode.value_or(2), handler);
            }

            return true;
        }


        static bool HandleTargetToggle(ChatHandler* handler, uint32 id, Optional<uint32> mode)
        {
            auto player = handler->GetPlayer();

            if (auto target = player->GetSelectedUnit())
            {
                EffectsHandler::GetInstance().Toggle(target, id, mode.value_or(2), handler);
            }

            return true;
        }


        static bool HandleTargetRemove(ChatHandler* handler, uint32 id)
        {
            auto player = handler->GetPlayer();

            if (auto target = player->GetSelectedUnit())
            {
                EffectsHandler::GetInstance().Remove(target, id, handler);
            }

            return true;
        }


        static bool HandleTargetReset(ChatHandler* handler)
        {
            auto player = handler->GetPlayer();

            if (auto target = player->GetSelectedUnit())
            {
                EffectsHandler::GetInstance().Reset(target, handler);
            }

            return true;
        }
    };
};

void AddSC_CustomEffectCommands()
{
    new Noblegarden::EffectsCommands();
}
