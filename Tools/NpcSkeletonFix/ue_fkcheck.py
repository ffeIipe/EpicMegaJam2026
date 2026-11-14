import unreal, os, math
DEST = '/Game/_NpcFixTest'
S = unreal.AnimPoseSpaces
def run(fbx, dest, **kw):
    t = unreal.AssetImportTask()
    for k, v in (('filename', fbx), ('destination_path', dest), ('automated', True), ('replace_existing', True), ('save', False)):
        t.set_editor_property(k, v)
    o = unreal.FbxImportUI()
    for k, v in dict(import_materials=False, import_textures=False, **kw).items(): o.set_editor_property(k, v)
    t.set_editor_property('options', o)
    try: unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    except RuntimeError as e: pass  # missing finger bones on 33-bone rigs
npc_fbx = os.environ['NPCFIX_NPC']
run(npc_fbx, DEST + '/K', import_mesh=True, import_as_skeletal=True, import_animations=False)
sk = unreal.load_asset(DEST + '/K/' + npc_fbx.split('/')[-1][:-4]).get_editor_property('skeleton')
CLIP = os.environ.get('NPCFIX_CLIP', 'Walking')
run('F:/Descargas/Ch36_nonPBR@%s.fbx' % CLIP, DEST + '/K', import_mesh=False, import_animations=True, mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION, skeleton=sk)
a_npc = unreal.load_asset('%s/K/Ch36_nonPBR_%s' % (DEST, CLIP))
run('F:/Descargas/Ch36_nonPBR.fbx', DEST + '/Ch36', import_mesh=True, import_as_skeletal=True, import_animations=False)
ch_sk = unreal.load_asset(DEST + '/Ch36/Ch36_nonPBR').get_editor_property('skeleton')
run('F:/Descargas/Ch36_nonPBR@%s.fbx' % CLIP, DEST + '/Ch36', import_mesh=False, import_animations=True, mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION, skeleton=ch_sk)
a_ch = unreal.load_asset('%s/Ch36/Ch36_nonPBR_%s' % (DEST, CLIP))
def qm(a, b): return (a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1], a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0], a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3], a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2])
def qr(q, v):
    p = qm(qm(q, (v[0], v[1], v[2], 0)), (-q[0], -q[1], -q[2], q[3])); return (p[0], p[1], p[2])
ref = sk.get_reference_pose(); names = [str(n) for n in ref.get_bone_names()]
par = {}
for n in names:
    p = [str(x) for x in unreal.AnimationLibrary.find_bone_path_to_root(a_npc, n)]
    par[n] = p[1] if len(p) > 1 else None
def fk(t):
    W = {}
    for n in names:  # parents come first in ref pose order
        loc = a_npc.get_bone_pose_for_time if False else None
        tr = unreal.AnimationLibrary.get_bone_pose_for_time(a_npc, n, t, False)
        r = (tr.rotation.x, tr.rotation.y, tr.rotation.z, tr.rotation.w)
        rl = ref.get_bone_pose(n, S.LOCAL)
        # Mixamo clip translation for Hips (height), NPC reference translation for every other bone
        tv = (tr.translation.x, tr.translation.y, tr.translation.z) if par[n] is None else (rl.translation.x, rl.translation.y, rl.translation.z)
        if par[n] is None: W[n] = (tv, r)
        else:
            pp, pq = W[par[n]]; o = qr(pq, tv); W[n] = ((pp[0]+o[0], pp[1]+o[1], pp[2]+o[2]), qm(pq, r))
    return W
tot = 0; cnt = 0
for t in (0.2, 0.5, 0.8):
    W = fk(t); pose = a_ch.get_anim_pose_at_time(t, unreal.AnimPoseEvaluationOptions())
    h = pose.get_bone_pose('Hips', S.WORLD).translation; hw = W['Hips'][0]
    row = []
    for n in ('Head', 'LeftHand', 'RightHand', 'LeftFoot', 'RightFoot', 'LeftForeArm', 'RightLeg'):
        w = pose.get_bone_pose(n, S.WORLD).translation
        ref_v = (w.x - h.x, w.y - h.y, w.z - h.z); mine = tuple(W[n][0][i] - hw[i] for i in range(3))
        d = math.dist(ref_v, mine); tot += d; cnt += 1; row.append('%s %.1f' % (n, d))
    print('NPCFIX FK t=%.1f diff(cm) vs Mixamo: %s' % (t, ', '.join(row)))
print('NPCFIX FK mean diff %.1f cm' % (tot / cnt))
