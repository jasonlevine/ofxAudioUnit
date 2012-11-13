#pragma once

#include "TargetConditionals.h"
#include <AudioToolbox/AudioToolbox.h>
#include <iostream>
#include <string>
#include <vector>
#include "ofPolyline.h"
#include "ofTypes.h"
#include "ofxAudioUnitUtils.h"

#pragma mark ofxAudioUnit

// ofxAudioUnit is a general-purpose class to simplify using Audio Units in 
// openFrameworks apps. There are also several subclasses defined in this file
// which provide convenience functions for specific Audio Units (eg. file player,
// mixer, output, speech synth...)

// There is also a class named ofxAudioUnitTap which acts like an Audio Unit,
// but allows you to read samples that are passing through your Audio Unit
// chains. For example, putting an ofxAudioUnitTap between a synth and an
// output unit will allow you to read the samples being sent from the synth
// to the output.

class ofxAudioUnit
{	
protected:
	AudioUnitRef _unit;
	
	AudioComponentDescription _desc;
	void initUnit();
	bool loadPreset(const CFURLRef &presetURL);
	bool savePreset(const CFURLRef &presetURL);
	
public:
	ofxAudioUnit(){};
	ofxAudioUnit(AudioComponentDescription description);
	ofxAudioUnit(OSType type,
				 OSType subType,
				 OSType manufacturer = kAudioUnitManufacturer_Apple);
	ofxAudioUnit(const ofxAudioUnit &orig);
	ofxAudioUnit& operator=(const ofxAudioUnit &orig);
	
	virtual ~ofxAudioUnit();
	
	virtual void connectTo(ofxAudioUnit &otherUnit, int destinationBus = 0, int sourceBus = 0);
	virtual void connectTo(ofxAudioUnitTap &tap);
	virtual ofxAudioUnit& operator>>(ofxAudioUnit& otherUnit);
	virtual ofxAudioUnitTap& operator>>(ofxAudioUnitTap& tap);
	
	virtual OSStatus render(AudioUnitRenderActionFlags *ioActionFlags,
							const AudioTimeStamp *inTimeStamp,
							UInt32 inOutputBusNumber, 
							UInt32 inNumberFrames, 
							AudioBufferList *ioData); 
	
	AudioUnitRef getUnit(){return _unit;}
	
	// This pair of functions will look for the preset in the 
	// apps's data folder and append ".aupreset" to the name
	bool loadCustomPreset(const std::string &presetName);
	bool saveCustomPreset(const std::string &presetName);
	
	// This pair of functions expect an absolute path (including
	// the file extension)
	bool saveCustomPresetAtPath(const std::string &presetPath);
	bool loadCustomPresetAtPath(const std::string &presetPath);
	
	void setRenderCallback(AURenderCallbackStruct callback, int destinationBus = 0);
	void setParameter(AudioUnitParameterID property, AudioUnitScope scope, AudioUnitParameterValue value, int bus = 0);
	void reset(){AudioUnitReset(*_unit, kAudioUnitScope_Global, 0);}
	
	bool setInputBusCount(unsigned int numberOfInputBusses);
	unsigned int getInputBusCount() const;
	bool setOutputBusCount(unsigned int numberOfOutputBusses);
	unsigned int getOutputBusCount() const;
	
#if !(TARGET_OS_IPHONE)
	void showUI(const std::string &title = "Audio Unit UI",
				int x = 100,
				int y = 100,
				bool forceGeneric = false);
#endif
};

static void AudioUnitDeleter(AudioUnit * unit);

#pragma mark - ofxAudioUnitMixer

// ofxAudioUnitMixer wraps the AUMultiChannelMixer
// This is a multiple-input, single-output mixer.
// Call setInputBusCount() to change the number
// of inputs on the mixer.

// You can use this unit to get access to the level
// of the audio going through it (in decibels).
// First call enableInputMetering() or 
// enableOutputMetering() (depending on which one
// you're interested in). After this, getInputLevel()
// or getOutputLevel() will return a float describing
// the current level in decibles (most likely in the 
// range -120 - 0)

class ofxAudioUnitMixer : public ofxAudioUnit
{
public:
	ofxAudioUnitMixer();
	
	void setInputVolume (float volume, int bus = 0);
	void setOutputVolume(float volume);
	void setPan(float pan, int bus = 0);
	
	float getInputLevel(int bus = 0);
	float getOutputLevel() const;
	void  enableInputMetering(int bus = 0);
	void  enableOutputMetering();
	void  disableInputMetering(int bus = 0);
	void  disableOutputMetering();
};

#pragma mark - ofxAudioUnitFilePlayer

// ofxAudioUnitFilePlayer wraps the AUAudioFilePlayer unit.
// This audio unit allows you to play any file that
// Core Audio supports (mp3, aac, caf, aiff, etc)

enum
{
	OFX_AU_LOOP_FOREVER = -1
};

class ofxAudioUnitFilePlayer : public ofxAudioUnit 
{
	AudioFileID _fileID[1];
	ScheduledAudioFileRegion _region;
	
public:
	ofxAudioUnitFilePlayer();
	~ofxAudioUnitFilePlayer();

	bool   setFile(const std::string &filePath);
	UInt32 getLength();
	void   setLength(UInt32 length);
	
	// You can get the startTime arg from mach_absolute_time().
	// Note that all of these args are optional; you can just
	// call play() / loop() and it will start right away.
	void play(uint64_t startTime = 0);
	void loop(unsigned int timesToLoop = OFX_AU_LOOP_FOREVER, uint64_t startTime = 0);
	void stop();
};

#pragma mark - ofxAudioUnitOutput

// ofxAudioUnitOutput wraps the AUHAL output unit on OSX
// and the RemoteIO unit on iOS

// This unit drives the "pull" model of Core Audio and
// sends audio to the actual hardware (ie. speakers / headphones)

class ofxAudioUnitOutput : public ofxAudioUnit
{
public:
	ofxAudioUnitOutput();
	~ofxAudioUnitOutput(){stop();}
	
	bool start();
	bool stop();
};

#pragma mark - ofxAudioUnitInput

class ofxAudioUnitInput : public ofxAudioUnit
{
	typedef ofPtr<AudioBufferList> AudioBufferListRef;
	
	class RingBuffer : private std::vector<AudioBufferListRef>
	{
		UInt64 _readItrIndex, _writeItrIndex;
		RingBuffer::iterator _readItr, _writeItr;
		void advanceItr(RingBuffer::iterator &itr);
		
	public:
		RingBuffer(UInt32 buffers = 3, 
				   UInt32 channelsPerBuffer = 2,
				   UInt32 samplesPerBuffer = 512);
		~RingBuffer();
		
		bool advanceReadHead();
		void advanceWriteHead();
		
		AudioBufferList * readHead() {return (*_readItr).get(); }
		AudioBufferList * writeHead(){return (*_writeItr).get();}
	};
	
	typedef ofPtr<RingBuffer> RingBufferRef;
	
	struct RenderContext
	{
		AudioUnitRef  inputUnit;
		RingBufferRef ringBuffer;
	};
	
	RenderContext _renderContext;
	RingBufferRef _ringBuffer;
	bool _isReady;
	bool configureInputDevice();
	
	static OSStatus renderCallback(void *inRefCon, 
								   AudioUnitRenderActionFlags *ioActionFlags,
								   const AudioTimeStamp *inTimeStamp,
								   UInt32 inBusNumber,
								   UInt32 inNumberFrames,
								   AudioBufferList *ioData);
	
	static OSStatus pullCallback(void *inRefCon, 
								 AudioUnitRenderActionFlags *ioActionFlags,
								 const AudioTimeStamp *inTimeStamp,
								 UInt32 inBusNumber,
								 UInt32 inNumberFrames,
								 AudioBufferList *ioData);
	
public:
	ofxAudioUnitInput();
	~ofxAudioUnitInput();
	
	void connectTo(ofxAudioUnit &otherUnit, int destinationBus = 0, int sourceBus = 0);
	OSStatus render(AudioUnitRenderActionFlags *ioActionFlags,
					const AudioTimeStamp *inTimeStamp,
					UInt32 inOutputBusNumber,
					UInt32 inNumberFrames,
					AudioBufferList *ioData);
	
	bool start();
	bool stop();
};

#pragma mark - ofxAudioUnitSampler

class ofxAudioUnitSampler : public ofxAudioUnit 
{
	
public:
	ofxAudioUnitSampler();
	ofxAudioUnitSampler(AudioComponentDescription description);
    ofxAudioUnitSampler(OSType type,
                        OSType subType,
                        OSType manufacturer = kAudioUnitManufacturer_Apple);
    ofxAudioUnitSampler(const ofxAudioUnitSampler &orig);
    ofxAudioUnitSampler& operator=(const ofxAudioUnitSampler &orig);
    
	bool setSample(const std::string &samplePath);
	bool setSamples(const std::vector<std::string> &samplePaths);
    
    void midiEvent(const UInt32 status, const UInt32 data1, const UInt32 data2);
    void setBank(const UInt32 msb, const UInt32 lsb);
    void setProgram(const UInt32 prog);
    void setChannel(const UInt32 chan) { midiChannelInUse = chan; };
    void midiNoteOn(const UInt32 note, const UInt32 vel);
    void midiNoteOff(const UInt32 note, const UInt32 vel);
    void setVolume(float volume);
    
    UInt32 midiChannelInUse;
    
    enum {
        kMidiMessage_ControlChange      = 0xB,
        kMidiMessage_ProgramChange      = 0xC,
        kMidiMessage_BankMSBControl     = 0,
        kMidiMessage_BankLSBControl     = 32,
        kMidiMessage_NoteOn             = 0x9,
        kMidiMessage_NoteOff            = 0x8
    };

};

#pragma mark - ofxAudioUnitTap

// ofxAudioUnitTap acts like an Audio Unit (as in, you
// can connect it to other Audio Units). In reality, it
// hooks up two Audio Units to each other, but also copies
// the samples that are passing through the chain. This
// allows you to extract the samples to do audio visualization,
// FFT analysis and so on.

// Samples retreived from the ofxAudioUnitTap will be floating
// point numbers between -1 and 1. It is possible for them to exceed
// this range, but this will typically be due to an Audio Unit
// overloading its output.

// Note that if you just want to know how loud the audio is,
// the ofxAudioUnitMixer will allow you to access that
// value with less CPU overhead.

// At the moment, the size of the vector of samples extracted
// is basically hardcoded to 512 samples

class ofxAudioUnitTap
{	
	struct TapContext
	{
		ofxAudioUnit * sourceUnit;
		AudioBufferList * trackedSamples;
		ofMutex * bufferMutex;
	};
	
	ofMutex _bufferMutex;
	AudioBufferList * _trackedSamples;
	ofxAudioUnit * _sourceUnit;
	ofxAudioUnit * _destinationUnit;
	UInt32 _destinationBus;
	TapContext _tapContext;
	
	static OSStatus renderAndCopy(void * inRefCon,
								  AudioUnitRenderActionFlags * ioActionFlags,
								  const AudioTimeStamp * inTimeStamp,
								  UInt32 inBusNumber,
								  UInt32 inNumberFrames,
								  AudioBufferList * ioData);
	
	void waveformForBuffer(AudioBuffer * buffer, float width, float height, ofPolyline &outLine);
	
public:
	ofxAudioUnitTap();
	~ofxAudioUnitTap();
	
	void connectTo(ofxAudioUnit &destination, int destinationBus = 0, int sourceBus = 0);
	ofxAudioUnit& operator>>(ofxAudioUnit& destination);
	
	void getSamples(ofxAudioUnitTapSamples &outData);
	void getStereoWaveform(ofPolyline &outLeft, ofPolyline &outRight, float width, float height);
	void getLeftWaveform(ofPolyline &outLine, float width, float height);
	void getRightWaveform(ofPolyline &outLine, float width, float height);
	
	void setSource(ofxAudioUnit * source);
};

#if !TARGET_OS_IPHONE

#pragma mark - - OSX only below here - -

#pragma mark ofxAudioUnitNetSend

// ofxAudioUnitNetSend wraps the AUNetSend unit.
// This audio unit allows you to send audio to
// an AUNetReceive via bonjour. It supports a handful
// of different stream formats as well.

// You can change the stream format by calling 
// setFormat() with one of "kAUNetSendPresetFormat_"
// constants

// Multiple AUNetSends are differentiated by port
// number, NOT by bonjour name

class ofxAudioUnitNetSend : public ofxAudioUnit
{
public:
	ofxAudioUnitNetSend();
	void setName(const std::string &name);
	void setPort(unsigned int portNumber);
	void setFormat(unsigned int formatIndex);
};

#pragma mark - ofxAudioUnitNetReceive

// ofxAudioUnitNetReceive wraps the AUNetReceive unit.
// This audio unit receives audio from an AUNetSend.
// Call connectToHost() with the IP Address of a
// host running an instance of AUNetSend in order to
// connect.

// For example, myNetReceive.connectToHost("127.0.0.1")
// will start streaming audio from an AUNetSend on the
// same machine.

class ofxAudioUnitNetReceive : public ofxAudioUnit
{
public:
	ofxAudioUnitNetReceive();
	void connectToHost(const std::string &ipAddress, unsigned long port = 52800);
	void disconnect();
};

#pragma mark - ofxAudioUnitSpeechSynth

// ofxAudioUnitSpeechSynth wraps the AUSpeechSynthesis unit.
// This unit lets you access the Speech Synthesis API
// for text-to-speech on your mac (the same thing that
// powers the VoiceOver utility).

class ofxAudioUnitSpeechSynth : public ofxAudioUnit
{
	SpeechChannel _channel;
public:
	ofxAudioUnitSpeechSynth();
	
	void say(const std::string &phrase);
	void stop();
	
	void printAvailableVoices();
	std::vector<std::string>getAvailableVoices();
	bool setVoice(int voiceIndex);
	bool setVoice(const std::string &voiceName);
	
	SpeechChannel getSpeechChannel(){return _channel;}
};

#endif
