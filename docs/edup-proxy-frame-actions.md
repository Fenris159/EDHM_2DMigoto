# EDUP frame generation and EDHM frame actions

Release 0.1.8 addresses the [issue #52 follow-up](https://github.com/Fenris159/EDHM_2DMigoto/issues/52#issuecomment-6908828546):
EDHM's static colors could work with XeSS Frame Generation while presets,
hotkeys, transitions and reload stopped updating. This is separate from the
native D3D12 swap-chain interface repair in 0.1.7.

## Cause

EDUP replaces the game swap chain's Present method. With its XeSS bridge active,
it reads buffer zero from the hidden D3D11 chain, copies the image and presents
its D3D12 helper. It deliberately skips the saved D3D11 Present. Our normal
`HackerSwapChain::Present` therefore never reaches `RunFrameActions`.
Static draw-time overrides can continue, hiding the missing per-frame work.

The reviewed upstream source is
[`0e01c22c`](https://github.com/characterecho-sean/ed-unofficial-patch/commit/0e01c22c9499a8ea08bdacb94a4668bd206b37b6):
[`hookedPresent`](https://github.com/characterecho-sean/ed-unofficial-patch/blob/0e01c22c9499a8ea08bdacb94a4668bd206b37b6/src/d3d11/device_hook.cpp#L1942)
chooses the bridge, and
[`xessFgPresent`](https://github.com/characterecho-sean/ed-unofficial-patch/blob/0e01c22c9499a8ea08bdacb94a4668bd206b37b6/src/d3d11/xess_fg.cpp#L1132)
reads the hidden chain before presenting the helper.

## Repair

When the game obtains its D3D11 back buffer, the wrapper checks whether another
module has replaced Present or Present1. It installs a detour on that outer
method using the runtime's existing hook manager. A private COM identity query
recognizes a live EDHM wrapper and keeps it alive during the call.

```text
game calls the outer Present
  EDHM pre commands, inputs, presets, transitions and reload
  proxy saves state, processes/copies the image, presents, and restores state
  EDHM post commands
```

The scope captures the game-visible pipeline before the proxy changes it and
runs post commands after the proxy consumes the frame. This preserves both
state-saving command pairs and post commands that clear the next frame's back
buffer. GetBuffer discovers the hook; it never executes a command phase.

If the proxy forwards to our saved Present, the active scope suppresses a
second execution. Scope registration includes ownership promotion before any
commands or forwarding can run. An active-frame count also suppresses saved
Present on a worker thread; input and commands stay on the established thread.
Test presents, nested calls, other threads and secondary chains do not advance
the established proxy frame source.

The first wrapped D3D11 chain is selected during creation, so an early secondary
Present cannot steal its role. Native private-data lifetime tracking retires the
source even when a native SwapChain2/3 interface is released last, allowing
recreation. The callback uses the Windows process heap and trivial atomics
without `G`, logging or CRT allocation. Post commands are retained even when
presentation returns a failure. Profiling includes command overhead on the
owning thread.

This adapts Duceowen's diagnosis and recovery proposal. The submitted patch used
D3D12 Present notifications, a minimum draw count and a quiet-time threshold to
infer a game frame from GetBuffer. The implemented boundary uses the outer
Present call itself: a zero-draw menu still advances, and generated frames and
repeated buffer reads do not become extra EDHM frames. It adds no D3D12 Present
hooks or draw-loop counters. Stable buffer reads require only cached target
comparisons after discovery.

The supported arrangement has one outer Present implementation per API and one
active game frame source. Hook discovery occurs when the game obtains its back
buffer after the outer hook is installed, as in the reviewed EDUP creation path.
This is not a general scheduler for arbitrary chains that never expose that
boundary. D3D12 helper objects retain their native interfaces.

## Validation

`RuntimeTests/ProxyFrameTests.cpp` loads the actual product DLL and a separate
proxy DLL that replaces Present/Present1, copies/queries the game's real back
buffer and can suppress or forward native presentation. Assertions read the
product's GPU IniParams through public D3D interfaces.

| Outcome | Observable check |
| --- | --- |
| A game frame advances once | Pre/post counters each increase by one on a zero-draw frame. |
| Copies and probes do not tick | Extra GetBuffer calls and test presents preserve both counters. |
| Failure cleanup survives | The proxy returns `E_FAIL`; both command phases still execute once. |
| Normal presentation resumes | Forwarding and removal/reinstallation of the outer hook preserve single increments. |
| Ownership is respected | A worker thread and a second wrapped chain leave the counters unchanged. |
| Recreation works | Releasing the primary chain permits a replacement to advance the counters. |
| Inputs and transitions survive | A controlled F6 toggle sets 9; a preset moves from 0 to 4 at 500 ms and 8 at 1000 ms. |
| Reload survives | Controlled F11 input loads new constants, including post-command work from the reloaded configuration. |

The flow and controls cases run through both Present APIs, with native Windows
WARP checks for x64 and Win32 in CI. The published 0.1.7 DLL must fail the same
probe specifically with both counters still zero. Proton/DXVK checks are
off-game checks on AMD; they are not hardware XeSS frame-generation tests.

Duceowen reported that the proposed recovery patch restored galaxy-map fog,
add-ons and reload in the installed game. That is reporter evidence for the
diagnosis; the adapted implementation's automated probes do not establish an
in-game XeSS image-quality or generated-frame result. No FPS or performance
gain for the combined stack is claimed.

## Configuration

Keep EDUP as `d3d11.dll`, our 0.1.8 runtime as `d3d11_edhm.dll`, and EDHM's
existing files beside the game executable. Set EDUP's `[advanced]`
`real_dll=d3d11_edhm.dll` in the applicable shared/flat configurations. No new
EDHM option is required for the frame-action repair.

To suppress EDHM's reload confirmation, use `[Logging]`
`show_reload_message=0` in `d3dx.ini`. This hides the message while retaining
reload behavior and disk logging; `show_warnings=0` controls warnings separately.
See the [wiki setup](https://github.com/Fenris159/EDHM_2DMigoto/wiki/EDUP-Compatibility-and-Configuration#recommended-configuration)
for the complete chain configuration and evidence limits.
