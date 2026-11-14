"""Step before replacing the NPC meshes: records each mesh's material slots so ue_apply.py can restore them."""
import unreal, json, os
ar = unreal.AssetRegistryHelpers.get_asset_registry()
out = {}
for a in ar.get_assets_by_path('/Game/Characters/NPCs', recursive=True):
    if str(a.asset_class_path.asset_name) != 'SkeletalMesh': continue
    sm = a.get_asset()
    out[str(a.package_name)] = [(str(m.get_editor_property('material_slot_name')), m.get_editor_property('material_interface').get_path_name() if m.get_editor_property('material_interface') else None) for m in sm.get_editor_property('materials')]
os.makedirs('I:/UnrealProjects/EpicMegaJam2026/Saved/Temp/npcfix', exist_ok=True)
json.dump(out, open('I:/UnrealProjects/EpicMegaJam2026/Saved/Temp/npcfix/materials.json', 'w'), indent=1)
print('NPCFIX dumped', len(out), 'meshes; slot counts', sorted(set(len(v) for v in out.values())))
