#pragma once

#include <JuceHeader.h>
#include <vector>
#include <array>
#include "Synthesizer.h"
#include "JucePluginDefines.h"

//==============================================================================
/**
 * FluidJustIntonationProcessor - Main audio processor for the Fluid Just Intonation VST
 */
class FluidJustIntonationProcessor  : public juce::AudioProcessor,
									 public juce::AudioProcessorValueTreeState::Listener
{
public:
	//==============================================================================
	FluidJustIntonationProcessor();
	~FluidJustIntonationProcessor() override;

	//==============================================================================
	void prepareToPlay (double sampleRate, int samplesPerBlock) override;
	void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
	bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
#endif

	void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

	//==============================================================================
	juce::AudioProcessorEditor* createEditor() override;
	bool hasEditor() const override;

	//==============================================================================
	const juce::String getName() const override;

	bool acceptsMidi() const override;
	bool producesMidi() const override;
	bool isMidiEffect() const override;
	bool isSynth() const; // New method to explicitly identify as a synth
	double getTailLengthSeconds() const override;

	//==============================================================================
	int getNumPrograms() override;
	int getCurrentProgram() override;
	void setCurrentProgram (int index) override;
	const juce::String getProgramName (int index) override;
	void changeProgramName (int index, const juce::String& newName) override;

	//==============================================================================
	void getStateInformation (juce::MemoryBlock& destData) override;
	void setStateInformation (const void* data, int sizeInBytes) override;

	//==============================================================================
	// Called when a parameter changes
	void parameterChanged(const juce::String& parameterID, float newValue) override;

	//==============================================================================
	// Just Intonation Parameters
	enum class IntonationMode {
		Set,    // Each new scale is based on the initial scale
		Shift   // Each new scale is based on the previous scale
	};

	// Set number of measures in sequence (4, 8, or 16)
	void setSequenceLength(int length);
	int getSequenceLength() const;

	// Set the mode (Set or Shift)
	void setIntonationMode(IntonationMode mode);
	IntonationMode getIntonationMode() const;

	// Set root note for a specific measure in the sequence
	void setMeasureRoot(int measureIndex, int rootNote);
	int getMeasureRoot(int measureIndex) const;
	
	int getCurrentMeasure() const 
	{
		return currentMeasure;
	}
	
	// Reset accumulated drift (for Shift mode)
	void resetAccumulatedDrift();
	
	// Get the frequency for a specific MIDI note based on current tuning
	double getFrequencyForNote(int midiNote) const;

	//==============================================================================
	// SoundFont support
	bool loadSoundFont(const juce::File& file);
	void unloadSoundFont();
	bool isSoundFontLoaded() const;
	juce::String getSoundFontName() const;
	juce::File getSoundFontFile() const;

	// Preset management
	int getPresetCount() const;
	juce::String getPresetName(int presetIndex) const;
	void setPreset(int presetIndex);
	int getCurrentPreset() const;

	// Synthesis mode
	void setSynthMode(FluidJustIntonationSynth::SynthMode mode);
	FluidJustIntonationSynth::SynthMode getSynthMode() const;

	// Gain control
	void setGlobalGain(float gainLinear);
	float getGlobalGain() const;
	
	// Parameter tree for automation and state saving
	juce::AudioProcessorValueTreeState parameters;

private:
	//==============================================================================
	// Constants
	static constexpr double CONCERT_A_FREQ = 440.0;  // A4 reference frequency
	static constexpr int MAX_SEQUENCE_LENGTH = 16;   // Maximum sequence length
	
	// Just Intonation frequency calculation
	double getJustFrequency(int midiNote, int rootNote, bool useCurrentRootAsReference = false);
	
	// Calculate the frequency ratios for just intonation
	std::array<double, 12> calculateJustRatios(int rootNote);
	
	// Helper: Calculate frequency of a note in a JI scale built from a given root
	double calculateFrequencyInScale(int noteToPlay, int scaleRoot, double scaleRootFreq);
	
	// Get the root frequency for the current measure
	double getCurrentMeasureRootFrequency();
	
	// Get the root frequency for a specific measure (used for recursion in SHIFT mode)
	double getCurrentMeasureRootFrequencyForMeasure(int measureIndex);
	
	// Current state
	int sequenceLength = 4;                 // Default to 4 measures
	IntonationMode intonationMode = IntonationMode::Set;
	std::array<int, MAX_SEQUENCE_LENGTH> measureRoots;  // Root note for each measure (MIDI note numbers)
	
	// Current playback state
	int currentMeasure = 0;
	int previousMeasure = -1;
	double ppqPosition = 0.0;
	double bpm = 120.0;
	bool wasPlaying = false;  // Track playback state to detect stop
	
	// Accumulated drift for Shift mode looping
	double accumulatedDriftFrequency = 0.0;
	bool hasAccumulatedDrift = false;
	
	// Update the current measure based on the playback position
	void updateCurrentMeasure(juce::AudioPlayHead* playHead);
	
	// Map from MIDI note number to frequency based on current tuning
	double midiNoteToFrequency(int midiNote);
	
	// Convert a note name (C, C#, D, etc.) to MIDI note number (with C4 = 60)
	int noteNameToMidiNumber(const juce::String& noteName);
	
	// Track active notes with their original MIDI note numbers and current frequencies
	struct ActiveNote {
		int midiNote;
		double frequency;
	};
	std::map<int, ActiveNote> activeNotes;  // Map from note ID to active note info
	
	// Synthesizer for audio output
	FluidJustIntonationSynth synth;
	
	// Map from MIDI note to frequency for the synth
	std::map<int, double> currentFrequencyMap;

	// SoundFont file path for state saving
	juce::String soundFontPath;
	int savedPresetIndex = 0;

	//==============================================================================
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FluidJustIntonationProcessor)
	void updateFrequencyMap();
};