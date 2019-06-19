/*
*        ~|  DreamControl |~
*
*	    Studio MIDI controller
*
*			 VST plugin
*
* ==========================================================================
*
*  Copyright (C) 2018 Dave Evans (dave@propertech.co.uk)
*  Licensed for personal non-commercial use only. All other rights reserved.
*
* ==========================================================================
*
*	TODO: This code has been written and expanded quickly so could do with refactoring.
*		  e.g. move MIDI I/O, REAPER and TotalMix integration to separate classes.
*
*/

#include <memory>
#include <string>
#include <math.h>
#include <map>

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "LufsProcessor.h"
#include "CrossoverFilter.h"
#include "RmeTotalMixFaderCurve.h"

#define CALLBACK_TIMER_PERIOD_MS 10									// How often parameters, meters etc are updated.
#define LOWEST_TRUE_PEAK_VALUE -125.0f
#define LOWEST_LUFS_VALUE -64.0f
#define LOWEST_VOLUME_VALUE -48.0f
#define VOLUME_CONTROL_MIDI_RANGE 96.0f
#define HIGHEST_TRUE_PEAK_VALUE 3.0f

#define MIDI_OUT_PORT_NAME "MIDIOUT2 (DreamControl)"				// Direct MIDI connection to our hardware.
#define MIDI_IN_PORT_NAME "MIDIIN2 (DreamControl)"					
#define MIDI_OUT_VOL_CONTROL_PORT_NAME "Loopback (DreamControl)"	// MIDI connection to an external volume control, currently RME TotalMix
#define MIDI_OUT_SWITCHER_PORT_NAME "DreamControlSwitcher"			// MIDI connection to our monitor switcher relay unit
#define MIDI_FADER_TOUCH_SENSE_CC 47

#define REAPER_OSC_SEND_IP "127.0.0.1"
#define REAPER_OSC_SEND_PORT 8000
#define REAPER_OSC_RECEIVE_PORT 9000

const int sysexManufacturerId[3] = { 0x00, 0x21, 0x69 };			// Our SysEx manufacturer ID.

enum sysexCommand {
	SYSEX_COMMAND_METER_DATA = 1,
	SYSEX_COMMAND_SYNC_BUTTONS = 2
};

enum midiNoteCommand {
	BUTTON_LOUD = 0,
	BUTTON_MONO = 1,
	BUTTON_SIDE = 2,
	BUTTON_LOW = 3,
	BUTTON_LOMID = 4,
	BUTTON_HIMID = 5,
	BUTTON_HIGH = 6,
	BUTTON_MIX = 7,
	BUTTON_CUE1 = 8,
	BUTTON_CUE2 = 9,
	BUTTON_CUE3 = 10,
	BUTTON_CUE4 = 11,
	BUTTON_EXT1 = 12,
	BUTTON_EXT2 = 13,
	BUTTON_MAIN = 14,
	BUTTON_ALT1 = 15,
	BUTTON_ALT2 = 16,
	BUTTON_ALT3 = 17,
	BUTTON_MONMUTE = 18,
	BUTTON_DIM = 19,
	BUTTON_REF = 20,
	BUTTON_RETURN = 21,
	BUTTON_LOOP = 22,
	BUTTON_BACK = 23,
	BUTTON_FORWARD = 24,
	BUTTON_PLAY = 25,
	BUTTON_STOP = 26,
	BUTTON_CLICK = 27,
	BUTTON_RECORD = 28,
	BUTTON_MUTE = 30,
	BUTTON_SOLO = 31,
	BUTTON_READ = 32,
	BUTTON_WRITE = 33,
	BUTTON_TALK = 34,
	BUTTON_RESET_METER = 35,
	BUTTON_PEAK_LUFS = 36,
	BUTTON_ABS_REL = 37,
	BUTTON_SAVE = 38,
	BUTTON_LOOP1 = 39,
	BUTTON_LOOP2 = 40,
	BUTTON_LOOP8 = 41,
	BUTTON_LOOP16 = 42,
	BUTTON_MUTE_CLEAR = 43,
	BUTTON_SOLO_CLEAR = 44,
	BUTTON_VOL_MOD = 45,
	BUTTON_READ_ALL = 46,
	BUTTON_WRITE_ALL = 47,
	BUTTON_3RD_METER_IS_MOMENTARY = 48,
	BUTTON_1DB_PEAK_SCALE = 49
};

// RME TotalMix monitor output assignments, corresponds to index of hardware output in TotalMix. { MAIN, ALT1, ALT2, ALT3 }
// ///////////////////////////////////////////////////////
// TODO: This is currently hardcoded for Dave's studio setup! In future we want to be able to select this in the plugin UI.
//       Output faders in TotalMix: AES, ADAT13/14, ADAT15/16 corresponding to MAIN, ALT1, ALT2 monitors.
// ///////////////////////////////////////////////////////
const std::vector<int> rmeTotalMixMidiChannelControllers = { 17, 7, 8, -1 };

//==============================================================================
DreamControlAudioProcessor::DreamControlAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
	: AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
		.withInput("Input", AudioChannelSet::stereo(), true)
#endif
		.withOutput("Output", AudioChannelSet::stereo(), true)
#endif
	)
#endif
	, MidiInputCallback()
{
	numChannels = getNumInputChannels();
	msSinceLastPeakReset = 0;

	// Init MIDI ports to our hardware, for sending meter values and receiving commands.
	// We use independent ports instead of our DAW port for better SysEx support.
	int outputDeviceId = MidiOutput::getDevices().indexOf(MIDI_OUT_PORT_NAME);
	if (outputDeviceId > -1)
	{
		midiOutput = MidiOutput::openDevice(outputDeviceId);
	}
	else
	{
		NativeMessageBox::showMessageBox(AlertWindow::AlertIconType::WarningIcon, "DreamControl", "Failed to open output port to hardware.");
		midiOutput = NULL;
	}

	int inputDeviceId = MidiInput::getDevices().indexOf(MIDI_IN_PORT_NAME);
	if (inputDeviceId > -1)
	{
		midiInput = MidiInput::openDevice(inputDeviceId, this);
		if (midiInput == NULL)
			NativeMessageBox::showMessageBox(AlertWindow::AlertIconType::WarningIcon, "DreamControl", "Failed to open input port from hardware.");
		else
			midiInput->start();
	}
	else
	{
		NativeMessageBox::showMessageBox(AlertWindow::AlertIconType::WarningIcon, "DreamControl", "Failed to open input port from hardware.");
		midiInput = NULL;
	}

	// Also try and connect to our DreamControlSwitcher unit if it's available.
	int switcherOutputDeviceId = MidiOutput::getDevices().indexOf(MIDI_OUT_SWITCHER_PORT_NAME);
	if (switcherOutputDeviceId > -1)
	{
		midiOutputToSwitcher = MidiOutput::openDevice(switcherOutputDeviceId);
	}
	else
	{
		midiOutputToSwitcher = NULL;
	}

	// Lambda for handling when a mode toggle changes.
	auto modeChangedFunction = [this](const String& paramName, bool newValue)
	{
		// We always reset LUFS meters when a mode changes, so the time-based values are accurate.
		// TODO: Make this an option!
		if (paramName != "dimMode" && paramName != "refMode" && paramName != "muteMode" && paramName != "volModMode")
		{
			lufsProcessor->reset();
		}

		// Mid/side solo is exclusive.
		if (newValue && (paramName == "midSolo"))
			sideSolo->setValueNotifyingHost(false);
		
		if (newValue && (paramName == "sideSolo"))
			midSolo->setValueNotifyingHost(false);
		
		// Dim/ref mode is exclusive.
		if (newValue && (paramName == "dimMode"))
			refMode->setValueNotifyingHost(false);
		
		if (newValue && (paramName == "refMode"))
			dimMode->setValueNotifyingHost(false);
		
		int button = 0;

		auto& params = this->getParameters();
		for (auto p : params)
		{
			if (auto* param = dynamic_cast<AudioParameterBoolNotify*> (p))
				if (param->paramID.compare(paramName) == 0)
					for (auto &keyval : this->buttonParamMap)
						if (keyval.second == param)
						{
							button = keyval.first;
							goto found_button;
						}
		}

	found_button:
		if (midiOutput != nullptr)
			midiOutput->sendMessageNow(MidiMessage::noteOn(1, button, newValue ? 1.0f : 0.0f));
	};

	// Lambda for handling when Reaper OSC integration is enabled/disabled.
	auto reaperOscChangedFunction = [this](const String& paramName, bool newValue)
	{
		if (newValue == true)
		{
			// Enable OSC send/receive.
			if (!this->reaperOscSender.connect(REAPER_OSC_SEND_IP, REAPER_OSC_SEND_PORT))
			{
				std::string send_ip(REAPER_OSC_SEND_IP);

				NativeMessageBox::showMessageBox(AlertWindow::AlertIconType::WarningIcon, "DreamControl",
					"Failed to open REAPER OSC send port (" + send_ip + ":" + std::to_string(REAPER_OSC_SEND_PORT) + ").");
			}

			if (this->connect(REAPER_OSC_RECEIVE_PORT))
			{
				this->OSCReceiver::addListener(this);
				oscConnected = true;
			}
			else
			{
				NativeMessageBox::showMessageBox(AlertWindow::AlertIconType::WarningIcon, "DreamControl",
					"Failed to open REAPER OSC receive port (" + std::to_string(REAPER_OSC_RECEIVE_PORT) + ").");
			}
		}
		else
		{
			this->OSCReceiver::removeListener(this);
			this->reaperOscSender.disconnect();
			//if (oscConnected) this->disconnect();
			oscConnected = false;
		}
	};

	// Lambda for handling when RME volume control is enabled/disabled.
	auto rmeVolControlChangedFunction = [this](const String& paramName, bool newValue)
	{
		// We support an optional RME volume control (TotalMix) on a loopback MIDI port.
		if (newValue == true)
		{
			auto devs = MidiOutput::getDevices();
			int volControlDeviceId = devs.indexOf(MIDI_OUT_VOL_CONTROL_PORT_NAME);
			if (volControlDeviceId > -1)
			{
				this->midiOutputToVolControl.reset(MidiOutput::openDevice(volControlDeviceId));
			}
			else
			{
				NativeMessageBox::showMessageBox(AlertWindow::AlertIconType::WarningIcon, "DreamControl", "Failed to open loopback MIDI port (must be named 'Loopback (DreamControl)'.");
				this->midiOutputToVolControl = nullptr;
			}
		}
		else
		{
			if (this->midiOutputToVolControl != nullptr)
			{
				this->midiOutputToVolControl.reset();
			}
		}
	};

	faderRpnDetector = new MidiRPNDetector();

	// Initialise level controls
	addParameter(monitorLevel = new AudioParameterFloat("monitorLevel", "Monitor Level", LOWEST_VOLUME_VALUE, 0.0f, LOWEST_VOLUME_VALUE));
	addParameter(muteMode = new AudioParameterBoolNotify("muteMode", "Mute", 0, modeChangedFunction));
	addParameter(dimMode = new AudioParameterBoolNotify("dimMode", "Dim", 0, modeChangedFunction));
	addParameter(refMode = new AudioParameterBoolNotify("refMode", "Ref", 0, modeChangedFunction));

	// Initialise our crossover filters
	numCrossovers = 3;
	numBands = numCrossovers + 1;			
	bandSolo.resize(numCrossovers + 1);
	crossoverFreq.resize(numCrossovers);

	for (int i = 0; i < numCrossovers + 1; i++) {
		addParameter(bandSolo[i] = new AudioParameterBoolNotify(
			"solo" + std::to_string(i + 1),
			"Band " + std::to_string(i + 1) + " Solo",
			0, modeChangedFunction)
		);
	}

	addParameter(midSolo = new AudioParameterBoolNotify("midSolo", "Mono / Mid Solo", 0, modeChangedFunction));
	addParameter(sideSolo = new AudioParameterBoolNotify("sideSolo", "Side Solo", 0, modeChangedFunction));
	addParameter(loudnessMode = new AudioParameterBoolNotify("loudMode", "Loud", 0, modeChangedFunction));

	// Initialise peak/RMS/clip meters
	peakMeterLeft = new AudioParameterFloat("peakMeterL", "True Peak L", NormalisableRange<float>(LOWEST_TRUE_PEAK_VALUE, HIGHEST_TRUE_PEAK_VALUE, 0.1f), 0.0f);
	peakMeterRight = new AudioParameterFloat("peakMeterR", "True Peak R", NormalisableRange<float>(LOWEST_TRUE_PEAK_VALUE, HIGHEST_TRUE_PEAK_VALUE, 0.1f), 0.0f);
	peakMeterMaxLeft = new AudioParameterFloat("peakMeterMaxL", "Max Peak L", NormalisableRange<float>(LOWEST_TRUE_PEAK_VALUE, HIGHEST_TRUE_PEAK_VALUE, 0.1f), 0.0f);
	peakMeterMaxRight = new AudioParameterFloat("peakMeterMaxR", "Max Peak R", NormalisableRange<float>(LOWEST_TRUE_PEAK_VALUE, HIGHEST_TRUE_PEAK_VALUE, 0.1f), 0.0f);
	clipMeterLeft = new AudioParameterBool("clipMeterL", "Clip L", 0);
	clipMeterRight = new AudioParameterBool("clipMeterR", "Clip R", 0);

	// Initialise our EBU R128 LUFS meter
	lufsProcessor = new LufsProcessor(getNumInputChannels());

	lufsMomentary = new AudioParameterFloat("lufsMomentary", "LUFS Momentary", NormalisableRange<float>(LOWEST_LUFS_VALUE, 0.0f, 0.1f), 0.0f);
	lufsShort = new AudioParameterFloat("lufsShort", "LUFS Short", NormalisableRange<float>(LOWEST_LUFS_VALUE, 0.0f, 0.1f), 0.0f);
	lufsIntegrated = new AudioParameterFloat("lufsIntegrated", "LUFS Integrated", NormalisableRange<float>(LOWEST_LUFS_VALUE, 0.0f, 0.1f), 0.0f);
	lufsReset = new AudioParameterBoolNotify("lufsReset", "LUFS Reset", false, modeChangedFunction);
	addParameter(lufsTarget = new AudioParameterFloat("lufsTarget", "LUFS Target", NormalisableRange<float>(LOWEST_LUFS_VALUE, 0.0f, 1.0f), -16.0f));
	addParameter(lufsRangeMin = new AudioParameterFloat("lufsRangeMin", "LUFS Range Min", NormalisableRange<float>(LOWEST_LUFS_VALUE, 0.0f, 0.1f), 0.0f));
	addParameter(lufsRangeMax = new AudioParameterFloat("lufsRangeMax", "LUFS Range Max", NormalisableRange<float>(LOWEST_LUFS_VALUE, 0.0f, 0.1f), 0.0f));

	// Initialise meter settings
	addParameter(lufsMode = new AudioParameterBoolNotify("lufsMode", "LUFS Mode", false, modeChangedFunction));
	addParameter(peakWithMomentaryMode = new AudioParameterBoolNotify("peakWithMomentaryMode", "Peak mode shows LUFS Momentary", false, modeChangedFunction));
	addParameter(relativeMode = new AudioParameterBoolNotify("relativeMode", "LUFS Relative Mode", false, modeChangedFunction));
	addParameter(is1dbPeakScale = new AudioParameterBoolNotify("is1dbPeakScale", "1dB Peak Meter Scale", false, modeChangedFunction));

	for (int i = 0; i < numCrossovers; i++) {
		addParameter(crossoverFreq[i] = new AudioParameterFloat(
			"crossover" + std::to_string(i + 1),
			"Band " + std::to_string(i + 1) + "/" + std::to_string(i + 2) + " Crossover Frequency",
			NormalisableRange<float>(20.0f, 10000.0f, 0.0f, 1.0f),
			i == 0 ? 100 : i == 1 ? 400 : i == 2 ? 4000 : 1000)
		);
	}

	addParameter(dimLevel = new AudioParameterFloat("dimLevel", "Dim Level", LOWEST_TRUE_PEAK_VALUE, 0.0f, -25.0f));
	addParameter(refLevel = new AudioParameterFloat("refLevel", "Ref Level", LOWEST_TRUE_PEAK_VALUE, 0.0f, -10.0f));

	addParameter(peakHoldSeconds = new AudioParameterFloat("peakHold", "Peak Hold (seconds)", 0.0f, 10.0f, 5.0f));
	addParameter(volModMode = new AudioParameterBoolNotify("volModMode", "Volume/Modulate Mode (dev)", false, modeChangedFunction));

	addParameter(useRMEVolControl = new AudioParameterBoolNotify("useRMEVolControl", "RME TotalMix integration", false, rmeVolControlChangedFunction));
	addParameter(useRMEMonitorSwitch = new AudioParameterBool("useRMEMonitorSwitch", "RME TotalMix monitor switch", false));
	addParameter(useReaperOsc = new AudioParameterBoolNotify("useReaperOsc", "REAPER OSC integration", false, reaperOscChangedFunction));

	// Our map of button note numbers to plugin parameters.
	buttonParamMap = {
		{ BUTTON_LOUD, loudnessMode },
		{ BUTTON_MONO, midSolo },
		{ BUTTON_SIDE, sideSolo },
		{ BUTTON_LOW, bandSolo[0] },
		{ BUTTON_LOMID, bandSolo[1] },
		{ BUTTON_HIMID, bandSolo[2] },
		{ BUTTON_HIGH, bandSolo[3] },
		{ BUTTON_MONMUTE, muteMode },
		{ BUTTON_DIM, dimMode },
		{ BUTTON_REF, refMode },
		{ BUTTON_RESET_METER, lufsReset },
		{ BUTTON_PEAK_LUFS, lufsMode },
		{ BUTTON_ABS_REL, relativeMode },
		{ BUTTON_VOL_MOD, volModMode },
		{ BUTTON_3RD_METER_IS_MOMENTARY, peakWithMomentaryMode },
		{ BUTTON_1DB_PEAK_SCALE, is1dbPeakScale }
	};

	// OSC address to button map, for REAPER integration.
	reaperOscButtonMap = {
		{ BUTTON_RETURN, "/action/40632" },
		{ BUTTON_LOOP, "/repeat" },
		{ BUTTON_BACK, "/action/40084" },
		{ BUTTON_FORWARD, "/action/40085" },
		{ BUTTON_PLAY, "/play" },
		{ BUTTON_STOP, "/stop" },
		{ BUTTON_CLICK, "/click" },
		{ BUTTON_RECORD, "/record" },
		{ BUTTON_MUTE, "/track/mute/toggle" },
		{ BUTTON_SOLO, "/track/solo/toggle" },
		{ BUTTON_READ, "/track/autoread" },
		{ BUTTON_WRITE, "/track/autowrite" },
		{ BUTTON_SAVE, "/action/40026" },
		{ BUTTON_LOOP1, "" },
		{ BUTTON_LOOP2, "" },
		{ BUTTON_LOOP8, "" },
		{ BUTTON_LOOP16, "" },
		{ BUTTON_MUTE_CLEAR, "/action/40339" },
		{ BUTTON_SOLO_CLEAR, "/action/40340" },
		{ BUTTON_READ_ALL, "/action/40086" },
		{ BUTTON_WRITE_ALL, "/action/40090" },
		{ BUTTON_MIX, "/anysolo" },
		{ BUTTON_CUE1, "/track/1/solo" },
		{ BUTTON_CUE2, "/track/2/solo" },
		{ BUTTON_CUE3, "/track/3/solo" },
		{ BUTTON_CUE4, "/track/4/solo" },
		{ BUTTON_EXT1, "/track/5/solo" },
		{ BUTTON_EXT2, "/track/6/solo" }
	};

	if (midiOutput != nullptr && useRMEVolControl->get())
	{
		midiOutput->sendMessageNow(MidiMessage::noteOn(1, BUTTON_MAIN, 1.0f));
		updateRMEVolumeControl();
	}

	if (midiOutput != nullptr)
	{
		midiOutput->sendMessageNow(MidiMessage::noteOn(1, BUTTON_MIX, 1.0f));
		for (int i = BUTTON_CUE1; i <= BUTTON_EXT2; i++)
			midiOutput->sendMessageNow(MidiMessage::noteOn(1, i, 0.0f));
	}
}

DreamControlAudioProcessor::~DreamControlAudioProcessor()
{
	if (midiInput != nullptr) midiInput->stop();
	stopTimer();
	delete lufsProcessor;
	if (midiInput != nullptr) delete midiInput;
	if (midiOutput != nullptr) delete midiOutput;
	if (midiOutputToSwitcher != nullptr) delete midiOutputToSwitcher;
	if (midiOutputToVolControl != nullptr) midiOutputToVolControl.reset();

	this->OSCReceiver::removeListener(this);
	reaperOscSender.disconnect();
	//if (oscConnected) this->disconnect();
}

//==============================================================================
const String DreamControlAudioProcessor::getName() const
{
	return JucePlugin_Name;
}

bool DreamControlAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
	return true;
#else
	return false;
#endif
}

bool DreamControlAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
	return true;
#else
	return false;
#endif
}

bool DreamControlAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
	return true;
#else
	return false;
#endif
}

double DreamControlAudioProcessor::getTailLengthSeconds() const
{
	return 0.0;
}

int DreamControlAudioProcessor::getNumPrograms()
{
	return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
				// so this should be at least 1, even if you're not really implementing programs.
}

int DreamControlAudioProcessor::getCurrentProgram()
{
	return 0;
}

void DreamControlAudioProcessor::setCurrentProgram(int index)
{
}

const String DreamControlAudioProcessor::getProgramName(int index)
{
	return {};
}

void DreamControlAudioProcessor::changeProgramName(int index, const String& newName)
{
}

//==============================================================================
void DreamControlAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
	numChannels = getNumInputChannels();
	int bufferSize = getBlockSize();

	//////////////////////////////////////////////////////////////////////////
	// Crossover filter initialisation
	//////////////////////////////////////////////////////////////////////////

	// Allocate memory for filter objects
	crossoverFilters.resize(numChannels);
	for (auto &it : crossoverFilters)
		it.resize(numCrossovers * 4);		// * 4 for duplicated filters for Linkwitz-Riley implementation

	if (crossoverFilters.size() != 0) {
		std::vector<std::vector<std::unique_ptr<CrossoverFilter>>>::iterator row;
		std::vector<std::unique_ptr<CrossoverFilter>>::iterator col;
		for (row = crossoverFilters.begin(); row != crossoverFilters.end(); row++) {
			for (col = row->begin(); col != row->end(); col++) {
				*col = std::make_unique<CrossoverFilter>(false, true);
			}
		}
	}

	// Update the filter settings to work with the current parameters and sample rate
	updateFilters(sampleRate);

	//////////////////////////////////////////////////////////////////////////
	// Loudness EQ initialisation
	//////////////////////////////////////////////////////////////////////////

	loudnessEqFilters.resize(numChannels);
	for (auto &it : loudnessEqFilters)
		it.resize(7);						// 7 bands of EQ for our loudness EQ

	if (loudnessEqFilters.size() != 0) {
		std::vector<std::vector<std::unique_ptr<IIRFilter>>>::iterator row;
		std::vector<std::unique_ptr<IIRFilter>>::iterator col;
		for (row = loudnessEqFilters.begin(); row != loudnessEqFilters.end(); row++) {
			for (col = row->begin(); col != row->end(); col++) {
				*col = std::make_unique<IIRFilter>(IIRFilter());
			}
		}
	}

	for (int chan = 0; chan < numChannels; chan++)
	{
		// EQ parameters taken from https://www.hometheatershack.com/forums/av-home-theater/23077-equal-loudness-db-phons-contours-eq-you-will-want-give-listen.html
		// TODO: This needs to be checked and has scope for improvement.
		loudnessEqFilters[chan][0]->setCoefficients(IIRCoefficients::makePeakFilter(sampleRate, 20.0, 4.45, Decibels::decibelsToGain(-38.9)));
		loudnessEqFilters[chan][1]->setCoefficients(IIRCoefficients::makePeakFilter(sampleRate, 1130.0, 0.65, Decibels::decibelsToGain(3.85)));
		loudnessEqFilters[chan][2]->setCoefficients(IIRCoefficients::makePeakFilter(sampleRate, 1490.0, 2.20, Decibels::decibelsToGain(-8.15)));
		loudnessEqFilters[chan][3]->setCoefficients(IIRCoefficients::makePeakFilter(sampleRate, 3290.0, 0.59, Decibels::decibelsToGain(6.55)));
		loudnessEqFilters[chan][4]->setCoefficients(IIRCoefficients::makePeakFilter(sampleRate, 8850.0, 1.78, Decibels::decibelsToGain(-12.88)));
		loudnessEqFilters[chan][5]->setCoefficients(IIRCoefficients::makePeakFilter(sampleRate, 12300.0, 4.50, Decibels::decibelsToGain(5.44)));
		loudnessEqFilters[chan][6]->setCoefficients(IIRCoefficients::makePeakFilter(sampleRate, 20000.0, 3.50, Decibels::decibelsToGain(-10.50)));
	}

	//////////////////////////////////////////////////////////////////////////
	// Loudness meter initialisation
	//////////////////////////////////////////////////////////////////////////

	lufsProcessor->prepareToPlay(sampleRate, samplesPerBlock);
	lufsProcessor->reset();

	startTimer(CALLBACK_TIMER_PERIOD_MS);
}

void DreamControlAudioProcessor::releaseResources()
{

}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DreamControlAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
	ignoreUnused(layouts);
	return true;
#else
	// This is the place where you check if the layout is supported.
	// In this template code we only support mono or stereo.
	if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
		&& layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
		return false;

	// This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
	if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
		return false;
#endif

	return true;
#endif
}
#endif

void DreamControlAudioProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
	//////////////////////////////////////////////////////////////////////////
	// Audio processing block
	//////////////////////////////////////////////////////////////////////////

	const int numInputChannels = getNumInputChannels();   
	const int numOutputChannels = getNumOutputChannels();   
	const int numSamples = buffer.getNumSamples();          												

	// Perform band filtering if any of our band solos are engaged.
	if (isAnyBandSolo()) 
	{
		AudioSampleBuffer inputBuffer;
		AudioSampleBuffer chanBuffer = AudioSampleBuffer(1, numSamples);
		inputBuffer.makeCopyOf(buffer);
		buffer.clear();

		// For each channel, for each band, perform LP and HP filtering on input, then sum to main output buffer.
		// Lowest and highest bands have 1 L/HPF, and mid bands have 1 LPF + 1 HPF. These are also doubled in number to make Linkwitz-Riley filters.
		for (int chan = 0; chan < numInputChannels; chan++)
		{	
			int filtIndex = 0;

			for (int band = 0; band < numBands; band++)
			{
				int filtCount = (band > 0) && (band < numBands - 1) ? 4 : 2;

				if (bandSolo[band]->get() == false)
				{
					// If band is not solo'd, skip this band.
					filtIndex += filtCount;
					continue;
				}

				for (int i = 0; i < filtCount; i++)
				{
					float* sourcePointer = (i == 0) ? inputBuffer.getWritePointer(chan) : chanBuffer.getWritePointer(0);
					crossoverFilters[chan][filtIndex]->applyFilter(sourcePointer, chanBuffer.getWritePointer(0), numSamples);
					filtIndex++;
				}				

				// Sum to output buffer
				for (int sample = 0; sample < numSamples; ++sample)
					buffer.getWritePointer(chan)[sample] += chanBuffer.getReadPointer(0)[sample];
			}
		}
	}

	// Mid/side solo
	if (midSolo->get() == true)
	{
		for (int sample = 0; sample < numSamples; ++sample)
		{
			buffer.getWritePointer(0)[sample] = (buffer.getReadPointer(0)[sample] + buffer.getReadPointer(1)[sample]) / 2.0;
			buffer.getWritePointer(1)[sample] = buffer.getReadPointer(0)[sample];
		}
	}
	else if (sideSolo->get() == true)
	{
		AudioSampleBuffer inputBuffer;
		inputBuffer.makeCopyOf(buffer);

		for (int sample = 0; sample < numSamples; ++sample)
		{
			buffer.getWritePointer(0)[sample] = ((inputBuffer.getReadPointer(1)[sample] - inputBuffer.getReadPointer(0)[sample]) 
				+ -(inputBuffer.getReadPointer(0)[sample] - inputBuffer.getReadPointer(1)[sample])) / 2.0;
			buffer.getWritePointer(1)[sample] = buffer.getReadPointer(0)[sample];
		}
	}

	// Loudness EQ
	if (loudnessMode->get() == true)
	{
		for (int chan = 0; chan < numChannels; chan++)
		{
			for (int i = 0; i < 7; i++)
				loudnessEqFilters[chan][i]->processSamples(buffer.getWritePointer(chan), numSamples);
		}
	}

	// Perform LUFS and True Peak measurements.
	lufsProcessor->processBlock(buffer);

	if (!useRMEVolControl->get()) 
	{
		// Monitor/ref/dim gain.
		float currentGainDb = dimMode->get() ? dimLevel->get() : refMode->get() ? refLevel->get() : monitorLevel->get();

		// Set gain, or mute
		if (muteMode->get() || currentGainDb <= LOWEST_VOLUME_VALUE)
		{
			buffer.clear();
		}
		else
		{
			float gain = Decibels::decibelsToGain(currentGainDb);
			buffer.applyGain(gain);
		}
	}
}

//==============================================================================
// Callback executed every 10ms.
void DreamControlAudioProcessor::hiResTimerCallback()
{
	// LUFS meter.
	lufsProcessor->update();
	const int validSize = lufsProcessor->getValidSize();

	float lufsSval = lufsProcessor->getShortTermVolumeArray()[validSize - 1];
	float lufsMval = lufsProcessor->getMomentaryVolumeArray()[validSize - 1];
	float lufsIval = lufsProcessor->getIntegratedVolumeArray()[validSize - 1];
	float lufsMinVal = lufsProcessor->getRangeMinVolume();
	float lufsMaxVal = lufsProcessor->getRangeMaxVolume();

	lufsShort->setValueNotifyingHost(((lufsSval >= LOWEST_LUFS_VALUE ? lufsSval : LOWEST_LUFS_VALUE) - LOWEST_LUFS_VALUE) / -LOWEST_LUFS_VALUE);
	lufsMomentary->setValueNotifyingHost(((lufsMval >= LOWEST_LUFS_VALUE ? lufsMval : LOWEST_LUFS_VALUE) - LOWEST_LUFS_VALUE) / -LOWEST_LUFS_VALUE);
	lufsIntegrated->setValueNotifyingHost(((lufsIval >= LOWEST_LUFS_VALUE ? lufsIval : LOWEST_LUFS_VALUE) - LOWEST_LUFS_VALUE) / -LOWEST_LUFS_VALUE);
	lufsRangeMin->setValueNotifyingHost(((lufsMinVal >= LOWEST_LUFS_VALUE ? lufsMinVal : LOWEST_LUFS_VALUE) - LOWEST_LUFS_VALUE) / -LOWEST_LUFS_VALUE);
	lufsRangeMax->setValueNotifyingHost(((lufsMaxVal >= LOWEST_LUFS_VALUE ? lufsMaxVal : LOWEST_LUFS_VALUE) - LOWEST_LUFS_VALUE) / -LOWEST_LUFS_VALUE);

	if (lufsReset->get() == true)
	{
		lufsReset->setValueNotifyingHost(false);
		lufsProcessor->reset();
		msSinceLastPeakReset = 0;
	}

	// True Peak meter.
	float truePeakRange = LOWEST_TRUE_PEAK_VALUE - HIGHEST_TRUE_PEAK_VALUE;
	float peakLval = lufsProcessor->getTruePeakChannelArray(0)[validSize - 1] - HIGHEST_TRUE_PEAK_VALUE;
	float peakRval = lufsProcessor->getTruePeakChannelArray(1)[validSize - 1] - HIGHEST_TRUE_PEAK_VALUE;
	peakMeterLeft->setValueNotifyingHost(((peakLval >= truePeakRange ? peakLval : truePeakRange) - truePeakRange) / -truePeakRange);
	peakMeterRight->setValueNotifyingHost(((peakRval >= truePeakRange ? peakRval : truePeakRange) - truePeakRange) / -truePeakRange);

	float peakHold = peakHoldSeconds->get();
	if (peakHold == 0.0f || msSinceLastPeakReset >= peakHold * 1000.0f || peakLval > lastMaxLeft)
	{
		peakMeterMaxLeft->setValueNotifyingHost(((peakLval >= truePeakRange ? peakLval : truePeakRange) - truePeakRange) / -truePeakRange);
		clipMeterLeft->setValueNotifyingHost(peakLval + HIGHEST_TRUE_PEAK_VALUE > 0.0f);
		lastMaxLeft = peakLval;
	}
	if (peakHold == 0.0f || msSinceLastPeakReset >= peakHold * 1000.0f || peakRval > lastMaxRight)
	{
		peakMeterMaxRight->setValueNotifyingHost(((peakRval >= truePeakRange ? peakRval : truePeakRange) - truePeakRange) / -truePeakRange);
		clipMeterRight->setValueNotifyingHost(peakRval + HIGHEST_TRUE_PEAK_VALUE > 0.0f);
		lastMaxRight = peakRval;
	}

	if (peakHold == 0.0f || msSinceLastPeakReset >= peakHold * 1000.0f)
		msSinceLastPeakReset = 0.0f;

	if (peakHold > 0.0f)
		msSinceLastPeakReset += CALLBACK_TIMER_PERIOD_MS;

	// Send MIDI SysEx packet to hardware with meter values.
	// We send float values as 2 bytes, integral and fractional.
	// This allows us a range of -99.99 to 0.00 dB, suitable for our meters. We could improve this.
	if (midiOutput != nullptr) {
		char* lufsSints = getMeterIntegralFractional(lufsSval);
		char* lufsMints = getMeterIntegralFractional(lufsMval);
		char* lufsIints = getMeterIntegralFractional(lufsIval);
		char* lufsMinInts = getMeterIntegralFractional(lufsMinVal);
		char* lufsMaxInts = getMeterIntegralFractional(lufsMaxVal);
		float lufsRangeVal = lufsMaxVal - lufsMinVal;
		char* lufsRangeInts = getMeterIntegralFractional(lufsRangeVal);
		char* lufsTargetVal = getMeterIntegralFractional(lufsTarget->get());
		char* peakLints = getMeterIntegralFractional(peakLval);
		char* peakRints = getMeterIntegralFractional(peakRval);

		char* lastMaxLeftInts = getMeterIntegralFractional(lastMaxLeft);
		char* lastMaxRightInts = getMeterIntegralFractional(lastMaxRight);

		float lastMaxTotalVal = lastMaxLeft > lastMaxRight ? lastMaxLeft : lastMaxRight;
		char* lastMaxTotalInts = getMeterIntegralFractional(lastMaxTotalVal);

		unsigned char sysexData[] = { 
			sysexManufacturerId[0], sysexManufacturerId[1], sysexManufacturerId[2],
			SYSEX_COMMAND_METER_DATA,
			lufsSints[0], lufsSints[1],
			lufsMints[0], lufsMints[1],
			lufsIints[0], lufsIints[1],
			lufsMinInts[0], lufsMinInts[1],
			lufsMaxInts[0], lufsMaxInts[1],
			lufsRangeInts[0], lufsRangeInts[1],
			lufsTargetVal[0], lufsTargetVal[1],
			peakLints[0], peakLints[1],
			peakRints[0], peakRints[1],
			lastMaxLeftInts[0], lastMaxLeftInts[1],
			lastMaxRightInts[0], lastMaxRightInts[1],
			lastMaxTotalInts[0], lastMaxTotalInts[1],
			peakLval + HIGHEST_TRUE_PEAK_VALUE > 0.0f ? 1 : 0, peakRval + HIGHEST_TRUE_PEAK_VALUE > 0.0f ? 1 : 0
		};

		MidiMessage msg = MidiMessage::createSysExMessage(sysexData, sizeof(sysexData));
		midiOutput->sendMessageNow(msg);
	}

	// Update crossover filter coefficients.
	updateFilters(getSampleRate());
}

char* DreamControlAudioProcessor::getMeterIntegralFractional(float val)
{
	// Get the integral and fractional of a floating point number, limited to 99 (-99.0dB is the lowest meter level we deal with).
	float integral;
	float fractional = modf(val, &integral);
	fractional = std::abs(roundf(fractional * 100.0));
	integral = std::abs(integral);
	if (fractional > 99.0) fractional = 99.0;
	if (integral > 99.0) integral = 99.0;
	
	char* output = new char[2];
	output[0] = (char)integral;
	output[1] = (char)fractional;
	return output;
}

// Update the coefficients of our crossover filters.
void DreamControlAudioProcessor::updateFilters(float sampleRate)
{
	// Iterate over each filter object and set coefficients from plugin params.
	// Lowest and highest bands have 1 L/HPF, and mid bands have 1 LPF + 1 HPF. These are also doubled in number to make Linkwitz-Riley filters.
	for (int chan = 0; chan < numChannels; chan++)
	{
		int filtIndex = 0;

		for (int band = 0; band < numBands; band++)
		{
			for (int i = 0; i < 2; i++)		// Duplication of filters for Linkwitz-Riley.
			{
				if (band == 0)
				{
					crossoverFilters[chan][filtIndex]->makeCrossover(*crossoverFreq[band], sampleRate, true, false);
					filtIndex++;
				}
				else if (band < numBands - 1)
				{
					crossoverFilters[chan][filtIndex]->makeCrossover(*crossoverFreq[band - 1], sampleRate, true, true);
					crossoverFilters[chan][filtIndex + 1]->makeCrossover(*crossoverFreq[band], sampleRate, true, false);
					filtIndex += 2;
				}
				else
				{
					crossoverFilters[chan][filtIndex]->makeCrossover(*crossoverFreq[band - 1], sampleRate, true, true);
					filtIndex++;
				}
			}
		}
	}
}

bool DreamControlAudioProcessor::isAnyBandSolo()
{
	for (int b = 0; b < bandSolo.size(); b++)
		if (bandSolo[b]->get() == true) return true;
	return false;
}

//==============================================================================
// MIDI Input handler.
void DreamControlAudioProcessor::handleIncomingMidiMessage(MidiInput* source, const MidiMessage& m)
{
	if (m.isSysEx())
	{
		// Incoming SysEx.
		const juce::uint8 *data = m.getSysExData();
		
		if (data[0] == SYSEX_COMMAND_SYNC_BUTTONS)
		{
			// Send all button values out.
			for (auto p : buttonParamMap)	
				if (midiOutput != nullptr)
					midiOutput->sendMessageNow(MidiMessage::noteOn(1, p.first, p.second->get() ? 1.0f : 0.0f));
			
		}
	}
	else if (m.isControllerOfType(7))
	{
		// Volume control.
		float volume = ((m.getControllerValue()) / VOLUME_CONTROL_MIDI_RANGE);
		monitorLevel->setValueNotifyingHost(volume);

		updateRMEVolumeControl();
	}
	else if (m.isNoteOn(false) && m.getVelocity() == 127
		&& (m.getNoteNumber() == BUTTON_MAIN || m.getNoteNumber() == BUTTON_ALT1 || m.getNoteNumber() == BUTTON_ALT2 || m.getNoteNumber() == BUTTON_ALT3))
	{
		// Monitor select.
		int newMonitorSelect = m.getNoteNumber() - BUTTON_MAIN;

		for (int i = BUTTON_MAIN; i <= BUTTON_ALT3; i++)
			midiOutput->sendMessageNow(MidiMessage::noteOn(1, i, 0.0f));

		if (newMonitorSelect == currentMonitorSelect)
		{
			// Monitors off (switches off relays in switcher unit).
			currentMonitorSelect = -1;
		}
		else
		{
			currentMonitorSelect = newMonitorSelect;
			midiOutput->sendMessageNow(MidiMessage::noteOn(1, m.getNoteNumber(), 1.0f));
		}

		if (midiOutputToSwitcher != nullptr)
		{
			// If switcher unit present, send Note Ons to switch relays
			midiOutputToSwitcher->sendMessageNow(MidiMessage::noteOn(1, currentMonitorSelect + 1, 127.0f));
		}

		updateRMEVolumeControl();
	}
	else if (m.isNoteOn(false) && m.getVelocity() == 127)
	{
		// Toggle parameter value on button press.
		int button = m.getNoteNumber();

		if (this->buttonParamMap.count(button) > 0)
			this->buttonParamMap[button]->setValueNotifyingHost(!this->buttonParamMap[button]->get());

		if (button >= BUTTON_MONMUTE && button <= BUTTON_REF)
		{
			updateRMEVolumeControl();
		}

		// Send OSC to REAPER on button press.
		if (useReaperOsc->get())
		{
			auto it = reaperOscButtonMap.find(button);
			if (it != reaperOscButtonMap.end())
			{
				auto address = it->second;

				if (address != "")
				{
					// Input select buttons.
					if (button >= BUTTON_MIX && button <= BUTTON_EXT2)
					{
						if (button == BUTTON_MIX || currentInputButton == button)
						{
							address = "/anysolo";
						}
						else 
						{
							reaperOscSender.send("/anysolo", 1.0f);
						}
					}

					// Special behaviour for automation read/write buttons, as REAPER has 4 different automation modes.
					if (button == BUTTON_READ && isReadEnabled && isWriteEnabled)
						reaperOscSender.send("/track/autowrite", 1.0f);
					else if (button == BUTTON_READ && !isReadEnabled && isWriteEnabled)
						reaperOscSender.send("/track/autotouch", 1.0f);
					else if (button == BUTTON_READ && isReadEnabled && !isWriteEnabled)
						reaperOscSender.send("/track/autotrim", 1.0f);
					else if (button == BUTTON_READ && !isReadEnabled && !isWriteEnabled)
						reaperOscSender.send("/track/autoread", 1.0f);
					else if (button == BUTTON_WRITE && isReadEnabled && isWriteEnabled)
						reaperOscSender.send("/track/autoread", 1.0f);
					else if (button == BUTTON_WRITE && isReadEnabled && !isWriteEnabled)
						reaperOscSender.send("/track/autotouch", 1.0f);
					else if (button == BUTTON_WRITE && !isReadEnabled && isWriteEnabled)
						reaperOscSender.send("/track/autotrim", 1.0f);
					else if (button == BUTTON_WRITE && !isReadEnabled && !isWriteEnabled)
						reaperOscSender.send("/track/autowrite", 1.0f);
					else
						reaperOscSender.send(address, 1.0f);
				}
			}
		}

		// Input select buttons.
		if (button >= BUTTON_MIX && button <= BUTTON_EXT2)
		{
			for (int i = BUTTON_MIX; i <= BUTTON_EXT2; i++)
				midiOutput->sendMessageNow(MidiMessage::noteOn(1, i, 0.0f));

			if (button == BUTTON_MIX || currentInputButton == button)
			{
				currentInputButton = BUTTON_MIX;
				midiOutput->sendMessageNow(MidiMessage::noteOn(1, BUTTON_MIX, 1.0f));
			}
			else
			{
				currentInputButton = button;
				midiOutput->sendMessageNow(MidiMessage::noteOn(1, button, 1.0f));
			}
		}
	}
	else if (m.isControllerOfType(MIDI_FADER_TOUCH_SENSE_CC) && useReaperOsc->get())
	{
		// Pass fader touch sense to REAPER.
		reaperOscSender.send("/track/volume/touch", m.getControllerValue() > 0 ? 1.0f : 0.0f);
	}
	else if (m.isController())
	{
		// Check all other controllers, parse to see if it is the track fader NRPN value.
		MidiRPNMessage msg;
		if (faderRpnDetector->parseControllerMessage(m.getChannel(), m.getControllerNumber(), m.getControllerValue(), msg))
		{
			if (useReaperOsc->get() && msg.isNRPN && msg.is14BitValue)
				reaperOscSender.send("/track/volume", (float)msg.value / 16383.0f);
		}
	}
}

void DreamControlAudioProcessor::updateRMEVolumeControl()
{
	// If external volume control enabled, send MIDI.
	if (midiOutputToVolControl != nullptr && useRMEVolControl->get())
	{
		// Monitor/mute/ref/dim value.
		float level = dimMode->get() ? dimLevel->get() : refMode->get() ? refLevel->get() : monitorLevel->get();
		if (muteMode->get()) level = LOWEST_VOLUME_VALUE;

		// Look up RME TotalMix fader MIDI value using our curve vector.
		std::vector<float>::const_iterator index = std::lower_bound(RME_TOTALMIX_FADER_CURVE.begin(), RME_TOTALMIX_FADER_CURVE.end(), level);
		int levelMidiVal = (index - RME_TOTALMIX_FADER_CURVE.begin());
		if (level <= LOWEST_VOLUME_VALUE + 0.5) levelMidiVal = 0;

		// Send volume control MIDI messages.
		for (int i = 0; i < rmeTotalMixMidiChannelControllers.size(); i++)
		{
			int rmeChan = rmeTotalMixMidiChannelControllers[i];
			if (rmeChan == -1) continue;

			// Get MIDI channel and CC for TotalMix hardware output fader.
			int midiChan = floor((rmeChan - 1) / 8) + 9;
			int midiCC = (((rmeChan - 1) % 8) * 2) + 102;

			MidiMessage msg = MidiMessage::controllerEvent(midiChan, midiCC, (currentMonitorSelect == i || useRMEMonitorSwitch->get() == false) ? levelMidiVal : 0);
			midiOutputToVolControl->sendMessageNow(msg);

			if (useRMEMonitorSwitch->get() == false && i > 0)
				return;
		}
	}
}

//==============================================================================
// OSC Input handler.
void DreamControlAudioProcessor::oscMessageReceived(const OSCMessage& message)
{
	auto pattern = message.getAddressPattern().toString();
	float value = message[0].getFloat32();

	if (midiOutput != nullptr)
	{
		if (pattern == "/track/volume")
		{
			// Track volume fader.
			midiOutput->sendBlockOfMessagesNow(MidiRPNGenerator::generate(1, 1, static_cast<int>(value * 16383), true, true));
		}
		else if (pattern == "/anysolo" && value == 0.0f)
		{
			midiOutput->sendMessageNow(MidiMessage::noteOn(1, BUTTON_MIX, 1.0f));
			for (int btn = BUTTON_CUE1; btn <= BUTTON_EXT2; btn++)
				midiOutput->sendMessageNow(MidiMessage::noteOn(1, btn, 0.0f));
		}
		else if (pattern == "/track/autotrim" && value == 1.0f)
		{
			// Special behaviour for read/write buttons.
			midiOutput->sendMessageNow(MidiMessage::noteOn(1, BUTTON_READ, 0.0f));
			midiOutput->sendMessageNow(MidiMessage::noteOn(1, BUTTON_WRITE, 0.0f));
			isReadEnabled = false;
			isWriteEnabled = false;
		}
		else if (pattern == "/track/autotouch" && value == 1.0f)
		{
			midiOutput->sendMessageNow(MidiMessage::noteOn(1, BUTTON_READ, 1.0f));
			midiOutput->sendMessageNow(MidiMessage::noteOn(1, BUTTON_WRITE, 1.0f));
			isReadEnabled = true;
			isWriteEnabled = true;
		}
		else if (pattern == "/track/autoread" && value == 1.0f)
		{
			midiOutput->sendMessageNow(MidiMessage::noteOn(1, BUTTON_READ, 1.0f));
			midiOutput->sendMessageNow(MidiMessage::noteOn(1, BUTTON_WRITE, 0.0f));
			isReadEnabled = true;
			isWriteEnabled = false;
		}
		else if (pattern == "/track/autowrite" && value == 1.0f)
		{
			midiOutput->sendMessageNow(MidiMessage::noteOn(1, BUTTON_READ, 0.0f));
			midiOutput->sendMessageNow(MidiMessage::noteOn(1, BUTTON_WRITE, 1.0f));
			isReadEnabled = false;
			isWriteEnabled = true;
		}
		else
		{
			// Check for a matching button in our map, and send MIDI note on/off based on value argument (turn LED on or off).
			// TODO: Useful OSC addresses for future: /track/number/str, /track/name
			for (auto it = reaperOscButtonMap.begin(); it != reaperOscButtonMap.end(); it++)
			{
				if (it->second.startsWith("/action") || it->second == "")
				{
					continue;
				}
				else if (it->second == pattern)
				{
					int button = it->first;

					midiOutput->sendMessageNow(MidiMessage::noteOn(1, button, value));
					return;
				}
			}
		}
	}
}

void DreamControlAudioProcessor::oscBundleReceived(const OSCBundle& bundle)
{
	for (auto it = bundle.begin(); it != bundle.end(); it++)
		if (it->isMessage())
			oscMessageReceived(it->getMessage());
}

//==============================================================================
bool DreamControlAudioProcessor::hasEditor() const
{
	return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* DreamControlAudioProcessor::createEditor()
{
	return new GenericAudioProcessorEditor(this);
	//return new DreamControlAudioProcessorEditor(*this);
}

//==============================================================================
// Parameter state persistence.
// TODO: Use AudioProcessorValueTreeState to manage parameters and persist state.

void DreamControlAudioProcessor::getStateInformation(MemoryBlock& destData)
{
	MemoryOutputStream stream(destData, true);

	stream.writeFloat(monitorLevel->get());
	stream.writeBool(*muteMode);
	stream.writeBool(*dimMode);
	stream.writeBool(*refMode);
	stream.writeBool(*midSolo);
	stream.writeBool(*sideSolo);
	stream.writeBool(*loudnessMode);
	stream.writeFloat(lufsTarget->get());
	stream.writeBool(*lufsMode);
	stream.writeBool(*peakWithMomentaryMode);
	stream.writeBool(*relativeMode);
	stream.writeBool(*is1dbPeakScale);
	stream.writeFloat(dimLevel->get());
	stream.writeFloat(refLevel->get());
	stream.writeFloat(peakHoldSeconds->get());
	stream.writeBool(*volModMode);
	stream.writeBool(*useRMEVolControl);
	stream.writeBool(*useReaperOsc);
}

void DreamControlAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
	MemoryInputStream stream(data, static_cast<size_t> (sizeInBytes), false);

	float volLevel = stream.readFloat();
	if (volLevel < LOWEST_VOLUME_VALUE) volLevel = LOWEST_VOLUME_VALUE;
	*monitorLevel = volLevel;

	muteMode->setValueNotifyingHost(stream.readBool());
	dimMode->setValueNotifyingHost(stream.readBool());
	refMode->setValueNotifyingHost(stream.readBool());
	midSolo->setValueNotifyingHost(stream.readBool());
	sideSolo->setValueNotifyingHost(stream.readBool());
	loudnessMode->setValueNotifyingHost(stream.readBool());
	*lufsTarget = stream.readFloat();
	lufsMode->setValueNotifyingHost(stream.readBool());
	peakWithMomentaryMode->setValueNotifyingHost(stream.readBool());
	relativeMode->setValueNotifyingHost(stream.readBool());
	is1dbPeakScale->setValueNotifyingHost(stream.readBool());
	*dimLevel = stream.readFloat();
	*refLevel = stream.readFloat();
	*peakHoldSeconds = stream.readFloat();
	volModMode->setValueNotifyingHost(stream.readBool());
	useRMEVolControl->setValueNotifyingHost(stream.readBool());
	useReaperOsc->setValueNotifyingHost(stream.readBool());
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new DreamControlAudioProcessor();
}
