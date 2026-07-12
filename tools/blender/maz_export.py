# Maz Engine — Blender export helper
#
# Exports Blender meshes as glTF 2.0 (.glb) with settings the Maz Engine expects,
# so models drop straight into assets/models/ and load via maz::assets::loadModel.
#
# Two ways to use it (see docs/BLENDER_PIPELINE.md):
#
#   1. As an add-on (recommended for artists):
#        Blender > Edit > Preferences > Add-ons > Install... > pick this file > enable it.
#        Then: File > Export > "Maz Engine (.glb)".
#
#   2. Headless, from a terminal (for automation / batch export):
#        blender myfile.blend --background --python tools/blender/maz_export.py -- \
#            --out assets/models/myfile.glb [--selected]
#
# Design notes:
#   * Exports as binary glTF (.glb): one self-contained file, no loose .bin/textures.
#   * Y-up, right-handed — the glTF standard and what the engine's loader assumes.
#   * Modifiers are applied and normals + UVs exported, so what you see is what you get.

bl_info = {
    "name": "Maz Engine glTF Export",
    "author": "Maz",
    "version": (1, 0, 0),
    "blender": (3, 6, 0),
    "location": "File > Export > Maz Engine (.glb)",
    "description": "Export selected meshes to glTF (.glb) for the Maz Engine",
    "category": "Import-Export",
}

import sys

import bpy
from bpy.props import BoolProperty, StringProperty
from bpy.types import Operator
from bpy_extras.io_utils import ExportHelper


# The single source of truth for how the Maz Engine wants glTF written. Both the UI
# operator and the headless entry point route through here so they never drift apart.
def export_maz_gltf(filepath, use_selection=False):
    bpy.ops.export_scene.gltf(
        filepath=filepath,
        export_format="GLB",
        use_selection=use_selection,
        export_apply=True,       # bake modifiers into the exported mesh
        export_yup=True,         # Y-up, right-handed (glTF standard; engine assumes this)
        export_normals=True,
        export_texcoords=True,
        export_tangents=False,
        export_materials="EXPORT",
        export_cameras=False,
        export_lights=False,
    )
    return filepath


class ExportMazGltf(Operator, ExportHelper):
    """Export the scene (or selection) to glTF (.glb) for the Maz Engine"""

    bl_idname = "export_scene.maz_gltf"
    bl_label = "Maz Engine (.glb)"
    filename_ext = ".glb"

    filter_glob: StringProperty(default="*.glb", options={"HIDDEN"})
    use_selection: BoolProperty(
        name="Selected Objects Only",
        description="Export only selected objects instead of the whole scene",
        default=False,
    )

    def execute(self, context):
        export_maz_gltf(self.filepath, use_selection=self.use_selection)
        self.report({"INFO"}, "Maz: exported %s" % self.filepath)
        return {"FINISHED"}


def _menu_export(self, context):
    self.layout.operator(ExportMazGltf.bl_idname, text="Maz Engine (.glb)")


def register():
    bpy.utils.register_class(ExportMazGltf)
    bpy.types.TOPBAR_MT_file_export.append(_menu_export)


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(_menu_export)
    bpy.utils.unregister_class(ExportMazGltf)


def _run_headless(argv):
    """Entry point for: blender file.blend --background --python maz_export.py -- --out X.glb"""
    out = None
    use_selection = False
    i = 0
    while i < len(argv):
        if argv[i] == "--out" and i + 1 < len(argv):
            out = argv[i + 1]
            i += 2
        elif argv[i] == "--selected":
            use_selection = True
            i += 1
        else:
            i += 1
    if not out:
        print("maz_export: --out <path.glb> is required in headless mode", file=sys.stderr)
        sys.exit(2)
    path = export_maz_gltf(out, use_selection=use_selection)
    print("maz_export: wrote %s" % path)


if __name__ == "__main__":
    # Arguments after a lone "--" belong to us, not to Blender.
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if argv:
        _run_headless(argv)
    else:
        # Loaded/run inside a Blender session with no args: just register the menu item.
        register()
