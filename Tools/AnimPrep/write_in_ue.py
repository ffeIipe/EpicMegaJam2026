"""Step 3 of 3: writes the tracks retarget_offline.py computed into an AnimSequence on the NPC skeleton (run with execute_python).

Short calls on purpose (the first all-in-one attempt crashed the editor): the caller prepends
  CLIP, GENDER, START, END, CREATE, SAVE
and this creates the asset when CREATE is set, writes the bones names[START:END], and saves when SAVE is set.
"""
import json

import unreal

RT = 'I:/UnrealProjects/EpicMegaJam2026/Saved/Temp/rt'
OUT_ROOT = '/Game/Characters/Anims/Final'
SKELETON = {'Male': '/Game/Characters/Rigs/Character_Male_36_Skeleton', 'Female': '/Game/Characters/Rigs/Character_41_Female_Skeleton'}

data = json.load(open(f'{RT}/out_{CLIP}.json'))
names = json.load(open(f'{RT}/rig_tgt_{GENDER}.json'))['names']
path = f'{OUT_ROOT}/{GENDER}/{CLIP}'

if CREATE:
    skeleton = unreal.load_asset(SKELETON[GENDER])
    factory = unreal.AnimSequenceFactory()
    factory.set_editor_property('target_skeleton', skeleton)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(CLIP, f'{OUT_ROOT}/{GENDER}', unreal.AnimSequence, factory)
    ctl = asset.controller
    ctl.open_bracket(unreal.Text('retarget'), False)
    ctl.set_frame_rate(unreal.FrameRate(30, 1), False)
    ctl.set_number_of_frames(unreal.FrameNumber(data['keys'] - 1), False)
    ctl.close_bracket(False)
else:
    asset = unreal.load_asset(path)
    ctl = asset.controller

ctl.open_bracket(unreal.Text('retarget tracks'), False)
for n in names[START:END]:
    t = data['tracks'][n]
    ctl.add_bone_curve(unreal.Name(n), False)
    keys = data['keys']
    # The controller wants the same number of keys in all three arrays: repeat the constant ones
    pos = [unreal.Vector(*p) for p in t['pos']] if len(t['pos']) == keys else [unreal.Vector(*t['pos'][0])] * keys
    scl = [unreal.Vector(*s) for s in t['scl']] if len(t['scl']) == keys else [unreal.Vector(*t['scl'][0])] * keys
    ctl.set_bone_track_keys(unreal.Name(n), pos, [unreal.Quat(*r) for r in t['rot']], scl, False)
ctl.close_bracket(False)
if SAVE:
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
print('OUT wrote', CLIP, names[START:END][:3], '...', 'saved' if SAVE else '')
