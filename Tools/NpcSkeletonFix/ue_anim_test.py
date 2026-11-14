import unreal, os
DEST = '/Game/_NpcFixTest'
S = unreal.AnimPoseSpaces
NPC_FBX = os.environ['NPCFIX_NPC']
MIX = 'F:/Descargas/'
def run(fbx, dest, **kw):
  try:
    _run(fbx, dest, **kw)
  except RuntimeError as e:
    print("NPCFIX warn", str(e)[:80])
def _run(fbx, dest, **kw):
    t = unreal.AssetImportTask()
    for k, v in (('filename', fbx), ('destination_path', dest), ('automated', True), ('replace_existing', True), ('save', False)):
        t.set_editor_property(k, v)
    o = unreal.FbxImportUI()
    for k, v in kw.items(): o.set_editor_property(k, v)
    t.set_editor_property('options', o)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
base = dict(import_materials=False, import_textures=False)
run(NPC_FBX, DEST, import_mesh=True, import_as_skeletal=True, import_animations=False, **base)
npc_sk = unreal.load_asset(DEST + '/' + NPC_FBX.split('/')[-1][:-4]).get_editor_property('skeleton')
run(MIX + 'Ch36_nonPBR.fbx', DEST + '/Ch36', import_mesh=True, import_as_skeletal=True, import_animations=False, **base)
ch_sk = unreal.load_asset(DEST + '/Ch36/Ch36_nonPBR').get_editor_property('skeleton')
def v(x): return '(%.1f,%.1f,%.1f)' % (x.x, x.y, x.z)
def refframes(tag, sk):
    p = sk.get_reference_pose()
    for n in ('Hips', 'LeftArm', 'LeftFoot', 'LeftToeBase'):
        t = p.get_bone_pose(n, S.WORLD); l = p.get_bone_pose(n, S.LOCAL)
        print('NPCFIX REF', tag, n, 'world', t.rotation.rotator(), 'worldT', v(t.translation))
refframes('NPC', npc_sk); refframes('CH36', ch_sk)
for clip in ('Walking',):
    out = {}
    for tag, sk in (('NPC', npc_sk), ('CH36', ch_sk)):
        run(MIX + 'Ch36_nonPBR@%s.fbx' % clip, DEST + '/' + tag, import_mesh=False, import_animations=True, mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION, skeleton=sk, **base)
        a = unreal.load_asset('%s/%s/Ch36_nonPBR@%s' % (DEST, tag, clip)) or unreal.load_asset('%s/%s/Ch36_nonPBR_%s' % (DEST, tag, clip))
        print('NPCFIX anim', tag, a, a.get_editor_property('sequence_length') if a else None)
        opt = unreal.AnimPoseEvaluationOptions(); opt.set_editor_property('evaluation_type', unreal.AnimPoseEvaluationType.ANIM_POSE_EVALUATION_TYPE_RAW) if hasattr(unreal, 'AnimPoseEvaluationType') else None
        pose = a.get_anim_pose_at_time(0.3, opt)
        h = pose.get_bone_pose('Hips', S.WORLD).translation
        for n in ('Head', 'LeftHand', 'RightHand', 'LeftFoot', 'RightFoot'):
            w = pose.get_bone_pose(n, S.WORLD).translation
            print('NPCFIX POSE', tag, n, 'rel-hips', v(unreal.Vector(w.x - h.x, w.y - h.y, w.z - h.z)))
