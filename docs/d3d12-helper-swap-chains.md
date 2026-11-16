# D3D12 helper swap chains

Version 0.1.7 incorporates the fix reported by
[Duceowen in issue #52](https://github.com/Fenris159/EDHM_2DMigoto/issues/52).
EDUP's XeSS Frame Generation bridge creates its own D3D12 presenter inside the
D3D11 game process. Our existing DXGI hook treated its command queue as an
unsupported game device, created a D3D11On12 warning device, and wrapped the
swap chain. `HackerSwapChain` rejects `IDXGISwapChain4`, which blocked XeFG
initialization with `0x80004002` / error `-17` in the reporter's log.

Microsoft documents D3D12's swap-chain `pDevice` argument as an
[`ID3D12CommandQueue`](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforhwnd).
The pinned [EDUP bridge source](https://github.com/characterecho-sean/ed-unofficial-patch/blob/0e01c22c9499a8ea08bdacb94a4668bd206b37b6/src/d3d11/xess_fg.cpp)
passes its queue and factory into XeFG initialization, then requests an
`IDXGISwapChain4` presenter. The report matches these two code paths.

## Runtime policy

All four factory creation hooks recognize a D3D12 command queue and forward the
original arguments and result directly to DXGI, under the existing re-entry
guard. Null device arguments also reach native DXGI validation instead of the
legacy device lookup, which otherwise dereferences them. They skip D3D11 descriptor overrides, window tracking, warning-device
creation and swap-chain wrapping. The temporary queue interface is released.
D3D11 devices continue through their existing EDHM path. This policy leaves
D3D12 helpers to their owner; it does not add D3D12 game rendering to EDHM.

Keep the [existing EDUP chain configuration](https://github.com/Fenris159/EDHM_2DMigoto/wiki/EDUP-Compatibility-and-Configuration#recommended-configuration):
EDUP owns `d3d11.dll` and loads our release as `d3d11_edhm.dll`. No extra DLL
variant or reverse chain is required. Quiet reload remains controlled by
`[Logging] show_reload_message=0` in the root `d3dx.ini`.

## Validation and limits

The off-game product checks use real D3D12 WARP queues and DXGI factories.
HWND, legacy and composition chains must retain `IDXGISwapChain4` and the
requested 32 by 24 dimensions and two buffers despite D3D11 configuration
overrides. An invalid CoreWindow call must preserve the result from native
DXGI without producing the unsupported-DirectX warning. Every API also checks
null-device rejection against native DXGI before loading the product; the old
path crashes with `0xc0000005` on both architectures. Further D3D11 creations
in the same process must retain resource tracking. Existing rendered reload and
GPU-constant checks protect the D3D11 configuration path.

Before the repair, the real HWND test reproduced `0x80004002` on x64 and Win32.
With only HWND repaired, legacy and composition still failed that query, while
the CoreWindow failure path emitted the false warning. These are correctness
checks, without a game or FPS benchmark. They do not run Intel's frame-generation
engine or certify its output, latency, hardware support or performance.

The reporter tested their proposed patch in-game on an RX 9070 XT with EDUP
0.22.2-13 and reported working XeFG and EDHM colors. That is external evidence;
our release still needs confirmation in the reporter's game configuration.
