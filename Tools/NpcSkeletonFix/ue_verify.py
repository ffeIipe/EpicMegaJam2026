import unreal
S = unreal.AnimPoseSpaces
for g, sk in (('Female', 'Character_41_Female_Skeleton'), ('Male', 'Character_Male_36_Skeleton')):
    abp = unreal.load_asset('/Game/DirectorsCut/Characters/ABP_NPC_' + g)
    unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    print('NPCFIX ABP', g, 'skeleton=', abp.get_editor_property('target_skeleton').get_name(), 'status=', abp.get_editor_property('status'))
bp = unreal.load_asset('/Game/DirectorsCut/Characters/BP_Client')
m = unreal.get_default_object(bp.generated_class()).get_editor_property('anim_by_skeleton')
print('NPCFIX BP map', {k.get_name() if k else None: (v.get_name() if v else None) for k, v in m.items()})
for p in ('Female/Character_17_Female_Police', 'Male/Character_01', 'Female/Character_56_Female_Exotic_dancer_01'):
    sm = unreal.load_asset('/Game/Characters/NPCs/%s/%s' % (p, p.split('/')[1]))
    print('NPCFIX MESH', p, sm.get_editor_property('skeleton').get_name(), [mm.get_editor_property('material_interface').get_name() for mm in sm.get_editor_property('materials')], sm.get_bounds().box_extent)
for n in ('Female/Female_Walking', 'Female/Female_Idle', 'Male/Male_Walking', 'Male/Male_Standard_Run', 'Male/Male_Idle'):
    a = unreal.load_asset('/Game/Characters/Anims/Final/' + n)
    L = a.get_editor_property('sequence_length')
    p0 = a.get_anim_pose_at_time(0.0, unreal.AnimPoseEvaluationOptions()).get_bone_pose('Hips', S.WORLD).translation
    p1 = a.get_anim_pose_at_time(L - 0.05, unreal.AnimPoseEvaluationOptions()).get_bone_pose('Hips', S.WORLD).translation
    print('NPCFIX ANIM %s len=%.2fs hips travel xy=(%.0f,%.0f) cm' % (n, L, p1.x - p0.x, p1.y - p0.y))
