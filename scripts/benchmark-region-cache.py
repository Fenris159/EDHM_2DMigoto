#!/usr/bin/env python3
"""Benchmark extracted, unmodified region-cache templates; never builds/deploys the DLL.

See docs/performance-audit.md for scope. Requires Python 3, git, and g++.
The middle variant changes only the hasher, isolating it from probe bounds.
"""

import argparse
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import re
import statistics
import subprocess


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BASE = "f8e7118a1220e43546e546efa31892b8304358df"
DEFAULT_CURRENT = "34d92f27f588bbc3f72320c0fa17b674638698c6"
CPP = r"""
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>
#include "extracted.hpp"
using Key = upstream::RegionHashKeyL2;
struct FoldedHasher {
    size_t operator()(const Key& key) const {
        return current::RegionHashKeyHasherL2{}({key.offset, key.size});
    }
};
using Value = payload::RegionCacheEntry;
using Old = upstream::FlatHashMap<Key, Value, upstream::RegionHashKeyHasherL2>;
using HashOnly = upstream::FlatHashMap<Key, Value, FoldedHasher>;
using New = current::FlatHashMap<Key, Value, FoldedHasher>;
volatile uint64_t sink = 0;
struct Measurement { double ns; uint64_t checksum; };
template<class Map>
__attribute__((noinline)) Measurement measure(Map& map, const std::vector<Key>& queries, size_t count) {
    uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (size_t i = 0; i < count; ++i) {
        const Value* value = map.find_ptr(queries[i % queries.size()]);
        checksum += value ? value->hash : 1;
    }
    const auto finish = std::chrono::steady_clock::now();
    sink = checksum;
    return {std::chrono::duration<double, std::nano>(finish - start).count() / count, checksum};
}
template<class Hasher> size_t buckets(const std::vector<Key>& keys, size_t capacity) {
    std::vector<bool> occupied(capacity, false);
    for (const Key& key : keys) occupied[Hasher{}(key) & (capacity - 1)] = true;
    return std::count(occupied.begin(), occupied.end(), true);
}
void workload(const char* name, size_t entries, uint32_t stride, bool variable_size, int samples, size_t count) {
    Old old(entries * 2);
    HashOnly hash_only(entries * 2);
    New latest(entries * 2);
    std::vector<Key> keys, hits, misses;
    for (size_t i = 0; i < entries; ++i) {
        const Key key{static_cast<uint32_t>(i) * stride,
                      variable_size ? 64u + static_cast<uint32_t>(i % 17) * 4u : 96u};
        keys.push_back(key);
        const Value value{static_cast<uint32_t>(i + 17), 1};
        old.insert(key, value);
        hash_only.insert(key, value);
        latest.insert(key, value);
    }
    hits = keys;
    for (const Key& key : keys) misses.push_back({key.offset + static_cast<uint32_t>(entries) * stride, key.size});
    std::mt19937 random(0xED113D);
    std::shuffle(hits.begin(), hits.end(), random);
    std::shuffle(misses.begin(), misses.end(), random);
    const size_t old_buckets = buckets<upstream::RegionHashKeyHasherL2>(keys, old.capacity());
    const size_t new_buckets = buckets<FoldedHasher>(keys, latest.capacity());
    for (int operation = 0; operation < 2; ++operation) {
        const auto& queries = operation ? misses : hits;
        // Warm every variant. Allocation and construction are outside timing.
        measure(old, queries, queries.size());
        measure(hash_only, queries, queries.size());
        measure(latest, queries, queries.size());
        for (int sample = 0; sample < samples; ++sample) {
            Measurement results[3];
            // Alternate direction to reduce systematic order/thermal effects.
            if (sample % 2) {
                results[2] = measure(latest, queries, count);
                results[1] = measure(hash_only, queries, count);
                results[0] = measure(old, queries, count);
            } else {
                results[0] = measure(old, queries, count);
                results[1] = measure(hash_only, queries, count);
                results[2] = measure(latest, queries, count);
            }
            if (results[0].checksum != results[1].checksum || results[0].checksum != results[2].checksum)
                throw std::runtime_error("benchmark variants disagree");
            const char* variants[] = {"xxmi_0.9.8", "hash_only", "edhm_head"};
            for (int variant = 0; variant < 3; ++variant)
                std::cout << name << ',' << (operation ? "miss" : "hit") << ',' << variants[variant] << ','
                          << sample << ',' << entries << ',' << latest.capacity() << ','
                          << (variant ? new_buckets : old_buckets) << ',' << count << ','
                          << results[variant].ns << ',' << results[variant].checksum << '\n';
        }
    }
}
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const int samples = std::stoi(argv[1]);
    const size_t count = std::stoull(argv[2]);
    std::cout << "workload,operation,variant,sample,entries,capacity,initial_buckets,iterations,ns_per_lookup,checksum\n";
    workload("dense_256", 256, 1, false, samples, count);
    workload("dense_1024", 1024, 1, false, samples, count);
    workload("aligned64_1024", 1024, 64, false, samples, count);
    workload("aligned64_4096", 4096, 64, false, samples, count);
    workload("mixed_size_1024", 1024, 64, true, samples, count);
}
"""


def run(*args):
    return subprocess.check_output(args, cwd=ROOT, text=True)


def extract(ref):
    full = run("git", "rev-parse", f"{ref}^{{commit}}").strip()
    source = run("git", "show", f"{full}:DirectX11/ResourceHash.h")
    start = source.index("template", source.index("// Thread safety"))
    end = source.index("struct RegionHashesCache")
    body = source[start:end]
    payload = re.search(r"struct RegionCacheEntry\s*\{.*?\};", source, re.DOTALL).group()
    return full, body, hashlib.sha256(body.encode()).hexdigest(), payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default=DEFAULT_BASE)
    parser.add_argument("--current", default=DEFAULT_CURRENT)
    parser.add_argument("--samples", type=int, default=15)
    parser.add_argument("--iterations", type=int, default=200000)
    parser.add_argument("--cpu", type=int, help="Pin benchmark process to one Linux logical CPU")
    parser.add_argument("--output", type=Path, default=ROOT / "docs/audit-data")
    args = parser.parse_args()
    if args.samples < 3 or args.iterations < 1000:
        parser.error("Use at least three samples and 1000 lookups per sample")
    base, old, old_hash, _ = extract(args.base)
    current, new, new_hash, payload = extract(args.current)
    build = ROOT / ".quality/audit-region-cache"
    build.mkdir(parents=True, exist_ok=True)
    args.output.mkdir(parents=True, exist_ok=True)
    header = ("namespace upstream {\n" + old + "}\nnamespace current {\n" + new
              + "}\nnamespace payload {\n" + payload + "\n}\n")
    (build / "extracted.hpp").write_text(header)
    (build / "benchmark.cpp").write_text(CPP)
    compiler = os.environ.get("CXX", "g++")
    flags = ["-std=c++17", "-O2", "-DNDEBUG", "-Wall", "-Wextra", "-Werror"]
    executable = build / "benchmark"
    subprocess.run([compiler, *flags, str(build / "benchmark.cpp"), "-o", str(executable)], check=True)
    command = [str(executable), str(args.samples), str(args.iterations)]
    if args.cpu is not None:
        command = ["taskset", "-c", str(args.cpu), *command]
    result = subprocess.run(command, check=True, text=True, capture_output=True)
    (args.output / "region-cache-raw.csv").write_text(result.stdout)
    rows = list(csv.DictReader(io.StringIO(result.stdout)))
    groups = {}
    for row in rows:
        key = (row["workload"], row["operation"], row["variant"])
        groups.setdefault(key, []).append(float(row["ns_per_lookup"]))
    with (args.output / "region-cache-summary.csv").open("w") as output:
        writer = csv.writer(output)
        writer.writerow(["workload", "operation", "variant", "median_ns", "min_ns", "max_ns", "samples"])
        for key, values in groups.items():
            writer.writerow([*key, statistics.median(values), min(values), max(values), len(values)])
    cpu_info = Path("/proc/cpuinfo").read_text() if Path("/proc/cpuinfo").exists() else ""
    cpu = next((line.split(":", 1)[1].strip() for line in cpu_info.splitlines() if line.startswith("model name")), "unknown")
    metadata = {
        "scope": "CPU-only extracted map lookup, synthetic keys, not game/D3D11/FPS profiling",
        "base_commit": base, "current_commit": current,
        "base_extraction_sha256": old_hash, "current_extraction_sha256": new_hash,
        "payload_extraction_sha256": hashlib.sha256(payload.encode()).hexdigest(),
        "payload": "Current RegionCacheEntry: uint32_t hash, uint64_t version; same payload in every variant",
        "lookup": "find_ptr + returned hash load; excludes GetRegionVersion and validation",
        "harness_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "compiler": run(compiler, "--version").splitlines()[0], "flags": flags,
        "platform": platform.platform(), "cpu": cpu, "pinned_cpu": args.cpu,
        "samples": args.samples, "iterations_per_sample": args.iterations,
        "order": "alternating forward/reverse, per variant warmup, fixed shuffle seed 0xED113D",
        "capacity": "2 * entries, 0.5 load, preallocated; no allocation timed",
        "variants": {"xxmi_0.9.8": "original map + original hash",
                     "hash_only": "original map + current hash (single-change isolation)",
                     "edhm_head": "current map + current hash (bounds isolation vs hash_only)"},
    }
    (args.output / "region-cache-environment.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps({"output": str(args.output), "raw_rows": len(rows), "groups": len(groups)}))


if __name__ == "__main__":
    main()
