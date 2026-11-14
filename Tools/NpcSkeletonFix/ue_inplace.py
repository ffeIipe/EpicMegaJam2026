"""Removes the linear horizontal travel of Hips from locomotion clips (movement comes from the capsule), keeping sway and height."""
import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
L = unreal.AnimationLibrary
for a in ar.get_assets_by_path('/Game/Characters/Anims/Final', recursive=True):
    n = str(a.asset_name)
    if not any(k in n.lower() for k in ('walk', 'run', 'strafe')): continue
    anim = a.get_asset(); ctl = anim.controller
    nk = L.get_num_keys(anim) if hasattr(L, 'get_num_keys') else L.get_num_frames(anim) + 1
    tr = [L.get_bone_pose_for_frame(anim, unreal.Name('Hips'), i, False) for i in range(nk)]
    d = tr[-1].translation - tr[0].translation
    pos, rot, scl = [], [], []
    for i, t in enumerate(tr):
        f = i / max(1, nk - 1)
        pos.append(unreal.Vector(t.translation.x - d.x * f, t.translation.y - d.y * f, t.translation.z))
        rot.append(t.rotation); scl.append(t.scale3d)
    ctl.open_bracket(unreal.Text('in place'), False)
    ctl.set_bone_track_keys(unreal.Name('Hips'), pos, rot, scl, False)
    ctl.close_bracket(False)
    p1 = L.get_bone_pose_for_frame(anim, unreal.Name('Hips'), nk - 1, False).translation
    p0 = L.get_bone_pose_for_frame(anim, unreal.Name('Hips'), 0, False).translation
    print('NPCFIX', n, 'keys', nk, 'travel was (%.0f,%.0f,%.0f) now (%.1f,%.1f,%.1f)' % (d.x, d.y, d.z, p1.x - p0.x, p1.y - p0.y, p1.z - p0.z))
    unreal.EditorAssetLibrary.save_loaded_asset(anim)
