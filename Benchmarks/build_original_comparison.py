"""Build existing component workloads against two unmodified runtime sources."""

import argparse
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET


NS = "http://schemas.microsoft.com/developer/msbuild/2003"
ET.register_namespace("", NS)


def node(name, text=None, **attributes):
    result = ET.Element(f"{{{NS}}}{name}", attributes)
    result.text = text
    return result


def build(source, harness, original):
    project = source / "DirectX11/DirectX11.vcxproj"
    tree = ET.parse(project)
    root = tree.getroot()
    for element in root.iter(f"{{{NS}}}ConfigurationType"):
        element.text = "Application"
    for element in root.iter(f"{{{NS}}}PostBuildEvent"):
        element.clear()
        element.tag = f"{{{NS}}}PostBuildEvent"
        element.append(node("Command", ""))
    props = node("PropertyGroup")
    output = source / ".quality/original-comparison-build"
    props.extend([
        node("TargetName", "OriginalComparison"),
        node("OutDir", str(output) + "/"),
        node("IntDir", str(output / "obj") + "/"),
    ])
    definitions = node("ItemDefinitionGroup")
    compile_options = node("ClCompile")
    compile_options.extend([
        node("LanguageStandard", "stdcpp17"),
        node("AdditionalIncludeDirectories", f"{source / 'DirectX11'};{harness / 'RuntimeTests'};%(AdditionalIncludeDirectories)"),
        node("PreprocessorDefinitions", ("ORIGINAL_3DMIGOTO;" if original else "") + "_HAS_STD_BYTE=0;_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING;%(PreprocessorDefinitions)"),
    ])
    link = node("Link")
    link.extend([
        node("SubSystem", "Console"),
        node("ModuleDefinitionFile", ""),
        node("ForceSymbolReferences", ""),
        node("EntryPointSymbol", ""),
    ])
    definitions.extend([compile_options, link])
    items = node("ItemGroup")
    entry = node("ClCompile", Include=str(harness / "Benchmarks/OriginalComparison.cpp"))
    entry.append(node("PrecompiledHeader", "NotUsing"))
    items.append(entry)
    # Retain the source list, project references and ordinary warning policy.
    # Only the entry point, output location and required common language mode change.
    import_index = next(i for i, element in enumerate(root) if element.tag == f"{{{NS}}}Import" and "Microsoft.Cpp.targets" in element.get("Project", ""))
    for element in (props, definitions, items):
        root.insert(import_index, element)
        import_index += 1
    generated = project.with_name("OriginalComparison.generated.vcxproj")
    tree.write(generated, encoding="utf-8", xml_declaration=True)
    subprocess.run([
        "msbuild", str(generated), "-target:Build", "-property:Configuration=Release",
        "-property:Platform=x64", f"-property:SolutionDir={source}/",
        "-property:WindowsTargetPlatformVersion=10.0", "-property:EdhmStrictBuild=false",
        "-nodeReuse:false", "-verbosity:minimal", "-restore:false",
    ], check=True, cwd=source)
    print(output / "OriginalComparison.exe")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--harness", type=Path, required=True)
    parser.add_argument("--original", action="store_true")
    args = parser.parse_args()
    build(args.source.resolve(), args.harness.resolve(), args.original)


if __name__ == "__main__":
    main()
