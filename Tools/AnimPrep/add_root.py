"""Prepares Mixamo animation FBXs for the NPC skeletons (they use Mixamo bone names without the prefix, plus a `root` bone).

For every FBX: strip the `mixamorig:` prefix from the bones, add a `root` bone at the origin as the parent of `Hips`
(the animation is unchanged: `root` has no keys of its own), and export an animation-only FBX.

Usage (Blender 4.3 or newer):
  blender --background --factory-startup --python add_root.py -- <out_dir> <in1.fbx> [<in2.fbx> ...]
"""
import sys
from pathlib import Path

import bpy
from mathutils import Vector

PREFIX = "mixamorig:"
ROOT = "root"


INPLACE_KEYWORDS = ("walk", "run", "strafe")  # clips whose Hips travel must be removed (movement comes from the capsule)


def make_in_place(arm):
    """Remove the linear horizontal travel of Hips, keeping its sway and height, so the loop has no seam."""
    f0, f1 = (int(v) for v in arm.animation_data.action.frame_range)
    hips, scene = arm.pose.bones["Hips"], bpy.context.scene
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


def process(src: Path, dst: Path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(src))
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

    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    eb = arm.data.edit_bones
    root = eb.new(ROOT)
    root.head, root.tail = Vector((0, 0, 0)), Vector((0, 10, 0))  # bone units are cm here (armature scale 0.01)
    hips = eb["Hips"]
    assert hips.parent is None, f"{src.name}: Hips is not the top bone"
    hips.parent = root
    bpy.ops.object.mode_set(mode="OBJECT")

    if any(k in src.stem.lower() for k in INPLACE_KEYWORDS):
        make_in_place(arm)

    dst.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.fbx(
        filepath=str(dst), object_types={"ARMATURE"}, add_leaf_bones=False, use_armature_deform_only=False,
        bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
        bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0,
    )
    print("OK", src.name, "->", dst.name)


def main():
    args = sys.argv[sys.argv.index("--") + 1:]
    out_dir, files = Path(args[0]), [Path(a) for a in args[1:]]
    for f in files:
        gender = "Female" if "female" in f.parent.name.lower() else "Male"
        name = "_".join(w.capitalize() for w in f.stem.replace(" 90", "").split())
        process(f, out_dir / gender / f"{gender}_{name}.fbx")


main()
