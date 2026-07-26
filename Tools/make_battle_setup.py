# Copyright Chris Lawrence. Personal project, not for distribution.
#
# make_battle_setup.py -- create /Game/Data/DA_DefaultBattle, the default v0.1 battle.
#
# Optional: the game mode also synthesises this battle in code when the asset is absent and the
# level has no units. Generating the asset lets you edit the rosters in the editor.
#
# Run inside the editor:  Tools -> Execute Python Script -> this file.  Idempotent (skips if present).

import unreal

DEST = "/Game/Data/DA_DefaultBattle"
UNIT_TABLE = "/Game/Data/dt_units"

# Melee, ranged (v0.2), a breakable block NIN_SOLDIERS (v0.3), and a large monster
# NIN_SAMURAIX (v0.5) for the Skulkin spears to brace against.
# Guests: LRD_ICEEMPEROR (freeze v0.9), LRD_SKALES (mind control v0.10), HRO_RONIN (vanish v0.17),
# LRD_HARUMI (passive stealth v0.19).
NINJA = ["HRO_KAI", "HRO_JAY", "HRO_COLE", "HRO_ZANE", "NIN_SHINTARO", "NIN_SOLDIERS", "NIN_SAMURAIX", "LRD_ICEEMPEROR", "LRD_SKALES", "HRO_RONIN", "LRD_HARUMI"]
# Guests: HRO_WYPLASH (def+3 buff v0.7), HRO_MACHIA (resurrect v0.15), LRD_CLOUSE (summon v0.21).
SKULKIN = ["SKU_MINERS", "SKU_WARRIORS", "SKU_WATCHMEN", "LRD_SAMUKAI", "SKU_ENGINEERS", "HRO_WYPLASH", "HRO_MACHIA", "LRD_CLOUSE"]


def main():
    if unreal.EditorAssetLibrary.does_asset_exist(DEST):
        unreal.log("DA_DefaultBattle already present; nothing to do.")
        return

    if not unreal.EditorAssetLibrary.does_asset_exist(UNIT_TABLE):
        unreal.log_error("dt_units not found at %s; run import_datatables.py first." % UNIT_TABLE)
        return

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.NinjagoBattleSetup)

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = tools.create_asset("DA_DefaultBattle", "/Game/Data", unreal.NinjagoBattleSetup, factory)
    if asset is None:
        unreal.log_error("Failed to create DA_DefaultBattle.")
        return

    asset.set_editor_property("unit_table", unreal.EditorAssetLibrary.load_asset(UNIT_TABLE))
    asset.set_editor_property("ninja_army", NINJA)
    asset.set_editor_property("skulkin_army", SKULKIN)

    unreal.EditorAssetLibrary.save_asset(DEST)
    unreal.log("Created %s (Ninja %d, Skulkin %d)." % (DEST, len(NINJA), len(SKULKIN)))


main()
