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
*/

#pragma once

#include <vector>
#include <array>
#include <map>

#include "JuceLibraryCode/JuceHeader.h"
#include "AudioParameterBoolNotify.h"
#include "LufsProcessor.h"
#include "CrossoverFilter.h"

//==============================================================================
/**
*/
class DreamControlAudioProcessor :  public AudioProcessor,
									public HighResolutionTimer,
									public MidiInputCallback
{
public:
	//==============================================================================
	std::map<int, AudioParameterBoolNotify*> button_param_map;

    //==============================================================================
    DreamControlAudioProcessor();
    ~DreamControlAudioProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;
	void hiResTimerCallback() override;
	bool isAnyBandSolo();
	void handleIncomingMidiMessage (MidiInput* source, const MidiMessage& m) override;

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
	//==============================================================================
	// Main
	MidiOutput* midiOutput;
	MidiInput* midiInput;

    //==============================================================================
	// Level
	AudioParameterFloat* monitorLevel;
	AudioParameterFloat* dimLevel;
	AudioParameterFloat* refLevel;

	AudioParameterBoolNotify* muteMode;
	AudioParameterBoolNotify* dimMode;
	AudioParameterBoolNotify* refMode;

	//==============================================================================
	// Meters
	LufsProcessor* lufsProcessor;

	AudioParameterFloat* lufsShort;
	AudioParameterFloat* lufsMomentary;
	AudioParameterFloat* lufsIntegrated;
	AudioParameterFloat* lufsTarget;
	AudioParameterFloat* lufsRangeMin;
	AudioParameterFloat* lufsRangeMax;
	AudioParameterBoolNotify* lufsReset;

	AudioParameterFloat* peakHoldSeconds;
	int msSinceLastPeakReset;
	float lastMaxLeft;
	float lastMaxRight;

	AudioParameterFloat* peakMeterLeft;
	AudioParameterFloat* peakMeterRight;
	AudioParameterFloat* peakMeterMaxLeft;
	AudioParameterFloat* peakMeterMaxRight;
	AudioParameterBool* clipMeterLeft;
	AudioParameterBool* clipMeterRight;

	AudioParameterBoolNotify* lufsMode;
	AudioParameterBoolNotify* peakWithMomentaryMode;
	AudioParameterBoolNotify* relativeMode;
	AudioParameterBoolNotify* is1dbPeakScale;

	//==============================================================================
	// Band solo
	void updateFilters(float sampleRate);

	int numChannels;
	int numCrossovers;
	int numBands;
	bool aSoloButtonJustEngaged;
	std::vector<std::vector<std::unique_ptr<CrossoverFilter>>> crossoverFilters;
	std::vector<AudioParameterFloat*> crossoverFreq;
	std::vector<AudioParameterBoolNotify*> bandSolo;

	//==============================================================================
	// M/S solo, loudness EQ
	AudioParameterBoolNotify* midSolo;
	AudioParameterBoolNotify* sideSolo;
	AudioParameterBoolNotify* loudnessMode;
	std::vector<std::vector<std::unique_ptr<IIRFilter>>> loudnessEqFilters;

	//==============================================================================
	// For development use only
	AudioParameterBoolNotify* volModMode;

	//==============================================================================
	char* getMeterIntegralFractional(float val);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DreamControlAudioProcessor)
};
