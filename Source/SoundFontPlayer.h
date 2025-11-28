#pragma once

#include <JuceHeader.h>
#include <map>
#include <memory>
#include <vector>

// Forward declaration for tinysoundfont
struct tsf;

//==============================================================================
/**
 * SoundFontPlayer - Handles loading and playing soundfonts with custom tuning support
 * Uses tinysoundfont library for SF2 file parsing and rendering
 */
class SoundFontPlayer
{
public:
	//==============================================================================
	SoundFontPlayer();
	~SoundFontPlayer();

	//==============================================================================
	// Initialization
	void prepareToPlay(double sampleRate, int samplesPerBlock);
	void releaseResources();

	//==============================================================================
	// Soundfont loading
	bool loadSoundFont(const juce::File& file);
	bool loadSoundFont(const void* data, int sizeInBytes);
	void unloadSoundFont();
	bool isSoundFontLoaded() const { return soundFont != nullptr; }

	// Get info about the loaded soundfont
	juce::String getSoundFontName() const { return soundFontName; }
	juce::File getSoundFontFile() const { return soundFontFile; }

	//==============================================================================
	// Preset management
	int getPresetCount() const;
	juce::String getPresetName(int presetIndex) const;
	void setPreset(int presetIndex);
	int getCurrentPreset() const { return currentPreset; }

	// Bank selection (for soundfonts with multiple banks)
	void setBank(int bank);
	int getCurrentBank() const { return currentBank; }

	//==============================================================================
	// MIDI note handling
	void noteOn(int midiChannel, int midiNote, float velocity);
	void noteOff(int midiChannel, int midiNote);
	void allNotesOff();

	//==============================================================================
	// Custom tuning support for just intonation
	void setNoteFrequency(int midiNote, double frequencyHz);
	void updateFrequencyMapping(const std::map<int, double>& midiNoteToFreqMap);
	void clearCustomTuning();

	//==============================================================================
	// Audio rendering
	void renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
	void renderNextBlock(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midiMessages,
						 int startSample, int numSamples);

	//==============================================================================
	// Global parameters
	void setGlobalGain(float gainLinear);
	float getGlobalGain() const { return globalGain; }

	void setMaxPolyphony(int maxVoices);
	int getMaxPolyphony() const { return maxPolyphony; }

private:
	//==============================================================================
	// tinysoundfont instance
	tsf* soundFont = nullptr;

	// Current state
	juce::String soundFontName;
	juce::File soundFontFile;
	int currentPreset = 0;
	int currentBank = 0;
	double sampleRate = 44100.0;
	int blockSize = 512;
	float globalGain = 1.0f;
	int maxPolyphony = 64;

	// Custom frequency mapping for just intonation
	std::map<int, double> noteFrequencyMap;

	// Track active notes for retuning
	struct ActiveNote
	{
		int midiChannel;
		int midiNote;
		double targetFrequency;
		bool needsRetune;
	};
	std::vector<ActiveNote> activeNotes;

	// Helper to calculate pitch bend for custom frequency
	float calculatePitchBendForFrequency(int midiNote, double targetFrequency);

	// Standard 12-TET frequency calculation
	double getMidiNoteFrequency(int midiNote) const;

	// Critical section for thread safety
	juce::CriticalSection lock;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundFontPlayer)
};