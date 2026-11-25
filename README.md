# Driftwater for MPC

Driftwater is a stereo insert effect for standalone Akai MPCs. It adds a gently
drifting delay to guitar, keys, drums, and other input. Rate, Damp, and Depth
shape the motion, LPG lets transients brighten the wet signal, and Mix blends
it with the dry input. The Info tab explains the controls and credits the
Fairfield Circuitry Shallow Water.

## Install

Download Driftwater 1.0.0 from [GitHub Releases](https://github.com/shhQuiet/DriftwaterMPC/releases).
Choose ARMv7 for Gen 1 or AArch64 for Gen 2. Enable External Plugin Support on
MPC OS 3.10.1 or later. The archive includes instructions for copying the plugin
folder into `VST2 Plugins`.

Version 1.0.0 loaded on an MPC X running 3.10.1.12, and its Info version label
was confirmed on the touchscreen. The exact Gen 1 binary passed the Open MPC
CPU benchmark at 7.0% worst-stage p99 and 7.3% worst block. The Gen 2 package
passed SDK and catalog validation. Gen 2 hardware testing is pending.

## Build and run the simulator

Use [Akai's MPC Plugin Builder SDK](https://github.com/inmusic-software/mpc-plugin-builder-sdk)
at revision `e78bad508a305a92443758eb2bc8b428e9e7a687`. Set `MPC_SDK` to the
absolute path of that checkout. From this repository:

```sh
./build.sh
"$MPC_SDK/.sdk-tools/host_sim" build/native/driftwater.dylib test
```

The simulator command assumes macOS. On Linux use `.so` in place of `.dylib`.
Use `--target native --toolchain gcc` with the build script for a simulator-only
build with the system C++ compiler. The default build includes Gen 1 (32-bit ARM)
and Gen 2 (64-bit ARM). SDK output goes to `build/`.

## License

The MPC wrapper and artwork are source available for noncommercial use under
[Elsewave's license](LICENSE). See [third-party notices](THIRD_PARTY_NOTICES.md)
for the exported engine and runtime. The original Max project and GenExpr
are maintained separately from this repository.
