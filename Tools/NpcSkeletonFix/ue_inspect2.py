import unreal, collections
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for sk in ('/Game/Characters/Rigs/Character_41_Female_Skeleton', '/Game/Characters/Rigs/Character_Male_36_Skeleton'):
    refs = ar.get_referencers(unreal.Name(sk), unreal.AssetRegistryDependencyOptions())
    c = collections.Counter(str(r).rsplit('/', 1)[0] for r in refs)
    print('NPCFIX REFS', sk.split('/')[-1], len(refs), dict(c))
    for r in refs:
        if '/NPCs/' not in str(r) and '/Anims/' not in str(r): print('NPCFIX  other-ref', r)
bp = unreal.load_asset('/Game/DirectorsCut/Characters/BP_Client')
cdo = unreal.get_default_object(bp.generated_class())
m = cdo.get_editor_property('mesh')
print('NPCFIX BP_Client mesh rel rot', m.get_editor_property('relative_rotation'), 'loc', m.get_editor_property('relative_location'), 'AnimBySkeleton', cdo.get_editor_property('anim_by_skeleton'))
print('NPCFIX capsule half', cdo.get_editor_property('capsule_component').get_editor_property('capsule_half_height'))
