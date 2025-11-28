#include "Synthesizer.h"

//==============================================================================
FluidJustIntonationSynth::FluidJustIntonationSynth()
{
	// Create and add a sound for sine wave mode
	addSound(new FluidJustSound());

	// Create some voices for sine wave mode
	for (int i = 0; i < 16; ++i) // 16 voices for polyphony
		addVoice(new FluidJustVoice());

	// Create the soundfont player
	soundFontPlayer = std::make_unique<SoundFontPlayer>();
}

FluidJustIntonationSynth::~FluidJustIntonationSynth()
{
}

void FluidJustIntonationSynth::setup(double sampleRate, int blockSize)
{
	currentSampleRate = sampleRate;
	currentBlockSize = blockSize;
	
	// Setup sine wave synth
	setCurrentPlaybackSampleRate(sampleRate);
	
	// Setup soundfont player
	if (soundFontPlayer)
	{
		soundFontPlayer->prepareToPlay(sampleRate, blockSize);
		soundFontPlayer->setGlobalGain(globalGain);
	}
}

double FluidJustIntonationSynth::getNoteFrequency(int midiNoteNumber)
{
	auto it = noteToFrequencyMap.find(midiNoteNumber);
	if (it != noteToFrequencyMap.end())
		return it->second;

	// Default to standard 12-TET tuning if no custom frequency is defined
	return juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
}

void FluidJustIntonationSynth::updateFrequencyMapping(const std::map<int, double>& midiNoteToFreqMap)
{
	noteToFrequencyMap = midiNoteToFreqMap;
	
	// Update based on current mode
	if (currentMode == SynthMode::SineWave)
	{
		updatePlayingVoices();
	}
	else if (currentMode == SynthMode::SoundFont && soundFontPlayer)
	{
		soundFontPlayer->updateFrequencyMapping(midiNoteToFreqMap);
	}
}

void FluidJustIntonationSynth::updatePlayingVoices()
{
	// Update all currently playing sine wave voices to the new tuning
	for (int i = 0; i < getNumVoices(); ++i)
	{
		if (auto* voice = dynamic_cast<FluidJustVoice*>(getVoice(i)))
		{
			if (voice->isVoiceActive())
			{
				// Get the current MIDI note this voice is playing
				int midiNote = voice->getCurrentlyPlayingNote();
				if (midiNote >= 0)
				{
					// Update to the new frequency
					double freq = getNoteFrequency(midiNote);
					voice->setCustomFrequency(freq);
				}
			}
		}
	}
}

//==============================================================================
void FluidJustIntonationSynth::setSynthMode(SynthMode mode)
{
	if (currentMode != mode)
	{
		// Stop all notes when switching modes
		allNotesOff(0, false); // midiChannel 0, don't allow tail off
		
		if (soundFontPlayer)
		{
			soundFontPlayer->allNotesOff();
		}
		
		currentMode = mode;
		
		DBG("FluidJustIntonationSynth: Switched to " + 
			juce::String(mode == SynthMode::SineWave ? "Sine Wave" : "SoundFont") + " mode");
	}
}

//==============================================================================
bool FluidJustIntonationSynth::loadSoundFont(const juce::File& file)
{
	if (soundFontPlayer)
	{
		bool success = soundFontPlayer->loadSoundFont(file);
		
		if (success)
		{
			// Automatically switch to soundfont mode when loading
			setSynthMode(SynthMode::SoundFont);
			
			// Apply current frequency mapping
			soundFontPlayer->updateFrequencyMapping(noteToFrequencyMap);
		}
		
		return success;
	}
	
	return false;
}

void FluidJustIntonationSynth::unloadSoundFont()
{
	if (soundFontPlayer)
	{
		soundFontPlayer->unloadSoundFont();
		
		// Switch back to sine wave mode
		setSynthMode(SynthMode::SineWave);
	}
}

bool FluidJustIntonationSynth::isSoundFontLoaded() const
{
	return soundFontPlayer && soundFontPlayer->isSoundFontLoaded();
}

juce::String FluidJustIntonationSynth::getSoundFontName() const
{
	if (soundFontPlayer)
		return soundFontPlayer->getSoundFontName();
	
	return juce::String();
}

juce::File FluidJustIntonationSynth::getSoundFontFile() const
{
	if (soundFontPlayer)
		return soundFontPlayer->getSoundFontFile();
	
	return juce::File();
}

int FluidJustIntonationSynth::getPresetCount() const
{
	if (soundFontPlayer)
		return soundFontPlayer->getPresetCount();
	
	return 0;
}

juce::String FluidJustIntonationSynth::getPresetName(int presetIndex) const
{
	if (soundFontPlayer)
		return soundFontPlayer->getPresetName(presetIndex);
	
	return juce::String();
}

void FluidJustIntonationSynth::setPreset(int presetIndex)
{
	if (soundFontPlayer)
		soundFontPlayer->setPreset(presetIndex);
}

int FluidJustIntonationSynth::getCurrentPreset() const
{
	if (soundFontPlayer)
		return soundFontPlayer->getCurrentPreset();
	
	return 0;
}

//==============================================================================
void FluidJustIntonationSynth::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
											   const juce::MidiBuffer& midiData,
											   int startSample, int numSamples)
{
	if (currentMode == SynthMode::SineWave)
	{
		// Use the standard JUCE Synthesiser rendering for sine wave mode
		juce::Synthesiser::renderNextBlock(outputBuffer, midiData, startSample, numSamples);
	}
	else if (currentMode == SynthMode::SoundFont && soundFontPlayer)
	{
		// Use the soundfont player for rendering
		soundFontPlayer->renderNextBlock(outputBuffer, midiData, startSample, numSamples);
	}
}

//==============================================================================
void FluidJustIntonationSynth::setGlobalGain(float gainLinear)
{
	globalGain = gainLinear;
	
	if (soundFontPlayer)
	{
		soundFontPlayer->setGlobalGain(gainLinear);
	}
}

//==============================================================================
// FluidJustVoice implementation

FluidJustIntonationSynth::FluidJustVoice::FluidJustVoice()
{
}

bool FluidJustIntonationSynth::FluidJustVoice::canPlaySound(juce::SynthesiserSound* sound)
{
	return dynamic_cast<FluidJustSound*>(sound) != nullptr;
}

void FluidJustIntonationSynth::FluidJustVoice::startNote(int midiNoteNumber, float velocity,
														juce::SynthesiserSound*, int /*currentPitchWheelPosition*/)
{
	// This will be overridden by setCustomFrequency if tuning is active
	frequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
	
	// Use custom frequency if set
	if (customFrequency > 0.0)
		frequency = customFrequency;
	
	level = velocity * 0.15;
	
	// Reset phase to avoid clicks
	phase = 0.0;
	
	// Start attack phase
	env = 0.0;
	isAttacking = true;
	isReleasing = false;
	tailOff = 0.0;
}

void FluidJustIntonationSynth::FluidJustVoice::stopNote(float /*velocity*/, bool allowTailOff)
{
	if (allowTailOff)
	{
		// Start release phase
		isReleasing = true;
		isAttacking = false;
	}
	else
	{
		// Note ended abruptly
		clearCurrentNote();
		level = 0.0;
	}
}

void FluidJustIntonationSynth::FluidJustVoice::pitchWheelMoved(int /*newValue*/)
{
	// Not implemented for this basic synth
}

void FluidJustIntonationSynth::FluidJustVoice::controllerMoved(int /*controllerNumber*/, int /*newValue*/)
{
	// Not implemented for this basic synth
}

void FluidJustIntonationSynth::FluidJustVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, 
															  int startSample, int numSamples)
{
	if (level <= 0.0)
		return;

	const double cyclesPerSample = frequency / getSampleRate();
	
	while (--numSamples >= 0)
	{
		// Simple envelope processing
		if (isAttacking)
		{
			env += attackRate;
			if (env >= 1.0)
			{
				env = 1.0;
				isAttacking = false;
			}
		}
		else if (isReleasing)
		{
			env -= releaseRate;
			if (env <= 0.0)
			{
				env = 0.0;
				isReleasing = false;
				clearCurrentNote();
				break;
			}
		}
		
		// Generate sine wave
		const float currentSample = static_cast<float>(std::sin(phase * 2.0 * juce::MathConstants<double>::pi) * level * env);
		
		// Add to output
		for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
			outputBuffer.addSample(i, startSample, currentSample);
		
		// Update phase
		phase += cyclesPerSample;
		if (phase >= 1.0)
			phase -= 1.0;
		
		++startSample;
	}
}

void FluidJustIntonationSynth::FluidJustVoice::setCustomFrequency(double freqHz)
{
	customFrequency = freqHz;
	
	// If note is already playing, update its frequency
	if (isVoiceActive())
		frequency = freqHz;
}