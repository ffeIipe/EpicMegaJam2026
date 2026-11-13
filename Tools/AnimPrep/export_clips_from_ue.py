# Step 1b of 3: run with execute_python after prepending CLIPS = [('Female', 'Female_Idle'), ...]. Dumps each source clip's local poses to Saved/Temp/rt.
import json, os, time
import unreal

# CLIPS is prepended by the caller: [(gender, clip), ...]
OUT = 'I:/UnrealProjects/EpicMegaJam2026/Saved/Temp/rt'
t0 = time.time()
for gender, clip in CLIPS:
    names = json.load(open(f'{OUT}/rig_src_{gender}.json'))['names']
    anim = unreal.load_asset(f'/Game/Characters/Anims/MixamoSrcB/{gender}/Clips/{clip}')
    keys = anim.get_editor_property('data_model_interface').get_number_of_keys()
    frames = []
    for f in range(keys):
        poses = unreal.AnimationLibrary.get_bone_poses_for_frame(anim, names, f, False)
        frames.append([[t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w, t.translation.x, t.translation.y, t.translation.z] for t in poses])
    with open(f'{OUT}/clip_{clip}.json', 'w') as fh:
        json.dump({'gender': gender, 'names': names, 'keys': keys, 'frames': frames}, fh)
    print('OUT', clip, keys, 'keys', round(time.time() - t0, 2), 's')

