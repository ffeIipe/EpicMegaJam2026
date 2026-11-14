import unreal
for cls in ('Skeleton', 'AnimSequence', 'AnimationLibrary', 'EditorAssetLibrary', 'BlueprintEditorLibrary'):
    c = getattr(unreal, cls, None)
    print('NPCFIX API', cls, [m for m in dir(c) if any(k in m.lower() for k in ('retarget', 'translation', 'compile', 'delete', 'rename', 'save_dir', 'virtual'))] if c else None)
print('NPCFIX has', [n for n in dir(unreal) if 'Retargeting' in n or 'SkeletonModifier' in n][:10])
