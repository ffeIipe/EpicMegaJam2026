import unreal
FBX='C:/Users/felipe/AppData/Local/Temp/ap/out/test17.fbx'
DEST='/Game/_NpcFixTest'
t=unreal.AssetImportTask()
t.set_editor_property('filename',FBX); t.set_editor_property('destination_path',DEST)
t.set_editor_property('automated',True); t.set_editor_property('replace_existing',True); t.set_editor_property('save',False)
o=unreal.FbxImportUI()
o.set_editor_property('import_mesh',True); o.set_editor_property('import_as_skeletal',True)
o.set_editor_property('import_materials',False); o.set_editor_property('import_textures',False); o.set_editor_property('import_animations',False)
t.set_editor_property('options',o)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
sm=unreal.load_asset(DEST+'/test17')
print('NPCFIX SM',sm, sm.get_bounds() if sm else None)
sk=sm.get_editor_property('skeleton')
print('NPCFIX SK',sk)
# bone names + reference transforms via an AnimSequence-free route
sub=unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
print('NPCFIX attrs',[a for a in dir(sk) if 'bone' in a.lower() or 'ref' in a.lower() or 'pose' in a.lower()])
rp=sk.get_reference_pose()
print('NPCFIX refpose type',type(rp), len(rp) if hasattr(rp,'__len__') else '')
try:
    names=sk.get_editor_property('bone_tree')
except Exception as e: print('NPCFIX bt',e)
meta=unreal.AnimationLibrary
print('NPCFIX lib',[a for a in dir(meta) if 'bone' in a.lower()][:15])
ns=[str(n) for n in rp.get_bone_names()]
print('NPCFIX nbones',len(ns),ns[:3])
S=unreal.AnimPoseSpaces
for n in ['root','Hips','LeftArm','RightArm','LeftToeBase']:
    if n in ns:
        for sp,tag in ((S.WORLD,'W'),(S.LOCAL,'L')):
            t=rp.get_bone_pose(n,sp); print('NPCFIX',tag,n,t.translation,t.rotation.rotator(),t.scale3d)
old=unreal.load_asset('/Game/Characters/Rigs/Character_41_Female_Skeleton').get_reference_pose()
on=[str(n) for n in old.get_bone_names()]
print('NPCFIX OLD',len(on),on[:3])
for n in ['root','Hips','mixamorig:LeftArm']:
    if n in on:
        t=old.get_bone_pose(n,S.WORLD); print('NPCFIX OLDW',n,t.translation,t.rotation.rotator(),t.scale3d)
