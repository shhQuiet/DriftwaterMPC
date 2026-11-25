#include "mpc_plugin.h"

#include <cmath>

namespace {

constexpr int kOutput = 0;

mpc::PluginInfo makeInfo()
{
    mpc::PluginInfo info;
    info.name = "Driftwater";
    info.vendor = "Elsewave";
    info.uniqueId = MPC_FOURCC('E', 'L', 'd', 'w');
    info.version = 100;
    info.isInstrument = false;
    info.numInputs = 2;
    info.numOutputs = 2;
    info.tailSamples = 1;
    return info;
}

class Driftwater : public mpc::Plugin
{
public:
    Driftwater() : mpc::Plugin(makeInfo())
    {
        addParam(mpc::floatParam("output", "Output", -24.0f, 0.0f, 0.0f, "dB", 1));
    }

    void prepare(double sampleRate, int) override
    {
        smoothing_ = static_cast<float>(-std::expm1(-1.0 / (sampleRate * 0.01)));
        reset();
    }

    void reset() override
    {
        gain_ = outputGain();
    }

    void process(mpc::ProcessContext& ctx) override
    {
        const float target = outputGain();
        for (int frame = 0; frame < ctx.numFrames; ++frame)
        {
            gain_ += smoothing_ * (target - gain_);
            for (int channel = 0; channel < ctx.numOutputs; ++channel)
            {
                const auto* input = ctx.inputs && channel < ctx.numInputs
                    ? ctx.inputs[channel] : nullptr;
                ctx.outputs[channel][frame] = input ? input[frame] * gain_ : 0.0f;
            }
        }
    }

private:
    float outputGain() const
    {
        return std::pow(10.0f, get(kOutput) * 0.05f);
    }

    float gain_ = 1.0f;
    float smoothing_ = 0.002f;
};

} // namespace

MPC_PLUGIN_ENTRY(Driftwater)
