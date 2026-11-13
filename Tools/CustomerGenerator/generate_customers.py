"""Generates DT_Customers rows (SystemsDesigns.md section 5.6) from the NPC meshes in Content/Characters/NPCs.

Usage:  python generate_customers.py [--out customers.csv]
Import the CSV into a Data Table that uses the FCustomerData row struct.
Deterministic: same config + same meshes = same output. Taste and temperament are seeded per mesh, so adding a mesh
doesn't change the others; names are dealt in sorted-mesh order, so adding a mesh can shift the names after it
(shortcut: fine until personas ship in a build; then freeze the CSV).
"""
import argparse
import csv
import json
import random
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
CONTENT = HERE.parents[1] / "Content"


def list_meshes(cfg):
    """Yield (gender, asset_path) for every mesh under NPCs/<Gender>/<Folder>/, minus the excluded ones.

    The mesh is the .uasset named like its folder; any other .uasset in there is a material (e.g. Character_29/Character_30).
    """
    root = CONTENT / cfg["npc_root"]
    for gender in cfg["names"]:
        for asset in sorted((root / gender).glob("*/*.uasset")):
            if asset.stem != asset.parent.name:
                continue
            if not any(x.lower() in asset.stem.lower() for x in cfg["exclude_if_name_contains"]):
                yield gender, asset


def pick_taste(rng, cfg, asset_name):
    skills = cfg["skills"]
    role = next((s for key, s in cfg["role_skill"].items() if key.lower() in asset_name.lower()), None)
    dominant = role or rng.choice(skills)
    secondary = rng.choice([s for s in skills if s != dominant])
    bias = dict.fromkeys(skills, 0)
    bias[dominant] = cfg["dominant_weight"]
    bias[secondary] = rng.randint(cfg["secondary_weight_min"], cfg["secondary_weight_max"])
    return bias, dominant


def build_rows(cfg):
    names = {g: random.Random(f"{cfg['seed']}:names:{g}").sample(n, len(n)) for g, n in cfg["names"].items()}
    temps = cfg["temperaments"]
    rows = []
    for gender, asset in list_meshes(cfg):
        key = f"{gender}_{asset.stem}"
        rng = random.Random(f"{cfg['seed']}:{key}")
        bias, dominant = pick_taste(rng, cfg, asset.stem)
        temperament = rng.choices(list(temps), weights=[t["weight"] for t in temps.values()])[0]
        t = temps[temperament]
        game_path = "/Game/" + asset.relative_to(CONTENT).with_suffix("").as_posix()
        rows.append({
            "Name": f"Customer_{key}",
            "DisplayName": names[gender].pop(),
            "Mesh": f"{game_path}.{asset.stem}",
            "TasteBias": "(" + ",".join(f"{k}={v}" for k, v in bias.items()) + ")",
            "Temperament": temperament,
            "TierMin": t["tier_min"],
            "TierMax": t["tier_max"],
            "SpawnPhase": "Any",
            "SpawnWeight": 1,
            "PatienceMult": t["patience_mult"],
            "TipMult": t["tip_mult"],
            "_dominant": dominant,
            "_gender": gender,
        })
    return rows


def validate(rows, cfg):
    assert rows, "no meshes found under " + str(CONTENT / cfg["npc_root"])
    for key in ("Name", "DisplayName", "Mesh"):
        assert len({r[key] for r in rows}) == len(rows), f"duplicate {key}"
    for r in rows:
        assert r["TierMin"] <= r["TierMax"], r["Name"]
        assert f"={cfg['dominant_weight']}" in r["TasteBias"], r["Name"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(HERE / "customers.csv"))
    args = ap.parse_args()
    cfg = json.loads((HERE / "config.json").read_text(encoding="utf-8"))
    rows = build_rows(cfg)
    validate(rows, cfg)
    cols = [c for c in rows[0] if not c.startswith("_")]
    with open(args.out, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=cols, extrasaction="ignore")
        w.writeheader()
        w.writerows(rows)
    print(f"{len(rows)} customers -> {args.out}")
    for label, key in (("gender", "_gender"), ("temperament", "Temperament"), ("dominant skill", "_dominant")):
        print(f"  {label}: {dict(Counter(r[key] for r in rows))}")


if __name__ == "__main__":
    main()
