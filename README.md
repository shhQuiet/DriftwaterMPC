# Driftwater for MPC

Driftwater is an Elsewave standalone MPC stereo insert effect in development. This
starter passes stereo audio through an Output trim to exercise the host, parameter
state, and generated skin. The modulation engine is the next development milestone.

The port's reference is the [original Driftwater Max for Live device](https://maxforlive.com/library/device/16070/driftwater).
Its Rate, Damp, and Depth controls shape the stereo delay modulation. LPG controls
the envelope-driven recovery filter, and Mix blends the dry and wet signals.

## Build and run the simulator

Use [Akai's MPC Plugin Builder SDK](https://github.com/inmusic-software/mpc-plugin-builder-sdk)
at revision `e78bad508a305a92443758eb2bc8b428e9e7a687`. Set `MPC_SDK` to the
absolute path of that checkout. From this repository:

```sh
python3 "$MPC_SDK/tools/build.py" .
"$MPC_SDK/.sdk-tools/host_sim" build/native/driftwater.dylib test
```

The simulator command assumes macOS. On Linux use `.so` in place of `.dylib`.
Use `--target native --toolchain gcc` with the build command for a simulator-only
build with the system C++ compiler. The default build includes Gen 1 (32-bit ARM)
and Gen 2 (64-bit ARM). SDK output goes to `build/`.

## OpenMPC

Follow the [OpenMPC release workflow](https://open-mpc.github.io/workflow.html)
once the modulation engine and touchscreen controls have been verified on
hardware. Publish only after manual testing and explicit approval of the exact
release package.
