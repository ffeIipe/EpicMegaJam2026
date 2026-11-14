import unreal, os, math
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
def ang(a, b):
    d = abs(a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w)
    return math.degrees(2*math.acos(min(1.0, d)))
npc = imp(os.environ['NPCFIX_NPC'], DEST + '/D')
ch = unreal.load_asset(DEST + '/Ch36/Ch36_nonPBR')
ch = ch.get_editor_property('skeleton') if ch else imp('F:/Descargas/Ch36_nonPBR.fbx', DEST + '/Ch36')
pn, pc = npc.get_reference_pose(), ch.get_reference_pose()
names = [str(n) for n in pn.get_bone_names()]
cn = set(str(n) for n in pc.get_bone_names())
bad = 0
for n in names:
    if n not in cn: print('NPCFIX missing-in-mixamo', n); continue
    a = pn.get_bone_pose(n, S.LOCAL).rotation; b = pc.get_bone_pose(n, S.LOCAL).rotation
    e = ang(a, b); bad += e > 15
    print('NPCFIX LOC %-16s err=%6.1f deg' % (n, e))
print('NPCFIX bones>15deg:', bad, 'of', len(names))
