#include "rack.hpp"

using namespace rack;

Plugin* pluginInstance;

struct NeuralFM : engine::Module {
    enum ParamIds {
        CHAOS_PARAM,
        OCTAVE_PARAM,  
        SWITCH1_PARAM,
        SWITCH2_PARAM,
        SWITCH3_PARAM,
        SWITCH4_PARAM,
        SWITCH5_PARAM,
        SWITCH6_PARAM,
        BYPASS_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        PITCH_INPUT,
        OCTAVE_CV_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        AUDIO_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        SCREEN_LIGHT,
        NUM_LIGHTS
    };

    float phaseMod = 0.0f;
    float phaseCar = 0.0f;
    float modDepth = 2.0f;
    float chaosFactor = 0.0f;
    float modRatio = 2.0f;
    float freq = 261.63f;
    float lastMod = 0.0f;
    float neuralOutput = 0.0f;
    float lastPitch = 0.0f;
    float lastOctaveCV = 0.0f;
    float inputTrigger = 0.0f;
    float weights1[500]; 
    float weights2[1000];
    float weights3[1000]; 

    NeuralFM() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(CHAOS_PARAM, 0.0f, 1.0f, 0.0f, "Chaos Factor");
        configParam(OCTAVE_PARAM, -2.0f, 2.0f, 0.0f, "Octave Shift");
        configSwitch(SWITCH1_PARAM, 0.0f, 1.0f, 0.0f, "Chaos Mode", {"Linear", "Exp"});
        configSwitch(SWITCH2_PARAM, 0.0f, 1.0f, 0.0f, "FM Topology", {"Parallel", "Cascade"});
        configSwitch(SWITCH3_PARAM, 0.0f, 1.0f, 0.0f, "Feedback Neuron", {"Off", "On"});
        configSwitch(SWITCH4_PARAM, 0.0f, 1.0f, 0.0f, "Mod Invert", {"Normal", "Inverted"});
        configSwitch(SWITCH5_PARAM, 0.0f, 1.0f, 0.0f, "Neuron x100", {"Off", "On"});
        configSwitch(SWITCH6_PARAM, 0.0f, 1.0f, 0.0f, "Neuron x1000", {"Off", "On"});
        configSwitch(BYPASS_PARAM, 0.0f, 1.0f, 0.0f, "Bypass", {"Off", "On"});

     
        for (int i = 0; i < 500; i++) {
            weights1[i] = 0.5f + random::uniform() * 1.0f; 
        }
        for (int i = 0; i < 1000; i++) {
            weights2[i] = 0.5f + random::uniform() * 1.0f;
        }
        for (int i = 0; i < 1000; i++) {
        weights3[i] = 0.5f + random::uniform() * 1.0f;
        }
    }

    void process(const ProcessArgs& args) override {
        float octaveShift = params[OCTAVE_PARAM].getValue();
        float octaveCV = inputs[OCTAVE_CV_INPUT].isConnected() ? inputs[OCTAVE_CV_INPUT].getVoltage() : 0.0f;
        float pitch = inputs[PITCH_INPUT].getVoltage();
        freq = 261.63f * std::pow(2.0f, pitch) * std::pow(2.0f, octaveShift + octaveCV);

        chaosFactor = params[CHAOS_PARAM].getValue();

        // Detect input changes
        float pitchDelta = std::abs(pitch - lastPitch);
        float octaveDelta = std::abs(octaveCV - lastOctaveCV);
        if (pitchDelta > 0.01f || octaveDelta > 0.01f) {
            inputTrigger = 1.0f;
        }
        inputTrigger = clamp(inputTrigger - args.sampleTime * 2.0f, 0.0f, 1.0f);
        lastPitch = pitch;
        lastOctaveCV = octaveCV;

        float outputSignal;

        if (params[BYPASS_PARAM].getValue() > 0.5f) {
            outputSignal = inputs[PITCH_INPUT].isConnected() ? pitch * 5.0f : 0.0f;
        } else {
            float chaosMod = 0.0f;
            if (params[SWITCH1_PARAM].getValue() > 0.5f) {
                chaosMod = std::exp(chaosFactor * (random::uniform() * 5.0f)) - 1.0f;
            } else {
                chaosMod = chaosFactor * (random::uniform() - 0.5f) * 10.0f;
            }

          
            float group1Sum = 0.0f; 
            for (int i = 0; i < 10; i++) {
                float feedback = params[SWITCH3_PARAM].getValue() > 0.5f ? lastMod * 0.8f : 0.0f;
                group1Sum += weights1[i] * std::tanh(pitch + feedback + random::uniform() * 0.1f); // 
            }
            
            group1Sum /= 100.0f; 

            float group2Sum = 0.0f; 
            for (int i = 0; i < 1000; i++) {
                float neuron = params[SWITCH4_PARAM].getValue() > 0.5f ? std::tanh(chaosFactor * 2.0f + random::uniform() * 0.1f) : 0.0f;
                group2Sum += weights2[i] * neuron;
            }
            group2Sum /= 1000.0f;

           
            int neuronCount = 10; 

            if (params[SWITCH6_PARAM].getValue() > 0.5f) {
                neuronCount = 1000;  
            } else if (params[SWITCH5_PARAM].getValue() > 0.5f) {
                neuronCount = 100;   
            }

            float layer2Sum = 0.0f;
            for (int i = 0; i < neuronCount; i++) {
                layer2Sum += weights3[i] * std::tanh(group1Sum * 0.5f + group2Sum * 0.5f + random::uniform() * 0.1f);
                }
            layer2Sum /= neuronCount;

           
            neuralOutput = layer2Sum * 5.0f;


            float mod;
            if (params[SWITCH2_PARAM].getValue() > 0.5f) {
                float intermediatePhase = std::fmod(phaseMod + (freq + chaosMod) * args.sampleTime, 1.0f);
                mod = std::sin(2.0f * M_PI * intermediatePhase + neuralOutput) * modDepth;
                phaseMod = intermediatePhase;
            } else {
                phaseMod = std::fmod(phaseMod + (freq * modRatio + chaosMod) * args.sampleTime, 1.0f);
                mod = std::sin(2.0f * M_PI * phaseMod) * modDepth;
            }

            float modSign = (params[SWITCH4_PARAM].getValue() > 0.5f) ? -1.0f : 1.0f;
            mod *= modSign;

            phaseCar = std::fmod(phaseCar + freq * args.sampleTime, 1.0f);
            float car = std::sin(2.0f * M_PI * phaseCar + mod);

            lastMod = mod;
            outputSignal = 5.0f * car;
        }

        outputs[AUDIO_OUTPUT].setVoltage(outputSignal);
        lights[SCREEN_LIGHT].setBrightness(chaosFactor);
    }
};

struct NeuralScreen : TransparentWidget {
    NeuralFM* module;

    NeuralScreen(NeuralFM* module) : module(module) {
        box.size = Vec(100, 60); 
    }

    void draw(const DrawArgs& args) override {
        
        nvgSave(args.vg);
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        nvgFillColor(args.vg, nvgRGB(0, 0, 0));
        nvgFill(args.vg);

        if (!module) {
            nvgRestore(args.vg);
            return;
        }

        float phaseCar = module->phaseCar;
        float phaseMod = module->phaseMod;
        float chaos = module->chaosFactor;
        float neural = module->neuralOutput;
        float trigger = module->inputTrigger;

        
        float switchSum = (module->params[NeuralFM::SWITCH1_PARAM].getValue() +
                          module->params[NeuralFM::SWITCH2_PARAM].getValue() +
                          module->params[NeuralFM::SWITCH3_PARAM].getValue() +
                          module->params[NeuralFM::SWITCH4_PARAM].getValue() +
                          module->params[NeuralFM::SWITCH5_PARAM].getValue() +
                          module->params[NeuralFM::SWITCH6_PARAM].getValue()) / 6.0f;
        float radiusFactor = 0.5f + chaos * 0.5f + switchSum * 1.5f; 

        
        float pulse = 0.5f + 0.5f * std::sin(2.0f * M_PI * phaseCar * 2.0f);

        
        float centerX = 25.0f;
        float centerY = 15.0f;
        float baseRadius = 15.0f * radiusFactor * (1.0f + trigger * 0.3f + pulse * 0.2f); 

        for (int y = 0; y < 30; y++) {
            float yNorm = (float)(y - centerY) / baseRadius; 
            if (std::abs(yNorm) > 1.0f) continue; 

            
            float radius = baseRadius * std::sqrt(1.0f - yNorm * yNorm);
            float depth = std::abs(yNorm); 
            float scale = 2.0f * (1.0f - depth * 0.5f); 
            float brightness = 1.0f - depth * 0.9f; 
            brightness *= (1.0f + trigger * 0.5f + pulse * 0.3f);
            
            for (int x = 0; x < 50; x++) {
                float xNorm = (float)(x - centerX) / radius; 
                if (std::abs(xNorm) > 1.0f) continue;

                
                float waveAngle = xNorm * M_PI + phaseCar ; 
                float baseWave = std::sin(2.0f * M_PI * waveAngle * (1.0f + chaos));
                float modWave = std::sin(2.0f * M_PI * (waveAngle + phaseMod) * 2.0f) * (0.5f + neural * 0.1f);
                float waveHeight = (baseWave + modWave) * 10.0f * (1.0f - depth) ; 

                
                float waveY = centerY + (yNorm * baseRadius) + waveHeight + 15;
                int waveYGrid = (int)(waveY / 2.0f);

                
                if (std::abs(waveYGrid - y) <= 1) {
                    float localBrightness = brightness * (1.0f - std::abs(waveYGrid - y) * 0.5f);
                    localBrightness *= (1.0f + random::uniform() * 0.2f); 
                    localBrightness = clamp(localBrightness, 0.0f, 1.0f);

                    if (localBrightness > 0.1f) {
                        nvgBeginPath(args.vg);
                        float offset = (2.0f - scale) / 2.0f;
                        nvgRect(args.vg, x * 2 + offset, y * 2 + offset, scale, scale);
                        nvgFillColor(args.vg, nvgRGBAf(0, 0, 1.0f, localBrightness));
                        nvgFill(args.vg);
                    }
                }
            }
        }
        nvgRestore(args.vg);
    }
};

struct NeuralFMWidget : app::ModuleWidget {
    NeuralFMWidget(NeuralFM* module) {
        setModule(module);

        box.size = Vec(30 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/NeuralFM.svg")));

        addParam(createParam<componentlibrary::RoundBlackKnob>(Vec(18, 200), module, NeuralFM::CHAOS_PARAM));
        addParam(createParam<componentlibrary::RoundBlackKnob>(Vec(56, 273), module, NeuralFM::OCTAVE_PARAM));

        addParam(createParam<componentlibrary::CKSS>(Vec(15, 65), module, NeuralFM::SWITCH1_PARAM));
        addParam(createParam<componentlibrary::CKSS>(Vec(15 + 15, 65), module, NeuralFM::SWITCH2_PARAM));
        addParam(createParam<componentlibrary::CKSS>(Vec(15 + 30, 65), module, NeuralFM::SWITCH3_PARAM));
        addParam(createParam<componentlibrary::CKSS>(Vec(15 + 45, 65), module, NeuralFM::SWITCH4_PARAM));
        addParam(createParam<componentlibrary::CKSS>(Vec(15 + 60, 65), module, NeuralFM::SWITCH5_PARAM));
        addParam(createParam<componentlibrary::CKSS>(Vec(15 + 75, 65), module, NeuralFM::SWITCH6_PARAM));

        auto screen = new NeuralScreen(module);
        screen->box.pos = Vec(10, 100);
        addChild(screen);

        addInput(createInput<componentlibrary::PJ301MPort>(Vec(18, 322), module, NeuralFM::PITCH_INPUT));
        addInput(createInput<componentlibrary::PJ301MPort>(Vec(18, 275), module, NeuralFM::OCTAVE_CV_INPUT));
        addOutput(createOutput<componentlibrary::PJ301MPort>(Vec(58, 322), module, NeuralFM::AUDIO_OUTPUT));

        addParam(createParam<componentlibrary::CKSS>(Vec(92, 325), module, NeuralFM::BYPASS_PARAM));

        addChild(createWidget<componentlibrary::ScrewBlack>(Vec(1, 1)));
        addChild(createWidget<componentlibrary::ScrewBlack>(Vec(1, 364)));
        addChild(createWidget<componentlibrary::ScrewBlack>(Vec(104, 1)));
        addChild(createWidget<componentlibrary::ScrewBlack>(Vec(104, 364)));
    }
};

extern "C" void init(Plugin* p) {
    pluginInstance = p;
    p->addModel(createModel<NeuralFM, NeuralFMWidget>("NeuralFM"));
}