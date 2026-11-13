"""Step 2 of 3: computes the retarget from the JSON that export_from_ue.py wrote (plain Python, run outside the editor).

Mixamo clip (on the Mixamo source skeleton) -> PSX NPC skeleton, all in UE's own space so units and axes match the NPC
skeleton by construction. Body bones: world-space rotation delta from the rest pose, turned to face the NPC. Arms: bone
direction (Mixamo rests in T-pose, the NPC in A-pose). Hips: scaled path. Writes <rt_dir>/out_<clip>.json.

Usage: python retarget_offline.py <rt_dir> <clip_name> [<clip_name> ...]
"""
import json
import math
import sys
from pathlib import Path

ARM_CHILD = {}
for _side in ("Left", "Right"):
    _chain = [_side + n for n in ("Shoulder", "Arm", "ForeArm", "Hand", "HandMiddle1")]
    ARM_CHILD.update(dict(zip(_chain, _chain[1:])))


# Quaternions are (x, y, z, w); product is Hamilton's and world = parent * local, as UE composes (checked on real data).
def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def qinv(q):
    return (-q[0], -q[1], -q[2], q[3])


def qnorm(q):
    n = math.sqrt(sum(c * c for c in q)) or 1.0
    return tuple(c / n for c in q)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def mul(a, s):
    return tuple(x * s for x in a)


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def unit(v):
    return mul(v, 1.0 / (math.sqrt(dot(v, v)) or 1.0))


def qrot(q, v):
    t = mul(cross(q[:3], v), 2.0)
    return add(add(v, mul(t, q[3])), cross(q[:3], t))


def q_from_to(a, b):
    w = 1.0 + dot(a, b)
    if w < 1e-6:
        axis = cross(a, (1, 0, 0)) if abs(a[0]) < 0.9 else cross(a, (0, 1, 0))
        return qnorm((*unit(axis), 0.0))
    return qnorm((*cross(a, b), w))


def q_yaw(theta):
    return (0.0, 0.0, math.sin(theta / 2), math.cos(theta / 2))


def facing(rig):
    d = sub(rig["world"]["LeftToeBase"]["p"], rig["world"]["LeftFoot"]["p"])
    return math.atan2(d[1], d[0])


def retarget(src, tgt, clip):
    names, parent = tgt["names"], tgt["parent"]
    rot0_s = {n: tuple(src["world"][n]["r"]) for n in names}
    pos0_s = {n: tuple(src["world"][n]["p"]) for n in names}
    rot0_t = {n: tuple(tgt["world"][n]["r"]) for n in names}
    pos0_t = {n: tuple(tgt["world"][n]["p"]) for n in names}
    A = q_yaw(facing(tgt) - facing(src))
    A_inv = qinv(A)
    ratio = pos0_t["Hips"][2] / pos0_s["Hips"][2]
    root_rot, root_pos, root_scale = rot0_t["root"], pos0_t["root"], tgt["world"]["root"]["s"][0]
    index = {n: i for i, n in enumerate(clip["names"])}
    rots = {n: [] for n in names}
    hips_local = []
    for frame in clip["frames"]:
        rs, ps = {}, {}
        for n in clip["names"]:  # source world pose (parents come first; the source skeleton has scale 1)
            x, y, z, w, px, py, pz = frame[index[n]]
            lr, lp, p = qnorm((x, y, z, w)), (px, py, pz), src["parent"][n]
            if p is None:
                rs[n], ps[n] = lr, lp
            else:
                rs[n] = qnorm(qmul(rs[p], lr))
                ps[n] = add(ps[p], qrot(rs[p], lp))
        world = {}
        for n in names:
            if n == "root":
                world[n] = rot0_t[n]
            elif n in ARM_CHILD:
                c = ARM_CHILD[n]
                d = unit(qrot(A, sub(ps[c], ps[n])))
                d_rest = unit(sub(pos0_t[c], pos0_t[n]))
                world[n] = qnorm(qmul(q_from_to(d_rest, d), rot0_t[n]))
            else:
                delta = qmul(rs[n], qinv(rot0_s[n]))
                world[n] = qnorm(qmul(qmul(qmul(A, delta), A_inv), rot0_t[n]))
        hips_world = add(pos0_t["Hips"], mul(qrot(A, sub(ps["Hips"], pos0_s["Hips"])), ratio))
        hips_local.append(mul(qrot(qinv(root_rot), sub(hips_world, root_pos)), 1.0 / root_scale))
        for n in names:
            if n == "root":
                rots[n].append(tuple(tgt["local"][n]["r"]))
            else:
                rots[n].append(qnorm(qmul(qinv(world[parent[n]]), world[n])))
    tracks = {}
    for n in names:
        ref = tgt["local"][n]
        tracks[n] = {"rot": rots[n], "pos": hips_local if n == "Hips" else [ref["p"]], "scl": [ref["s"]]}
    return tracks


def main():
    rt, clips = Path(sys.argv[1]), sys.argv[2:]
    for name in clips:
        clip = json.loads((rt / f"clip_{name}.json").read_text())
        gender = clip["gender"]
        src = json.loads((rt / f"rig_src_{gender}.json").read_text())
        tgt = json.loads((rt / f"rig_tgt_{gender}.json").read_text())
        tracks = retarget(src, tgt, clip)
        (rt / f"out_{name}.json").write_text(json.dumps({"gender": gender, "keys": clip["keys"], "tracks": tracks}))
        print("OK", name, clip["keys"], "keys")


if __name__ == "__main__":
    main()
