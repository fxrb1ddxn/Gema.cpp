// Gema.cpp
#include "public.sdk/source/vst2.x/audioeffectx.h"
#include <math.h>

#define PLUGIN_NAME     "GEMA CEPEPE"
#define PLUGIN_VENDOR   "yasashii ian"
#define PLUGIN_ID       'dlyvst'
#define NUM_PROGRAMS    1
#define NUM_PARAMS      9
#define BUFFER_SIZE     44100 * 10   // 10 seconds max delay at 44.1 kHz

enum {
    pTimeL = 0,
    pTimeR,
    pSync,
    pFeedback,
    pWet,
    pHPF,
    pLPF,
    pPingPong,
    pTempoSyncDivision
};

class Gema : public AudioEffectX
{
public:
    Gema(audioMasterCallback audioMaster);
    ~Gema();

    virtual void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames);
    virtual VstInt32 processEvents(VstEvents* events);
    virtual void setParameter(VstInt32 index, float value);
    virtual float getParameter(VstInt32 index);
    virtual void getParameterLabel(VstInt32 index, char* label);
    virtual void getParameterDisplay(VstInt32 index, char* text);
    virtual void getParameterName(VstInt32 index, char* text);
    virtual void setProgramName(char* name) { }
    virtual void getProgramName(char* name) { strcpy(name, "Default"); }
    virtual bool getEffectName(char* name) { strcpy(name, PLUGIN_NAME); return true; }
    virtual bool getVendorString(char* text) { strcpy(text, PLUGIN_VENDOR); return true; }
    virtual bool getProductString(char* text) { strcpy(text, PLUGIN_NAME); return true; }
    virtual VstInt32 getVendorVersion() { return 1000; }
    virtual VstPlugCategory getPlugCategory() { return kPlugCategEffect; }

protected:
    float timeL, timeR, feedback, wet, hpf, lpf;
    bool sync, pingpong;
    int division; // 0=1/4, 1=1/4T, 2=1/8 etc.

    float* delayBufferL;
    float* delayBufferR;
    long writePos;
    long readPosL, readPosR;
    double sampleRate;

    // Filter coefficients (simple one-pole)
    float y1L, y1R, x1L, x1R; // HPF
    float z1L, z1R, w1L, w1R; // LPF

    void updateDelayTimes();
    float tempoDivisionToFactor(int div);
};

float Gema::tempoDivisionToFactor(int div)
{
    const float factors[8] = { 1.0f, 2.0f/3.0f, 0.5f, 1.0f/3.0f, 0.25f, 0.5f*1.333333f, 0.125f, 0.125f*1.333333f };
    if (div < 0) div = 0; if (div > 7) div = 7;
    return factors[div];
}

//-------------------------------------------------------------------
Gema::Gema(audioMasterCallback audioMaster)
    : AudioEffectX(audioMaster, NUM_PROGRAMS, NUM_PARAMS)
{
    setNumInputs(2);
    setNumOutputs(2);
    canProcessReplacing();
    isSynth(false);
    setUniqueID(PLUGIN_ID);

    delayBufferL = new float[BUFFER_SIZE];
    delayBufferR = new float[BUFFER_SIZE];
    memset(delayBufferL, 0, BUFFER_SIZE * sizeof(float));
    memset(delayBufferR, 0, BUFFER_SIZE * sizeof(float));

    writePos = readPosL = readPosR = 0;
    sampleRate = 44100.0;

    timeL = timeR = 0.3f;
    feedback = 0.4f;
    wet = 0.5f;
    hpf = 0.05f;
    lpf = 0.95f;
    sync = false;
    pingpong = false;
    division = 0;

    y1L = y1R = x1L = x1R = z1L = z1R = w1L = w1R = 0.0f;

    if (audioMaster)
    {
        double sr = getSampleRate();
        if (sr > 0) sampleRate = sr;
    }
}

Gema::~Gema()
{
    delete[] delayBufferL;
    delete[] delayBufferR;
}

void Gema::updateDelayTimes()
{
    double delaySamplesL, delaySamplesR;

    if (sync && getTimeInfo(0))
    {
        double bpm = getTimeInfo(kVstTempoValid)->tempo;
        if (bpm <= 0) bpm = 120.0;
        double beatSec = 60.0 / bpm;
        float factor = tempoDivisionToFactor(division);
        double baseTime = beatSec * factor;

        delaySamplesL = baseTime * sampleRate;
        delaySamplesR = baseTime * sampleRate;
        if (pingpong) delaySamplesR = baseTime * 1.5 * sampleRate; // simple ping-pong offset
    }
    else
    {
        delaySamplesL = timeL * sampleRate;
        delaySamplesR = timeR * sampleRate;
    }

    readPosL = (long)(writePos - delaySamplesL + BUFFER_SIZE * 4L) % BUFFER_SIZE;
    readPosR = (long)(writePos - delaySamplesR + BUFFER_SIZE * 4L) % BUFFER_SIZE;
}

void Gema::setParameter(VstInt32 index, float value)
{
    switch (index)
    {
        case pTimeL:    timeL = value; break;
        case pTimeR:    timeR = value; break;
        case pSync:     sync = value > 0.5f; break;
        case pFeedback: feedback = value; break;
        case pWet:      wet = value; break;
        case pHPF:      hpf = value; break;
        case pLPF:      lpf = value; break;
        case pPingPong: pingpong = value > 0.5f; break;
        case pTempoSyncDivision: division = (int)(value * 7.99f); break;
    }
    updateDelayTimes();
}

float Gema::getParameter(VstInt32 index)
{
    switch (index)
    {
        case pTimeL: return timeL;
        case pTimeR: return timeR;
        case pSync: return sync ? 1.f : 0.f;
        case pFeedback: return feedback;
        case pWet: return wet;
        case pHPF: return hpf;
        case pLPF: return lpf;
        case pPingPong: return pingpong ? 1.f : 0.f;
        case pTempoSyncDivision: return division / 7.99f;
        default: return 0.f;
    }
}

void Gema::getParameterName(VstInt32 index, char* label)
{
    switch (index)
    {
        case pTimeL: strcpy(label, "Time L"); break;
        case pTimeR: strcpy(label, "Time R"); break;
        case pSync: strcpy(label, "Tempo Sync"); break;
        case pFeedback: strcpy(label, "Feedback"); break;
        case pWet: strcpy(label, "Wet"); break;
        case pHPF: strcpy(label, "HP Filter"); break;
        case pLPF: strcpy(label, "LP Filter"); break;
        case pPingPong: strcpy(label, "Ping-Pong"); break;
        case pTempoSyncDivision: strcpy(label, "Division"); break;
    }
}

void Gema::getParameterDisplay(VstInt32 index, char* text)
{
    switch (index)
    {
        case pTimeL: sprintf(text, "%.0f ms", timeL * 2000.f); break;
        case pTimeR: sprintf(text, "%.0f ms", timeR * 2000.f); break;
        case pSync: strcpy(text, sync ? "On" : "Off"); break;
        case pFeedback: sprintf(text, "%.1f %%", feedback * 100.f); break;
        case pWet: sprintf(text, "%.1f %%", wet * 100.f); break;
        case pHPF: sprintf(text, "%.0f Hz", 20 + hpf * 800.f); break;
        case pLPF: sprintf(text, "%.0f Hz", 1000 + lpf * 19000.f); break;
        case pPingPong: strcpy(text, pingpong ? "On" : "Off"); break;
        case pTempoSyncDivision:
            {
                const char* divs[8] = { "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T" };
                strcpy(text, divs[division]);
            }
            break;
    }
}

void Gema::getParameterLabel(VstInt32 index, char* label)
{
    switch (index)
    {
        case pTimeL:
        case pTimeR: strcpy(label, "ms"); break;
        case pFeedback:
        case pWet: strcpy(label, "%"); break;
        default: strcpy(label, ""); break;
    }
}

void Gema::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames)
{
    float* inL = inputs[0];
    float* inR = inputs[1];
    float* outL = outputs[0];
    float* outR = outputs[1];

    float fb = feedback;
    float dry = 1.0f - wet;
    float hp = hpf;   // 0..1
    float lp = lpf;   // 0..1

    for (VstInt32 i = 0; i < sampleFrames; i++)
    {
        // Write position
        long wp = writePos % BUFFER_SIZE;

        float wetL = delayBufferL[readPosL % BUFFER_SIZE];
        float wetR = delayBufferR[readPosR % BUFFER_SIZE];

        // Simple one-pole filters in feedback path
        // HPF
        y1L = wetL - x1L; x1L = wetL + hp * (y1L - x1L);
        y1R = wetR - x1R; x1R = wetR + hp * (y1R - x1R);
        // LPF
        z1L = y1L + lp * (z1L - y1L);
        z1R = y1R + lp * (z1R - y1R);

        float fbL = z1L * fb;
        float fbR = z1R * fb;

        if (pingpong) { float tmp = fbL; fbL = fbR; fbR = tmp; } // cross feedback

        float newL = inL[i] + fbR;  // note the cross for ping-pong
        float newR = inR[i] + fbL;

        delayBufferL[wp] = newL;
        delayBufferR[wp] = newR;

        outL[i] = inL[i] * dry + wetL * wet;
        outR[i] = inR[i] * dry + wetR * wet;

        writePos++;
        readPosL++;
        readPosR++;
    }

    // Keep positions in bounds
    writePos %= BUFFER_SIZE;
    readPosL %= BUFFER_SIZE;
    readPosR %= BUFFER_SIZE;
}

//-------------------------------------------------------------------
AudioEffect* createEffectInstance(audioMasterCallback audioMaster)
{
    return new Gema(audioMaster);
}