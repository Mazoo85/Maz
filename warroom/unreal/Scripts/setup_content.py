"""One-time content setup for War Room 1936, run inside the Unreal Editor:

    Tools > Execute Python Script...  -> pick this file
    (or: UnrealEditor-Cmd.exe WarRoom.uproject -run=pythonscript -script=Scripts/setup_content.py)

Everything the game needs that cannot be plain text is created here, so there are no
hand-made assets to lose:
  * imports data/map/provinces.png as an uncompressed, nearest-filtered, non-sRGB texture
    (ids must survive exactly) and data/map/relief.png as a normal map;
  * imports the Blender glTF models from models/ as static meshes;
  * builds the M_Globe material (one Custom HLSL node that includes Shaders/Globe.ush)
    and the M_Counter material (tinted by per-instance custom data);
  * creates the WarRoom level the project opens on.
Run it again any time; it overwrites what it made.
"""
import os
import unreal

PROJECT = os.path.normpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
WARROOM = os.path.normpath(os.path.join(PROJECT, ".."))
DATA = os.path.join(WARROOM, "data")
MODELS = os.path.join(WARROOM, "models")
ROOT = "/Game/WarRoom"

tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log(f"[warroom setup] {msg}")


def import_texture(path, name, nearest=False, normal=False):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = ROOT + "/Textures"
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    tex = lib.load_asset(f"{ROOT}/Textures/{name}")
    if tex is None:
        raise RuntimeError(f"could not import {path}")
    if normal:
        # read as plain linear colour; the shader unpacks it itself
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        tex.set_editor_property("srgb", False)
    else:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)  # uncompressed RGBA
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
        tex.set_editor_property("never_stream", True)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("max_texture_size", 8192)
    lib.save_loaded_asset(tex)
    log(f"texture {name} from {path}")
    return tex


def import_models():
    import json
    with open(os.path.join(MODELS, "manifest.json"), encoding="utf-8") as f:
        manifest = json.load(f)["models"]
    meshes = {}
    for name, m in manifest.items():
        # each model gets its own folder: every glTF carries materials called Body/Accent/Base
        folder = f"{ROOT}/Models/{name}"
        target = f"{ROOT}/Models/SM_{name}"
        if lib.does_asset_exist(target):
            lib.delete_asset(target)
        if lib.does_directory_exist(folder):
            lib.delete_directory(folder)
        task = unreal.AssetImportTask()
        task.filename = os.path.join(MODELS, m["file"])
        task.destination_path = folder
        task.replace_existing = True
        task.automated = True
        task.save = True
        tools.import_asset_tasks([task])
        asset = None
        for a in lib.list_assets(folder, recursive=True):
            obj = lib.load_asset(a)
            if isinstance(obj, unreal.StaticMesh):
                asset = obj
                break
        if asset is None:
            log(f"WARNING: {name} did not import as a static mesh")
            continue
        # a fixed name the game looks for
        if lib.rename_asset(asset.get_path_name().split(".")[0], target):
            asset = lib.load_asset(target)
        meshes[name] = asset
    log(f"{len(meshes)} models imported")
    return meshes


def make_globe_material(id_tex, relief_tex):
    path = f"{ROOT}/M_Globe"
    if lib.does_asset_exist(path):
        lib.delete_asset(path)
    mat = tools.create_asset("M_Globe", ROOT, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)   # robust to either triangle winding

    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -500, 0)
    custom.set_editor_property("description", "WarRoom globe")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("include_file_paths", ["/Project/Globe.ush"])
    custom.set_editor_property("code", (
        "return WR_GlobeColor(IdMap, IdMapSampler, ColorLUT, ColorLUTSampler, InfoLUT, InfoLUTSampler, "
        "FlagLUT, FlagLUTSampler, Relief, ReliefSampler, UV.xy, Texel.xy, BorderLevel, HoverId, SelectedId, "
        "WorldNormal.xyz, SunDir.xyz, Night, ReliefStrength);"))
    names = ["IdMap", "ColorLUT", "InfoLUT", "FlagLUT", "Relief", "UV", "Texel", "BorderLevel", "HoverId", "SelectedId", "WorldNormal", "SunDir", "Night", "ReliefStrength"]
    inputs = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)

    def tex_param(name, tex, sampler, y):
        e = mel.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, -900, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("texture", tex)
        e.set_editor_property("sampler_type", sampler)
        mel.connect_material_expressions(e, "", custom, name)
        return e

    def scalar_param(name, value, y):
        e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -900, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", value)
        mel.connect_material_expressions(e, "", custom, name)

    # placeholder 128x128 lookup textures: the game swaps in its own at runtime
    lut_placeholder = id_tex
    tex_param("IdMap", id_tex, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, -400)
    tex_param("ColorLUT", lut_placeholder, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, -300)
    tex_param("InfoLUT", lut_placeholder, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, -200)
    tex_param("FlagLUT", lut_placeholder, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, -100)
    tex_param("Relief", relief_tex, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 0)

    uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, 100)
    mel.connect_material_expressions(uv, "", custom, "UV")
    texel = mel.create_material_expression(mat, unreal.MaterialExpressionConstant2Vector, -900, 200)
    texel.set_editor_property("r", 1.0 / 8192.0)
    texel.set_editor_property("g", 1.0 / 4096.0)
    mel.connect_material_expressions(texel, "", custom, "Texel")
    scalar_param("BorderLevel", 2.0, 300)
    scalar_param("HoverId", 0.0, 400)
    scalar_param("SelectedId", 0.0, 500)
    normal = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, 600)
    mel.connect_material_expressions(normal, "", custom, "WorldNormal")
    sun = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, 700)
    sun.set_editor_property("parameter_name", "SunDir")
    sun.set_editor_property("default_value", unreal.LinearColor(1, 0, 0, 0))
    mel.connect_material_expressions(sun, "", custom, "SunDir")
    scalar_param("Night", 0.55, 800)
    scalar_param("ReliefStrength", 1.1, 900)

    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    lib.save_loaded_asset(mat)
    log("M_Globe built")
    return mat


def unlit_material(name, build):
    """Unlit, so the pieces read clearly without any lights in the level."""
    path = f"{ROOT}/{name}"
    if lib.does_asset_exist(path):
        lib.delete_asset(path)
    mat = tools.create_asset(name, ROOT, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    build(mat)
    mel.recompile_material(mat)
    lib.save_loaded_asset(mat)
    return mat


def make_counter_material(meshes):
    def tinted(mat):
        # the owning country's colour, set per counter by the game (custom data 0..2)
        data = mel.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData3Vector, -500, 0)
        mel.connect_material_property(data, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    def dark(mat):
        c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -500, 0)
        c.set_editor_property("constant", unreal.LinearColor(0.02, 0.02, 0.025, 1.0))
        mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    body = unlit_material("M_Counter", tinted)
    other = unlit_material("M_CounterDark", dark)
    for name, mesh in meshes.items():
        mats = mesh.get_editor_property("static_materials")
        for sm in mats:
            slot = str(sm.get_editor_property("material_slot_name"))
            mi = sm.get_editor_property("material_interface")
            is_body = slot.startswith("Body") or (mi is not None and mi.get_name().startswith("Body"))
            sm.set_editor_property("material_interface", body if is_body else other)
        mesh.set_editor_property("static_materials", mats)
        lib.save_loaded_asset(mesh)
    log("M_Counter (tinted) and M_CounterDark built and applied")


def make_level():
    level_path = f"{ROOT}/Maps/WarRoom"
    if lib.does_asset_exist(level_path):
        log("level exists; keeping it")
        return
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    subsystem.new_level(level_path)
    # a globe where the game mode expects one; the game mode spawns one anyway if missing
    actor_class = unreal.load_class(None, "/Script/WarRoom.GlobeActor")
    if actor_class:
        unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(actor_class, unreal.Vector(0, 0, 0))
    subsystem.save_current_level()
    log("level WarRoom created")


def main():
    for p in (os.path.join(DATA, "map", "provinces.png"), os.path.join(DATA, "map", "relief.png"), os.path.join(MODELS, "manifest.json")):
        if not os.path.exists(p):
            raise RuntimeError(f"missing {p}; the project expects the warroom/data and warroom/models folders next to unreal/")
    lib.make_directory(ROOT)
    id_tex = import_texture(os.path.join(DATA, "map", "provinces.png"), "T_ProvinceIds", nearest=True)
    relief = import_texture(os.path.join(DATA, "map", "relief.png"), "T_Relief", normal=True)
    meshes = import_models()
    make_globe_material(id_tex, relief)
    make_counter_material(meshes)
    make_level()
    lib.save_directory(ROOT, only_if_is_dirty=True, recursive=True)
    log("done. Press Play.")


main()
