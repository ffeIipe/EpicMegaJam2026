"""Builds the Mixamo *source* for retargeting onto the NPC skeletons.

The NPC skeletons keep bones in meters under a root with scale 100 and Blender bone axes, so Mixamo clips can't be
imported on them directly (the mesh skins wrong). Instead: import a Mixamo skeleton (with a tiny skinned mesh, UE needs a
mesh to make a skeleton) plus the clips exactly as Mixamo made them (in place for locomotion), and let UE's IK
Retargeter convert them to the NPC skeletons.

Usage (Blender 4.3 or newer):
  blender --background --factory-startup --python make_source.py -- <out_dir> <Male|Female> <in1.fbx> [<in2.fbx> ...]
Writes <out_dir>/<Gender>/Mixamo_<Gender>_Source.fbx (skeleton + mesh) and <out_dir>/<Gender>/Anims/<Gender>_<Clip>.fbx.
"""
import sys
from math import radians
from pathlib import Path

import bpy
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
import add_root as ar  # noqa: E402  (reuses make_in_place and INPLACE_KEYWORDS)

HIPS = ar.PREFIX + "Hips"


def load(src: Path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(src))
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    first, last = arm.animation_data.action.frame_range
    bpy.context.scene.frame_start, bpy.context.scene.frame_end = int(first), int(last)
    return arm


def export(dst: Path, types):
    dst.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.fbx(
        filepath=str(dst), object_types=types, add_leaf_bones=False, use_armature_deform_only=False,
        bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
        bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0,
    )


def make_mesh(src: Path, dst: Path):
    arm = load(src)
    bpy.context.scene.frame_set(int(bpy.context.scene.frame_start))
    hips_world = arm.matrix_world @ arm.pose.bones[HIPS].head
    bpy.ops.mesh.primitive_cube_add(size=0.2, location=hips_world)
    cube = bpy.context.active_object
    cube.name = "SourceProxy"
    cube.vertex_groups.new(name=HIPS).add(list(range(len(cube.data.vertices))), 1.0, "REPLACE")
    mod = cube.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    cube.parent = arm
    add_root_bone(arm)
    export(dst, {"ARMATURE", "MESH"})
    print("MESH", dst.name)


def add_root_bone(arm):
    """Same `root` the NPC skeletons have: parent of Hips, identity rest pose (UE turns the first bone upright itself)."""
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    root = arm.data.edit_bones.new(ar.ROOT)
    root.head, root.tail = Vector((0, 0, 0)), Vector((0, 10, 0))
    arm.data.edit_bones[HIPS].parent = root
    bpy.ops.object.mode_set(mode="OBJECT")


def stand_up(arm, degrees=90.0):
    """UE reads the clip's Y-up data as written: turning `root` upright (checked with analyze_animation) fixes the pose."""
    pb = arm.pose.bones[ar.ROOT]
    pb.rotation_mode = "XYZ"
    pb.rotation_euler = (radians(degrees), 0, 0)
    f0, f1 = int(bpy.context.scene.frame_start), int(bpy.context.scene.frame_end)
    pb.keyframe_insert("rotation_euler", frame=f0)
    pb.keyframe_insert("rotation_euler", frame=f1)


def make_clip(src: Path, dst: Path):
    arm = load(src)
    if any(k in src.stem.lower() for k in ar.INPLACE_KEYWORDS):
        ar.make_in_place(arm, HIPS)
    add_root_bone(arm)
    stand_up(arm)
    export(dst, {"ARMATURE"})
    print("CLIP", src.name, "->", dst.name)


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    out_dir, gender, files = Path(args[0]), args[1], [Path(a) for a in args[2:]]
    make_mesh(next(f for f in files if f.stem == "idle"), out_dir / gender / f"Mixamo_{gender}_Source.fbx")
    for f in files:
        name = "_".join(w.capitalize() for w in f.stem.replace(" 90", "").split())
        make_clip(f, out_dir / gender / "Clips" / f"{gender}_{name}.fbx")


main()
