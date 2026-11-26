#include "mpc_plugin.h"
#include "../generated/rnbo/rnbo/RNBO.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace {

struct Control
{
    const char* id;
    const char* label;
    float initial;
};

constexpr std::array<Control, 5> kControls{{
    {"rate", "Rate", 28.0f},
    {"damp", "Damp", 72.0f},
    {"depth", "Depth", 32.0f},
    {"lpg", "LPG", 58.0f},
    {"mix", "Mix", 48.0f},
}};

mpc::PluginInfo makeInfo()
{
    mpc::PluginInfo info;
    info.name = "Driftwater";
    info.vendor = "Elsewave";
    info.uniqueId = MPC_FOURCC('E', 'L', 'd', 'w');
    info.version = 1001;
    info.isInstrument = false;
    info.numInputs = 2;
    info.numOutputs = 2;
    return info;
}

class Driftwater : public mpc::Plugin
{
public:
    Driftwater() : mpc::Plugin(makeInfo())
    {
        for (const auto& control : kControls)
            addParam(mpc::floatParam(control.id, control.label, 0.0f, 100.0f, control.initial, "%", 1));

        for (std::size_t i = 0; i < kControls.size(); ++i)
            engineParams_[i] = engine_.getParameterIndexForID(
                (std::string("DriftwaterDSP/") + kControls[i].id).c_str());
    }

    void prepare(double sampleRate, int blockSize) override
    {
        silence_.assign(static_cast<std::size_t>(blockSize > 0 ? blockSize : 1), 0.0f);
        engine_.prepareToProcess(sampleRate, silence_.size());
        reset();
    }

    void reset() override
    {
        last_.fill(-1.0f);
    }

    void process(mpc::ProcessContext& ctx) override
    {
        for (std::size_t i = 0; i < engineParams_.size(); ++i)
        {
            const float value = get(i) * 0.01f;
            if (value != last_[i])
            {
                engine_.setParameterValue(engineParams_[i], value);
                last_[i] = value;
            }
        }

        const int capacity = static_cast<int>(silence_.size());
        for (int offset = 0; offset < ctx.numFrames; offset += capacity)
        {
            const int count = std::min(capacity, ctx.numFrames - offset);
            const float* inputs[2];
            float* outputs[2];
            for (int channel = 0; channel < 2; ++channel)
            {
                const auto* input = ctx.inputs && channel < ctx.numInputs
                    ? ctx.inputs[channel] : nullptr;
                inputs[channel] = input ? input + offset : silence_.data();
                outputs[channel] = ctx.outputs[channel] + offset;
            }
            engine_.process(static_cast<const float* const*>(inputs), 2,
                            static_cast<float* const*>(outputs), 2, count);
        }
    }

private:
    RNBO::CoreObject engine_;
    std::array<RNBO::ParameterIndex, kControls.size()> engineParams_{};
    std::array<float, kControls.size()> last_{};
    std::vector<float> silence_;
};

} // namespace

MPC_PLUGIN_ENTRY(Driftwater)
