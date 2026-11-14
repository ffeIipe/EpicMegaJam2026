"""Make a PSX NPC FBX import into UE with a Mixamo-compatible skeleton, so Mixamo clips play on it unchanged.

Run: blender -b --python fix_npc.py -- <in.fbx> <out.fbx>
 or: blender -b --python fix_npc.py -- <manifest.txt>      (one `in.fbx|out.fbx` per line)

What it fixes (see README.md):
 1. The source armature carries a 90deg-X object rotation and meters: UE turned that into a `root` bone with scale 100.
    Baked here, the hierarchy starts at Hips in real cm.
 2. Character faces -Y (Blender) like a Mixamo character, arms in a T-pose, so rest rotations line up with Mixamo's.
 3. Per-bone roll set to Mixamo's frames (roll_z), so local rest rotations match within ~10deg.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
P = 'mixamorig:'


def select(objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]


def clear_pose(arm):
    select([arm])
    bpy.ops.object.mode_set(mode='POSE')
    bpy.ops.pose.select_all(action='SELECT')
    bpy.ops.pose.transforms_clear()
    bpy.ops.object.mode_set(mode='OBJECT')


def roll_z(n):
    """World direction of a bone's Z axis in Mixamo's T-pose frames (measured on a real Mixamo rig)."""
    if n.endswith('Foot'):
        return Vector((0, -1, 1))
    if 'Toe' in n:
        return Vector((0, 0, 1))
    if n.startswith(('Left', 'Right')) and not n.endswith('Leg'):
        return Vector((0, 0, -1))  # shoulders, arms, hands, fingers
    return Vector((0, -1, 0))  # Hips, spine, neck, head, legs


def process(src, dst):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=src)
    arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    meshes = [o for o in bpy.data.objects if o.type == 'MESH']

    # The FBX stores a pose that differs from the rest pose; work from the rest pose.
    clear_pose(arm)

    # 1. Facing: rotate about Z so the feet point to -Y (Mixamo's forward in Blender). Also meters -> cm.
    rest = lambda n: arm.matrix_world @ arm.data.bones[P + n].head_local
    fwd = rest('LeftToeBase') - rest('LeftFoot')
    turn = -math.pi / 2 - math.atan2(fwd.y, fwd.x)
    for m in meshes:
        mw = m.matrix_world.copy()
        m.parent = None
        m.matrix_world = mw
    for o in [arm] + meshes:
        o.matrix_world = Matrix.Rotation(turn, 4, 'Z') @ Matrix.Scale(100, 4) @ o.matrix_world
    bpy.context.scene.unit_settings.scale_length = 0.01  # 1 unit = 1 cm: the FBX then carries no unit conversion
    select([arm] + meshes)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    # 2. Arms -> T-pose along the shoulder line.
    select([arm])
    bpy.ops.object.mode_set(mode='POSE')
    pb = arm.pose.bones
    lat = pb[P + 'LeftArm'].head - pb[P + 'RightArm'].head
    lat.z = 0
    lat.normalize()

    def aim(bone, child, target):
        """Rotate `bone` around its head so that head -> child head points along `target`."""
        b, c = pb[P + bone], pb[P + child]
        q = (c.head - b.head).normalized().rotation_difference(target)
        h = b.head.copy()
        b.matrix = Matrix.Translation(h) @ q.to_matrix().to_4x4() @ Matrix.Translation(-h) @ b.matrix
        bpy.context.view_layer.update()

    for side, t in (('Left', lat), ('Right', -lat)):
        aim(f'{side}Arm', f'{side}ForeArm', t)
        aim(f'{side}ForeArm', f'{side}Hand', t)
        tip = next((n for n in (f'{side}HandMiddle1', f'{side}HandIndex1') if P + n in pb), None)
        if tip:
            aim(f'{side}Hand', tip, t)
    bpy.ops.object.mode_set(mode='OBJECT')

    # 3. Bake the posed mesh, then make that pose the rest pose.
    for m in meshes:
        select([m])
        bpy.ops.object.modifier_apply(modifier=next(x.name for x in m.modifiers if x.type == 'ARMATURE'))
    select([arm])
    bpy.ops.object.mode_set(mode='POSE')
    bpy.ops.pose.select_all(action='SELECT')
    bpy.ops.pose.armature_apply()
    bpy.ops.object.mode_set(mode='EDIT')
    for eb in arm.data.edit_bones:
        eb.align_roll(roll_z(eb.name[len(P):]))
    bpy.ops.object.mode_set(mode='OBJECT')
    clear_pose(arm)  # the roll edit leaves a non-identity pose that would deform the mesh
    for m in meshes:
        m.modifiers.new('Armature', 'ARMATURE').object = arm
        m.parent = arm

    # 4. Export. No leaf bones; textures stay in UE (materials are reassigned from the old assets there).
    select([arm] + meshes)
    bpy.ops.export_scene.fbx(filepath=dst, use_selection=True, add_leaf_bones=False, bake_anim=False,
                             object_types={'ARMATURE', 'MESH'}, mesh_smooth_type='FACE', path_mode='STRIP')
    print('FIXED', dst)


args = sys.argv[sys.argv.index('--') + 1:]
if args[0].endswith('.txt'):
    for line in open(args[0]):
        if '|' in line:
            a, b = line.strip().split('|')
            try:
                process(a, b)
            except Exception as e:  # keep going: report at the end
                print('FAILED', a, repr(e))
else:
    process(args[0], args[1])
