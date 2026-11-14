import unreal, collections
ar = unreal.AssetRegistryHelpers.get_asset_registry()
assets = ar.get_assets_by_path('/Game/Characters/NPCs', recursive=True)
cnt = collections.Counter(); rows = []
for a in assets:
    if str(a.asset_class_path.asset_name) != 'SkeletalMesh': continue
    sm = a.get_asset()
    sk = sm.get_editor_property('skeleton')
    nb = len(sk.get_reference_pose().get_bone_names()) if sk else 0
    key = (sk.get_path_name() if sk else None, nb); cnt[key] += 1
    rows.append((str(a.asset_name), sk.get_name() if sk else None, nb, len(sm.get_editor_property('materials'))))
for k, v in cnt.items(): print('NPCFIX SKEL', v, k)
for r in rows[:6]: print('NPCFIX ROW', r)
print('NPCFIX total meshes', len(rows))
