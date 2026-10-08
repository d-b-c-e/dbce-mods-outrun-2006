import csv
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("discovery", Path(__file__).parents[1] / "analyze_tick_discovery.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class DiscoveryAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def case(self, version=2, edit=None, outcome=None):
        names = module.fields(version)
        rows = []
        for i in range(4):
            row = {k: 0 for k in names}
            row.update(phase="pre" if i % 2 == 0 else "post", update=100 + i // 2,
                       micros=i*100, ticks=1, position_x=(i+1)//2)
            if version == 2:
                row.update(camera_observed=1, camera_finite=1, camera_fov_ac=55)
                # Camera changes between car callbacks, car changes inside them.
                row["camera_140_41"] = i//2
            rows.append(row)
        if edit:
            edit(rows)
        with (self.root / "discovery.tsv").open("w", newline="") as out:
            writer = csv.DictWriter(out, names, delimiter="\t")
            writer.writeheader()
            writer.writerows(rows)
        values = {"schema": f"outrun2006.tick-discovery@{version}", "outcome": "observed",
                  "unmatchedPre": "false", "rows": "4", "pairs": "2", "dataFile": "discovery.tsv"}
        values.update(outcome or {})
        (self.root / "outcome.txt").write_text("\n".join(f"{k}={v}" for k, v in values.items()))

    def test_camera_and_car_boundaries_stay_distinct(self):
        self.case()
        report = module.analyze(self.root)
        self.assertEqual(2, report["changes"]["position"]["changedInside"])
        self.assertEqual(0, report["changes"]["position"]["changedBetween"])
        self.assertEqual(0, report["changes"]["camera_140"]["changedInside"])
        self.assertEqual(1, report["changes"]["camera_140"]["changedBetween"])
        self.assertEqual(4, report["cameraValidRows"])
        self.assertFalse(report["replayable"])
        json.dumps(report, allow_nan=False)

    def test_old_schema_has_no_camera_claim(self):
        self.case(1)
        report = module.analyze(self.root)
        self.assertEqual(0, report["cameraValidRows"])
        self.assertNotIn("camera_140", report["changes"])

    def test_unobserved_or_nonfinite_camera_excluded_not_zero_filled(self):
        def edit(rows):
            rows[1].update(camera_finite=0, camera_140_11=float("nan"))
            rows[2].update(camera_observed=0, camera_finite=0)
        self.case(edit=edit)
        report = module.analyze(self.root)
        self.assertEqual(2, report["cameraValidRows"])
        self.assertEqual(0, report["changes"]["camera_140"]["insidePairs"])
        self.assertIsNone(report["changes"]["camera_140"]["largestInsideDelta"])
        self.assertEqual(2, report["changes"]["position"]["insidePairs"])
        json.dumps(report, allow_nan=False)

    def test_contradictory_camera_flags_refused(self):
        for values in ({"camera_finite": 2}, {"camera_observed": 0}, {"camera_140_11": float("nan")}):
            with self.subTest(values=values):
                self.case(edit=lambda rows: rows[1].update(values))
                with self.assertRaises(ValueError): module.analyze(self.root)

    def test_nonfinite_car_refused(self):
        self.case(edit=lambda rows: rows[0].update(position_x=float("inf")))
        with self.assertRaisesRegex(ValueError, "non-finite car"): module.analyze(self.root)

    def test_malformed_pairing_refused(self):
        for values in ({"phase": "pre"}, {"update": 102}, {"car_instance": 1}, {"micros": -1}):
            with self.subTest(values=values):
                self.case(edit=lambda rows: rows[1].update(values))
                with self.assertRaises(ValueError): module.analyze(self.root)

    def test_incomplete_and_outside_paths_refused(self):
        for values in ({"rows": "5"}, {"pairs": "1"}, {"unmatchedPre": "true"},
                       {"outcome": "failed"}, {"dataFile": "../discovery.tsv"}, {"schema": "future"}):
            with self.subTest(values=values):
                self.case(outcome=values)
                with self.assertRaises(ValueError): module.analyze(self.root)

    def test_schema_mismatch_refused(self):
        self.case(1, outcome={"schema": "outrun2006.tick-discovery@2"})
        with self.assertRaisesRegex(ValueError, "header"): module.analyze(self.root)


if __name__ == "__main__":
    unittest.main()
