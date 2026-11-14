# Step 1a of 3: run with execute_python. Dumps the reference poses of the Mixamo source and NPC skeletons to Saved/Temp/rt.
import json, os, time
import unreal

OUT = 'I:/UnrealProjects/EpicMegaJam2026/Saved/Temp/rt'
os.makedirs(OUT, exist_ok=True)
t0 = time.time()
W, L = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL

def tr(t):
    return {'r': [t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w], 'p': [t.translation.x, t.translation.y, t.translation.z],
            's': [t.scale3d.x, t.scale3d.y, t.scale3d.z]}

def dump(tag, skeleton_path, anim_for_parents):
    skel = unreal.load_asset(skeleton_path)
    pose = skel.get_reference_pose()
    names = [str(n) for n in pose.get_bone_names()]
    data = {'names': names, 'world': {}, 'local': {}, 'parent': {}}
    for n in names:
        data['world'][n] = tr(pose.get_bone_pose(n, W))
        data['local'][n] = tr(pose.get_bone_pose(n, L))
    anim = unreal.load_asset(anim_for_parents)
    for n in names:
        path = [str(p) for p in unreal.AnimationLibrary.find_bone_path_to_root(anim, n)]
        data['parent'][n] = path[1] if len(path) > 1 else None
    with open(f'{OUT}/rig_{tag}.json', 'w') as f:
        json.dump(data, f)
    print('OUT', tag, len(names), 'bones', round(time.time() - t0, 2), 's')

anim_f = '/Game/Characters/Anims/MixamoSrcB/Female/Clips/Female_Idle'
anim_m = '/Game/Characters/Anims/MixamoSrcB/Male/Clips/Male_Idle'
dump('src_Female', '/Game/Characters/Anims/MixamoSrcB/Female/Mixamo_Female_Source_Skeleton', anim_f)
dump('tgt_Female', '/Game/Characters/Rigs/Character_41_Female_Skeleton', anim_f)
dump('src_Male', '/Game/Characters/Anims/MixamoSrcB/Male/Mixamo_Male_Source_Skeleton', anim_m)
dump('tgt_Male', '/Game/Characters/Rigs/Character_Male_36_Skeleton', anim_m)

