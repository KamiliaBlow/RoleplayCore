/*
* Copyright 2025 RoleplayCore and etc OS core
*/
 
// Info: Call Repack scripts here

void AddSC_zone_elwyn_forest();
void AddSC_zone_westfall();
void AddSC_zone_gilneas_city1();
void AddSC_duskwood_custom();

void AddSC_zone_tirisfal_glades();

// #################################################### //
// Add in AddCoreExtendedScripts(){ .. } without void. //
// #################################################### //

void AddCoreExtendedScripts()
{
    AddSC_zone_elwyn_forest();
    AddSC_zone_westfall();

    AddSC_zone_gilneas_city1();
    AddSC_duskwood_custom();

    AddSC_zone_tirisfal_glades();
}

