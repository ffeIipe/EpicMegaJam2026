import unreal, collections, json
ar = unreal.AssetRegistryHelpers.get_asset_registry()
rows = {}
for a in ar.get_assets_by_path('/Game/Characters', recursive=False):
    if str(a.asset_class_path.asset_name) == 'MaterialInstanceConstant':
        p = a.get_asset().get_editor_property('parent'); rows[str(a.asset_name)] = p.get_name() if p else None
mats = json.load(open('I:/UnrealProjects/EpicMegaJam2026/Saved/Temp/npcfix/materials.json'))
for k, v in mats.items():
    mi = (v[0][1] or '').split('.')[-1]
    if rows.get(mi) != 'M_NPC_Master': print('NPCFIX NOT-ON-MASTER', k.split('/')[-1], 'slot', v[0], 'parent', rows.get(mi))
cdo = unreal.get_default_object(unreal.load_asset('/Game/DirectorsCut/Characters/BP_Client').generated_class())
mv = cdo.get_editor_property('character_movement')
print('NPCFIX BP_Client max_walk_speed', mv.get_editor_property('max_walk_speed'), 'accel', mv.get_editor_property('max_acceleration'), 'ai', cdo.get_editor_property('ai_controller_class'))
