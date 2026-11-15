"""Creates the Main Menu media assets, Blueprints, and MAP_MainMenu level in UE 5.8."""
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
editor_asset_lib = unreal.EditorAssetLibrary
level_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

MEDIA_DIR = "/Game/DirectorsCut/UI/Media"
CORE_DIR = "/Game/DirectorsCut/Core"
LEVEL_PATH = "/Game/_MainGame/Level/MAP_MainMenu"


def ensure_dir(path):
    if not editor_asset_lib.does_directory_exist(path):
        editor_asset_lib.make_directory(path)


def create_or_load(asset_path, asset_class, factory):
    if editor_asset_lib.does_asset_exist(asset_path):
        return unreal.load_asset(asset_path)
    pkg, name = asset_path.rsplit("/", 1)
    return asset_tools.create_asset(name, pkg, asset_class, factory)


def create_blueprint(asset_path, parent_class):
    if editor_asset_lib.does_asset_exist(asset_path):
        return unreal.load_asset(asset_path)
    pkg, name = asset_path.rsplit("/", 1)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return asset_tools.create_asset(name, pkg, unreal.Blueprint, factory)


ensure_dir(MEDIA_DIR)
ensure_dir(CORE_DIR)
ensure_dir("/Game/_MainGame/Level")

# 1. FileMediaSource -> ./Movies/Menu.mp4
fms = create_or_load(f"{MEDIA_DIR}/FMS_MainMenu", unreal.FileMediaSource, unreal.FileMediaSourceFactoryNew())
fms.set_file_path(unreal.Paths.project_content_dir() + "Movies/Menu.mp4")
fms.set_editor_property("precache_file", True)
editor_asset_lib.save_loaded_asset(fms)

# 2. MediaPlayer
mp = create_or_load(f"{MEDIA_DIR}/MP_MainMenu", unreal.MediaPlayer, unreal.MediaPlayerFactoryNew())
mp.set_editor_property("play_on_open", True)
mp.set_looping(True)
editor_asset_lib.save_loaded_asset(mp)

# 3. MediaTexture (set both serialized media_player property and runtime player)
mt = create_or_load(f"{MEDIA_DIR}/MT_MainMenu", unreal.MediaTexture, unreal.MediaTextureFactoryNew())
mt.set_editor_property("new_style_output", True)
mt.set_editor_property("media_player", mp)
mt.set_media_player(mp)
mt.update_resource()
editor_asset_lib.save_loaded_asset(mt)

# 4. UI Material (opaque, samples MediaTexture -> EmissiveColor)
mat_path = f"{MEDIA_DIR}/M_UI_MainMenuVideo"
mat = create_or_load(mat_path, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
tex_node = unreal.MaterialEditingLibrary.create_material_expression(
    mat, unreal.MaterialExpressionTextureSample, -300, 0
)
tex_node.set_editor_property("texture", mt)
tex_node.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
connected = unreal.MaterialEditingLibrary.connect_material_property(
    tex_node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
)
print("M_UI_MainMenuVideo connected:", connected)
unreal.MaterialEditingLibrary.recompile_material(mat)
editor_asset_lib.save_loaded_asset(mat)

# 5. BP_MainMenuPlayerController
pc_cls = unreal.load_class(None, "/Script/EpicMegaJam2026.DCMainMenuPlayerController")
bp_pc = create_blueprint(f"{CORE_DIR}/BP_MainMenuPlayerController", pc_cls)
wbp_menu_cls = unreal.load_class(None, "/Game/DirectorsCut/UI/WBP_MainMenu.WBP_MainMenu_C")
if wbp_menu_cls:
    unreal.get_default_object(bp_pc.generated_class()).set_editor_property("main_menu_widget_class", wbp_menu_cls)
unreal.BlueprintEditorLibrary.compile_blueprint(bp_pc)
editor_asset_lib.save_loaded_asset(bp_pc)

# 6. BP_MainMenuGameMode
gm_cls = unreal.load_class(None, "/Script/EpicMegaJam2026.DCMainMenuGameMode")
bp_gm = create_blueprint(f"{CORE_DIR}/BP_MainMenuGameMode", gm_cls)
gm_cdo = unreal.get_default_object(bp_gm.generated_class())
gm_cdo.set_editor_property("player_controller_class", bp_pc.generated_class())
audio_cls = unreal.load_class(None, "/Game/DirectorsCut/Audio/BP_AudioManager.BP_AudioManager_C")
if audio_cls:
    gm_cdo.set_editor_property("audio_manager_class", audio_cls)
config_da = unreal.load_asset("/Game/DirectorsCut/Data/DA_GameConfig")
if config_da:
    gm_cdo.set_editor_property("config", config_da)
unreal.BlueprintEditorLibrary.compile_blueprint(bp_gm)
editor_asset_lib.save_loaded_asset(bp_gm)

# 7. MAP_MainMenu level
if not editor_asset_lib.does_asset_exist(LEVEL_PATH):
    level_subsys.new_level(LEVEL_PATH, False)
else:
    level_subsys.load_level(LEVEL_PATH)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
ws = world.get_world_settings()
ws.set_editor_property("default_game_mode", bp_gm.generated_class())
level_subsys.save_current_level()
print("SETUP_MAIN_MENU_OK", LEVEL_PATH)
