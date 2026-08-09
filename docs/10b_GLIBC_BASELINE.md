# Tinexus Glibc Baseline (Phase 1.5)

> **Document:** 10b_GLIBC_BASELINE.md  
> **Status:** ACTIVE  
> **Depends on:** ADR-006 Third-Party App Support

## Current Baseline

The current baseline for the Tinexus root filesystem is **glibc 2.43** (Ubuntu GLIBC 2.43-2ubuntu2.3).

## Maintenance Commitment

Because Phase 1.5 relies on FUSE + AppImages (which run without sandboxing and expect a host glibc), the `libc.so.6` version shipped in the Tinexus ISO is a critical piece of the ABI boundary. 

Any upgrade to the host build environment that bumps the glibc version must be treated as a **breaking change candidate** for AppImage compatibility. 

AppImages compiled against newer glibc versions will fail to run on Tinexus if the Tinexus base glibc is older than what the AppImage was built against. Conversely, bumping the Tinexus glibc is generally safe, but should be tracked explicitly here.

## Verified Shared Libraries

The following core libraries are staged automatically and must remain present to ensure basic AppImage startup compatibility:
- `libc.so.6`
- `libdl.so.2` (or integrated into libc)
- `libm.so.6`
- `libpthread.so.0` (or integrated into libc)
- `libz.so.1`
- `libfuse3.so.3` (specifically added for Phase 1.5)
