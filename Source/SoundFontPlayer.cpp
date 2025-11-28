// Define this before including tinysoundfont to get the implementation
#define TSF_IMPLEMENTATION
#include "SoundFontPlayer.h"
#include "tsf.h"  // tinysoundfont header - download from https://github.com/schellingb/TinySoundFont

//==============================================================================
SoundFontPlayer::SoundFontPlayer()
{
}

SoundFontPlayer::~SoundFontPlayer()
{
	unloadSoundFont();
}

//==============================================================================
void SoundFontPlayer::prepareToPlay(double newSampleRate, int newBlockSize)
{
	juce::ScopedLock sl(lock);
	
	sampleRate = newSampleRate;
	blockSize = newBlockSize;
	
	if (soundFont != nullptr)
	{
		tsf_set_output(soundFont, TSF_STEREO_INTERLEAVED, static_cast<int>(sampleRate), globalGain);
	}
}

void SoundFontPlayer::releaseResources()
{
	juce::ScopedLock sl(lock);
	allNotesOff();
}

//==============================================================================
bool SoundFontPlayer::loadSoundFont(const juce::File& file)
{
	juce::ScopedLock sl(lock);
	
	// Unload any existing soundfont
	unloadSoundFont();
	
	if (!file.existsAsFile())
	{
		DBG("SoundFontPlayer: File does not exist: " + file.getFullPathName());
		return false;
	}
	
	// Load the soundfont
	soundFont = tsf_load_filename(file.getFullPathName().toRawUTF8());
	
	if (soundFont == nullptr)
	{
		DBG("SoundFontPlayer: Failed to load soundfont: " + file.getFullPathName());
		return false;
	}
	
	// Configure the soundfont
	tsf_set_output(soundFont, TSF_STEREO_INTERLEAVED, static_cast<int>(sampleRate), globalGain);
	tsf_set_max_voices(soundFont, maxPolyphony);
	
	// Store file info
	soundFontFile = file;
	soundFontName = file.getFileNameWithoutExtension();
	
	// Select the first preset by default
	currentPreset = 0;
	currentBank = 0;
	
	DBG("SoundFontPlayer: Loaded soundfont: " + soundFontName + " with " + 
		juce::String(getPresetCount()) + " presets");
	
	return true;
}

bool SoundFontPlayer::loadSoundFont(const void* data, int sizeInBytes)
{
	juce::ScopedLock sl(lock);
	
	// Unload any existing soundfont
	unloadSoundFont();
	
	// Load from memory
	soundFont = tsf_load_memory(data, sizeInBytes);
	
	if (soundFont == nullptr)
	{
		DBG("SoundFontPlayer: Failed to load soundfont from memory");
		return false;
	}
	
	// Configure the soundfont
	tsf_set_output(soundFont, TSF_STEREO_INTERLEAVED, static_cast<int>(sampleRate), globalGain);
	tsf_set_max_voices(soundFont, maxPolyphony);
	
	soundFontName = "Memory SoundFont";
	soundFontFile = juce::File();
	
	// Select the first preset by default
	currentPreset = 0;
	currentBank = 0;
	
	return true;
}

void SoundFontPlayer::unloadSoundFont()
{
	juce::ScopedLock sl(lock);
	
	if (soundFont != nullptr)
	{
		allNotesOff();
		tsf_close(soundFont);
		soundFont = nullptr;
	}
	
	soundFontName.clear();
	soundFontFile = juce::File();
	activeNotes.clear();
}

//==============================================================================
int SoundFontPlayer::getPresetCount() const
{
	if (soundFont == nullptr)
		return 0;
	
	return tsf_get_presetcount(soundFont);
}

juce::String SoundFontPlayer::getPresetName(int presetIndex) const
{
	if (soundFont == nullptr || presetIndex < 0 || presetIndex >= getPresetCount())
		return juce::String();
	
	const char* name = tsf_get_presetname(soundFont, presetIndex);
	return name ? juce::String(name) : juce::String("Preset " + juce::String(presetIndex));
}

void SoundFontPlayer::setPreset(int presetIndex)
{
	juce::ScopedLock sl(lock);
	
	if (presetIndex >= 0 && presetIndex < getPresetCount())
	{
		currentPreset = presetIndex;
		DBG("SoundFontPlayer: Selected preset " + juce::String(presetIndex) + 
			": " + getPresetName(presetIndex));
	}
}

void SoundFontPlayer::setBank(int bank)
{
	juce::ScopedLock sl(lock);
	currentBank = bank;
}

//==============================================================================
void SoundFontPlayer::noteOn(int midiChannel, int midiNote, float velocity)
{
	juce::ScopedLock sl(lock);
	
	if (soundFont == nullptr)
		return;
	
	// Calculate the target frequency (use custom tuning if available)
	double targetFreq = getMidiNoteFrequency(midiNote);
	auto it = noteFrequencyMap.find(midiNote);
	if (it != noteFrequencyMap.end())
	{
		targetFreq = it->second;
	}
	
	// Calculate pitch bend needed for this frequency
	float pitchBend = calculatePitchBendForFrequency(midiNote, targetFreq);
	
	// Apply pitch wheel (tinysoundfont uses -1 to 1 range for pitch bend)
	// We need to set this per-channel, but for simplicity we'll use channel 0
	tsf_channel_set_pitchwheel(soundFont, midiChannel, static_cast<int>((pitchBend + 1.0f) * 8192.0f));
	
	// Start the note using preset selection
	tsf_channel_set_presetindex(soundFont, midiChannel, currentPreset);
	tsf_channel_note_on(soundFont, midiChannel, midiNote, velocity);
	
	// Track the active note
	ActiveNote note;
	note.midiChannel = midiChannel;
	note.midiNote = midiNote;
	note.targetFrequency = targetFreq;
	note.needsRetune = false;
	activeNotes.push_back(note);
}

void SoundFontPlayer::noteOff(int midiChannel, int midiNote)
{
	juce::ScopedLock sl(lock);
	
	if (soundFont == nullptr)
		return;
	
	tsf_channel_note_off(soundFont, midiChannel, midiNote);
	
	// Remove from active notes
	activeNotes.erase(
		std::remove_if(activeNotes.begin(), activeNotes.end(),
			[midiChannel, midiNote](const ActiveNote& note) {
				return note.midiChannel == midiChannel && note.midiNote == midiNote;
			}),
		activeNotes.end()
	);
}

void SoundFontPlayer::allNotesOff()
{
	juce::ScopedLock sl(lock);
	
	if (soundFont != nullptr)
	{
		tsf_reset(soundFont);
	}
	
	activeNotes.clear();
}

//==============================================================================
void SoundFontPlayer::setNoteFrequency(int midiNote, double frequencyHz)
{
	juce::ScopedLock sl(lock);
	noteFrequencyMap[midiNote] = frequencyHz;
}

void SoundFontPlayer::updateFrequencyMapping(const std::map<int, double>& midiNoteToFreqMap)
{
	juce::ScopedLock sl(lock);
	noteFrequencyMap = midiNoteToFreqMap;
	
	// Mark all active notes for retuning
	for (auto& note : activeNotes)
	{
		auto it = noteFrequencyMap.find(note.midiNote);
		if (it != noteFrequencyMap.end())
		{
			if (std::abs(note.targetFrequency - it->second) > 0.01)
			{
				note.targetFrequency = it->second;
				note.needsRetune = true;
			}
		}
	}
	
	// Apply retuning to active notes
	if (soundFont != nullptr)
	{
		for (auto& note : activeNotes)
		{
			if (note.needsRetune)
			{
				float pitchBend = calculatePitchBendForFrequency(note.midiNote, note.targetFrequency);
				tsf_channel_set_pitchwheel(soundFont, note.midiChannel, 
					static_cast<int>((pitchBend + 1.0f) * 8192.0f));
				note.needsRetune = false;
			}
		}
	}
}

void SoundFontPlayer::clearCustomTuning()
{
	juce::ScopedLock sl(lock);
	noteFrequencyMap.clear();
}

//==============================================================================
void SoundFontPlayer::renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
	juce::ScopedLock sl(lock);
	
	if (soundFont == nullptr || numSamples <= 0)
		return;
	
	// Create interleaved buffer for tinysoundfont
	std::vector<float> interleavedBuffer(numSamples * 2);
	
	// Render audio
	tsf_render_float(soundFont, interleavedBuffer.data(), numSamples, 0);
	
	// Deinterleave to JUCE buffer
	float* leftChannel = buffer.getWritePointer(0, startSample);
	float* rightChannel = buffer.getNumChannels() > 1 ? 
						  buffer.getWritePointer(1, startSample) : nullptr;
	
	for (int i = 0; i < numSamples; ++i)
	{
		leftChannel[i] += interleavedBuffer[i * 2];
		if (rightChannel != nullptr)
			rightChannel[i] += interleavedBuffer[i * 2 + 1];
	}
}

void SoundFontPlayer::renderNextBlock(juce::AudioBuffer<float>& buffer, 
									  const juce::MidiBuffer& midiMessages,
									  int startSample, int numSamples)
{
	// Process MIDI messages
	for (const auto metadata : midiMessages)
	{
		const auto msg = metadata.getMessage();
		const int samplePosition = metadata.samplePosition;
		
		// Render audio up to this MIDI event
		if (samplePosition > startSample)
		{
			renderNextBlock(buffer, startSample, samplePosition - startSample);
			numSamples -= (samplePosition - startSample);
			startSample = samplePosition;
		}
		
		// Process the MIDI message
		if (msg.isNoteOn())
		{
			noteOn(msg.getChannel() - 1, msg.getNoteNumber(), msg.getFloatVelocity());
		}
		else if (msg.isNoteOff())
		{
			noteOff(msg.getChannel() - 1, msg.getNoteNumber());
		}
		else if (msg.isAllNotesOff() || msg.isAllSoundOff())
		{
			allNotesOff();
		}
		else if (msg.isController())
		{
			// Handle CC messages if needed
			if (soundFont != nullptr)
			{
				tsf_channel_midi_control(soundFont, msg.getChannel() - 1,
					msg.getControllerNumber(), msg.getControllerValue());
			}
		}
	}
	
	// Render remaining audio
	if (numSamples > 0)
	{
		renderNextBlock(buffer, startSample, numSamples);
	}
}

//==============================================================================
void SoundFontPlayer::setGlobalGain(float gainLinear)
{
	juce::ScopedLock sl(lock);
	
	globalGain = gainLinear;
	
	if (soundFont != nullptr)
	{
		tsf_set_output(soundFont, TSF_STEREO_INTERLEAVED, static_cast<int>(sampleRate), globalGain);
	}
}

void SoundFontPlayer::setMaxPolyphony(int maxVoices)
{
	juce::ScopedLock sl(lock);
	
	maxPolyphony = maxVoices;
	
	if (soundFont != nullptr)
	{
		tsf_set_max_voices(soundFont, maxPolyphony);
	}
}

//==============================================================================
float SoundFontPlayer::calculatePitchBendForFrequency(int midiNote, double targetFrequency)
{
	// Calculate the standard frequency for this MIDI note
	double standardFreq = getMidiNoteFrequency(midiNote);
	
	// Calculate the pitch bend in semitones
	double semitones = 12.0 * std::log2(targetFrequency / standardFreq);
	
	// Convert to pitch bend range (-1 to 1, where 1 = +2 semitones by default)
	// tinysoundfont default pitch bend range is 2 semitones
	float pitchBend = static_cast<float>(semitones / 2.0);
	
	// Clamp to valid range
	return juce::jlimit(-1.0f, 1.0f, pitchBend);
}

double SoundFontPlayer::getMidiNoteFrequency(int midiNote) const
{
	// Standard 12-TET calculation: A4 (MIDI note 69) = 440 Hz
	return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
}