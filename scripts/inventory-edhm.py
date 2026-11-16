#!/usr/bin/env python3
"""Read an EDHM installation into an evidence inventory; never execute its INI."""

import argparse
from collections import Counter
import fnmatch
import hashlib
import json
import os
from pathlib import Path
import re
import sys


SECTION = re.compile(r"^\[([^\]]+)\]$")
SHADER = re.compile(r"^([0-9a-f]{16})-(vs|ps|gs|hs|ds|cs)(_replace)?\.(txt|bin)$", re.I)
MODEL = re.compile(r"^\s*((?:vs|ps|gs|hs|ds|cs)_\d_\d)\s*$", re.M | re.I)
ROW = re.compile(r"(?<![\w$\\])([xyzw])(\d+)(?!\w)", re.I)
QUERY = re.compile(r"\b(?:vs|ps|hs|ds|gs|cs)-t\d+\b", re.I)
SETTINGS = {
    "shader_hash", "texture_hash", "override_directory", "cache_directory",
    "assemble_signature_comments", "recursive_include", "skip_early_includes_load",
}


class InventoryLimit(RuntimeError):
    """A hard stop rather than a partially successful inventory."""


def read_text(data):
    """Keep undecodable input explicit; do not silently replace bytes."""
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        return data.decode("utf-16"), "utf-16"
    return data.decode("utf-8-sig"), "utf-8"


def entries(text):
    section = ""
    for number, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith((";", "#")):
            continue
        match = SECTION.fullmatch(line)
        if match:
            section = match[1]
            yield number, section, None, None
        elif "=" in line:
            key, value = line.split("=", 1)
            yield number, section, key.strip().casefold(), value.strip()
        else:
            yield number, section, "", line


class Inventory:
    def __init__(self, root, max_files=10000):
        self.root = root.absolute()
        self.base = self.root.parent
        self.max_files = max_files
        self.files = []
        self.edges = []
        self.diagnostics = []
        self.settings = {}
        self.rules = []
        self.sections = Counter()
        self.features = Counter()
        self.seen_files = set()
        self.seen_directories = set()
        self.exclusions = []
        self.user_config = "d3dx_user.ini"

    def label(self, path):
        # Normalize '..' without replacing a user's symlink/junction label.
        return Path(os.path.relpath(path.absolute(), self.base)).as_posix()

    def issue(self, path, code, detail):
        self.diagnostics.append({"path": self.label(path), "code": code, "detail": detail})

    def path_from(self, parent, value):
        # An offline host cannot infer the mapping of arbitrary Windows drives.
        if re.match(r"^[A-Za-z]:", value) or value.startswith(("\\\\", "/")):
            raise ValueError("absolute include/directory needs an explicitly prepared local layout")
        return parent / value.replace("\\", "/")

    def record(self, path, role):
        identity = path.resolve()
        if identity in self.seen_files:
            return None
        if len(self.files) >= self.max_files:
            raise InventoryLimit(f"inventory exceeds --max-files={self.max_files}")
        self.seen_files.add(identity)
        data = path.read_bytes()
        record = {
            "path": self.label(path), "role": role, "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
        }
        self.files.append(record)
        return record, data

    def scan_ini(self, path, role="include", inherited_activation="unconditional"):
        try:
            result = self.record(path, role)
            if result is None:
                self.issue(path, "already_seen", "duplicate/cyclic include was not read twice")
                return
            record, data = result
            text, encoding = read_text(data)
            record["encoding"] = encoding
        except (OSError, UnicodeError) as error:
            self.issue(path, "unreadable_ini", type(error).__name__)
            return
        lines = list(entries(text))
        condition = next(((n, v) for n, s, k, v in lines if not s and k == "condition"), None)
        if condition:
            number, value = condition
            if value in ("0", "0.0"):
                record["activation"] = "excluded_literal_zero"
                record["condition_line"] = number
                return
            # Evaluating expressions would turn this into another interpreter.
            if value not in ("1", "1.0"):
                record["activation"] = "unknown_condition"
                self.issue(path, "unknown_condition", f"line {number}; contents are candidate-only")
        record.setdefault("activation", inherited_activation)
        if role == "root":
            self.exclusions = [v for _, s, k, v in lines if s.casefold() == "include" and k == "exclude_recursive"]
            self.user_config = next((v for _, s, k, v in lines if s.casefold() == "include" and k == "user_config"), self.user_config)
        includes = []
        for number, section, key, value in lines:
            lower = section.casefold()
            if key is None:
                kind = next((name for name in ("shaderoverride", "textureoverride", "commandlist", "customshader", "preset", "pool", "resource", "key") if lower.startswith(name)), lower)
                self.sections[kind] += 1
                continue
            if key in SETTINGS and lower in ("rendering", "system"):
                candidate = {"value": value, "path": self.label(path), "line": number}
                if key in self.settings and self.settings[key]["value"] != value:
                    self.issue(path, "conflicting_setting", f"line {number}: {key}; runtime selection is not evaluated")
                self.settings.setdefault(key, candidate)
            if lower.startswith("include") and key in ("include", "include_recursive"):
                includes.append((number, key, value))
            if lower.startswith(("shaderoverride", "textureoverride")) and (key in ("hash", "filter_index", "handling") or key.startswith("match_")):
                self.rules.append({"path": self.label(path), "line": number, "section": section, "key": key, "value": value, "activation": record["activation"]})
            command = f"{key} = {value}"
            if re.search(r"\bRegion\s*\(", command, re.I):
                self.features["region_expressions"] += 1
            if "checktextureoverride" in key:
                self.features["effectful_texture_override_commands"] += 1
            self.features["direct_texture_query_occurrences"] += len(QUERY.findall(command))
        rows = {}
        for component, index in ROW.findall("\n".join(f"{k} = {v}" for _, _, k, v in lines if k is not None)):
            rows.setdefault(int(index), set()).add(component.lower())
        record["row_references"] = [{"row": n, "components": "".join(c for c in "xyzw" if c in rows[n])} for n in sorted(rows)]
        for number, key, value in includes:
            edge = {"from": self.label(path), "line": number, "directive": key, "value": value}
            self.edges.append(edge)
            try:
                target = self.path_from(path.parent, value)
                edge["target"] = self.label(target)
                if key == "include":
                    self.scan_ini(target, inherited_activation=record["activation"])
                else:
                    self.scan_directory(target, record["activation"])
            except (ValueError, OSError) as error:
                self.issue(path, "unresolved_include", f"line {number}: {error}")

    def scan_directory(self, directory, activation="unconditional"):
        identity = directory.resolve()
        if identity in self.seen_directories:
            self.issue(directory, "directory_cycle", "recursive directory was not traversed twice")
            return
        self.seen_directories.add(identity)
        if len(self.seen_directories) > self.max_files:
            raise InventoryLimit(f"directory traversal exceeds --max-files={self.max_files}")
        children = sorted(directory.iterdir(), key=lambda p: (p.name.casefold(), p.name))
        remaining = []
        for child in children:
            if any(fnmatch.fnmatchcase(child.name.casefold(), pattern.casefold()) for pattern in self.exclusions):
                self.issue(child, "excluded_recursive", "root exclude_recursive filename pattern")
            else:
                remaining.append(child)
        # Match runtime traversal: sorted INIs first, then sorted directories.
        for child in remaining:
            if child.is_file() and child.suffix == ".ini":
                self.scan_ini(child, inherited_activation=activation)
        for child in remaining:
            if child.is_dir():
                self.scan_directory(child, activation)

    def scan_shaders(self, value, role):
        try:
            directory = self.path_from(self.base, value)
            children = sorted(directory.iterdir(), key=lambda p: (p.name.casefold(), p.name))
        except (OSError, ValueError) as error:
            self.issue(self.root, "unresolved_shader_directory", f"{role}: {error}")
            return
        for child in children:
            match = SHADER.fullmatch(child.name)
            if not match or not child.is_file():
                continue
            try:
                result = self.record(child, role)
                if result is None:
                    continue
                record, data = result
                key, stage, replacement, extension = match.groups()
                record.update({
                    "filename_key": key.lower(), "stage": stage.lower(),
                    "kind": "binary_candidate" if extension.lower() == "bin" else "hlsl" if replacement else "assembly",
                    "original_identity": {"status": "unverified", "original_container_sha256": None, "game_build": None},
                })
                if extension.lower() == "txt":
                    text, encoding = read_text(data)
                    record["encoding"] = encoding
                    model = MODEL.search(text)
                    record["declared_shader_model"] = model[1].lower() if model else None
                    record["signature_comments"] = {name: bool(re.search(r"^\s*//\s*" + name + r" signature:", text, re.M | re.I)) for name in ("Input", "Output", "Patch Constant")}
            except (OSError, UnicodeError) as error:
                self.issue(child, "unreadable_shader", type(error).__name__)

    def run(self):
        if not self.root.is_file():
            raise ValueError("root INI is not a readable file")
        self.scan_ini(self.root, "root")
        try:
            user = self.path_from(self.base, self.user_config)
            if user.is_file():
                self.scan_ini(user, "persistent_settings")
        except ValueError as error:
            self.issue(self.root, "unresolved_user_config", str(error))
        for name, default, role in (("override_directory", "ShaderFixes", "replacement"), ("cache_directory", "ShaderCache", "cache_candidate")):
            self.scan_shaders(self.settings.get(name, {}).get("value", default), role)
        return {
            "schema_version": 1,
            "scope": "passive candidates; INI expressions and shader code were not executed",
            "complete_runtime_equivalence": False,
            "identity_policy": "Filename keys are claims; replacement/cache bytes do not establish original identity or game build.",
            "root": self.root.name,
            "settings": self.settings,
            "summary": {"files_by_role": dict(sorted(Counter(f["role"] for f in self.files).items())), "sections": dict(sorted(self.sections.items())), "features": dict(sorted(self.features.items()))},
            "files": self.files, "include_edges": self.edges, "rules": self.rules,
            "diagnostics": self.diagnostics,
            "limits": ["No command execution, static-expression evaluator, runtime section merge or active override selection.", "Conditional content remains candidate-only; exclusion patterns use filename glob matching, not the runtime PCRE2 implementation.", "No original-container identity, game build, performance or backend compatibility is inferred."],
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root_ini", type=Path)
    parser.add_argument("--output", type=Path, help="New JSON file; existing output and input files are never overwritten")
    parser.add_argument("--max-files", type=int, default=10000)
    arguments = parser.parse_args()
    if arguments.max_files < 1:
        parser.error("--max-files must be positive")
    try:
        result = Inventory(arguments.root_ini, arguments.max_files).run()
        text = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
        if arguments.output:
            with arguments.output.open("x", encoding="utf-8") as output:
                output.write(text)
        else:
            sys.stdout.write(text)
    except (OSError, ValueError, InventoryLimit) as error:
        parser.exit(1, f"inventory failed: {error}\n")


if __name__ == "__main__":
    main()
