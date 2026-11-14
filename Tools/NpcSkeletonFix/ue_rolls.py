"""Measures, per bone, the twist about the bone's own axis between an NPC rig and a real Mixamo rig (Ch36), and writes
rolls.json for fix_npc.py. Run with NPCFIX_FILES="a.fbx;b.fbx" on the calibration FBXs (fix_npc.py output made with an empty rolls.json)."""
import unreal, os, math, json
DEST = '/Game/_NpcFixTest'
S = unreal.AnimPoseSpaces
def imp(fbx, dest):
    t = unreal.AssetImportTask()
    for k, v in (('filename', fbx), ('destination_path', dest), ('automated', True), ('replace_existing', True), ('save', False)):
        t.set_editor_property(k, v)
    o = unreal.FbxImportUI()
    for k, v in (('import_mesh', True), ('import_as_skeletal', True), ('import_materials', False), ('import_textures', False), ('import_animations', False)):
        o.set_editor_property(k, v)
    t.set_editor_property('options', o)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    return unreal.load_asset(dest + '/' + fbx.split('/')[-1][:-4]).get_editor_property('skeleton')
def mul(a, b):
    return (a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1], a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
            a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3], a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2])
def q(t): return (t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w)
def conj(a): return (-a[0], -a[1], -a[2], a[3])
def twist(rel): return (math.degrees(2 * math.atan2(rel[1], rel[3])) + 180) % 360 - 180
ch = unreal.load_asset(DEST + '/Ch36/Ch36_nonPBR')
ch = ch.get_editor_property('skeleton') if ch else imp('F:/Descargas/Ch36_nonPBR.fbx', DEST + '/Ch36')
pc = ch.get_reference_pose(); cn = set(str(n) for n in pc.get_bone_names())
merged = {}
for fbx in [f for f in os.environ['NPCFIX_FILES'].split(';') if f]:
    pn = imp(fbx, DEST + '/Cal').get_reference_pose()
    for n in [str(n) for n in pn.get_bone_names()]:
        if n not in cn: continue
        tw = twist(mul(conj(q(pn.get_bone_pose(n, S.WORLD))), q(pc.get_bone_pose(n, S.WORLD))))
        if n in merged and abs((merged[n] - tw + 180) % 360 - 180) > 15:
            print('NPCFIX DISAGREE', n, merged[n], round(tw, 1), fbx.split('/')[-1])
        merged[n] = round(tw, 1)
rolls = {n: d for n, d in merged.items() if abs(d) > 20}
json.dump(rolls, open('I:/UnrealProjects/EpicMegaJam2026/Tools/NpcSkeletonFix/rolls.json', 'w'), indent=1)
print('NPCFIX rolls', len(rolls), 'of', len(merged), rolls)
