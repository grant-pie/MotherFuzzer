#include "plugin.hpp"


struct MotherFuzzer : Module {
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

	MotherFuzzer() {
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


struct MotherFuzzerPanel : Widget {
	MotherFuzzerPanel(Vec size) {
		box.size = size;
	}

	void draw(const DrawArgs& args) override {
		// mm to Rack pixel units (SVG_DPI=75, 25.4mm/in)
		auto mpx = [](float mm) { return mm * 75.f / 25.4f; };

		// Dark outer background (#2a2a2a)
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFillColor(args.vg, nvgRGB(0x2a, 0x2a, 0x2a));
		nvgFill(args.vg);

		// Slightly darker inner panel face with border (#222222 / #333333)
		float inset = 4.f;
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, inset, inset, box.size.x - 2.f * inset, box.size.y - 2.f * inset, 5.f);
		nvgFillColor(args.vg, nvgRGB(0x22, 0x22, 0x22));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGB(0x33, 0x33, 0x33));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		// Separator line between controls and I/O
		auto sep = [&](float y) {
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, inset + 3.f, y);
			nvgLineTo(args.vg, box.size.x - inset - 3.f, y);
			nvgStrokeColor(args.vg, nvgRGB(0x44, 0x44, 0x44));
			nvgStrokeWidth(args.vg, 0.8f);
			nvgStroke(args.vg);
		};
		sep(mpx(83.f)); // between volume knob (70mm) and audio I/O (95mm)

		// Text labels
		std::shared_ptr<Font> font = APP->window->loadFont(
			asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font) return;

		nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		float cx = box.size.x / 2.f;

		// Title (white)
		nvgFontSize(args.vg, 10.f);
		nvgFillColor(args.vg, nvgRGB(0xff, 0xff, 0xff));
		nvgText(args.vg, cx, mpx(9.0f), "MotherFuzzer", NULL);

		// Knob labels (#888888)
		nvgFontSize(args.vg, 8.5f);
		nvgFillColor(args.vg, nvgRGB(0x88, 0x88, 0x88));
		nvgText(args.vg, cx, mpx(39.6f), "Drive", NULL);
		nvgText(args.vg, cx, mpx(58.7f), "Tone", NULL);
		nvgText(args.vg, cx, mpx(78.7f), "Volume", NULL);

		// I/O labels (#cccccc)
		nvgFillColor(args.vg, nvgRGB(0xcc, 0xcc, 0xcc));
		nvgText(args.vg, mpx(10.0f), mpx(102.f), "In", NULL);
		nvgText(args.vg, mpx(20.5f), mpx(102.f), "Out", NULL);

		// Brand name (#555555)
		nvgFontSize(args.vg, 7.f);
		nvgFillColor(args.vg, nvgRGB(0x55, 0x55, 0x55));
		nvgText(args.vg, cx, mpx(114.f), "KNOPPIES", NULL);
	}
};

struct MotherFuzzerWidget : ModuleWidget {
	MotherFuzzerWidget(MotherFuzzer* module) {
		setModule(module);

		// 6HP wide, standard rack height
		box.size = Vec(RACK_GRID_WIDTH * 6, RACK_GRID_HEIGHT);
		addChild(new MotherFuzzerPanel(box.size));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.0, 30.0)), module, MotherFuzzer::DRIVE_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.5, 30.0)), module, MotherFuzzer::DRIVE_CV));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.0, 50.0)), module, MotherFuzzer::TONE_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.5, 50.0)), module, MotherFuzzer::TONE_CV));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.0, 70.0)), module, MotherFuzzer::VOLUME_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(20.5, 70.0)), module, MotherFuzzer::VOLUME_CV));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.0, 95.0)), module, MotherFuzzer::AUDIO_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.5, 95.0)), module, MotherFuzzer::AUDIO_OUTPUT));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 18.0)), module, MotherFuzzer::CLIP_LIGHT));
	}
};


Model* modelMotherFuzzer = createModel<MotherFuzzer, MotherFuzzerWidget>("MotherFuzzer");
