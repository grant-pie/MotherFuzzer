#include "plugin.hpp"


struct MyModule : Module {
	// Low-pass filter state for tone control
	float lowpassState = 0.f;

	enum ParamId {
		DRIVE_PARAM,
		TONE_PARAM,
		VOLUME_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		AUDIO_INPUT,
		DRIVE_CV,
		TONE_CV,
		VOLUME_CV,
		INPUTS_LEN
	};
	enum OutputId {
		AUDIO_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		CLIP_LIGHT,
		LIGHTS_LEN
	};

	MyModule() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(DRIVE_PARAM, 1.f, 100.f, 10.f, "Drive", "x");
		configParam(TONE_PARAM, 0.f, 1.f, 0.5f, "Tone", "%", 0.f, 100.f);
		configParam(VOLUME_PARAM, 0.f, 2.f, 1.f, "Volume", "%", 0.f, 100.f);
		configInput(AUDIO_INPUT, "Audio");
		configInput(DRIVE_CV, "Drive CV");
		configInput(TONE_CV, "Tone CV");
		configInput(VOLUME_CV, "Volume CV");
		configOutput(AUDIO_OUTPUT, "Audio");
	}

	void process(const ProcessArgs &args) override {
		// Get input signal (0V if nothing connected)
		float input = inputs[AUDIO_INPUT].getVoltage();

		// Get parameter values and apply CV modulation
		// Drive: 1-100, CV range 0-10V maps to full parameter range
		float drive = params[DRIVE_PARAM].getValue();
		if (inputs[DRIVE_CV].isConnected()) {
			float cv = inputs[DRIVE_CV].getVoltage();
			// Map 0-10V CV to 0-99 and add to base drive value
			drive += (cv / 10.f) * 99.f;
			drive = clamp(drive, 1.f, 100.f);
		}

		// Tone: 0-1, CV range 0-10V maps to full parameter range
		float tone = params[TONE_PARAM].getValue();
		if (inputs[TONE_CV].isConnected()) {
			float cv = inputs[TONE_CV].getVoltage();
			// Map 0-10V CV to 0-1 range and add to base tone value
			tone += cv / 10.f;
			tone = clamp(tone, 0.f, 1.f);
		}

		// Volume: 0-2, CV range 0-10V maps to full parameter range
		float volume = params[VOLUME_PARAM].getValue();
		if (inputs[VOLUME_CV].isConnected()) {
			float cv = inputs[VOLUME_CV].getVoltage();
			// Map 0-10V CV to 0-2 range and add to base volume value
			volume += (cv / 10.f) * 2.f;
			volume = clamp(volume, 0.f, 2.f);
		}

		// STAGE 1: Pre-gain/Drive
		// Multiply input by drive amount (1x to 100x gain)
		float driven = input * drive;

		// STAGE 2: Soft clipping using hyperbolic tangent
		// This creates smooth, musical distortion similar to tube/transistor saturation
		// tanh() asymptotically approaches ±1, creating natural compression
		float clipped = std::tanh(driven);

		// STAGE 3: Tone control (simple one-pole low-pass filter)
		// This filters out harsh high frequencies from the distortion
		// Higher tone values = brighter sound (less filtering)
		// Lower tone values = darker sound (more filtering)
		float cutoffFreq = 20.f + tone * 19980.f; // 20Hz to 20kHz range
		float rc = 1.f / (2.f * M_PI * cutoffFreq);
		float alpha = args.sampleTime / (rc + args.sampleTime);
		lowpassState = lowpassState + alpha * (clipped - lowpassState);

		// STAGE 4: Mix filtered and unfiltered signal based on tone
		// This preserves some bite/edge even with low tone settings
		float toned = lowpassState * (1.f - tone * 0.5f) + clipped * (tone * 0.5f);

		// STAGE 5: Output volume control
		// Allows user to compensate for the gain added by the effect
		float output = toned * volume * 5.f; // Scale to ±5V standard

		// Safety clamp to prevent extreme voltages
		output = clamp(output, -10.f, 10.f);

		// Set the output voltage
		outputs[AUDIO_OUTPUT].setVoltage(output);

		// Clip indicator light (lights up when signal is heavily driven)
		// Threshold of 3.0 means the signal has been amplified 3x before clipping
		bool isClipping = std::abs(driven) > 3.f;
		lights[CLIP_LIGHT].setBrightness(isClipping ? 1.f : 0.f);
	}
};


struct MyModuleWidget : ModuleWidget {
	MyModuleWidget(MyModule* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/MyModule.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Drive knob and CV input
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.0, 30.0)), module, MyModule::DRIVE_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.5, 30.0)), module, MyModule::DRIVE_CV));

		// Tone knob and CV input
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.0, 50.0)), module, MyModule::TONE_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.5, 50.0)), module, MyModule::TONE_CV));

		// Volume knob and CV input
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.0, 70.0)), module, MyModule::VOLUME_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.5, 70.0)), module, MyModule::VOLUME_CV));

		// Audio input
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.0, 95.0)), module, MyModule::AUDIO_INPUT));

		// Audio output
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.5, 95.0)), module, MyModule::AUDIO_OUTPUT));

		// Clip indicator LED (top)
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 15.0)), module, MyModule::CLIP_LIGHT));
	}
};


Model* modelMyModule = createModel<MyModule, MyModuleWidget>("MyModule");