import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for a in ar.get_assets_by_path('/Game/DirectorsCut', recursive=True):
    n = str(a.asset_name)
    if 'Client' in n or n.startswith('ABP_') or 'NPC' in n: print('NPCFIX ASSET', a.package_name, a.asset_class_path.asset_name)
for p in ('/Game/DirectorsCut/Characters/ABP_NPC_Female', '/Game/DirectorsCut/Characters/ABP_NPC_Male'):
    abp = unreal.load_asset(p)
    print('NPCFIX ABP', p, abp, abp.get_editor_property('target_skeleton') if abp else None)
    print('NPCFIX ABPdeps', [str(d) for d in ar.get_dependencies(unreal.Name(p), unreal.AssetRegistryDependencyOptions())][:40])
