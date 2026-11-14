"""Replaces the NPC meshes, their two skeletons and the locomotion clips with the Mixamo-compatible versions.
Run headless with the editor closed, after fix_npc.py has written the FBXs and the old assets were removed from disk."""
import unreal, json, os
FIXED = 'C:/Users/felipe/AppData/Local/Temp/ap/npc'
MAT = json.load(open('I:/UnrealProjects/EpicMegaJam2026/Saved/Temp/npcfix/materials.json'))
SP = 'C:/Users/felipe/AppData/Local/Temp/claude/3caca63e703670a3/904509be-a328-44cd-93f7-7b547157906e/scratchpad/female_pack/'
MALE = 'F:/Descargas/Male Locomotion Pack/'
SEED = {'Female': 'Character_41_Female', 'Male': 'Character_Male_36'}
CLIPS = {
    'Female': {'Female_Idle': SP + 'idle.fbx', 'Female_Jump': SP + 'jump.fbx', 'Female_Left_Strafe': SP + 'left strafe.fbx', 'Female_Left_Strafe_Walk': SP + 'left strafe walk.fbx',
               'Female_Left_Turn': SP + 'left turn.fbx', 'Female_Right_Strafe': SP + 'right strafe.fbx', 'Female_Right_Strafe_Walk': SP + 'right strafe walk.fbx',
               'Female_Right_Turn': SP + 'right turn.fbx', 'Female_Running': SP + 'running.fbx', 'Female_Walking': SP + 'walking.fbx'},
    'Male': {'Male_Idle': MALE + 'idle.fbx', 'Male_Jump': MALE + 'jump.fbx', 'Male_Left_Strafe': MALE + 'left strafe.fbx', 'Male_Left_Strafe_Walking': MALE + 'left strafe walking.fbx',
             'Male_Left_Turn': MALE + 'left turn 90.fbx', 'Male_Right_Strafe': MALE + 'right strafe.fbx', 'Male_Right_Strafe_Walking': MALE + 'right strafe walking.fbx',
             'Male_Right_Turn': MALE + 'right turn 90.fbx', 'Male_Standard_Run': MALE + 'standard run.fbx', 'Male_Walking': MALE + 'walking.fbx'},
}
fails = []
def run(fbx, dest, name=None, **kw):
    t = unreal.AssetImportTask()
    for k, v in (('filename', fbx), ('destination_path', dest), ('automated', True), ('replace_existing', True), ('save', False)):
        t.set_editor_property(k, v)
    if name: t.set_editor_property('destination_name', name)
    o = unreal.FbxImportUI()
    for k, v in dict(import_materials=False, import_textures=False, **kw).items(): o.set_editor_property(k, v)
    t.set_editor_property('options', o)
    try: unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    except RuntimeError as e: fails.append((fbx, str(e)[:120]))
for g in ('Female', 'Male'):
    seed = SEED[g]
    run('%s/%s/%s.fbx' % (FIXED, g, seed), '/Game/Characters/Rigs', import_mesh=True, import_as_skeletal=True, import_animations=False)
    sk = unreal.load_asset('/Game/Characters/Rigs/%s_Skeleton' % seed)
    print('NPCFIX skeleton', g, sk, len(sk.get_reference_pose().get_bone_names()))
    unreal.DCSkeletonTools.use_skeleton_proportions(sk)
    for f in sorted(os.listdir('%s/%s' % (FIXED, g))):
        n = f[:-4]; path = '/Game/Characters/NPCs/%s/%s' % (g, n)
        run('%s/%s/%s' % (FIXED, g, f), path, import_mesh=True, import_as_skeletal=True, import_animations=False, skeleton=sk)
        sm = unreal.load_asset('%s/%s' % (path, n))
        if not sm: fails.append((n, 'not created')); continue
        old = MAT.get('%s/%s' % (path, n))
        mats = list(sm.get_editor_property('materials'))
        if old and len(old) == len(mats):
            for m, (slot, mi) in zip(mats, old): m.set_editor_property('material_interface', unreal.load_asset(mi) if mi else None)
            sm.set_editor_property('materials', mats)
        else: fails.append((n, 'material slots %d vs %s' % (len(mats), len(old) if old else None)))
        if sm.get_editor_property('skeleton') != sk: fails.append((n, 'wrong skeleton'))
    for name, fbx in CLIPS[g].items():
        run(fbx, '/Game/Characters/Anims/Final/%s' % g, name=name, import_mesh=False, import_animations=True, mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION, skeleton=sk)
    unreal.EditorAssetLibrary.delete_asset('/Game/Characters/Rigs/%s' % seed)  # seed mesh was only needed to create the skeleton
for d in ('/Game/Characters/NPCs', '/Game/Characters/Rigs', '/Game/Characters/Anims/Final'):
    unreal.EditorAssetLibrary.save_directory(d, only_if_is_dirty=False)
for f in fails[:40]: print('NPCFIX FAIL', f)
print('NPCFIX apply done, failures:', len(fails))
