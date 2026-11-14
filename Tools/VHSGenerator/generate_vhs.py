#!/usr/bin/env python3
"""Deterministic DT_VHS generator (SystemsDesigns.md section 4.2). Python 3, standard library only.

Usage: python generate_vhs.py [--genres genres.json] [--words title_words.json] [--out out]
Writes <out>/vhs_rows.json (rows ready for the editor's import_rows) and <out>/report.txt.
Same seed -> same tapes; each genre has its own seed, so changing one only regenerates that genre.
Exit code 1 when a validation fails.
"""
import argparse, json, os, random, sys

SKILLS = ["funny", "scary", "brave", "thoughtful", "romantic"]


def gen_tape(rng, points, bias, floor, cap, dominant, taken):
    """Spread `points` one at a time, weight = bias + floor, never above `cap` per skill."""
    weights = [b + floor for b in bias]
    for _ in range(10000):
        v = [0] * len(SKILLS)
        for _ in range(points):
            open_skills = [i for i in range(len(SKILLS)) if v[i] < cap]
            v[rng.choices(open_skills, [weights[i] for i in open_skills])[0]] += 1
        if (dominant is None or v[dominant] > 0) and tuple(v) not in taken:
            return v
    raise RuntimeError("could not find a new skill vector")


def dominant_skill(bias):
    """Index of the single highest bias, or None when the maximum is tied."""
    top = max(bias)
    return bias.index(top) if bias.count(top) == 1 else None


def title_for(rng, genre, star, n, words, used):
    if genre["Genre"] in words:  # optional team word lists: {"Horror": {"first": [...], "second": [...]}}
        w = words[genre["Genre"]]
        for _ in range(1000):
            t = f'{rng.choice(w["first"])} {rng.choice(w["second"])}'
            if t not in used:
                return t
    return f'{genre["DisplayName"]} Tape {star}★ {n:02d}'  # placeholder until the word lists exist


def generate(cfg, genres, words):
    cap, per_star, floor = cfg["TapeSkillMax"], cfg["TapesPerStar"], cfg["GeneratorBiasFloor"]
    rows, used_titles = [], set()
    for g in genres:
        rng = random.Random(g["Seed"])
        dom = dominant_skill(g["SkillBias"])
        taken = set()
        for star, points in enumerate(cfg["StarPoints"], start=1):
            for n in range(1, per_star + 1):
                if star == 1 and n == 1 and dom is not None:  # the pure tape: every point in the dominant skill
                    v = [0] * len(SKILLS)
                    v[dom] = points
                else:
                    v = gen_tape(rng, points, g["SkillBias"], floor, cap, dom, taken)
                taken.add(tuple(v))
                title = title_for(rng, g, star, n, words, used_titles)
                used_titles.add(title)
                rows.append({"rowName": f'VHS_{g["Genre"]}_{star}_{n:02d}', "rowData": {
                    "Title": title, "PrimaryGenre": g["Genre"], "Rating": star,
                    "Skills": dict(zip(SKILLS, v))}})
    return rows


def validate(cfg, genres, rows):
    errors, per_genre = [], {}
    titles = [r["rowData"]["Title"] for r in rows]
    if len(set(titles)) != len(titles):
        errors.append("titles are not unique")
    for g in genres:
        mine = [r for r in rows if r["rowData"]["PrimaryGenre"] == g["Genre"]]
        per_genre[g["Genre"]] = len(mine)
        if len(mine) != len(cfg["StarPoints"]) * cfg["TapesPerStar"]:
            errors.append(f'{g["Genre"]}: {len(mine)} tapes')
        vectors = [tuple(r["rowData"]["Skills"].values()) for r in mine]
        if len(set(vectors)) != len(vectors):
            errors.append(f'{g["Genre"]}: duplicate skill vectors')
        for star, points in enumerate(cfg["StarPoints"], start=1):
            stars = [r for r in mine if r["rowData"]["Rating"] == star]
            if len(stars) != cfg["TapesPerStar"]:
                errors.append(f'{g["Genre"]}: {len(stars)} tapes at {star} stars')
            for r in stars:
                s = r["rowData"]["Skills"]
                if sum(s.values()) != points:
                    errors.append(f'{r["rowName"]}: points {sum(s.values())} != {points}')
                if max(s.values()) > cfg["TapeSkillMax"]:
                    errors.append(f'{r["rowName"]}: skill above TapeSkillMax')
        dom = dominant_skill(g["SkillBias"])
        if dom is not None:
            pure = next(r for r in mine if r["rowName"] == f'VHS_{g["Genre"]}_1_01')["rowData"]["Skills"]
            if pure[SKILLS[dom]] != cfg["StarPoints"][0]:
                errors.append(f'{g["Genre"]}: missing pure tape')
    return errors, per_genre


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("--genres", default=os.path.join(here, "genres.json"))
    ap.add_argument("--words", default=os.path.join(here, "title_words.json"))
    ap.add_argument("--out", default=os.path.join(here, "out"))
    a = ap.parse_args()
    data = json.load(open(a.genres, encoding="utf-8"))
    words = json.load(open(a.words, encoding="utf-8")) if os.path.exists(a.words) else {}
    rows = generate(data["config"], data["genres"], words)
    errors, per_genre = validate(data["config"], data["genres"], rows)
    os.makedirs(a.out, exist_ok=True)
    json.dump(rows, open(os.path.join(a.out, "vhs_rows.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    report = [f"{len(rows)} tapes", *(f"{k}: {v}" for k, v in per_genre.items()),
              "VALIDATION OK" if not errors else "VALIDATION FAILED", *errors]
    open(os.path.join(a.out, "report.txt"), "w", encoding="utf-8").write("\n".join(report) + "\n")
    print("\n".join(report))
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
