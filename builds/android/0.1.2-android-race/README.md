# 0.1.2 Android race checkpoint

**Experimental working-race APK, not a stable release.** Built from
`9809202a33be4f823202293612ee8469c984c0f4` on `android/rp6-bringup`.

[Download SpeedBreaker-One-arm64-race.apk](https://github.com/onenonlygit/speedbreaker-one/releases/download/android-v0.1.2-race/SpeedBreaker-One-arm64-race.apk)

Juan completed a Quick Race sprint on a Retroid Pocket 6 on October 8, 2026.
World geometry, cars, HUD, music and some effects work. Engine audio is missing
and other audio is selective. Racing log samples are approximately 21–30 FPS
at internal 1280×720. Multiple consecutive races and broad compatibility are
not yet verified. No promise of locked 60 FPS or complete graphics accuracy.

Install over the previous SpeedBreaker One APK; retain the selected ISO and
app data. Direct ISO access avoids extraction. Package:
`com.onenonlygit.speedbreakerone`; version code 3; ARM64 only. Bring your own
supported Xbox 360 disc image. No disc image, XEX or extracted assets included.
The APK contains the project's compiled native runtime/recompiled executable.

See [test instructions](TEST-INSTRUCTIONS.txt), [build metadata](build.json),
[checksum](SHA256SUMS), and [development checkpoint](../../../docs/ANDROID_PROGRESS.md).
The certificate is consistent with previous fork APKs. Private signing keys
are deliberately excluded; future builds must retain that certificate to install over.

The exact tested binary is retained in numbered `.apk.part-*` files in this
folder. The publishing workflow assembles and checks it before uploading the APK
as a GitHub prerelease. These parts are not individual installable APKs.
