# Automatic typed UAV load protection

The runtime automatically skips a pixel draw or compute dispatch when its
effective shader reads a bound UAV format that the device cannot load. This
protects older GPUs from unsafe typed UAV loads such as the four-channel squadron
logo reads reported in Elite Dangerous: Odyssey. It uses shader instructions and
device capabilities, so game updates do not require shader hashes or replacement
files. No INI setting is needed, and turning EDHM fixes off keeps the guard active.

Only slots read by `ld_uav_typed` or its status-returning variant are checked.
Write-only UAVs, raw buffers and structured buffers do not trigger a skip. The
baseline `R32_FLOAT`, `R32_UINT` and `R32_SINT` formats remain allowed. For example,
a shader may read an `R32_FLOAT` UAV at `u1` while writing a four-channel UAV at
`u0`, even if the latter format cannot support typed loads.

At device creation, `d3d11_log.txt` records the feature level,
`TypedUAVLoadAdditionalFormats`, typed-load support for `R32G32B32A32_FLOAT`, and
whether the guard is armed. Creating a typed UAV without typed-load support also
arms it, including resources created by command lists. Until armed, draw and
dispatch checks perform only relaxed atomic reads, without D3D queries, map
lookups or locks. Once armed, the guard queries the effective bound shader and
checks only the UAV slots that shader reads. Original, ShaderFixes, deferred,
reloaded and custom shaders register the bytecode actually submitted to D3D.
Cleanup and registration replace guard metadata under one lock, preserving
protection when a driver returns an already-bound shader for identical bytecode.

Each skipped stage/shader/slot/format combination is logged once per device.
Affected rendering is omitted; the guard does not emulate the unsupported
operation or convert its texture format. Support rules follow Microsoft's
[typed UAV load documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d11/typed-unordered-access-view-loads).

Some older drivers may reject an additional-format shader during creation,
before it can be bound. A bounded `Typed UAV shader creation failed` diagnostic
records the HRESULT, stage, hash and device capabilities. The runtime preserves
that failure result; it does not synthesize a no-op shader.

The native tests use real SM5 compilation and WARP execution with capability
overrides to simulate unsupported hardware. They do not establish behavior on an
affected physical GPU or validate an installed game. See
[the native test guide](../RuntimeTests/README.md) for cases and mutation checks.
