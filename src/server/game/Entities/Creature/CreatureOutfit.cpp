#include "CreatureOutfit.h"
#include "DB2Structure.h" // ChrRacesEntry, ItemModifiedAppearanceEntry, ItemAppearanceEntry
#include "DB2Stores.h"    // sChrRacesStore, sItemAppearanceStore
#include "TransmogMgr.h"  // TransmogMgr

constexpr uint32 CreatureOutfit::invisible_model;
constexpr uint32 CreatureOutfit::max_real_modelid;
constexpr EquipmentSlots CreatureOutfit::item_slots[];

CreatureOutfit::CreatureOutfit(uint8 race, Gender gender) : race(race), gender(gender)
{
    auto* model = sDB2Manager.GetChrModel(race, gender);
    if (!model)
    {
        model = sDB2Manager.GetChrModel(RACE_HUMAN, GENDER_MALE);
        ASSERT(model);
    }
    displayId = model->DisplayID;
}

CreatureOutfit& CreatureOutfit::SetItemEntry(EquipmentSlots slot, uint32 item_entry, uint32 appearancemodid)
{
    uint32 display = 0;

    if (ItemModifiedAppearanceEntry const* modifiedAppearance = TransmogMgr::GetItemModifiedAppearance(item_entry, appearancemodid))
        if (ItemAppearanceEntry const* itemAppearance = sItemAppearanceStore.LookupEntry(modifiedAppearance->ItemAppearanceID))
            display = itemAppearance->ItemDisplayInfoID;

    outfitdisplays[slot] = display;
    return *this;
}
