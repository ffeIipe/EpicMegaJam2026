import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry(); bad = []; n = 0
for a in ar.get_assets_by_path('/Game/Characters/NPCs', recursive=True):
    if str(a.asset_class_path.asset_name) != 'SkeletalMesh': continue
    n += 1
    for m in a.get_asset().get_editor_property('materials'):
        mi = m.get_editor_property('material_interface'); p = mi.get_editor_property('parent') if mi and isinstance(mi, unreal.MaterialInstanceConstant) else None
        if not p or p.get_name() != 'M_NPC_Master': bad.append(a.asset_name)
print('NPCFIX final: meshes', n, 'not on M_NPC_Master:', [str(b) for b in bad])
