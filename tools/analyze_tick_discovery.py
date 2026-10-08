"""Read-only statistics for completed OutRun discovery, not playback qualification."""
import argparse
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import statistics

BASE = ("phase update car_instance micros ticks app_time power_on_timer current_mode game_mode stage "
        "car_id car_kind car_colour manual_transmission flags_4 cur_gear_208 pedal_amount_34 field_1c4 "
        "position_x position_y position_z spd_mb_x spd_mb_y spd_mb_z").split()
CAR_MATRICES = ["matrix_70", "matrix_b0", "matrix_f0"]
CAM_MATRICES = ["camera_140", "camera_180", "camera_1c0", "camera_200", "camera_240", "camera_280", "camera_2c0"]
CAM_SCALARS = ("camera_observed camera_finite camera_mode camera_fov_ac camera_znear_bc "
               "camera_zfar_c0 camera_timer_364").split()
CAM_VECTORS = ["camera_pos_f8", "camera_look_104", "camera_angle_128"]
FORCE_FIELDS = ("force_inputs_observed force_inputs_finite force_output_enabled force_legacy_requested "
                "force_periodic_requested field_8 field_1c8 field_1cc field_1d0 field_1d4 field_1dc field_1e0 "
                "field_264 field_268 surface_mask_0 surface_mask_1 surface_mask_2 surface_mask_3 load_coli_type "
                "surface_roughness surface_water ffb_lateral_deadzone ffb_grip_loss ffb_wall_impact ffb_road_texture "
                "ffb_tire_slip ffb_engine_idle ffb_spring_strength ffb_damper_strength ffb_steering_weight "
                "ffb_weight_transfer ffb_gear_shift ffb_invert ffb_global_strength").split()
HUD_SPEED_FIELDS = ["hud_speed_observed", "hud_speed_finite", "field_1f8"]


def matrix_fields(name):
    return [f"{name}_{r}{c}" for r in range(1, 5) for c in range(1, 5)]


def fields(version):
    result = BASE + [k for m in CAR_MATRICES for k in matrix_fields(m)]
    if version >= 2:
        result += CAM_SCALARS + [f"{v}_{a}" for v in CAM_VECTORS for a in "xyz"]
        result += [k for m in CAM_MATRICES for k in matrix_fields(m)]
    if version >= 3:
        result += FORCE_FIELDS
    if version >= 4:
        result += HUD_SPEED_FIELDS
    return result


def kv(text):
    result = {}
    for line in text.splitlines():
        key, sep, value = line.partition("=")
        if not sep or key in result:
            raise ValueError("malformed/repeated outcome key")
        result[key] = value
    return result


def summary(values):
    return None if not values else {"min": min(values), "median": statistics.median(values), "max": max(values)}


def analyze(folder):
    folder = Path(folder)
    outcome_bytes = (folder / "outcome.txt").read_bytes()
    outcome = kv(outcome_bytes.decode("utf-8-sig"))
    schema = outcome.get("schema")
    if schema not in tuple(f"outrun2006.tick-discovery@{v}" for v in (1, 2, 3, 4)):
        raise ValueError("unsupported discovery schema")
    if outcome.get("outcome") != "observed" or outcome.get("unmatchedPre") != "false":
        raise ValueError("only a completed observed window with no unmatched pre is accepted")
    name = outcome.get("dataFile")
    if name not in ("discovery.tsv", "discovery.retry.tsv"):
        raise ValueError("unexpected data filename")
    version = int(schema[-1])
    data = (folder / name).read_bytes()
    reader = csv.DictReader(io.StringIO(data.decode("utf-8-sig")), delimiter="\t")
    expected = fields(version)
    if reader.fieldnames != expected:
        raise ValueError("header differs from the declared schema")
    rows = []
    for index, raw in enumerate(reader):
        if None in raw or any(v is None for v in raw.values()):
            raise ValueError("wrong row width")
        row = {k: float(v) for k, v in raw.items() if k != "phase"}
        row["phase"] = raw["phase"]
        if row["phase"] != ("pre" if index % 2 == 0 else "post"):
            raise ValueError("pre/post ordering")
        for k in expected[1:72]:
            if not math.isfinite(row[k]):
                raise ValueError("non-finite car value")
        if version >= 2:
            if row["camera_observed"] not in (0, 1) or row["camera_finite"] not in (0, 1):
                raise ValueError("invalid camera flags")
            if row["camera_finite"] and (not row["camera_observed"] or
                                          not all(math.isfinite(row[k]) for k in expected[74:200])):
                raise ValueError("camera finite flag disagrees with values")
        if version >= 3:
            if any(row[k] not in (0, 1) for k in FORCE_FIELDS[:5]):
                raise ValueError("invalid force observation flags")
            if row["force_inputs_finite"] and (not row["force_inputs_observed"] or
                                               not all(math.isfinite(row[k]) for k in FORCE_FIELDS[5:])):
                raise ValueError("force finite flag disagrees with values")
        if version >= 4:
            if any(row[k] not in (0, 1) for k in HUD_SPEED_FIELDS[:2]):
                raise ValueError("invalid HUD speed flags")
            if row["hud_speed_finite"] and (not row["hud_speed_observed"] or not math.isfinite(row["field_1f8"])):
                raise ValueError("HUD speed finite flag disagrees with value")
        if row["update"] != int(row["update"]) or row["micros"] < 0:
            raise ValueError("invalid update/time")
        if rows and row["micros"] < rows[-1]["micros"]:
            raise ValueError("time goes backwards")
        if index % 2:
            if row["update"] != rows[-1]["update"] or row["car_instance"] != rows[-1]["car_instance"]:
                raise ValueError("pair identity mismatch")
        elif rows and row["update"] != rows[-1]["update"] + 1:
            raise ValueError("recorded update gap")
        rows.append(row)
    if not rows or len(rows) % 2 or len(rows) != int(outcome["rows"]) or len(rows) // 2 != int(outcome["pairs"]):
        raise ValueError("row/pair count mismatch or empty capture")
    if any(row[k] != rows[0][k] for row in rows for k in
           ("car_instance", "car_id", "car_kind", "car_colour", "manual_transmission", "stage")):
        raise ValueError("car/stage identity changed")
    pairs = list(zip(rows[::2], rows[1::2]))
    groups = {"position": [f"position_{a}" for a in "xyz"], "spd_mb": [f"spd_mb_{a}" for a in "xyz"]}
    groups.update({m: matrix_fields(m) for m in CAR_MATRICES})
    if version >= 2:
        groups.update({m: matrix_fields(m) for m in CAM_MATRICES})
        groups.update({v: [f"{v}_{a}" for a in "xyz"] for v in CAM_VECTORS})
    changes = {}
    for group, keys in groups.items():
        def valid(a, b):
            return not group.startswith("camera_") or all(r["camera_observed"] and r["camera_finite"] for r in (a, b))
        inside = [(a, b) for a, b in pairs if valid(a, b)]
        between = [(a, b) for a, b in zip(rows[1:-1:2], rows[2::2]) if valid(a, b)]
        changes[group] = {
            "insidePairs": len(inside), "changedInside": sum(any(a[k] != b[k] for k in keys) for a, b in inside),
            "betweenPairs": len(between), "changedBetween": sum(any(a[k] != b[k] for k in keys) for a, b in between),
            "largestInsideDelta": max((abs(a[k] - b[k]) for a, b in inside for k in keys), default=None),
        }
    scalar_keys = ["field_1c4", "pedal_amount_34", "cur_gear_208", "ticks"] + [f"position_{a}" for a in "xyz"]
    ranges = {k: summary([r[k] for r in rows]) for k in scalar_keys}
    if version >= 2:
        ranges.update({k: summary([r[k] for r in rows if r["camera_observed"] and r["camera_finite"]]) for k in CAM_SCALARS[2:]})
    force_pre = [r for r in rows[::2] if r.get("force_inputs_observed") and r.get("force_inputs_finite")]
    hud_rows = [r for r in rows if r.get("hud_speed_observed") and r.get("hud_speed_finite")]
    return {"schema": "outrun2006.discovery-analysis@1", "inputSchema": schema,
            "outcomeSha256": hashlib.sha256(outcome_bytes).hexdigest(), "dataFile": name,
            "dataSha256": hashlib.sha256(data).hexdigest(), "rows": len(rows), "pairs": len(pairs),
            "rowSpanSeconds": (rows[-1]["micros"] - rows[0]["micros"]) / 1e6,
            "hookMicroseconds": summary([b["micros"] - a["micros"] for a, b in pairs]),
            "modes": sorted(set(r["current_mode"] for r in rows)), "ranges": ranges,
            "cameraValidRows": sum(bool(r.get("camera_observed") and r.get("camera_finite")) for r in rows),
            "forceInputPreRows": len(force_pre),
            "forceInputRanges": {k: summary([r[k] for r in force_pre]) for k in FORCE_FIELDS[5:]} if version >= 3 else {},
            "forceOutputEnabledPreRows": sum(bool(r["force_output_enabled"]) for r in force_pre),
            "hudSpeedValidRows": len(hud_rows),
            "hudSpeedBaseRange": summary([r["field_1f8"] for r in hud_rows]),
            "hudSpeedQualification": "raw HUD base field; live unit/display correlation not inferred",
            "forceQualification": "raw inputs only; no original force calculation, native route, command or calibrated speed",
            "changes": changes, "replayable": False,
            "limits": "Observed value changes only; not exclusive writer ownership, rendered timing, calibrated units or matrix roles."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = analyze(args.folder)
    with args.output.open("x", encoding="utf-8") as out:
        json.dump(result, out, indent=2, allow_nan=False)
        out.write("\n")
    print(f"{result['pairs']} observed pairs; {result['cameraValidRows']} valid camera rows; {args.output}")
