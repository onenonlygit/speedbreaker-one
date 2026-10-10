# SpeedBreaker One Android 0.1.6

Experimental ARM64 release with upstream v0.1.1's far-plane shadow-depth packing
fix. The old resolve converted depth 1.0 to zero; the imported clamp preserves
far depth. Android visual/performance validation remains pending.

- Mandatory evidence-first methodology in docs/ENGINEERING_METHODOLOGY.md,
  enforced through AGENTS.md; performance roadmap and selective upstream review.
- Same package/signing certificate; install over 0.1.4 or 0.1.5 without clearing
  data. Direct ISO access, audio, settings and bounded visual capture retained.
- No additional performance optimization or speculative flicker correction.
- Optimized RelWithDebInfo ARM64 build (-O2), diagnostics probe OFF, bring-up ON
  (debuggable for private log collection). Name 0.1.6, code 7; source 2efca3b,
  clean. Previous 0.1.4 and 0.1.5 APKs preserved unchanged.
- Passed: exhaustive depth pack/renderer integration, eight optimized resolve
  SPIR-V variants, eight actual-translator SPIR-V modules, ISO/concurrency,
  visual capture limits, atomics, block ranges, frame policy, whole-vblank and
  write-watch tests. APK v2 signature, package/version, ARM64, 56,165 translated
  functions and packaged/unstripped Build IDs verified. See build.json/SHA256SUMS.
- No RP6/ADB connection, Career progression or desktop gameplay test. No claimed
  FPS/power gain. Post-fix shadow flickering is reported upstream; cause unknown.

**Next test:** same Quick Race/car/track at 720p, drive two minutes. Check sunlight
and shadows first, wait for capture completion before judging FPS, then verify
engine audio, mirrors/HUD and start a second race. Keep diagnostic archives private.

Parts in this directory preserve the exact signed APK for the existing release
publishing workflow; they are not installable individually. No XEX, assets,
credentials, raw logs, shaders or generated PPC source are included.
