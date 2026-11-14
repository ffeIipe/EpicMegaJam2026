import unreal, sys
FILES = [f for f in __import__('os').environ.get('NPCFIX_FILES','').split(';') if f]
DEST = '/Game/_NpcFixTest'
S = unreal.AnimPoseSpaces
def imp(fbx):
    t = unreal.AssetImportTask()
    for k, v in (('filename', fbx), ('destination_path', DEST), ('automated', True), ('replace_existing', True), ('save', False)):
        t.set_editor_property(k, v)
    o = unreal.FbxImportUI()
    for k, v in (('import_mesh', True), ('import_as_skeletal', True), ('import_materials', False), ('import_textures', False), ('import_animations', False)):
        o.set_editor_property(k, v)
    t.set_editor_property('options', o)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    return unreal.load_asset(DEST + '/' + fbx.split('/')[-1][:-4])
def f(v): return '(%.1f,%.1f,%.1f)' % (v.x, v.y, v.z)
for fbx in FILES:
    sm = imp(fbx); sk = sm.get_editor_property('skeleton'); p = sk.get_reference_pose()
    names = [str(n) for n in p.get_bone_names()]
    h = p.get_bone_pose('Hips', S.WORLD); la = p.get_bone_pose('LeftArm', S.WORLD); ra = p.get_bone_pose('RightArm', S.WORLD)
    hl = p.get_bone_pose('Hips', S.LOCAL); sp = p.get_bone_pose('Spine', S.LOCAL)
    b = sm.get_bounds()
    print('NPCFIX', fbx.split('/')[-1], 'n=%d first=%s' % (len(names), names[0]), 'HipsW', f(h.translation), 'scale', f(h.scale3d), 'rot', h.rotation.rotator(),
          '| LArmW', f(la.translation), 'RArmW', f(ra.translation), '| SpineLocal', f(sp.translation), 'ext', f(b.box_extent))
