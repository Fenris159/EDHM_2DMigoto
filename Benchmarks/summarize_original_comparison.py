"""Summarize the archived, off-game original/develop comparison using stdlib only."""

import argparse
from collections import Counter, defaultdict
import csv
import gzip
import itertools
import json
from pathlib import Path
import re
from statistics import median


def percentile(values, fraction):
    """Linear interpolation between order statistics (inclusive endpoints)."""
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)


def comparison(a, b):
    if len(a) != 4 or len(b) != 4:
        raise ValueError("expected four independent processes per variant")
    center_a, center_b = median(a), median(b)
    result = {"original_process_medians": a, "develop_process_medians": b,
              "original_median": center_a, "develop_median": center_b}
    if not center_a or not center_b:
        result.update(status="unavailable", reason="PrivateUsage returned zero under Wine")
        return result
    # Enumerate every n=4, with-replacement resample in each group: 256**2 ratios.
    boot_a = [median(sample) for sample in itertools.product(a, repeat=4)]
    boot_b = [median(sample) for sample in itertools.product(b, repeat=4)]
    changes = [100 * (mb / ma - 1) for ma in boot_a for mb in boot_b]
    result.update(status="measured", delta=center_b - center_a,
                  percent_change=100 * (center_b / center_a - 1),
                  percent_change_95_interval=[percentile(changes, 0.025), percentile(changes, 0.975)])
    return result


def components(folder):
    processes = defaultdict(lambda: defaultdict(list))
    counts = defaultdict(lambda: defaultdict(set))
    for file in sorted(folder.glob("*.csv")):
        variant = file.stem.rsplit("-", 1)[1]
        groups = defaultdict(list)
        for row in csv.DictReader(file.open(encoding="utf-8-sig")):
            kind = file.name.split("-", 1)[0]
            workload = row.get("workload", "scalar_increment")
            key = f"{kind}/{workload}/profiling_{row['mode']}"
            if kind == "lists" and int(row["value"]) != 500000:
                raise ValueError("command-list output differs")
            if kind == "views" and int(row["bound_resource_matches"]) != 1:
                raise ValueError("resource identity differs")
            if int(row["sample"]) > 0:
                groups[key].append(float(row["ns_per_run"]))
                if kind == "views":
                    counts[key][variant].add(int(row["create_calls"]))
        for key, samples in groups.items():
            if len(samples) != 10:
                raise ValueError("expected ten post-warmup samples per process")
            processes[key][variant].append(median(samples))
    result = {}
    for key, values in sorted(processes.items()):
        result[key] = dict(unit="ns_per_operation", **comparison(values["A"], values["B"]))
        if key in counts:
            result[key]["create_calls_per_10000_operations"] = {
                variant: sorted(values) for variant, values in counts[key].items()}
    return result


def offscreen(folder):
    manifest = json.loads((folder / "manifest.json").read_text())
    if manifest["status"] != "passed" or not all(r["pass"] and r["exit"] == 0 for r in manifest["runs"]):
        raise ValueError("refusing a failed collection")
    values = defaultdict(lambda: defaultdict(list))
    units = {}
    for run in manifest["runs"]:
        file = folder / f"{run['profile']}-{run['trial']}-{run['variant']}.jsonl"
        groups = defaultdict(list)
        records = [json.loads(line) for line in file.read_text().splitlines() if line.startswith("{")]
        if len(records) != run["record_count"]:
            raise ValueError("record count differs from manifest")
        for row in records:
            if row["unit"] not in {"ns_per_operation", "ms", "bytes"}:
                continue
            if row["unit"] == "ns_per_operation" and row["sample"] == 0:
                continue
            key = f"{run['profile']}/{row['metric']}"
            units[key] = row["unit"]
            groups[key].append(row["value"])
        for key, samples in groups.items():
            expected = 6 if units[key] == "ns_per_operation" else 1
            if len(samples) != expected:
                raise ValueError(f"unexpected sample count for {file.name}/{key}")
            values[key][run["variant"]].append(median(samples))
    result = {}
    for key, samples in sorted(values.items()):
        result[key] = dict(unit=units[key], **comparison(samples["A"], samples["B"]))
        if "R" in samples:
            result[key]["raw_system_process_medians"] = samples["R"]
    return result


def traces(folder):
    result = {}
    for file in sorted(folder.glob("*.trace.txt.gz")):
        methods = Counter()
        slots = Counter()
        with gzip.open(file, "rt", encoding="utf-8") as source:
            for line in source:
                match = re.match(r"^\d+ (\w+)::(\w+)\(", line)
                if match:
                    methods[match[2]] += 1
                    if match[2].endswith("SetShaderResources"):
                        slot = re.search(r"StartSlot = (\d+), NumViews = (\d+)", line)
                        if slot:
                            slots[f"{match[2]}/start_{slot[1]}/count_{slot[2]}"] += 1
        result[file.name.removesuffix(".trace.txt.gz")] = {
            "method_calls": dict(sorted(methods.items())),
            "shader_resource_bindings": dict(sorted(slots.items()))}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = {
        "method": "Median of post-warmup samples per process, then median of four processes per variant. "
                  "Init/memory retain their single observations. Exact independent n=4 bootstrap median ratios; "
                  "65,536 combinations; interpolated percentile 95% intervals. Descriptive, one-host intervals, "
                  "not multiplicity-adjusted or a cross-hardware population estimate. Negative change means lower cost.",
        "components": components(args.archive / "components"),
        "native_warp": offscreen(args.archive / "native-warp"),
        "amd_proton": offscreen(args.archive / "amd-proton"),
        "native_trace": traces(args.archive / "native-trace"),
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
