#pragma once

#include <JuceHeader.h>
#include "SoundFontPlayer.h"

//==============================================================================
/**
 * A synthesizer that supports both simple sine wave synthesis and soundfont playback
 * with custom tuning for just intonation
 */
class FluidJustIntonationSynth : public juce::Synthesiser
{
public:
	//==============================================================================
	enum class SynthMode
	{
		SineWave,       // Original simple sine wave mode
		SoundFont       // SoundFont-based synthesis
	};

	//==============================================================================
	FluidJustIntonationSynth();
	~FluidJustIntonationSynth() override;

	// Set up basic parameters
	void setup(double sampleRate, int blockSize);

	// Update the frequency mapping for MIDI notes (just intonation)
	void updateFrequencyMapping(const std::map<int, double>& midiNoteToFreqMap);

	//==============================================================================
	// Synthesis mode
	void setSynthMode(SynthMode mode);
	SynthMode getSynthMode() const { return currentMode; }

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

	//==============================================================================
	// Audio rendering - override to handle soundfont mode
	void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
						 const juce::MidiBuffer& midiData,
						 int startSample, int numSamples);

	//==============================================================================
	// Volume control
	void setGlobalGain(float gainLinear);
	float getGlobalGain() const { return globalGain; }

private:
	//==============================================================================
	// A simple sine wave voice (original implementation)
	class FluidJustVoice : public juce::SynthesiserVoice
	{
	public:
		FluidJustVoice();

		bool canPlaySound(juce::SynthesiserSound*) override;

		void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override;
		void stopNote(float velocity, bool allowTailOff) override;

		void pitchWheelMoved(int) override;
		void controllerMoved(int, int) override;

		void renderNextBlock(juce::AudioBuffer<float>&, int startSample, int numSamples) override;

		// Set a custom frequency for this voice
		void setCustomFrequency(double freqHz);

	private:
		double level = 0.0;
		double frequency = 440.0;
		double customFrequency = 0.0;
		double phase = 0.0;
		double tailOff = 0.0;

		// Simple attack/release envelope
		double attackRate = 0.1;
		double releaseRate = 0.1;
		double env = 0.0;
		bool isAttacking = false;
		bool isReleasing = false;
	};

	//==============================================================================
	// A simple sine wave sound (original implementation)
	class FluidJustSound : public juce::SynthesiserSound
	{
	public:
		FluidJustSound() {}

		bool appliesToNote(int) override { return true; }
		bool appliesToChannel(int) override { return true; }
	};

	//==============================================================================
	// Current synthesis mode
	SynthMode currentMode = SynthMode::SineWave;

	// SoundFont player instance
	std::unique_ptr<SoundFontPlayer> soundFontPlayer;

	// Map from MIDI note number to actual frequency
	std::map<int, double> noteToFrequencyMap;

	// Global gain
	float globalGain = 1.0f;

	// Sample rate for soundfont player
	double currentSampleRate = 44100.0;
	int currentBlockSize = 512;

	// Helper function to get the frequency for a MIDI note
	double getNoteFrequency(int midiNoteNumber);

	// Update all currently playing voices to the new tuning
	void updatePlayingVoices();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FluidJustIntonationSynth)
};