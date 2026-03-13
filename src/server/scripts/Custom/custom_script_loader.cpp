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

// This is where scripts' loading functions should be declared:

//void AddSC_PrivateWork();
//void AddSC_custom_commandscript();
void AddSC_dragonriding_spell_scripts();
//ROLEPLAY FUNC
void AddSC_free_share_scripts();
void AddSC_CustomDisplayHandler();
void AddSC_CustomDisplayCommands();
void AddSC_CustomEffectHandler();
void AddSC_CustomEffectCommands();
void AddSC_toy_spell_scripts();

// The name of this function should match:
// void Add${NameOfDirectory}Scripts()
void AddCustomScripts()
{
    //AddSC_PrivateWork();
    //AddSC_custom_commandscript();
    AddSC_dragonriding_spell_scripts();
    AddSC_toy_spell_scripts();
    //ROLEPLAY FUNC
    AddSC_free_share_scripts();
    AddSC_CustomDisplayHandler();
    AddSC_CustomDisplayCommands();
    AddSC_CustomEffectHandler();
    AddSC_CustomEffectCommands();
}
