#include "PluginProcessor.h"
#include "PluginEditor.h"

#ifndef JucePlugin_Name
#define JucePlugin_Name "Flooid"
#endif

//==============================================================================
FluidJustIntonationProcessor::FluidJustIntonationProcessor()
	: AudioProcessor (BusesProperties()
					#if ! JucePlugin_IsMidiEffect
					#if ! JucePlugin_IsSynth
						.withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
					#endif
						.withOutput ("Output", juce::AudioChannelSet::stereo(), true)
					#endif
					),
	parameters (*this, nullptr, "FluidJustIntonation",
		{
			std::make_unique<juce::AudioParameterChoice> ("sequenceLength", "Sequence Length", 
														  juce::StringArray {"4", "8", "12", "16"}, 0),
			std::make_unique<juce::AudioParameterChoice> ("intonationMode", "Intonation Mode", 
														  juce::StringArray {"Set", "Shift"}, 0)
		})
{

	// Initialize measure roots to C (MIDI note 60)
	for (int i = 0; i < MAX_SEQUENCE_LENGTH; i++) {
		measureRoots[i] = 60; // Middle C
	}
	
	// Add parameters for each possible measure root (up to MAX_SEQUENCE_LENGTH)
	for (int i = 0; i < MAX_SEQUENCE_LENGTH; i++) {
		juce::String paramID = "measureRoot" + juce::String(i);
		juce::String paramName = "Measure " + juce::String(i + 1) + " Root";
		
		// Create and add the parameter to the tree
		parameters.createAndAddParameter(
			std::make_unique<juce::AudioParameterChoice>(
				paramID, paramName, 
				juce::StringArray {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"},
				0
			)
		);
		
		// Now add the listener
		parameters.addParameterListener(paramID, this);
	}
	
	// Add listeners for sequence length and mode
	parameters.addParameterListener("sequenceLength", this);
	parameters.addParameterListener("intonationMode", this);
	
}

FluidJustIntonationProcessor::~FluidJustIntonationProcessor()
{
}
//==============================================================================
const juce::String FluidJustIntonationProcessor::getName() const
{
	return JucePlugin_Name;
}

bool FluidJustIntonationProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
	return true;
   #else
	return false;
   #endif
}

bool FluidJustIntonationProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
	return true;
   #else
	return false;
   #endif
}

bool FluidJustIntonationProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
	return true;
   #else
	return false;
   #endif
}

// Explicitly state we're a synth for hosts that check this directly
bool FluidJustIntonationProcessor::isSynth() const
{
	return true;
}

double FluidJustIntonationProcessor::getTailLengthSeconds() const
{
	return 0.0;
}

int FluidJustIntonationProcessor::getNumPrograms()
{
	return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
				// so this should be at least 1, even if you're not really implementing programs.
}

int FluidJustIntonationProcessor::getCurrentProgram()
{
	return 0;
}

void FluidJustIntonationProcessor::setCurrentProgram (int index)
{
	juce::ignoreUnused(index);
}

const juce::String FluidJustIntonationProcessor::getProgramName (int index)
{
	juce::ignoreUnused(index);
	return {};
}

void FluidJustIntonationProcessor::changeProgramName (int index, const juce::String& newName)
{
	juce::ignoreUnused(index, newName);
}

//==============================================================================
void FluidJustIntonationProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{

	// Initialize the synthesizer
	synth.setup(sampleRate, samplesPerBlock);
	
	// Initialize with the current tuning
	updateCurrentMeasure(getPlayHead());
	updateFrequencyMap();
	
}

void FluidJustIntonationProcessor::releaseResources()
{
	// Release any allocated resources
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FluidJustIntonationProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
	juce::ignoreUnused (layouts);
	return true;
  #else
	// Support mono or stereo output
	if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
	 && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
		return false;

   #if ! JucePlugin_IsSynth
	// Input layout must match output layout if not a synth
	if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
		return false;
   #endif

	return true;
  #endif
}
#endif

void FluidJustIntonationProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	static bool firstProcessBlock = true;
	if (firstProcessBlock) {
		firstProcessBlock = false;
	}

	juce::ScopedNoDenormals noDenormals;
	auto totalNumInputChannels  = getTotalNumInputChannels();
	auto totalNumOutputChannels = getTotalNumOutputChannels();

	// Clear output if we have more output channels than input channels
	for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
		buffer.clear (i, 0, buffer.getNumSamples());

	// Get playback position and update current measure
	updateCurrentMeasure(getPlayHead());
	
	// Update the frequency map if the measure has changed
	updateFrequencyMap();
	
	// Process MIDI through the synthesizer
	synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
	
	// Important: if the synth is silent, add a small amount of noise
	// This helps FL Studio recognize it's actually processing audio
	if (buffer.getRMSLevel(0, 0, buffer.getNumSamples()) < 0.000001f)
	{
		// Add extremely quiet noise (below -120dB)
		float* leftChannel = buffer.getWritePointer(0);
		float* rightChannel = totalNumOutputChannels > 1 ? buffer.getWritePointer(1) : nullptr;
		
		for (int i = 0; i < buffer.getNumSamples(); ++i)
		{
			// Extremely quiet random noise
			float noise = ((juce::Random::getSystemRandom().nextFloat() * 2.0f) - 1.0f) * 0.0000001f;
			leftChannel[i] += noise;
			
			if (rightChannel != nullptr)
				rightChannel[i] += noise;
		}
	}
}

//==============================================================================
bool FluidJustIntonationProcessor::hasEditor() const
{
	return true;
}

juce::AudioProcessorEditor* FluidJustIntonationProcessor::createEditor()
{
	return new FluidJustIntonationEditor (*this);
}

//==============================================================================
void FluidJustIntonationProcessor::getStateInformation (juce::MemoryBlock& destData)
{
	// Save parameters to XML
	auto state = parameters.copyState();
	std::unique_ptr<juce::XmlElement> xml(state.createXml());
	
	// Add soundfont path and preset to the state
	if (isSoundFontLoaded())
	{
		xml->setAttribute("soundFontPath", getSoundFontFile().getFullPathName());
		xml->setAttribute("soundFontPreset", getCurrentPreset());
	}
	
	copyXmlToBinary(*xml, destData);
}

void FluidJustIntonationProcessor::setStateInformation (const void* data, int sizeInBytes)
{
	// Restore parameters from XML
	std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
	
	if (xmlState.get() != nullptr)
	{
		if (xmlState->hasTagName(parameters.state.getType()))
			parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
		
		// Restore soundfont if it was saved
		juce::String sfPath = xmlState->getStringAttribute("soundFontPath", "");
		if (sfPath.isNotEmpty())
		{
			juce::File sfFile(sfPath);
			if (sfFile.existsAsFile())
			{
				if (loadSoundFont(sfFile))
				{
					int presetIndex = xmlState->getIntAttribute("soundFontPreset", 0);
					setPreset(presetIndex);
				}
			}
		}
	}
}

//==============================================================================
// Parameter listener
void FluidJustIntonationProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{

	if (parameterID == "sequenceLength") {
		// Update sequence length based on parameter value
		int length = 4;
		if (newValue == 1.0f)
			length = 8;
		else if (newValue == 2.0f)
			length = 12;
		else if (newValue == 3.0f)
			length = 16;
		
		setSequenceLength(length);
	}
	else if (parameterID == "intonationMode") {
		// Update intonation mode based on parameter value
		IntonationMode mode = IntonationMode::Set;
		if (newValue == 1.0f)
			mode = IntonationMode::Shift;
		
		setIntonationMode(mode);
	}
	else if (parameterID.startsWith("measureRoot")) {
		// Extract the measure index from the parameter ID
		int measureIndex = parameterID.getTrailingIntValue();
		
		// Convert parameter value to MIDI note number
		int noteIndex = static_cast<int>(newValue);
		int midiNote = 60 + noteIndex; // Middle C (60) + offset
		
		setMeasureRoot(measureIndex, midiNote);
	}
	
	// Update the frequency mapping
	updateFrequencyMap();
}

// Just Intonation Implementation

// Function to update the frequency map based on current settings
void FluidJustIntonationProcessor::updateFrequencyMap()
{
	// Clear the current mapping
	currentFrequencyMap.clear();
	
	// Generate frequencies for all MIDI notes
	for (int note = 0; note < 128; ++note) {
		double freq = midiNoteToFrequency(note);
		currentFrequencyMap[note] = freq;
	}
	
	// Update the synthesizer with the new mapping
	synth.updateFrequencyMapping(currentFrequencyMap);
}

void FluidJustIntonationProcessor::setSequenceLength(int length)
{
	if (length == 4 || length == 8 || length == 12 || length == 16)
	{
		if (length != sequenceLength)
		{
			// Reset drift when sequence length changes
			resetAccumulatedDrift();
		}
		sequenceLength = length;
	}
}

int FluidJustIntonationProcessor::getSequenceLength() const
{
	return sequenceLength;
}

void FluidJustIntonationProcessor::setIntonationMode(IntonationMode mode)
{
	if (mode == IntonationMode::Set && intonationMode == IntonationMode::Shift)
	{
		// Reset drift when switching from Shift to Set
		resetAccumulatedDrift();
	}
	intonationMode = mode;
}

FluidJustIntonationProcessor::IntonationMode FluidJustIntonationProcessor::getIntonationMode() const
{
	return intonationMode;
}

void FluidJustIntonationProcessor::setMeasureRoot(int measureIndex, int rootNote)
{
	if (measureIndex >= 0 && measureIndex < MAX_SEQUENCE_LENGTH)
		measureRoots[measureIndex] = rootNote;
}

int FluidJustIntonationProcessor::getMeasureRoot(int measureIndex) const
{
	if (measureIndex >= 0 && measureIndex < MAX_SEQUENCE_LENGTH)
		return measureRoots[measureIndex];
	
	return 60; // Default to middle C
}

int FluidJustIntonationProcessor::noteNameToMidiNumber(const juce::String& noteName)
{
	// Map from note name to MIDI note number (with C4 = 60)
	static const std::map<juce::String, int> noteMap = {
		{"C", 60}, {"C#", 61}, {"D", 62}, {"D#", 63}, {"E", 64}, 
		{"F", 65}, {"F#", 66}, {"G", 67}, {"G#", 68}, {"A", 69}, 
		{"A#", 70}, {"B", 71}
	};
	
	auto it = noteMap.find(noteName);
	if (it != noteMap.end())
		return it->second;
	
	return 60; // Default to middle C
}

std::array<double, 12> FluidJustIntonationProcessor::calculateJustRatios(int rootNote)
{
	// Just intonation ratios for a major scale (relative to the root)
	// These ratios are based on the harmonic series: 1/1, 9/8, 5/4, 4/3, 3/2, 5/3, 15/8, 2/1
	static const std::array<double, 12> justRatios = {
		1.0,       // Root (perfect unison)
		16.0/15.0, // Minor second
		9.0/8.0,   // Major second
		6.0/5.0,   // Minor third
		5.0/4.0,   // Major third
		4.0/3.0,   // Perfect fourth
		45.0/32.0, // Augmented fourth / Diminished fifth
		3.0/2.0,   // Perfect fifth
		8.0/5.0,   // Minor sixth
		5.0/3.0,   // Major sixth
		9.0/5.0,   // Minor seventh
		15.0/8.0   // Major seventh
	};
	
	return justRatios;
}

// Helper function: Calculate frequency of a note in a JI scale built from a given root
double FluidJustIntonationProcessor::calculateFrequencyInScale(int noteToPlay, int scaleRoot, double scaleRootFreq)
{
	// Calculate the interval from scaleRoot to noteToPlay in semitones
	int semitoneDistance = noteToPlay - scaleRoot;
	
	// Get the number of octaves and interval within octave
	int octaves = semitoneDistance / 12;
	int intervalWithinOctave = semitoneDistance % 12;
	
	// Handle negative intervals properly
	if (intervalWithinOctave < 0) {
		intervalWithinOctave += 12;
		octaves -= 1;
	}
	
	// Get JI ratio for this interval
	auto ratios = calculateJustRatios(scaleRoot);
	double ratio = ratios[intervalWithinOctave];
	
	// Apply octave shifts
	ratio *= std::pow(2.0, octaves);
	
	return scaleRootFreq * ratio;
}

// Get the root frequency for the current measure based on mode
double FluidJustIntonationProcessor::getCurrentMeasureRootFrequency()
{
	int currentRoot = measureRoots[currentMeasure];
	
	if (intonationMode == IntonationMode::Set) {
		if (currentMeasure == 0) {
			// First measure always uses 12-TET in Set mode
			return CONCERT_A_FREQ * std::pow(2.0, (currentRoot - 69) / 12.0);
		}
		
		// SET mode: Get root from measure 0's JI scale
		int measure0Root = measureRoots[0];
		double measure0RootFreq = CONCERT_A_FREQ * std::pow(2.0, (measure0Root - 69) / 12.0);
		
		return calculateFrequencyInScale(currentRoot, measure0Root, measure0RootFreq);
	}
	else {
		// SHIFT mode
		if (currentMeasure == 0) {
			if (hasAccumulatedDrift) {
				// Use the accumulated drift from the previous loop
				// accumulatedDriftFrequency is the frequency of the last measure's root
				// We need to calculate the current root relative to that
				int lastMeasureRoot = measureRoots[sequenceLength - 1];
				return calculateFrequencyInScale(currentRoot, lastMeasureRoot, accumulatedDriftFrequency);
			}
			else {
				// First time through, use 12-TET
				return CONCERT_A_FREQ * std::pow(2.0, (currentRoot - 69) / 12.0);
			}
		}
		
		// SHIFT mode: Get root from previous measure's JI scale
		int previousMeasureIdx = currentMeasure - 1;
		int previousRoot = measureRoots[previousMeasureIdx];
		double previousRootFreq = getCurrentMeasureRootFrequencyForMeasure(previousMeasureIdx);
		
		return calculateFrequencyInScale(currentRoot, previousRoot, previousRootFreq);
	}
}

// Helper to get root frequency for any measure (used for SHIFT mode recursion)
double FluidJustIntonationProcessor::getCurrentMeasureRootFrequencyForMeasure(int measureIndex)
{
	int rootForMeasure = measureRoots[measureIndex];
	
	if (intonationMode == IntonationMode::Set) {
		if (measureIndex == 0) {
			return CONCERT_A_FREQ * std::pow(2.0, (rootForMeasure - 69) / 12.0);
		}
		int measure0Root = measureRoots[0];
		double measure0RootFreq = CONCERT_A_FREQ * std::pow(2.0, (measure0Root - 69) / 12.0);
		return calculateFrequencyInScale(rootForMeasure, measure0Root, measure0RootFreq);
	}
	else {
		// SHIFT mode
		if (measureIndex == 0) {
			if (hasAccumulatedDrift) {
				int lastMeasureRoot = measureRoots[sequenceLength - 1];
				return calculateFrequencyInScale(rootForMeasure, lastMeasureRoot, accumulatedDriftFrequency);
			}
			else {
				return CONCERT_A_FREQ * std::pow(2.0, (rootForMeasure - 69) / 12.0);
			}
		}
		int previousMeasureIdx = measureIndex - 1;
		int previousRoot = measureRoots[previousMeasureIdx];
		double previousRootFreq = getCurrentMeasureRootFrequencyForMeasure(previousMeasureIdx);
		return calculateFrequencyInScale(rootForMeasure, previousRoot, previousRootFreq);
	}
}

double FluidJustIntonationProcessor::getJustFrequency(int midiNote, int rootNote, bool useCurrentRootAsReference)
{
	// This function is now simplified - just a wrapper for backwards compatibility
	// Get the root frequency for the current measure
	double rootFreq = getCurrentMeasureRootFrequency();
	
	// Get the current measure's root note
	int currentRoot = measureRoots[currentMeasure];
	
	// Calculate the played note's frequency in the current measure's JI scale
	return calculateFrequencyInScale(midiNote, currentRoot, rootFreq);
}

double FluidJustIntonationProcessor::midiNoteToFrequency(int midiNote)
{
	// Get the root frequency for the current measure
	double rootFreq = getCurrentMeasureRootFrequency();
	
	// Get the current measure's root note
	int currentRoot = measureRoots[currentMeasure];
	
	// Calculate the played note's frequency in the current measure's JI scale
	return calculateFrequencyInScale(midiNote, currentRoot, rootFreq);
}

void FluidJustIntonationProcessor::updateCurrentMeasure(juce::AudioPlayHead* playHead)
{
	if (playHead == nullptr)
		return;
		
	auto position = playHead->getPosition();
	
	if (position)
	{
		// Check if playback is currently active
		bool isPlaying = position->getIsPlaying();
		
		// Detect stop: was playing, now stopped
		if (wasPlaying && !isPlaying)
		{
			// Reset drift when playback stops
			resetAccumulatedDrift();
			previousMeasure = -1;
			currentMeasure = 0;
		}
		
		wasPlaying = isPlaying;
		
		if (position->getPpqPosition().hasValue())
		{
			ppqPosition = position->getPpqPosition().orFallback(0.0);
			bpm = position->getBpm().orFallback(120.0);
			
			// Calculate current measure (assuming 4/4 time signature)
			int measuresPassed = static_cast<int>(ppqPosition / 4.0); // 4 beats per measure
			int newMeasure = measuresPassed % sequenceLength;
			
			// Detect loop transition (going from last measure to first)
			if (intonationMode == IntonationMode::Shift && 
				previousMeasure == sequenceLength - 1 && 
				newMeasure == 0 && 
				previousMeasure != -1)
			{
				// Store the frequency of the last measure's root for the next loop
				accumulatedDriftFrequency = getCurrentMeasureRootFrequencyForMeasure(sequenceLength - 1);
				hasAccumulatedDrift = true;
			}
			
			previousMeasure = currentMeasure;
			currentMeasure = newMeasure;
		}
	}
}

//==============================================================================
// Drift management

void FluidJustIntonationProcessor::resetAccumulatedDrift()
{
	accumulatedDriftFrequency = 0.0;
	hasAccumulatedDrift = false;
}

double FluidJustIntonationProcessor::getFrequencyForNote(int midiNote) const
{
	// This is a const version for the UI to query frequencies
	// We need to calculate based on current state without modifying anything
	
	// Get the root frequency for the current measure
	int currentRoot = measureRoots[currentMeasure];
	double rootFreq;
	
	if (intonationMode == IntonationMode::Set) {
		if (currentMeasure == 0) {
			rootFreq = CONCERT_A_FREQ * std::pow(2.0, (currentRoot - 69) / 12.0);
		}
		else {
			int measure0Root = measureRoots[0];
			double measure0RootFreq = CONCERT_A_FREQ * std::pow(2.0, (measure0Root - 69) / 12.0);
			
			// Calculate current root in measure 0's scale
			int semitoneDistance = currentRoot - measure0Root;
			int octaves = semitoneDistance / 12;
			int intervalWithinOctave = semitoneDistance % 12;
			if (intervalWithinOctave < 0) {
				intervalWithinOctave += 12;
				octaves -= 1;
			}
			
			static const std::array<double, 12> justRatios = {
				1.0, 16.0/15.0, 9.0/8.0, 6.0/5.0, 5.0/4.0, 4.0/3.0,
				45.0/32.0, 3.0/2.0, 8.0/5.0, 5.0/3.0, 9.0/5.0, 15.0/8.0
			};
			
			double ratio = justRatios[intervalWithinOctave] * std::pow(2.0, octaves);
			rootFreq = measure0RootFreq * ratio;
		}
	}
	else {
		// Shift mode - need to calculate iteratively
		if (currentMeasure == 0 && hasAccumulatedDrift) {
			int lastMeasureRoot = measureRoots[sequenceLength - 1];
			int semitoneDistance = currentRoot - lastMeasureRoot;
			int octaves = semitoneDistance / 12;
			int intervalWithinOctave = semitoneDistance % 12;
			if (intervalWithinOctave < 0) {
				intervalWithinOctave += 12;
				octaves -= 1;
			}
			
			static const std::array<double, 12> justRatios = {
				1.0, 16.0/15.0, 9.0/8.0, 6.0/5.0, 5.0/4.0, 4.0/3.0,
				45.0/32.0, 3.0/2.0, 8.0/5.0, 5.0/3.0, 9.0/5.0, 15.0/8.0
			};
			
			double ratio = justRatios[intervalWithinOctave] * std::pow(2.0, octaves);
			rootFreq = accumulatedDriftFrequency * ratio;
		}
		else if (currentMeasure == 0) {
			rootFreq = CONCERT_A_FREQ * std::pow(2.0, (currentRoot - 69) / 12.0);
		}
		else {
			// Calculate iteratively from measure 0
			double iterFreq;
			if (hasAccumulatedDrift) {
				int lastMeasureRoot = measureRoots[sequenceLength - 1];
				int firstRoot = measureRoots[0];
				int semitoneDistance = firstRoot - lastMeasureRoot;
				int octaves = semitoneDistance / 12;
				int intervalWithinOctave = semitoneDistance % 12;
				if (intervalWithinOctave < 0) {
					intervalWithinOctave += 12;
					octaves -= 1;
				}
				
				static const std::array<double, 12> justRatios = {
					1.0, 16.0/15.0, 9.0/8.0, 6.0/5.0, 5.0/4.0, 4.0/3.0,
					45.0/32.0, 3.0/2.0, 8.0/5.0, 5.0/3.0, 9.0/5.0, 15.0/8.0
				};
				
				double ratio = justRatios[intervalWithinOctave] * std::pow(2.0, octaves);
				iterFreq = accumulatedDriftFrequency * ratio;
			}
			else {
				iterFreq = CONCERT_A_FREQ * std::pow(2.0, (measureRoots[0] - 69) / 12.0);
			}
			
			for (int m = 1; m <= currentMeasure; ++m) {
				int prevRoot = measureRoots[m - 1];
				int thisRoot = measureRoots[m];
				int semitoneDistance = thisRoot - prevRoot;
				int octaves = semitoneDistance / 12;
				int intervalWithinOctave = semitoneDistance % 12;
				if (intervalWithinOctave < 0) {
					intervalWithinOctave += 12;
					octaves -= 1;
				}
				
				static const std::array<double, 12> justRatios = {
					1.0, 16.0/15.0, 9.0/8.0, 6.0/5.0, 5.0/4.0, 4.0/3.0,
					45.0/32.0, 3.0/2.0, 8.0/5.0, 5.0/3.0, 9.0/5.0, 15.0/8.0
				};
				
				double ratio = justRatios[intervalWithinOctave] * std::pow(2.0, octaves);
				iterFreq = iterFreq * ratio;
			}
			rootFreq = iterFreq;
		}
	}
	
	// Now calculate the frequency of midiNote relative to currentRoot
	int semitoneDistance = midiNote - currentRoot;
	int octaves = semitoneDistance / 12;
	int intervalWithinOctave = semitoneDistance % 12;
	if (intervalWithinOctave < 0) {
		intervalWithinOctave += 12;
		octaves -= 1;
	}
	
	static const std::array<double, 12> justRatios = {
		1.0, 16.0/15.0, 9.0/8.0, 6.0/5.0, 5.0/4.0, 4.0/3.0,
		45.0/32.0, 3.0/2.0, 8.0/5.0, 5.0/3.0, 9.0/5.0, 15.0/8.0
	};
	
	double ratio = justRatios[intervalWithinOctave] * std::pow(2.0, octaves);
	return rootFreq * ratio;
}

//==============================================================================
// SoundFont support methods

bool FluidJustIntonationProcessor::loadSoundFont(const juce::File& file)
{
	return synth.loadSoundFont(file);
}

void FluidJustIntonationProcessor::unloadSoundFont()
{
	synth.unloadSoundFont();
}

bool FluidJustIntonationProcessor::isSoundFontLoaded() const
{
	return synth.isSoundFontLoaded();
}

juce::String FluidJustIntonationProcessor::getSoundFontName() const
{
	return synth.getSoundFontName();
}

juce::File FluidJustIntonationProcessor::getSoundFontFile() const
{
	return synth.getSoundFontFile();
}

int FluidJustIntonationProcessor::getPresetCount() const
{
	return synth.getPresetCount();
}

juce::String FluidJustIntonationProcessor::getPresetName(int presetIndex) const
{
	return synth.getPresetName(presetIndex);
}

void FluidJustIntonationProcessor::setPreset(int presetIndex)
{
	synth.setPreset(presetIndex);
}

int FluidJustIntonationProcessor::getCurrentPreset() const
{
	return synth.getCurrentPreset();
}

void FluidJustIntonationProcessor::setSynthMode(FluidJustIntonationSynth::SynthMode mode)
{
	synth.setSynthMode(mode);
}

FluidJustIntonationSynth::SynthMode FluidJustIntonationProcessor::getSynthMode() const
{
	return synth.getSynthMode();
}

void FluidJustIntonationProcessor::setGlobalGain(float gainLinear)
{
	synth.setGlobalGain(gainLinear);
}

float FluidJustIntonationProcessor::getGlobalGain() const
{
	return synth.getGlobalGain();
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	// Log that we're being created - this will help debug FL Studio issues
	juce::Logger::writeToLog("FluidJustIntonation: Creating new plugin instance");
	
	return new FluidJustIntonationProcessor();
}