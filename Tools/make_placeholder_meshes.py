# Copyright Chris Lawrence. Personal project, not for distribution.
#
# make_placeholder_meshes.py -- build placeholder minifig parts + the master material.
#
# Run inside the editor:  Tools -> Execute Python Script -> this file.
# Idempotent: existing assets are left in place (delete them to force a rebuild).
#
# Produces under /Game/Placeholders:
#   SM_Minifig_Hips / _Torso / _ArmL / _ArmR / _Head / _Hat   (engine BasicShapes, duplicated)
#   M_NinjagoMinifig  -- reads PerInstanceCustomData[0..2] as RGB into Base Color
#
# The renderer (UNinjagoModelRenderer) scales/positions these unit primitives into a minifig and
# writes each instance's faction colour into PerInstanceCustomData, so this material colours them.

import unreal

DEST_DIR = "/Game/Placeholders"

CUBE = "/Engine/BasicShapes/Cube"
CYL = "/Engine/BasicShapes/Cylinder"

# (asset name, source engine shape)
MESHES = [
    ("SM_Minifig_Hips", CUBE),
    ("SM_Minifig_Torso", CUBE),
    ("SM_Minifig_ArmL", CYL),
    ("SM_Minifig_ArmR", CYL),
    ("SM_Minifig_Head", CYL),
    ("SM_Minifig_Hat", CUBE),
]

MATERIAL_NAME = "M_NinjagoMinifig"


def ensure_dir():
    if not unreal.EditorAssetLibrary.does_directory_exist(DEST_DIR):
        unreal.EditorAssetLibrary.make_directory(DEST_DIR)


def make_meshes():
    made = 0
    for name, src in MESHES:
        dst = "%s/%s" % (DEST_DIR, name)
        if unreal.EditorAssetLibrary.does_asset_exist(dst):
            continue
        if not unreal.EditorAssetLibrary.does_asset_exist(src):
            unreal.log_error("Source shape missing: %s" % src)
            continue
        if unreal.EditorAssetLibrary.duplicate_asset(src, dst) is not None:
            unreal.EditorAssetLibrary.save_asset(dst)
            made += 1
    return made


def make_material():
    dst = "%s/%s" % (DEST_DIR, MATERIAL_NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(dst):
        return False  # already built

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(MATERIAL_NAME, DEST_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary

    def custom_data(index, y):
        node = mel.create_material_expression(
            mat, unreal.MaterialExpressionPerInstanceCustomData, -600, y
        )
        node.set_editor_property("data_index", index)
        node.set_editor_property("const_default_value", 0.5)
        return node

    r = custom_data(0, -120)
    g = custom_data(1, 0)
    b = custom_data(2, 120)

    app_rg = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -350, -60)
    app_rgb = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -150, 0)

    mel.connect_material_expressions(r, "", app_rg, "A")
    mel.connect_material_expressions(g, "", app_rg, "B")
    mel.connect_material_expressions(app_rg, "", app_rgb, "A")
    mel.connect_material_expressions(b, "", app_rgb, "B")
    mel.connect_material_property(app_rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(dst)
    return True


def main():
    ensure_dir()
    made = make_meshes()
    mat_built = make_material()

    unreal.log("==================== Ninjago placeholder assets ====================")
    unreal.log("  meshes created this run: %d (of %d)" % (made, len(MESHES)))
    unreal.log("  master material: %s" % ("built" if mat_built else "already present"))
    unreal.log("  location: %s" % DEST_DIR)
    unreal.log("====================================================================")


main()
