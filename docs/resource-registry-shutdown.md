# Resource registry shutdown

The 0.1.6 shutdown repair addresses a quit-time access violation reported with
EDUP 0.22.2 and the 0.1.4 runtime. The supplied dump retained the initiating
exception beneath the final CRT fast-fail: `ResourceReleaseTracker::Release`
called `G->mResources.erase(resource)` after the map's bucket storage was
destroyed. The reconstructed stack reached that callback from Windows D3D11
private-data destruction during EDUP's CRT exit cleanup.

`SetPrivateDataInterface` keeps the tracker alive independently of our global
registry. A resource held by another DLL can therefore outlive `StaticG`.
[Microsoft's ownership contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicechild-setprivatedatainterface)
explicitly releases the interface when its device child dies.

## Lifetime contract

`RetireResourceRegistry()` permanently disables tracker access to `Globals` and
its resource-map lock. Its marker is constant-initialized and trivially
destructible, independently of `StaticG` and translation-unit teardown order.

Retirement occurs at both boundaries:

- The `Globals` destructor body, before any map or command-list member is
  destroyed. This also protects resource owners inside the runtime's globals.
- Entry to `DLL_PROCESS_DETACH`, before CRT teardown and explicit unload work.
  This protects resources retained by other DLLs during process exit.

While the registry is active, a final tracker release takes the existing
resource lock and erases the resource's metadata. Once retired, the callback
releases its own tracker without reading `G` or taking its locks. Trackers use
paired Windows process-heap allocation/free operations, so final cleanup also
works after the DLL's CRT heap state is gone. Late interface queries skip CRT
logging. The lifetime check runs only on final resource release, outside draw and lookup paths.

Retirement is not a live configuration switch. Whole-process termination has
stopped other worker threads; ordinary DLL unload requires the caller to finish
runtime work and release its D3D objects first. This repair does not make
unloading executable callback code while another thread/object still uses it a
supported operation. A new DLL load creates a new lifetime marker.

Returning early from user `DllMain` does not stop CRT destructors. Microsoft
[documents that teardown order](https://learn.microsoft.com/en-us/cpp/build/run-time-library-behavior?view=msvc-170).
The explicit `Globals` destructor closes the gap even when the runtime sources
are linked into an executable rather than loaded as a DLL.

## Validation

The approved native cases require metadata to remain while a view owns the
resource and disappear on final release. A separate case destroys an actual
registry owner, protects its storage with `PAGE_NOACCESS`, then releases the
retained buffer/view. This makes a late registry or lock read fail without
relying on allocator reuse or timing. The unmodified implementation fails this
case with an access violation on both Windows architectures.

Product-DLL cases create tracked WARP buffers/views through the built runtime,
retain the actual private-data `IUnknown` returned by D3D11, then release GPU
objects while the backend is live. An earlier-loaded fixture releases the last
callback reference after the runtime's DLL/CRT teardown. Each child must exit
with code zero and write a completion receipt **after** that final `Release`
returns zero. Exit code alone can conceal a teardown exception. Standalone,
outer-proxy, official EDUP 0.22.2, and ordinary unload paths are covered.

This separates callback lifetime from WARP's own process-exit limitations. The
initial fixture retained complete GPU objects through CRT teardown and failed
inside WARP. A system-only control, with EDHM never loaded, also failed with
`0xc000000d`. That driver cleanup is therefore unsuitable as a pass/fail test of
this repair. The native protected-registry case still releases real buffers and
views after destroying the registry; the product case tests the exact shipped
callback after CRT teardown. Neither establishes hardware-driver or live-game
shutdown behavior. See [the native checks](../RuntimeTests/README.md).

The dump's final `0xc0000409`, parameter `7`, denotes a fatal application exit;
it does not by itself identify a stack overwrite. This repair targets the
retained initiating access violation. No driver or AA-engine fault was
established by that dump. Matching optimized-build PDB/map bundles are now
retained with releases to make future attribution more direct.
