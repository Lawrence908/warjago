# Copyright Chris Lawrence. Personal project, not for distribution.
#
# import_datatables.py -- (re)build the four Ninjago DataTable assets from Content/Data/*.csv.
#
# Run inside the editor:  Tools -> Execute Python Script -> this file.
# Idempotent and safe to re-run: existing tables are emptied and refilled in place.
# Requires the Ninjago C++ module to be compiled first (the row structs must exist).
#
# On success it logs a row-count summary and asserts the expected counts
# (units=209, factions=17, archetypes=16, abilities=44).

import os
import unreal

# (asset base name, CSV base name, row-struct object path, expected row count)
TABLES = [
    ("dt_units",      "dt_units",      "/Script/Ninjago.NinjagoUnitRow",      209),
    ("dt_factions",   "dt_factions",   "/Script/Ninjago.NinjagoFactionRow",    17),
    ("dt_archetypes", "dt_archetypes", "/Script/Ninjago.NinjagoArchetypeRow",  16),
    ("dt_abilities",  "dt_abilities",  "/Script/Ninjago.NinjagoAbilityRow",    44),
]

PKG_DIR = "/Game/Data"
DATA_DIR = os.path.join(unreal.Paths.project_content_dir(), "Data")


def load_struct(path):
    struct = unreal.load_object(None, path)
    if struct is None:
        raise RuntimeError(
            "Row struct not found: %s\n"
            "Compile the Ninjago C++ module before running this script." % path
        )
    return struct


def ensure_table(asset_name, struct):
    """Return the DataTable asset, creating it with the right row struct if absent."""
    pkg = "%s/%s" % (PKG_DIR, asset_name)
    if unreal.EditorAssetLibrary.does_asset_exist(pkg):
        return unreal.EditorAssetLibrary.load_asset(pkg)

    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", struct)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    return tools.create_asset(asset_name, PKG_DIR, unreal.DataTable, factory)


def row_count(data_table):
    """Robust row count across API variants."""
    try:
        return len(unreal.DataTableFunctionLibrary.get_data_table_row_names(data_table))
    except Exception:
        try:
            return len(data_table.get_row_names())
        except Exception:
            return -1


def main():
    if not unreal.EditorAssetLibrary.does_directory_exist(PKG_DIR):
        unreal.EditorAssetLibrary.make_directory(PKG_DIR)

    all_ok = True
    summary = []

    for asset_name, csv_name, struct_path, expected in TABLES:
        csv_path = os.path.join(DATA_DIR, csv_name + ".csv")
        if not os.path.isfile(csv_path):
            unreal.log_error("Missing CSV: %s" % csv_path)
            summary.append((asset_name, -1, expected, "MISSING CSV"))
            all_ok = False
            continue

        struct = load_struct(struct_path)
        data_table = ensure_table(asset_name, struct)

        with open(csv_path, "r", encoding="utf-8") as handle:
            csv_string = handle.read()

        # Passing import_row_struct forces an automated import (no dialogs).
        ok = unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(
            data_table, csv_string, struct
        )
        unreal.EditorAssetLibrary.save_loaded_asset(data_table)

        count = row_count(data_table)
        if not ok:
            status = "FILL FAILED (see log)"
            all_ok = False
        elif count != expected:
            status = "ROW MISMATCH (expected %d)" % expected
            all_ok = False
        else:
            status = "OK"
        summary.append((asset_name, count, expected, status))

    unreal.log("==================== Ninjago DataTable import ====================")
    for asset_name, count, expected, status in summary:
        unreal.log("  %-14s %4d / %-4d  %s" % (asset_name, count, expected, status))
    unreal.log("=================================================================")

    if all_ok:
        unreal.log("All four DataTables imported; row counts match. M2 import OK.")
    else:
        unreal.log_error(
            "DataTable import finished WITH PROBLEMS -- see rows above and the Output Log."
        )


main()
