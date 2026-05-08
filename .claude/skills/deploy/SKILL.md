---
name: deploy
description: Build the SkyrimNVDA plugin and deploy the DLL to Skyrim for testing. Picks the right build script based on which game install the user will test on (Steam AE 1.6.x = Debug only; Thana Khan SE 1.5.97 = Release required because Debug crashes on 1.5.97).
when_to_use: User-triggered only. Run when the user types `/deploy` after code changes. Default to `build_skyrim_release.bat` (covers BOTH targets) unless the user explicitly says they only test on Steam AE.
disable-model-invocation: true
---

# Build and Deploy

Build the SkyrimNVDA plugin and deploy it to Skyrim for testing.

## Build script choice (CRITICAL)

Two scripts exist with very different behaviour:

- **`C:\tmp\build_skyrim.bat`** — Debug only → Steam AE (1.6.x).
  ⚠️ The Debug DLL **CRASHES** on Skyrim SE 1.5.97 (Thana Khan modpack) with
  a `BSTHashMap.h: std::has_single_bit(_capacity)` assertion failure. Never
  deploy this Debug DLL to MO2 Thana Khan.

- **`C:\tmp\build_skyrim_release.bat`** — Release → MO2 Thana Khan AND Debug → Steam AE.
  Builds both flavours and deploys each one to its proper target. **This is
  the safe default** when you don't know which install the user will test on.

**Decision rule:**
- User says "je teste sur Thana Khan" / "modpack" / "MO2" → `build_skyrim_release.bat`
- User says "Steam AE" / "AE only" / no target mentioned → `build_skyrim_release.bat`
  (covers both, only adds ~10 seconds)
- Only use the plain Debug script if the user explicitly says "rapide" / "AE only" / "skip release"

## Steps

1. **Build the plugin** (default):
   ```bash
   cmd.exe //c "C:\tmp\build_skyrim_release.bat"
   ```
   Report any compilation errors. Stop if build fails.

2. **Report status**:
   - Confirm Release succeeded (`RELEASE_BUILD_EXIT_CODE=0`)
   - Confirm Debug succeeded (`DEBUG_BUILD_EXIT_CODE=0`)
   - Confirm deployment (`MO2_THANAKHAN_RESULT=0`, `STEAM_AE_DLL_RESULT=0`)
   - Remind user: "Launch Skyrim through SKSE to test"

## On Failure

If the build fails, analyze the compiler errors and suggest fixes. Common issues:
- Missing includes
- Syntax errors in menu_*.h files
- CommonLibSSE-NG API changes
- nvdaControllerClient linkage issues
- Multi-targeting issues (SE/AE/VR) requiring REL::RelocateMember

## Sanity check after build

If the user is testing on Thana Khan, double-check that the deployed DLL is
the Release build (~1.2 MB) and NOT the Debug build (~6 MB). The wrong DLL
in the modpack will crash on launch.
