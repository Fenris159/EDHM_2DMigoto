"""Prove the typed UAV guard regressions against compiled production mutations.

Run only in a disposable checkout, like check_mutations.py. Its existing runner
restores exact source bytes, rebuilds production and quarantines unsafe binaries.
"""

import check_mutations as runner


def main():
    root = runner.ROOT / "DirectX11"
    scanner = root / "TypedUavLoadGuard.cpp"
    device = root / "HackerDevice.cpp"
    context = root / "HackerContext.cpp"
    commands = root / "CommandList.cpp"
    runner.OUTPUT = runner.ROOT / ".quality" / "typed-uav-mutations"
    runner.main([
        ("typed-uav-scan", scanner, "additionalFormats = 0x800", "additionalFormats = 0x100"),
        ("typed-uav-malformed", scanner,
         "word(bytes, 0) != dxbc || word(bytes, 5) != 1", "word(bytes, 5) != 1"),
        ("typed-uav-formats", device,
         "return !IsConcreteTypedUav(desc) || SupportsTypedUavLoadFormat(desc.Format);",
         "return IsConcreteTypedUav(desc) || SupportsTypedUavLoadFormat(desc.Format);"),
        ("typed-uav-compute", context, "return skip;", "return skip && !compute;"),
        ("typed-uav-mixed", scanner,
         "uint64_t{1} << slot : ~uint64_t{0}", "~uint64_t{0} : ~uint64_t{0}"),
        ("typed-uav-pixel", context,
         "!data.call_info.skip && ShouldSkipTypedUavLoad(false)",
         "!data.call_info.skip && ShouldSkipTypedUavLoad(true)"),
        ("typed-uav-effective", device,
         'if (!shader || (wcscmp(stage, L"ps") != 0 && wcscmp(stage, L"cs") != 0))',
         'if (!mGuardArmed.load(std::memory_order_relaxed) || !shader || '
         '(wcscmp(stage, L"ps") != 0 && wcscmp(stage, L"cs") != 0))'),
        ("typed-uav-lifetime", device,
         "G->mTypedUavLoadShaders.erase(handle);", "(void)handle;"),
        ("typed-uav-commands--submission", commands,
         "state->mHackerContext->ShouldSkipTypedUavLoad(compute)",
         "state->mHackerContext->ShouldSkipTypedUavLoad(!compute)"),
        ("typed-uav-commands--creation", commands,
         "guardDevice->RegisterTypedUavShader(cs, cs_bytecode->GetBufferPointer()",
         "guardDevice->RegisterTypedUavShader(nullptr, cs_bytecode->GetBufferPointer()"),
        ("typed-uav-commands--uav", commands,
         "state->mHackerDevice->ObserveTypedUavView(view);", "(void)view;"),
        ("typed-uav-replacement", device,
         "RegisterTypedUavShader(*ppShader, replaceShader, replaceShaderSize, shaderType, hash);",
         "RegisterTypedUavShader(*ppShader, pShaderBytecode, BytecodeLength, shaderType, hash);"),
    ])


if __name__ == "__main__":
    main()
