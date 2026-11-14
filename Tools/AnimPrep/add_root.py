"""Prepares Mixamo animation FBXs for the NPC skeletons (they use Mixamo bone names without the prefix, plus a `root` bone).

For every FBX: strip the `mixamorig:` prefix from the bones, add a `root` bone at the origin as the parent of `Hips`
(the animation is unchanged: `root` has no keys of its own), and export an animation-only FBX.

Usage (Blender 4.3 or newer):
  blender --background --factory-startup --python add_root.py -- <out_dir> <in1.fbx> [<in2.fbx> ...]
"""
import os
import sys
from math import radians
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

PREFIX = "mixamorig:"
ROOT = "root"
ROOT_ROT_X_DEG = float(os.environ.get("ROOT_ROT_X", "0"))  # experiment knob, see the notes in the README of this tool
# The NPC skeletons are Z-up, Mixamo's bone data is Y-up and UE reads it as written: +90 on the root stands the pose up
# (checked with UE's analyze_animation: both feet end up level; -90 flips it, 0 lies it on its side).
ROOT_POSE_X_DEG = float(os.environ.get("ROOT_POSE_X", "90"))
OUT_SUFFIX = os.environ.get("OUT_SUFFIX", "")


INPLACE_KEYWORDS = ("walk", "run", "strafe")  # clips whose Hips travel must be removed (movement comes from the capsule)


def make_in_place(arm, hips_name="Hips"):
    """Remove the linear horizontal travel of Hips, keeping its sway and height, so the loop has no seam."""
    f0, f1 = (int(v) for v in arm.animation_data.action.frame_range)
    hips, scene = arm.pose.bones[hips_name], bpy.context.scene
    to_world = arm.matrix_world
    mats = {}
    for f in range(f0, f1 + 1):
        scene.frame_set(f)
        mats[f] = hips.matrix.copy()
    # Horizontal means world X/Y: the armature's own axes are rotated (forward travel isn't along its Y)
    travel = to_world @ mats[f1].translation - to_world @ mats[f0].translation
    for f in range(f0, f1 + 1):
        t = (f - f0) / max(1, f1 - f0)
        pos = to_world @ mats[f].translation
        pos.x -= travel.x * t
        pos.y -= travel.y * t
        mats[f].translation = to_world.inverted() @ pos
    for f in range(f0, f1 + 1):
        scene.frame_set(f)
        hips.matrix = mats[f]
        hips.keyframe_insert("location", frame=f)
    scene.frame_set(f0)


METERS = os.environ.get("METERS", "0") == "1"
ROOT_SCALE = float(os.environ.get("ROOT_SCALE", "1"))
BONE_SCALE = 0.01  # Mixamo bones are in cm; the NPC skeletons keep them in meters under a root with scale 100


def to_meters(arm):
    """Rewrite the bone data (rest pose and location keys) in meters and drop the 0.01 object scale: same pose in the world."""
    for fc in arm.animation_data.action.fcurves:
        if fc.data_path.endswith(".location"):
            for kp in fc.keyframe_points:
                kp.co.y *= BONE_SCALE
                kp.handle_left.y *= BONE_SCALE
                kp.handle_right.y *= BONE_SCALE
            fc.update()
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for eb in arm.data.edit_bones:
        eb.head *= BONE_SCALE
        eb.tail *= BONE_SCALE
    bpy.ops.object.mode_set(mode="OBJECT")
    arm.scale = (1.0, 1.0, 1.0)


def process(src: Path, dst: Path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(src), automatic_bone_orientation=os.environ.get("AUTO_BONE", "0") == "1")
    arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    assert len(arms) == 1, f"{src.name}: expected 1 armature, found {len(arms)}"
    arm = arms[0]
    assert all(b.name.startswith(PREFIX) for b in arm.data.bones), f"{src.name}: unexpected bone names"
    assert ROOT not in arm.data.bones and ROOT not in {b.name.removeprefix(PREFIX) for b in arm.data.bones}, f"{src.name}: already has a root"

    # The exporter bakes the scene range (default 1-250), not the action's: match it or short clips get stretched
    first, last = arm.animation_data.action.frame_range
    bpy.context.scene.frame_start, bpy.context.scene.frame_end = int(first), int(last)

    for b in arm.data.bones:  # RNA rename also updates the action's fcurve paths
        b.name = b.name.removeprefix(PREFIX)

    if METERS:
        to_meters(arm)

    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    eb = arm.data.edit_bones
    root = eb.new(ROOT)
    root.head, root.tail = Vector((0, 0, 0)), Vector((0, 10 * (BONE_SCALE if METERS else 1), 0))
    root.matrix = Matrix.Rotation(radians(ROOT_ROT_X_DEG), 4, "X")  # rest orientation of root: must match the skeleton's root
    hips = eb["Hips"]
    assert hips.parent is None, f"{src.name}: Hips is not the top bone"
    hips.parent = root
    bpy.ops.object.mode_set(mode="OBJECT")

    if any(k in src.stem.lower() for k in INPLACE_KEYWORDS):
        make_in_place(arm)

    if ROOT_POSE_X_DEG or ROOT_SCALE != 1.0:
        # UE reads the bone data as written (it ignores the Armature node above `root`): turn the whole pose upright
        # and give `root` the scale the skeleton has, so meter bones come out in cm like the skeleton's bind pose
        pb = arm.pose.bones[ROOT]
        pb.rotation_mode = "XYZ"
        pb.rotation_euler = (radians(ROOT_POSE_X_DEG), 0, 0)
        pb.scale = (ROOT_SCALE, ROOT_SCALE, ROOT_SCALE)
        for frame in (int(first), int(last)):
            pb.keyframe_insert("rotation_euler", frame=frame)
            pb.keyframe_insert("scale", frame=frame)

    dst.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.fbx(
        filepath=str(dst), object_types={"ARMATURE"}, add_leaf_bones=False, use_armature_deform_only=False,
        bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
        bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0,
        axis_forward=os.environ.get("AXIS_FWD", "-Z"), axis_up=os.environ.get("AXIS_UP", "Y"),
        apply_scale_options=os.environ.get("SCALE_OPT", "FBX_SCALE_NONE"),
    )
    print("OK", src.name, "->", dst.name)


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    out_dir, files = Path(args[0]), [Path(a) for a in args[1:]]
    for f in files:
        gender = "Female" if "female" in f.parent.name.lower() else "Male"
        name = "_".join(w.capitalize() for w in f.stem.replace(" 90", "").split())
        process(f, out_dir / gender / f"{gender}_{name}{OUT_SUFFIX}.fbx")


if __name__ == "__main__":
    main()
