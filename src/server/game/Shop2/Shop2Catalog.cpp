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

#include "Shop2Service.h"
#include "BattlePayMgr.h"
#include "GameTime.h"
#include <algorithm>

namespace Shop2
{
// The only place the shop2 service reads world state. Kept in its own translation unit so the wire
// rendering (Shop2Service.cpp) stays free of game dependencies and can be exercised standalone.
//
// Called from BattlePayMgr::LoadCatalog on the world thread, i.e. at startup, on `.reload
// shop_catalog` and when an availability window boundary passes.
void Shop2Service::RebuildCatalog()
{
    if (!IsRunning())
        return;

    time_t const now = GameTime::GetGameTime();

    // Same eligibility rule the legacy BattlePay catalog uses: enabled and inside its window.
    std::vector<ShopProduct const*> products;
    for (auto const& [id, product] : sBattlePayMgr->GetProducts())
    {
        if (!product.Enabled)
            continue;
        if (product.AvailableFrom && now < product.AvailableFrom)
            continue;
        if (product.AvailableUntil && now > product.AvailableUntil)
            continue;

        products.push_back(&product);
    }

    // Same display order as the legacy catalog: featured first, then Ordering, then product id.
    std::sort(products.begin(), products.end(), [](ShopProduct const* a, ShopProduct const* b)
    {
        if (a->Featured != b->Featured) return a->Featured > b->Featured;
        if (a->Ordering != b->Ordering) return a->Ordering < b->Ordering;
        return a->ProductID < b->ProductID;
    });

    BuildSnapshot(products, now);
}
}
