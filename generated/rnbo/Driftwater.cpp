#ifdef RNBO_LIB_PREFIX
#define STR_IMPL(A) #A
#define STR(A) STR_IMPL(A)
#define RNBO_LIB_INCLUDE(X) STR(RNBO_LIB_PREFIX/X)
#else
#define RNBO_LIB_INCLUDE(X) #X
#endif // RNBO_LIB_PREFIX
#ifdef RNBO_INJECTPLATFORM
#define RNBO_USECUSTOMPLATFORM
#include RNBO_INJECTPLATFORM
#endif // RNBO_INJECTPLATFORM

#include RNBO_LIB_INCLUDE(RNBO_Common.h)
#include RNBO_LIB_INCLUDE(RNBO_AudioSignal.h)

namespace RNBO {


#define trunc(x) ((Int)(x))
#define autoref auto&

#if defined(__GNUC__) || defined(__clang__)
    #define RNBO_RESTRICT __restrict__
#elif defined(_MSC_VER)
    #define RNBO_RESTRICT __restrict
#endif

#define FIXEDSIZEARRAYINIT(...) { }

template <class ENGINE = INTERNALENGINE> class Driftwater : public PatcherInterfaceImpl {

friend class EngineCore;
friend class Engine;
friend class MinimalEngine<>;
public:

Driftwater()
: _internalEngine(this)
{
}

~Driftwater()
{
    deallocateSignals();
}

Index getNumMidiInputPorts() const {
    return 0;
}

void processMidiEvent(MillisecondTime , int , ConstByteArray , Index ) {}

Index getNumMidiOutputPorts() const {
    return 0;
}

void process(
    const SampleValue * const* inputs,
    Index numInputs,
    SampleValue * const* outputs,
    Index numOutputs,
    Index n
) {
    this->vs = n;
    this->updateTime(this->getEngine()->getCurrentTime(), (ENGINE*)nullptr, true);
    SampleValue * out1 = (numOutputs >= 1 && outputs[0] ? outputs[0] : this->dummyBuffer);
    SampleValue * out2 = (numOutputs >= 2 && outputs[1] ? outputs[1] : this->dummyBuffer);
    const SampleValue * in1 = (numInputs >= 1 && inputs[0] ? inputs[0] : this->zeroBuffer);
    const SampleValue * in2 = (numInputs >= 2 && inputs[1] ? inputs[1] : this->zeroBuffer);

    this->gen_01_perform(
        in1,
        in2,
        this->gen_01_rate,
        this->gen_01_damp,
        this->gen_01_depth,
        this->gen_01_lpg,
        this->gen_01_mix,
        this->gen_01_bypass,
        out1,
        out2,
        n
    );

    this->stackprotect_perform(n);
    this->globaltransport_advance();
    this->advanceTime((ENGINE*)nullptr);
    this->audioProcessSampleCount += this->vs;
}

void prepareToProcess(number sampleRate, Index maxBlockSize, bool force) {
    RNBO_ASSERT(this->_isInitialized);

    if (this->maxvs < maxBlockSize || !this->didAllocateSignals) {
        this->globaltransport_tempo = resizeSignal(this->globaltransport_tempo, this->maxvs, maxBlockSize);
        this->globaltransport_state = resizeSignal(this->globaltransport_state, this->maxvs, maxBlockSize);
        this->zeroBuffer = resizeSignal(this->zeroBuffer, this->maxvs, maxBlockSize);
        this->dummyBuffer = resizeSignal(this->dummyBuffer, this->maxvs, maxBlockSize);
        this->didAllocateSignals = true;
    }

    const bool sampleRateChanged = sampleRate != this->sr;
    const bool maxvsChanged = maxBlockSize != this->maxvs;
    const bool forceDSPSetup = sampleRateChanged || maxvsChanged || force;

    if (sampleRateChanged || maxvsChanged) {
        this->vs = maxBlockSize;
        this->maxvs = maxBlockSize;
        this->sr = sampleRate;
        this->invsr = 1 / sampleRate;
    }

    this->gen_01_dspsetup(forceDSPSetup);
    this->globaltransport_dspsetup(forceDSPSetup);

    if (sampleRateChanged)
        this->onSampleRateChanged(sampleRate);
}

number msToSamps(MillisecondTime ms, number sampleRate) {
    return ms * sampleRate * 0.001;
}

MillisecondTime sampsToMs(SampleIndex samps) {
    return samps * (this->invsr * 1000);
}

Index getNumInputChannels() const {
    return 2;
}

Index getNumOutputChannels() const {
    return 2;
}

DataRef* getDataRef(DataRefIndex index)  {
    switch (index) {
    case 0:
        {
        return addressOf(this->gen_01_memL_bufferobj);
        break;
        }
    case 1:
        {
        return addressOf(this->gen_01_memR_bufferobj);
        break;
        }
    default:
        {
        return nullptr;
        }
    }
}

DataRefIndex getNumDataRefs() const {
    return 2;
}

void processDataViewUpdate(DataRefIndex index, MillisecondTime time) {
    this->updateTime(time, (ENGINE*)nullptr);

    if (index == 0) {
        this->gen_01_memL_buffer = reInitDataView(this->gen_01_memL_buffer, this->gen_01_memL_bufferobj);
    }

    if (index == 1) {
        this->gen_01_memR_buffer = reInitDataView(this->gen_01_memR_buffer, this->gen_01_memR_bufferobj);
    }
}

void initialize() {
    RNBO_ASSERT(!this->_isInitialized);

    this->gen_01_memL_bufferobj = initDataRef(
        this->gen_01_memL_bufferobj,
        this->dataRefStrings->name0,
        true,
        this->dataRefStrings->file0,
        this->dataRefStrings->tag0
    );

    this->gen_01_memR_bufferobj = initDataRef(
        this->gen_01_memR_bufferobj,
        this->dataRefStrings->name1,
        true,
        this->dataRefStrings->file1,
        this->dataRefStrings->tag1
    );

    this->assign_defaults();
    this->applyState();
    this->gen_01_memL_bufferobj->setIndex(0);
    this->gen_01_memL_buffer = new Float64Buffer(this->gen_01_memL_bufferobj);
    this->gen_01_memR_bufferobj->setIndex(1);
    this->gen_01_memR_buffer = new Float64Buffer(this->gen_01_memR_bufferobj);
    this->initializeObjects();
    this->allocateDataRefs();
    this->startup();
    this->_isInitialized = true;
}

void getPreset(PatcherStateInterface& preset) {
    this->updateTime(this->getEngine()->getCurrentTime(), (ENGINE*)nullptr);
    preset["__presetid"] = "rnbo";
    this->gen_01_param_DriftwaterDSP_rate_getPresetValue(getSubState(preset, "DriftwaterDSP/rate"));
    this->gen_01_param_DriftwaterDSP_damp_getPresetValue(getSubState(preset, "DriftwaterDSP/damp"));
    this->gen_01_param_DriftwaterDSP_depth_getPresetValue(getSubState(preset, "DriftwaterDSP/depth"));
    this->gen_01_param_DriftwaterDSP_lpg_getPresetValue(getSubState(preset, "DriftwaterDSP/lpg"));
    this->gen_01_param_DriftwaterDSP_mix_getPresetValue(getSubState(preset, "DriftwaterDSP/mix"));
    this->gen_01_param_DriftwaterDSP_bypass_getPresetValue(getSubState(preset, "DriftwaterDSP/bypass"));
}

void setPreset(MillisecondTime time, PatcherStateInterface& preset) {
    this->updateTime(time, (ENGINE*)nullptr);
    this->gen_01_param_DriftwaterDSP_rate_setPresetValue(getSubState(preset, "DriftwaterDSP/rate"));
    this->gen_01_param_DriftwaterDSP_damp_setPresetValue(getSubState(preset, "DriftwaterDSP/damp"));
    this->gen_01_param_DriftwaterDSP_depth_setPresetValue(getSubState(preset, "DriftwaterDSP/depth"));
    this->gen_01_param_DriftwaterDSP_lpg_setPresetValue(getSubState(preset, "DriftwaterDSP/lpg"));
    this->gen_01_param_DriftwaterDSP_mix_setPresetValue(getSubState(preset, "DriftwaterDSP/mix"));
    this->gen_01_param_DriftwaterDSP_bypass_setPresetValue(getSubState(preset, "DriftwaterDSP/bypass"));
}

void setParameterValue(ParameterIndex index, ParameterValue v, MillisecondTime time) {
    this->updateTime(time, (ENGINE*)nullptr);

    switch (index) {
    case 0:
        {
        this->gen_01_param_DriftwaterDSP_rate_value_set(v);
        break;
        }
    case 1:
        {
        this->gen_01_param_DriftwaterDSP_damp_value_set(v);
        break;
        }
    case 2:
        {
        this->gen_01_param_DriftwaterDSP_depth_value_set(v);
        break;
        }
    case 3:
        {
        this->gen_01_param_DriftwaterDSP_lpg_value_set(v);
        break;
        }
    case 4:
        {
        this->gen_01_param_DriftwaterDSP_mix_value_set(v);
        break;
        }
    case 5:
        {
        this->gen_01_param_DriftwaterDSP_bypass_value_set(v);
        break;
        }
    }
}

void processParameterEvent(ParameterIndex index, ParameterValue value, MillisecondTime time) {
    this->setParameterValue(index, value, time);
}

void processParameterBangEvent(ParameterIndex index, MillisecondTime time) {
    this->setParameterValue(index, this->getParameterValue(index), time);
}

void processNormalizedParameterEvent(ParameterIndex index, ParameterValue value, MillisecondTime time) {
    this->setParameterValueNormalized(index, value, time);
}

ParameterValue getParameterValue(ParameterIndex index)  {
    switch (index) {
    case 0:
        {
        return this->gen_01_param_DriftwaterDSP_rate_value;
        }
    case 1:
        {
        return this->gen_01_param_DriftwaterDSP_damp_value;
        }
    case 2:
        {
        return this->gen_01_param_DriftwaterDSP_depth_value;
        }
    case 3:
        {
        return this->gen_01_param_DriftwaterDSP_lpg_value;
        }
    case 4:
        {
        return this->gen_01_param_DriftwaterDSP_mix_value;
        }
    case 5:
        {
        return this->gen_01_param_DriftwaterDSP_bypass_value;
        }
    default:
        {
        return 0;
        }
    }
}

ParameterIndex getNumSignalInParameters() const {
    return 0;
}

ParameterIndex getNumSignalOutParameters() const {
    return 0;
}

ParameterIndex getNumParameters() const {
    return 6;
}

ConstCharPointer getParameterName(ParameterIndex index) const {
    switch (index) {
    case 0:
        {
        return "DriftwaterDSP/rate";
        }
    case 1:
        {
        return "DriftwaterDSP/damp";
        }
    case 2:
        {
        return "DriftwaterDSP/depth";
        }
    case 3:
        {
        return "DriftwaterDSP/lpg";
        }
    case 4:
        {
        return "DriftwaterDSP/mix";
        }
    case 5:
        {
        return "DriftwaterDSP/bypass";
        }
    default:
        {
        return "bogus";
        }
    }
}

ConstCharPointer getParameterId(ParameterIndex index) const {
    switch (index) {
    case 0:
        {
        return "DriftwaterDSP/rate";
        }
    case 1:
        {
        return "DriftwaterDSP/damp";
        }
    case 2:
        {
        return "DriftwaterDSP/depth";
        }
    case 3:
        {
        return "DriftwaterDSP/lpg";
        }
    case 4:
        {
        return "DriftwaterDSP/mix";
        }
    case 5:
        {
        return "DriftwaterDSP/bypass";
        }
    default:
        {
        return "bogus";
        }
    }
}

void getParameterInfo(ParameterIndex index, ParameterInfo * info) const {
    {
        switch (index) {
        case 0:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0.28;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 1:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0.72;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 2:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0.32;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 3:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0.58;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 4:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0.48;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 5:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        }
    }
}

ParameterValue applyStepsToNormalizedParameterValue(ParameterValue normalizedValue, int steps) const {
    if (steps == 1) {
        if (normalizedValue > 0) {
            normalizedValue = 1.;
        }
    } else {
        ParameterValue oneStep = (number)1. / (steps - 1);
        ParameterValue numberOfSteps = rnbo_fround(normalizedValue / oneStep * 1 / (number)1) * (number)1;
        normalizedValue = numberOfSteps * oneStep;
    }

    return normalizedValue;
}

ParameterValue convertToNormalizedParameterValue(ParameterIndex index, ParameterValue value) const {
    switch (index) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        {
        {
            value = (value < 0 ? 0 : (value > 1 ? 1 : value));
            ParameterValue normalizedValue = (value - 0) / (1 - 0);
            return normalizedValue;
        }
        }
    default:
        {
        return value;
        }
    }
}

ParameterValue convertFromNormalizedParameterValue(ParameterIndex index, ParameterValue value) const {
    value = (value < 0 ? 0 : (value > 1 ? 1 : value));

    switch (index) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        {
        {
            {
                return 0 + value * (1 - 0);
            }
        }
        }
    default:
        {
        return value;
        }
    }
}

ParameterValue constrainParameterValue(ParameterIndex index, ParameterValue value) const {
    switch (index) {
    case 0:
        {
        return this->gen_01_param_DriftwaterDSP_rate_value_constrain(value);
        }
    case 1:
        {
        return this->gen_01_param_DriftwaterDSP_damp_value_constrain(value);
        }
    case 2:
        {
        return this->gen_01_param_DriftwaterDSP_depth_value_constrain(value);
        }
    case 3:
        {
        return this->gen_01_param_DriftwaterDSP_lpg_value_constrain(value);
        }
    case 4:
        {
        return this->gen_01_param_DriftwaterDSP_mix_value_constrain(value);
        }
    case 5:
        {
        return this->gen_01_param_DriftwaterDSP_bypass_value_constrain(value);
        }
    default:
        {
        return value;
        }
    }
}

void processNumMessage(MessageTag , MessageTag , MillisecondTime , number ) {}

void processListMessage(MessageTag , MessageTag , MillisecondTime , const list& ) {}

void processBangMessage(MessageTag , MessageTag , MillisecondTime ) {}

MessageTagInfo resolveTag(MessageTag tag) const {
    switch (tag) {

    }

    return "";
}

MessageIndex getNumMessages() const {
    return 0;
}

const MessageInfo& getMessageInfo(MessageIndex index) const {
    switch (index) {

    }

    return NullMessageInfo;
}

protected:

		
void advanceTime(EXTERNALENGINE*) {}
void advanceTime(INTERNALENGINE*) {
	_internalEngine.advanceTime(sampstoms(this->vs));
}

void processInternalEvents(MillisecondTime time) {
	_internalEngine.processEventsUntil(time);
}

void updateTime(MillisecondTime time, INTERNALENGINE*, bool inProcess = false) {
	if (time == TimeNow) time = getPatcherTime();
	processInternalEvents(inProcess ? time + sampsToMs(this->vs) : time);
	updateTime(time, (EXTERNALENGINE*)nullptr);
}

Driftwater* operator->() {
    return this;
}
const Driftwater* operator->() const {
    return this;
}
Driftwater* getTopLevelPatcher() {
    return this;
}

void cancelClockEvents()
{
}

template<typename LISTTYPE = list> void listquicksort(LISTTYPE& arr, LISTTYPE& sortindices, Int l, Int h, bool ascending) {
    if (l < h) {
        Int p = (Int)(this->listpartition(arr, sortindices, l, h, ascending));
        this->listquicksort(arr, sortindices, l, p - 1, ascending);
        this->listquicksort(arr, sortindices, p + 1, h, ascending);
    }
}

template<typename LISTTYPE = list> Int listpartition(LISTTYPE& arr, LISTTYPE& sortindices, Int l, Int h, bool ascending) {
    number x = arr[(Index)h];
    Int i = (Int)(l - 1);

    for (Int j = (Int)(l); j <= h - 1; j++) {
        bool asc = (bool)((bool)(ascending) && arr[(Index)j] <= x);
        bool desc = (bool)((bool)(!(bool)(ascending)) && arr[(Index)j] >= x);

        if ((bool)(asc) || (bool)(desc)) {
            i++;
            this->listswapelements(arr, i, j);
            this->listswapelements(sortindices, i, j);
        }
    }

    i++;
    this->listswapelements(arr, i, h);
    this->listswapelements(sortindices, i, h);
    return i;
}

template<typename LISTTYPE = list> void listswapelements(LISTTYPE& arr, Int a, Int b) {
    auto tmp = arr[(Index)a];
    arr[(Index)a] = arr[(Index)b];
    arr[(Index)b] = tmp;
}

inline number linearinterp(number frac, number x, number y) {
    return x + (y - x) * frac;
}

inline number cubicinterp(number a, number w, number x, number y, number z) {
    number a1 = 1. + a;
    number aa = a * a1;
    number b = 1. - a;
    number b1 = 2. - a;
    number bb = b * b1;
    number fw = -.1666667 * bb * a;
    number fx = .5 * bb * a1;
    number fy = .5 * aa * b1;
    number fz = -.1666667 * aa * b;
    return w * fw + x * fx + y * fy + z * fz;
}

inline number fastcubicinterp(number a, number w, number x, number y, number z) {
    number a2 = a * a;
    number f0 = z - y - w + x;
    number f1 = w - x - f0;
    number f2 = y - w;
    number f3 = x;
    return f0 * a * a2 + f1 * a2 + f2 * a + f3;
}

inline number splineinterp(number a, number w, number x, number y, number z) {
    number a2 = a * a;
    number f0 = -0.5 * w + 1.5 * x - 1.5 * y + 0.5 * z;
    number f1 = w - 2.5 * x + 2 * y - 0.5 * z;
    number f2 = -0.5 * w + 0.5 * y;
    return f0 * a * a2 + f1 * a2 + f2 * a + x;
}

inline number spline6interp(number a, number y0, number y1, number y2, number y3, number y4, number y5) {
    number ym2py2 = y0 + y4;
    number ym1py1 = y1 + y3;
    number y2mym2 = y4 - y0;
    number y1mym1 = y3 - y1;
    number sixthym1py1 = (number)1 / (number)6.0 * ym1py1;
    number c0 = (number)1 / (number)120.0 * ym2py2 + (number)13 / (number)60.0 * ym1py1 + (number)11 / (number)20.0 * y2;
    number c1 = (number)1 / (number)24.0 * y2mym2 + (number)5 / (number)12.0 * y1mym1;
    number c2 = (number)1 / (number)12.0 * ym2py2 + sixthym1py1 - (number)1 / (number)2.0 * y2;
    number c3 = (number)1 / (number)12.0 * y2mym2 - (number)1 / (number)6.0 * y1mym1;
    number c4 = (number)1 / (number)24.0 * ym2py2 - sixthym1py1 + (number)1 / (number)4.0 * y2;
    number c5 = (number)1 / (number)120.0 * (y5 - y0) + (number)1 / (number)24.0 * (y1 - y4) + (number)1 / (number)12.0 * (y3 - y2);
    return ((((c5 * a + c4) * a + c3) * a + c2) * a + c1) * a + c0;
}

inline number cosT8(number r) {
    number t84 = 56.0;
    number t83 = 1680.0;
    number t82 = 20160.0;
    number t81 = 2.4801587302e-05;
    number t73 = 42.0;
    number t72 = 840.0;
    number t71 = 1.9841269841e-04;

    if (r < 0.785398163397448309615660845819875721 && r > -0.785398163397448309615660845819875721) {
        number rr = r * r;
        return 1.0 - rr * t81 * (t82 - rr * (t83 - rr * (t84 - rr)));
    } else if (r > 0.0) {
        r -= 1.57079632679489661923132169163975144;
        number rr = r * r;
        return -r * (1.0 - t71 * rr * (t72 - rr * (t73 - rr)));
    } else {
        r += 1.57079632679489661923132169163975144;
        number rr = r * r;
        return r * (1.0 - t71 * rr * (t72 - rr * (t73 - rr)));
    }
}

inline number cosineinterp(number frac, number x, number y) {
    number a2 = (1.0 - this->cosT8(frac * 3.14159265358979323846)) / (number)2.0;
    return x * (1.0 - a2) + y * a2;
}

number mstosamps(MillisecondTime ms) {
    return ms * this->sr * 0.001;
}

number maximum(number x, number y) {
    return (x < y ? y : x);
}

inline number safediv(number num, number denom) {
    return (denom == 0.0 ? 0.0 : num / denom);
}

number __wrapped_op_clamp(number in1, number in2, number in3) {
    return (in1 > in3 ? in3 : (in1 < in2 ? in2 : in1));
}

MillisecondTime sampstoms(number samps) {
    return samps * 1000 / this->sr;
}

void gen_01_param_DriftwaterDSP_rate_value_set(number v) {
    v = this->gen_01_param_DriftwaterDSP_rate_value_constrain(v);
    this->gen_01_param_DriftwaterDSP_rate_value = v;
    this->sendParameter(0, false);

    if (this->gen_01_param_DriftwaterDSP_rate_value != this->gen_01_param_DriftwaterDSP_rate_lastValue) {
        {
            this->getEngine()->presetTouched();
        }

        this->gen_01_param_DriftwaterDSP_rate_lastValue = this->gen_01_param_DriftwaterDSP_rate_value;
    }

    this->gen_01_rate_set(v);
}

void gen_01_param_DriftwaterDSP_damp_value_set(number v) {
    v = this->gen_01_param_DriftwaterDSP_damp_value_constrain(v);
    this->gen_01_param_DriftwaterDSP_damp_value = v;
    this->sendParameter(1, false);

    if (this->gen_01_param_DriftwaterDSP_damp_value != this->gen_01_param_DriftwaterDSP_damp_lastValue) {
        {
            this->getEngine()->presetTouched();
        }

        this->gen_01_param_DriftwaterDSP_damp_lastValue = this->gen_01_param_DriftwaterDSP_damp_value;
    }

    this->gen_01_damp_set(v);
}

void gen_01_param_DriftwaterDSP_depth_value_set(number v) {
    v = this->gen_01_param_DriftwaterDSP_depth_value_constrain(v);
    this->gen_01_param_DriftwaterDSP_depth_value = v;
    this->sendParameter(2, false);

    if (this->gen_01_param_DriftwaterDSP_depth_value != this->gen_01_param_DriftwaterDSP_depth_lastValue) {
        {
            this->getEngine()->presetTouched();
        }

        this->gen_01_param_DriftwaterDSP_depth_lastValue = this->gen_01_param_DriftwaterDSP_depth_value;
    }

    this->gen_01_depth_set(v);
}

void gen_01_param_DriftwaterDSP_lpg_value_set(number v) {
    v = this->gen_01_param_DriftwaterDSP_lpg_value_constrain(v);
    this->gen_01_param_DriftwaterDSP_lpg_value = v;
    this->sendParameter(3, false);

    if (this->gen_01_param_DriftwaterDSP_lpg_value != this->gen_01_param_DriftwaterDSP_lpg_lastValue) {
        {
            this->getEngine()->presetTouched();
        }

        this->gen_01_param_DriftwaterDSP_lpg_lastValue = this->gen_01_param_DriftwaterDSP_lpg_value;
    }

    this->gen_01_lpg_set(v);
}

void gen_01_param_DriftwaterDSP_mix_value_set(number v) {
    v = this->gen_01_param_DriftwaterDSP_mix_value_constrain(v);
    this->gen_01_param_DriftwaterDSP_mix_value = v;
    this->sendParameter(4, false);

    if (this->gen_01_param_DriftwaterDSP_mix_value != this->gen_01_param_DriftwaterDSP_mix_lastValue) {
        {
            this->getEngine()->presetTouched();
        }

        this->gen_01_param_DriftwaterDSP_mix_lastValue = this->gen_01_param_DriftwaterDSP_mix_value;
    }

    this->gen_01_mix_set(v);
}

void gen_01_param_DriftwaterDSP_bypass_value_set(number v) {
    v = this->gen_01_param_DriftwaterDSP_bypass_value_constrain(v);
    this->gen_01_param_DriftwaterDSP_bypass_value = v;
    this->sendParameter(5, false);

    if (this->gen_01_param_DriftwaterDSP_bypass_value != this->gen_01_param_DriftwaterDSP_bypass_lastValue) {
        {
            this->getEngine()->presetTouched();
        }

        this->gen_01_param_DriftwaterDSP_bypass_lastValue = this->gen_01_param_DriftwaterDSP_bypass_value;
    }

    this->gen_01_bypass_set(v);
}

MillisecondTime getPatcherTime() const {
    return this->_currentTime;
}

void deallocateSignals() {
    Index i;
    this->globaltransport_tempo = freeSignal(this->globaltransport_tempo);
    this->globaltransport_state = freeSignal(this->globaltransport_state);
    this->zeroBuffer = freeSignal(this->zeroBuffer);
    this->dummyBuffer = freeSignal(this->dummyBuffer);
}

Index getMaxBlockSize() const {
    return this->maxvs;
}

number getSampleRate() const {
    return this->sr;
}

bool hasFixedVectorSize() const {
    return false;
}

void setProbingTarget(MessageTag ) {}

void fillDataRef(DataRefIndex , DataRef& ) {}

void zeroDataRef(DataRef& ref) {
    ref->setZero();
}

void allocateDataRefs() {
    this->gen_01_memL_buffer = this->gen_01_memL_buffer->allocateIfNeeded();

    if (this->gen_01_memL_bufferobj->hasRequestedSize()) {
        if (this->gen_01_memL_bufferobj->wantsFill())
            this->zeroDataRef(this->gen_01_memL_bufferobj);

        this->getEngine()->sendDataRefUpdated(0);
    }

    this->gen_01_memR_buffer = this->gen_01_memR_buffer->allocateIfNeeded();

    if (this->gen_01_memR_bufferobj->hasRequestedSize()) {
        if (this->gen_01_memR_bufferobj->wantsFill())
            this->zeroDataRef(this->gen_01_memR_bufferobj);

        this->getEngine()->sendDataRefUpdated(1);
    }
}

void initializeObjects() {
    this->gen_01_memL_init();
    this->gen_01_memR_init();
    this->gen_01_targetL_init();
    this->gen_01_targetR_init();
    this->gen_01_driftL_init();
    this->gen_01_driftR_init();
    this->gen_01_countL_init();
    this->gen_01_countR_init();
    this->gen_01_envL_init();
    this->gen_01_envR_init();
    this->gen_01_lpL_init();
    this->gen_01_lpR_init();
    this->gen_01_bypassFade_init();
}

Index getIsMuted()  {
    return this->isMuted;
}

void setIsMuted(Index v)  {
    this->isMuted = v;
}

void onSampleRateChanged(double ) {}

void extractState(PatcherStateInterface& ) {}

void applyState() {}

void processClockEvent(MillisecondTime , ClockId , bool , ParameterValue ) {}

void processOutletAtCurrentTime(EngineLink* , OutletIndex , ParameterValue ) {}

void processOutletEvent(
    EngineLink* sender,
    OutletIndex index,
    ParameterValue value,
    MillisecondTime time
) {
    this->updateTime(time, (ENGINE*)nullptr);
    this->processOutletAtCurrentTime(sender, index, value);
}

void sendOutlet(OutletIndex index, ParameterValue value) {
    this->getEngine()->sendOutlet(this, index, value);
}

void startup() {
    this->updateTime(this->getEngine()->getCurrentTime(), (ENGINE*)nullptr);

    {
        this->scheduleParamInit(0, 0);
    }

    {
        this->scheduleParamInit(1, 0);
    }

    {
        this->scheduleParamInit(2, 0);
    }

    {
        this->scheduleParamInit(3, 0);
    }

    {
        this->scheduleParamInit(4, 0);
    }

    {
        this->scheduleParamInit(5, 0);
    }

    this->processParamInitEvents();
}

number gen_01_param_DriftwaterDSP_rate_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));
    return v;
}

number gen_01_rate_constrain(number v) const {
    if (v < 0)
        v = 0;

    if (v > 1)
        v = 1;

    return v;
}

void gen_01_rate_set(number v) {
    v = this->gen_01_rate_constrain(v);
    this->gen_01_rate = v;
}

number gen_01_param_DriftwaterDSP_damp_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));
    return v;
}

number gen_01_damp_constrain(number v) const {
    if (v < 0)
        v = 0;

    if (v > 1)
        v = 1;

    return v;
}

void gen_01_damp_set(number v) {
    v = this->gen_01_damp_constrain(v);
    this->gen_01_damp = v;
}

number gen_01_param_DriftwaterDSP_depth_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));
    return v;
}

number gen_01_depth_constrain(number v) const {
    if (v < 0)
        v = 0;

    if (v > 1)
        v = 1;

    return v;
}

void gen_01_depth_set(number v) {
    v = this->gen_01_depth_constrain(v);
    this->gen_01_depth = v;
}

number gen_01_param_DriftwaterDSP_lpg_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));
    return v;
}

number gen_01_lpg_constrain(number v) const {
    if (v < 0)
        v = 0;

    if (v > 1)
        v = 1;

    return v;
}

void gen_01_lpg_set(number v) {
    v = this->gen_01_lpg_constrain(v);
    this->gen_01_lpg = v;
}

number gen_01_param_DriftwaterDSP_mix_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));
    return v;
}

number gen_01_mix_constrain(number v) const {
    if (v < 0)
        v = 0;

    if (v > 1)
        v = 1;

    return v;
}

void gen_01_mix_set(number v) {
    v = this->gen_01_mix_constrain(v);
    this->gen_01_mix = v;
}

number gen_01_param_DriftwaterDSP_bypass_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));
    return v;
}

number gen_01_bypass_constrain(number v) const {
    if (v < 0)
        v = 0;

    if (v > 1)
        v = 1;

    return v;
}

void gen_01_bypass_set(number v) {
    v = this->gen_01_bypass_constrain(v);
    this->gen_01_bypass = v;
}

void gen_01_perform(
    const Sample * in1,
    const Sample * in2,
    number rate,
    number damp,
    number depth,
    number lpg,
    number mix,
    number bypass,
    SampleValue * out1,
    SampleValue * out2,
    Index n
) {
    auto __gen_01_bypassFade_value = this->gen_01_bypassFade_value;
    auto __gen_01_lpR_value = this->gen_01_lpR_value;
    auto __gen_01_lpL_value = this->gen_01_lpL_value;
    auto __gen_01_envR_value = this->gen_01_envR_value;
    auto __gen_01_envL_value = this->gen_01_envL_value;
    auto __gen_01_driftR_value = this->gen_01_driftR_value;
    auto __gen_01_driftL_value = this->gen_01_driftL_value;
    auto __gen_01_targetR_value = this->gen_01_targetR_value;
    auto __gen_01_targetL_value = this->gen_01_targetL_value;
    auto __gen_01_countR_value = this->gen_01_countR_value;
    auto __gen_01_countL_value = this->gen_01_countL_value;
    Index i;

    for (i = 0; i < (Index)n; i++) {
        number r_1 = this->gen_01_slide_0_next(rate, 2000, 2000);
        number d_3 = this->gen_01_slide_2_next(damp, 2000, 2000);
        number dep_5 = this->gen_01_slide_4_next(depth, 2000, 2000);
        number wet_7 = this->gen_01_slide_6_next(mix, 800, 800);
        number interval_8 = this->sr * (2.8 + r_1 * (0.055 - 2.8));
        __gen_01_countL_value = __gen_01_countL_value - 1;
        __gen_01_countR_value = __gen_01_countR_value - 1;
        number seedL_9 = fract(rnbo_sin((__gen_01_countL_value + 17.13) * 12.9898) * 43758.5453);
        number seedR_10 = fract(rnbo_sin((__gen_01_countR_value + 91.77) * 78.233) * 24634.6345);
        number changeL_11 = __gen_01_countL_value <= 0;
        number changeR_12 = __gen_01_countR_value <= 0;
        __gen_01_targetL_value = __gen_01_targetL_value + changeL_11 * (seedL_9 * 2 - 1 - __gen_01_targetL_value);
        __gen_01_targetR_value = __gen_01_targetR_value + changeR_12 * ((seedR_10 * 1.55 + __gen_01_targetL_value * 0.25) / (number)1.8 - __gen_01_targetR_value);
        __gen_01_countL_value = __gen_01_countL_value + changeL_11 * (interval_8 * (0.72 + seedR_10 * (1.28 - 0.72)) - __gen_01_countL_value);
        __gen_01_countR_value = __gen_01_countR_value + changeR_12 * (interval_8 * (0.78 + seedL_9 * (1.22 - 0.78)) - __gen_01_countR_value);

        number slew_13 = rnbo_exp(
            (this->maximum(1, this->sr * (0.004 + d_3 * d_3 * (2.4 - 0.004))) == 0. ? 0. : -1 / this->maximum(1, this->sr * (0.004 + d_3 * d_3 * (2.4 - 0.004))))
        );

        __gen_01_driftL_value = __gen_01_driftL_value * slew_13 + __gen_01_targetL_value * (1 - slew_13);
        __gen_01_driftR_value = __gen_01_driftR_value * slew_13 + __gen_01_targetR_value * (1 - slew_13);
        number motionL_14 = __gen_01_driftL_value;
        number motionR_15 = __gen_01_driftR_value;
        number spanMs_16 = 13 * dep_5 * dep_5;
        number delayL_17 = (22 + motionL_14 * spanMs_16) * this->sr / (number)1000;
        number delayR_18 = (22 + motionR_15 * spanMs_16) * this->sr / (number)1000;
        number tapL_19 = this->gen_01_memL_read(delayL_17, 0);
        number tapR_20 = this->gen_01_memR_read(delayR_18, 0);
        __gen_01_envL_value = this->maximum(rnbo_abs(in1[(Index)i]), __gen_01_envL_value * 0.9992);
        __gen_01_envR_value = this->maximum(rnbo_abs(in2[(Index)i]), __gen_01_envR_value * 0.9992);

        number cutL_21 = 280 + this->__wrapped_op_clamp(
            0.05 + lpg * ((__gen_01_envL_value * 5 <= 0 ? 0 : rnbo_sqrt(__gen_01_envL_value * 5))),
            0,
            1
        ) * (15500 - 280);

        number cutR_22 = 280 + this->__wrapped_op_clamp(
            0.05 + lpg * ((__gen_01_envR_value * 5 <= 0 ? 0 : rnbo_sqrt(__gen_01_envR_value * 5))),
            0,
            1
        ) * (15500 - 280);

        number aL_23 = 1 - rnbo_exp((this->sr == 0. ? 0. : -6.2831853071796 * cutL_21 / this->sr));
        number aR_24 = 1 - rnbo_exp((this->sr == 0. ? 0. : -6.2831853071796 * cutR_22 / this->sr));
        __gen_01_lpL_value = __gen_01_lpL_value + aL_23 * (tapL_19 - __gen_01_lpL_value);
        __gen_01_lpR_value = __gen_01_lpR_value + aR_24 * (tapR_20 - __gen_01_lpR_value);
        this->gen_01_memL_write(in1[(Index)i]);
        this->gen_01_memR_write(in2[(Index)i]);
        number dryGain_25 = rnbo_cos(wet_7 * 3.14159265358979323846 * 0.5);
        number wetGain_26 = rnbo_sin(wet_7 * 3.14159265358979323846 * 0.5);
        number fxL_27 = in1[(Index)i] * dryGain_25 + __gen_01_lpL_value * wetGain_26;
        number fxR_28 = in2[(Index)i] * dryGain_25 + __gen_01_lpR_value * wetGain_26;
        __gen_01_bypassFade_value = this->gen_01_slide_29_next(bypass, 1000, 1000);
        number expr_1_30 = fxL_27 + __gen_01_bypassFade_value * (in1[(Index)i] - fxL_27);
        number expr_2_31 = fxR_28 + __gen_01_bypassFade_value * (in2[(Index)i] - fxR_28);
        out1[(Index)i] = expr_1_30;
        out2[(Index)i] = expr_2_31;
        this->gen_01_memL_step();
        this->gen_01_memR_step();
    }

    this->gen_01_countL_value = __gen_01_countL_value;
    this->gen_01_countR_value = __gen_01_countR_value;
    this->gen_01_targetL_value = __gen_01_targetL_value;
    this->gen_01_targetR_value = __gen_01_targetR_value;
    this->gen_01_driftL_value = __gen_01_driftL_value;
    this->gen_01_driftR_value = __gen_01_driftR_value;
    this->gen_01_envL_value = __gen_01_envL_value;
    this->gen_01_envR_value = __gen_01_envR_value;
    this->gen_01_lpL_value = __gen_01_lpL_value;
    this->gen_01_lpR_value = __gen_01_lpR_value;
    this->gen_01_bypassFade_value = __gen_01_bypassFade_value;
}

void stackprotect_perform(Index n) {
    RNBO_UNUSED(n);
    auto __stackprotect_count = this->stackprotect_count;
    __stackprotect_count = 0;
    this->stackprotect_count = __stackprotect_count;
}

void gen_01_memL_step() {
    this->gen_01_memL_reader++;

    if (this->gen_01_memL_reader >= (Int)(this->gen_01_memL_buffer->getSize()))
        this->gen_01_memL_reader = 0;
}

number gen_01_memL_read(number size, Int interp) {
    RNBO_UNUSED(interp);

    {
        number r = (Int)(this->gen_01_memL_buffer->getSize()) + this->gen_01_memL_reader - ((size > this->gen_01_memL__maxdelay ? this->gen_01_memL__maxdelay : (size < (this->gen_01_memL_reader != this->gen_01_memL_writer) ? this->gen_01_memL_reader != this->gen_01_memL_writer : size)));
        Int index1 = (Int)(rnbo_floor(r));
        number frac = r - index1;
        Int index2 = (Int)(index1 + 1);

        return this->linearinterp(frac, this->gen_01_memL_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_memL_wrap))
        ), this->gen_01_memL_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->gen_01_memL_wrap))
        ));
    }

    number r = (Int)(this->gen_01_memL_buffer->getSize()) + this->gen_01_memL_reader - ((size > this->gen_01_memL__maxdelay ? this->gen_01_memL__maxdelay : (size < (this->gen_01_memL_reader != this->gen_01_memL_writer) ? this->gen_01_memL_reader != this->gen_01_memL_writer : size)));
    Int index1 = (Int)(rnbo_floor(r));

    return this->gen_01_memL_buffer->getSample(
        0,
        (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_memL_wrap))
    );
}

void gen_01_memL_write(number v) {
    this->gen_01_memL_writer = this->gen_01_memL_reader;
    this->gen_01_memL_buffer[(Index)this->gen_01_memL_writer] = v;
}

number gen_01_memL_next(number v, Int size) {
    number effectiveSize = (size == -1 ? this->gen_01_memL__maxdelay : size);
    number val = this->gen_01_memL_read(effectiveSize, 0);
    this->gen_01_memL_write(v);
    this->gen_01_memL_step();
    return val;
}

array<Index, 2> gen_01_memL_calcSizeInSamples() {
    number sizeInSamples = 0;
    Index allocatedSizeInSamples = 0;

    {
        sizeInSamples = this->gen_01_memL_evaluateSizeExpr(this->sr, this->vs);
        this->gen_01_memL_sizemode = 0;
    }

    sizeInSamples = rnbo_floor(sizeInSamples);
    sizeInSamples = this->maximum(sizeInSamples, 2);
    allocatedSizeInSamples = (Index)(sizeInSamples);
    allocatedSizeInSamples = nextpoweroftwo(allocatedSizeInSamples);
    return {sizeInSamples, allocatedSizeInSamples};
}

void gen_01_memL_init() {
    auto result = this->gen_01_memL_calcSizeInSamples();
    this->gen_01_memL__maxdelay = result[0];
    Index requestedSizeInSamples = (Index)(result[1]);
    this->gen_01_memL_buffer->requestSize(requestedSizeInSamples, 1);
    this->gen_01_memL_wrap = requestedSizeInSamples - 1;
}

void gen_01_memL_clear() {
    this->gen_01_memL_buffer->setZero();
}

void gen_01_memL_reset() {
    auto result = this->gen_01_memL_calcSizeInSamples();
    this->gen_01_memL__maxdelay = result[0];
    Index allocatedSizeInSamples = (Index)(result[1]);
    this->gen_01_memL_buffer->setSize(allocatedSizeInSamples);
    updateDataRef(this, this->gen_01_memL_buffer);
    this->gen_01_memL_wrap = this->gen_01_memL_buffer->getSize() - 1;
    this->gen_01_memL_clear();

    if (this->gen_01_memL_reader >= this->gen_01_memL__maxdelay || this->gen_01_memL_writer >= this->gen_01_memL__maxdelay) {
        this->gen_01_memL_reader = 0;
        this->gen_01_memL_writer = 0;
    }
}

void gen_01_memL_dspsetup() {
    this->gen_01_memL_reset();
}

number gen_01_memL_evaluateSizeExpr(number samplerate, number vectorsize) {
    RNBO_UNUSED(vectorsize);
    RNBO_UNUSED(samplerate);
    return 16384;
}

number gen_01_memL_size() {
    return this->gen_01_memL__maxdelay;
}

void gen_01_memR_step() {
    this->gen_01_memR_reader++;

    if (this->gen_01_memR_reader >= (Int)(this->gen_01_memR_buffer->getSize()))
        this->gen_01_memR_reader = 0;
}

number gen_01_memR_read(number size, Int interp) {
    RNBO_UNUSED(interp);

    {
        number r = (Int)(this->gen_01_memR_buffer->getSize()) + this->gen_01_memR_reader - ((size > this->gen_01_memR__maxdelay ? this->gen_01_memR__maxdelay : (size < (this->gen_01_memR_reader != this->gen_01_memR_writer) ? this->gen_01_memR_reader != this->gen_01_memR_writer : size)));
        Int index1 = (Int)(rnbo_floor(r));
        number frac = r - index1;
        Int index2 = (Int)(index1 + 1);

        return this->linearinterp(frac, this->gen_01_memR_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_memR_wrap))
        ), this->gen_01_memR_buffer->getSample(
            0,
            (Index)((BinOpInt)((BinOpInt)index2 & (BinOpInt)this->gen_01_memR_wrap))
        ));
    }

    number r = (Int)(this->gen_01_memR_buffer->getSize()) + this->gen_01_memR_reader - ((size > this->gen_01_memR__maxdelay ? this->gen_01_memR__maxdelay : (size < (this->gen_01_memR_reader != this->gen_01_memR_writer) ? this->gen_01_memR_reader != this->gen_01_memR_writer : size)));
    Int index1 = (Int)(rnbo_floor(r));

    return this->gen_01_memR_buffer->getSample(
        0,
        (Index)((BinOpInt)((BinOpInt)index1 & (BinOpInt)this->gen_01_memR_wrap))
    );
}

void gen_01_memR_write(number v) {
    this->gen_01_memR_writer = this->gen_01_memR_reader;
    this->gen_01_memR_buffer[(Index)this->gen_01_memR_writer] = v;
}

number gen_01_memR_next(number v, Int size) {
    number effectiveSize = (size == -1 ? this->gen_01_memR__maxdelay : size);
    number val = this->gen_01_memR_read(effectiveSize, 0);
    this->gen_01_memR_write(v);
    this->gen_01_memR_step();
    return val;
}

array<Index, 2> gen_01_memR_calcSizeInSamples() {
    number sizeInSamples = 0;
    Index allocatedSizeInSamples = 0;

    {
        sizeInSamples = this->gen_01_memR_evaluateSizeExpr(this->sr, this->vs);
        this->gen_01_memR_sizemode = 0;
    }

    sizeInSamples = rnbo_floor(sizeInSamples);
    sizeInSamples = this->maximum(sizeInSamples, 2);
    allocatedSizeInSamples = (Index)(sizeInSamples);
    allocatedSizeInSamples = nextpoweroftwo(allocatedSizeInSamples);
    return {sizeInSamples, allocatedSizeInSamples};
}

void gen_01_memR_init() {
    auto result = this->gen_01_memR_calcSizeInSamples();
    this->gen_01_memR__maxdelay = result[0];
    Index requestedSizeInSamples = (Index)(result[1]);
    this->gen_01_memR_buffer->requestSize(requestedSizeInSamples, 1);
    this->gen_01_memR_wrap = requestedSizeInSamples - 1;
}

void gen_01_memR_clear() {
    this->gen_01_memR_buffer->setZero();
}

void gen_01_memR_reset() {
    auto result = this->gen_01_memR_calcSizeInSamples();
    this->gen_01_memR__maxdelay = result[0];
    Index allocatedSizeInSamples = (Index)(result[1]);
    this->gen_01_memR_buffer->setSize(allocatedSizeInSamples);
    updateDataRef(this, this->gen_01_memR_buffer);
    this->gen_01_memR_wrap = this->gen_01_memR_buffer->getSize() - 1;
    this->gen_01_memR_clear();

    if (this->gen_01_memR_reader >= this->gen_01_memR__maxdelay || this->gen_01_memR_writer >= this->gen_01_memR__maxdelay) {
        this->gen_01_memR_reader = 0;
        this->gen_01_memR_writer = 0;
    }
}

void gen_01_memR_dspsetup() {
    this->gen_01_memR_reset();
}

number gen_01_memR_evaluateSizeExpr(number samplerate, number vectorsize) {
    RNBO_UNUSED(vectorsize);
    RNBO_UNUSED(samplerate);
    return 16384;
}

number gen_01_memR_size() {
    return this->gen_01_memR__maxdelay;
}

number gen_01_targetL_getvalue() {
    return this->gen_01_targetL_value;
}

void gen_01_targetL_setvalue(number val) {
    this->gen_01_targetL_value = val;
}

void gen_01_targetL_reset() {
    this->gen_01_targetL_value = 0;
}

void gen_01_targetL_init() {
    this->gen_01_targetL_value = 0;
}

number gen_01_targetR_getvalue() {
    return this->gen_01_targetR_value;
}

void gen_01_targetR_setvalue(number val) {
    this->gen_01_targetR_value = val;
}

void gen_01_targetR_reset() {
    this->gen_01_targetR_value = 0;
}

void gen_01_targetR_init() {
    this->gen_01_targetR_value = 0;
}

number gen_01_driftL_getvalue() {
    return this->gen_01_driftL_value;
}

void gen_01_driftL_setvalue(number val) {
    this->gen_01_driftL_value = val;
}

void gen_01_driftL_reset() {
    this->gen_01_driftL_value = 0;
}

void gen_01_driftL_init() {
    this->gen_01_driftL_value = 0;
}

number gen_01_driftR_getvalue() {
    return this->gen_01_driftR_value;
}

void gen_01_driftR_setvalue(number val) {
    this->gen_01_driftR_value = val;
}

void gen_01_driftR_reset() {
    this->gen_01_driftR_value = 0;
}

void gen_01_driftR_init() {
    this->gen_01_driftR_value = 0;
}

number gen_01_countL_getvalue() {
    return this->gen_01_countL_value;
}

void gen_01_countL_setvalue(number val) {
    this->gen_01_countL_value = val;
}

void gen_01_countL_reset() {
    this->gen_01_countL_value = 0;
}

void gen_01_countL_init() {
    this->gen_01_countL_value = 1;
}

number gen_01_countR_getvalue() {
    return this->gen_01_countR_value;
}

void gen_01_countR_setvalue(number val) {
    this->gen_01_countR_value = val;
}

void gen_01_countR_reset() {
    this->gen_01_countR_value = 0;
}

void gen_01_countR_init() {
    this->gen_01_countR_value = 777;
}

number gen_01_envL_getvalue() {
    return this->gen_01_envL_value;
}

void gen_01_envL_setvalue(number val) {
    this->gen_01_envL_value = val;
}

void gen_01_envL_reset() {
    this->gen_01_envL_value = 0;
}

void gen_01_envL_init() {
    this->gen_01_envL_value = 0;
}

number gen_01_envR_getvalue() {
    return this->gen_01_envR_value;
}

void gen_01_envR_setvalue(number val) {
    this->gen_01_envR_value = val;
}

void gen_01_envR_reset() {
    this->gen_01_envR_value = 0;
}

void gen_01_envR_init() {
    this->gen_01_envR_value = 0;
}

number gen_01_lpL_getvalue() {
    return this->gen_01_lpL_value;
}

void gen_01_lpL_setvalue(number val) {
    this->gen_01_lpL_value = val;
}

void gen_01_lpL_reset() {
    this->gen_01_lpL_value = 0;
}

void gen_01_lpL_init() {
    this->gen_01_lpL_value = 0;
}

number gen_01_lpR_getvalue() {
    return this->gen_01_lpR_value;
}

void gen_01_lpR_setvalue(number val) {
    this->gen_01_lpR_value = val;
}

void gen_01_lpR_reset() {
    this->gen_01_lpR_value = 0;
}

void gen_01_lpR_init() {
    this->gen_01_lpR_value = 0;
}

number gen_01_bypassFade_getvalue() {
    return this->gen_01_bypassFade_value;
}

void gen_01_bypassFade_setvalue(number val) {
    this->gen_01_bypassFade_value = val;
}

void gen_01_bypassFade_reset() {
    this->gen_01_bypassFade_value = 0;
}

void gen_01_bypassFade_init() {
    this->gen_01_bypassFade_value = 0;
}

number gen_01_slide_0_next(number x, number up, number down) {
    RNBO_UNUSED(down);
    RNBO_UNUSED(up);
    number temp = x - this->gen_01_slide_0_prev;
    auto iup = this->safediv(1., this->maximum(1., rnbo_abs(2000)));
    auto idown = this->safediv(1., this->maximum(1., rnbo_abs(2000)));
    this->gen_01_slide_0_prev = this->gen_01_slide_0_prev + ((x > this->gen_01_slide_0_prev ? iup : idown)) * temp;
    return this->gen_01_slide_0_prev;
}

void gen_01_slide_0_reset() {
    this->gen_01_slide_0_prev = 0;
}

number gen_01_slide_2_next(number x, number up, number down) {
    RNBO_UNUSED(down);
    RNBO_UNUSED(up);
    number temp = x - this->gen_01_slide_2_prev;
    auto iup = this->safediv(1., this->maximum(1., rnbo_abs(2000)));
    auto idown = this->safediv(1., this->maximum(1., rnbo_abs(2000)));
    this->gen_01_slide_2_prev = this->gen_01_slide_2_prev + ((x > this->gen_01_slide_2_prev ? iup : idown)) * temp;
    return this->gen_01_slide_2_prev;
}

void gen_01_slide_2_reset() {
    this->gen_01_slide_2_prev = 0;
}

number gen_01_slide_4_next(number x, number up, number down) {
    RNBO_UNUSED(down);
    RNBO_UNUSED(up);
    number temp = x - this->gen_01_slide_4_prev;
    auto iup = this->safediv(1., this->maximum(1., rnbo_abs(2000)));
    auto idown = this->safediv(1., this->maximum(1., rnbo_abs(2000)));
    this->gen_01_slide_4_prev = this->gen_01_slide_4_prev + ((x > this->gen_01_slide_4_prev ? iup : idown)) * temp;
    return this->gen_01_slide_4_prev;
}

void gen_01_slide_4_reset() {
    this->gen_01_slide_4_prev = 0;
}

number gen_01_slide_6_next(number x, number up, number down) {
    RNBO_UNUSED(down);
    RNBO_UNUSED(up);
    number temp = x - this->gen_01_slide_6_prev;
    auto iup = this->safediv(1., this->maximum(1., rnbo_abs(800)));
    auto idown = this->safediv(1., this->maximum(1., rnbo_abs(800)));
    this->gen_01_slide_6_prev = this->gen_01_slide_6_prev + ((x > this->gen_01_slide_6_prev ? iup : idown)) * temp;
    return this->gen_01_slide_6_prev;
}

void gen_01_slide_6_reset() {
    this->gen_01_slide_6_prev = 0;
}

number gen_01_slide_29_next(number x, number up, number down) {
    RNBO_UNUSED(down);
    RNBO_UNUSED(up);
    number temp = x - this->gen_01_slide_29_prev;
    auto iup = this->safediv(1., this->maximum(1., rnbo_abs(1000)));
    auto idown = this->safediv(1., this->maximum(1., rnbo_abs(1000)));
    this->gen_01_slide_29_prev = this->gen_01_slide_29_prev + ((x > this->gen_01_slide_29_prev ? iup : idown)) * temp;
    return this->gen_01_slide_29_prev;
}

void gen_01_slide_29_reset() {
    this->gen_01_slide_29_prev = 0;
}

void gen_01_dspsetup(bool force) {
    if ((bool)(this->gen_01_setupDone) && (bool)(!(bool)(force)))
        return;

    this->gen_01_setupDone = true;
    this->gen_01_memL_dspsetup();
    this->gen_01_memR_dspsetup();
}

void gen_01_param_DriftwaterDSP_rate_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->gen_01_param_DriftwaterDSP_rate_value;
}

void gen_01_param_DriftwaterDSP_rate_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->gen_01_param_DriftwaterDSP_rate_value_set(preset["value"]);
}

void gen_01_param_DriftwaterDSP_damp_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->gen_01_param_DriftwaterDSP_damp_value;
}

void gen_01_param_DriftwaterDSP_damp_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->gen_01_param_DriftwaterDSP_damp_value_set(preset["value"]);
}

void gen_01_param_DriftwaterDSP_depth_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->gen_01_param_DriftwaterDSP_depth_value;
}

void gen_01_param_DriftwaterDSP_depth_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->gen_01_param_DriftwaterDSP_depth_value_set(preset["value"]);
}

void gen_01_param_DriftwaterDSP_lpg_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->gen_01_param_DriftwaterDSP_lpg_value;
}

void gen_01_param_DriftwaterDSP_lpg_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->gen_01_param_DriftwaterDSP_lpg_value_set(preset["value"]);
}

void gen_01_param_DriftwaterDSP_mix_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->gen_01_param_DriftwaterDSP_mix_value;
}

void gen_01_param_DriftwaterDSP_mix_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->gen_01_param_DriftwaterDSP_mix_value_set(preset["value"]);
}

void gen_01_param_DriftwaterDSP_bypass_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->gen_01_param_DriftwaterDSP_bypass_value;
}

void gen_01_param_DriftwaterDSP_bypass_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->gen_01_param_DriftwaterDSP_bypass_value_set(preset["value"]);
}

void globaltransport_advance() {}

void globaltransport_dspsetup(bool ) {}

bool stackprotect_check() {
    this->stackprotect_count++;

    if (this->stackprotect_count > 128) {
        console->log("STACK OVERFLOW DETECTED - stopped processing branch !");
        return true;
    }

    return false;
}

Index getPatcherSerial() const {
    return 0;
}

void sendParameter(ParameterIndex index, bool ignoreValue) {
    this->getEngine()->notifyParameterValueChanged(index, (ignoreValue ? 0 : this->getParameterValue(index)), ignoreValue);
}

void scheduleParamInit(ParameterIndex index, Index order) {
    this->paramInitIndices->push(index);
    this->paramInitOrder->push(order);
}

void processParamInitEvents() {
    this->listquicksort(
        this->paramInitOrder,
        this->paramInitIndices,
        0,
        (int)(this->paramInitOrder->length - 1),
        true
    );

    for (Index i = 0; i < this->paramInitOrder->length; i++) {
        this->getEngine()->scheduleParameterBang(this->paramInitIndices[i], 0);
    }
}

void updateTime(MillisecondTime time, EXTERNALENGINE* engine, bool inProcess = false) {
    RNBO_UNUSED(inProcess);
    RNBO_UNUSED(engine);
    this->_currentTime = time;
    auto offset = rnbo_fround(this->msToSamps(time - this->getEngine()->getCurrentTime(), this->sr));

    if (offset >= (SampleIndex)(this->vs))
        offset = (SampleIndex)(this->vs) - 1;

    if (offset < 0)
        offset = 0;

    this->sampleOffsetIntoNextAudioBuffer = (Index)(offset);
}

void assign_defaults()
{
    gen_01_in1 = 0;
    gen_01_in2 = 0;
    gen_01_rate = 0.28;
    gen_01_damp = 0.72;
    gen_01_depth = 0.32;
    gen_01_lpg = 0.58;
    gen_01_mix = 0.48;
    gen_01_bypass = 0;
    gen_01_param_DriftwaterDSP_rate_value = 0.28;
    gen_01_param_DriftwaterDSP_damp_value = 0.72;
    gen_01_param_DriftwaterDSP_depth_value = 0.32;
    gen_01_param_DriftwaterDSP_lpg_value = 0.58;
    gen_01_param_DriftwaterDSP_mix_value = 0.48;
    gen_01_param_DriftwaterDSP_bypass_value = 0;
    _currentTime = 0;
    audioProcessSampleCount = 0;
    sampleOffsetIntoNextAudioBuffer = 0;
    zeroBuffer = nullptr;
    dummyBuffer = nullptr;
    didAllocateSignals = 0;
    vs = 0;
    maxvs = 0;
    sr = 44100;
    invsr = 0.000022675736961451248;
    gen_01_memL__maxdelay = 0;
    gen_01_memL_sizemode = 0;
    gen_01_memL_wrap = 0;
    gen_01_memL_reader = 0;
    gen_01_memL_writer = 0;
    gen_01_memR__maxdelay = 0;
    gen_01_memR_sizemode = 0;
    gen_01_memR_wrap = 0;
    gen_01_memR_reader = 0;
    gen_01_memR_writer = 0;
    gen_01_targetL_value = 0;
    gen_01_targetR_value = 0;
    gen_01_driftL_value = 0;
    gen_01_driftR_value = 0;
    gen_01_countL_value = 0;
    gen_01_countR_value = 0;
    gen_01_envL_value = 0;
    gen_01_envR_value = 0;
    gen_01_lpL_value = 0;
    gen_01_lpR_value = 0;
    gen_01_bypassFade_value = 0;
    gen_01_slide_0_prev = 0;
    gen_01_slide_2_prev = 0;
    gen_01_slide_4_prev = 0;
    gen_01_slide_6_prev = 0;
    gen_01_slide_29_prev = 0;
    gen_01_setupDone = false;
    gen_01_param_DriftwaterDSP_rate_lastValue = 0;
    gen_01_param_DriftwaterDSP_damp_lastValue = 0;
    gen_01_param_DriftwaterDSP_depth_lastValue = 0;
    gen_01_param_DriftwaterDSP_lpg_lastValue = 0;
    gen_01_param_DriftwaterDSP_mix_lastValue = 0;
    gen_01_param_DriftwaterDSP_bypass_lastValue = 0;
    globaltransport_tempo = nullptr;
    globaltransport_state = nullptr;
    stackprotect_count = 0;
    _voiceIndex = 0;
    _noteNumber = 0;
    isMuted = 1;
}

    // data ref strings
    struct DataRefStrings {
    	static constexpr auto& name0 = "gen_01_memL_bufferobj";
    	static constexpr auto& file0 = "";
    	static constexpr auto& tag0 = "buffer~";
    	static constexpr auto& name1 = "gen_01_memR_bufferobj";
    	static constexpr auto& file1 = "";
    	static constexpr auto& tag1 = "buffer~";
    	DataRefStrings* operator->() { return this; }
    	const DataRefStrings* operator->() const { return this; }
    };

    DataRefStrings dataRefStrings;

// member variables

    number gen_01_in1;
    number gen_01_in2;
    number gen_01_rate;
    number gen_01_damp;
    number gen_01_depth;
    number gen_01_lpg;
    number gen_01_mix;
    number gen_01_bypass;
    number gen_01_param_DriftwaterDSP_rate_value;
    number gen_01_param_DriftwaterDSP_damp_value;
    number gen_01_param_DriftwaterDSP_depth_value;
    number gen_01_param_DriftwaterDSP_lpg_value;
    number gen_01_param_DriftwaterDSP_mix_value;
    number gen_01_param_DriftwaterDSP_bypass_value;
    MillisecondTime _currentTime;
    ENGINE _internalEngine;
    UInt64 audioProcessSampleCount;
    Index sampleOffsetIntoNextAudioBuffer;
    signal zeroBuffer;
    signal dummyBuffer;
    bool didAllocateSignals;
    Index vs;
    Index maxvs;
    number sr;
    number invsr;
    Float64BufferRef gen_01_memL_buffer;
    Index gen_01_memL__maxdelay;
    Int gen_01_memL_sizemode;
    Index gen_01_memL_wrap;
    Int gen_01_memL_reader;
    Int gen_01_memL_writer;
    Float64BufferRef gen_01_memR_buffer;
    Index gen_01_memR__maxdelay;
    Int gen_01_memR_sizemode;
    Index gen_01_memR_wrap;
    Int gen_01_memR_reader;
    Int gen_01_memR_writer;
    number gen_01_targetL_value;
    number gen_01_targetR_value;
    number gen_01_driftL_value;
    number gen_01_driftR_value;
    number gen_01_countL_value;
    number gen_01_countR_value;
    number gen_01_envL_value;
    number gen_01_envR_value;
    number gen_01_lpL_value;
    number gen_01_lpR_value;
    number gen_01_bypassFade_value;
    number gen_01_slide_0_prev;
    number gen_01_slide_2_prev;
    number gen_01_slide_4_prev;
    number gen_01_slide_6_prev;
    number gen_01_slide_29_prev;
    bool gen_01_setupDone;
    number gen_01_param_DriftwaterDSP_rate_lastValue;
    number gen_01_param_DriftwaterDSP_damp_lastValue;
    number gen_01_param_DriftwaterDSP_depth_lastValue;
    number gen_01_param_DriftwaterDSP_lpg_lastValue;
    number gen_01_param_DriftwaterDSP_mix_lastValue;
    number gen_01_param_DriftwaterDSP_bypass_lastValue;
    signal globaltransport_tempo;
    signal globaltransport_state;
    number stackprotect_count;
    DataRef gen_01_memL_bufferobj;
    DataRef gen_01_memR_bufferobj;
    Index _voiceIndex;
    Int _noteNumber;
    Index isMuted;
    indexlist paramInitIndices;
    indexlist paramInitOrder;
    bool _isInitialized = false;
};

static PatcherInterface* createDriftwater()
{
    return new Driftwater<EXTERNALENGINE>();
}

#ifndef RNBO_NO_PATCHERFACTORY
extern "C" PatcherFactoryFunctionPtr GetPatcherFactoryFunction()
#else
extern "C" PatcherFactoryFunctionPtr DriftwaterFactoryFunction()
#endif
{
    return createDriftwater;
}

#ifndef RNBO_NO_PATCHERFACTORY
extern "C" void SetLogger(Logger* logger)
#else
void DriftwaterSetLogger(Logger* logger)
#endif
{
    console = logger;
}

} // end RNBO namespace

