/*
 * sfx.c
 *
 *  Created on: Dec 23, 2019
 *      Author: josnyder
 */

#include "main.h"
#include "audiostream.h"
#include "sfx.h"
#include "ui.h"
#include "tunings.h"


//audio objects
t808Kick myKick;
t808Snare mySnare;
tEnvelopeFollower myAtkDtk[2];

tFormantShifter fs;
tAutotune autotuneMono;
tAutotune autotunePoly;
tRetune retune;
tRetune retune2;
tRamp pitchshiftRamp;
tRamp nearWetRamp;
tRamp nearDryRamp;
tPoly poly;
tRamp polyRamp[NUM_VOC_VOICES];
tSawtooth osc[NUM_VOC_VOICES];
tTalkbox vocoder;
tTalkbox vocoder2;
tTalkbox vocoder3;
tRamp comp;

tBuffer buff;
tBuffer buff2;
tSampler sampler;

tEnvelopeFollower envfollow;

tOversampler oversampler;


tLockhartWavefolder wavefolder1;
tLockhartWavefolder wavefolder2;
tLockhartWavefolder wavefolder3;
tLockhartWavefolder wavefolder4;

tCrusher crush;
tCrusher crush2;

tTapeDelay delay;
tSVF delayLP;
tSVF delayHP;
tTapeDelay delay2;
tSVF delayLP2;
tSVF delayHP2;
tHighpass delayShaperHp;
tFeedbackLeveler feedbackControl;

tDattorroReverb reverb;
tNReverb reverb2;
tSVF lowpass;
tSVF highpass;
tSVF bandpass;
tSVF lowpass2;
tSVF highpass2;
tSVF bandpass2;

tCycle testSine;

tExpSmooth smoother1;
tExpSmooth smoother2;
tExpSmooth smoother3;

tExpSmooth neartune_smoother;

#define NUM_STRINGS 4
tLivingString theString[NUM_STRINGS];

float myFreq;
float myDetune[NUM_STRINGS];

//control objects
float notePeriods[128];
float noteFreqs[128];
int chordArray[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int chromaticArray[12] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
int autotuneChromatic = 0;
int lockArray[12];
float freq[NUM_VOC_VOICES];
float oversamplerArray[OVERSAMPLER_RATIO];
int delayShaper = 0;

//sampler objects
int samplePlayStart = 0;
int samplePlayEnd = 0;
int sampleLength = 0;
int crossfadeLength = 0;
float samplerRate = 1.0f;
float maxSampleSizeSeconds = 1.0f;


//autosamp objects
volatile float currentPower = 0.0f;
volatile float previousPower = 0.0f;
float samp_thresh = 0.0f;
volatile int samp_triggered = 0;
uint32_t sample_countdown = 0;
PlayMode samplerMode = PlayLoop;
uint32_t powerCounter = 0;



//reverb objects
uint32_t freeze = 0;

void initGlobalSFXObjects()
{
	calculatePeriodArray();

	tPoly_init(&poly, NUM_VOC_VOICES);
	tPoly_setPitchGlideActive(&poly, FALSE);
	for (int i = 0; i < NUM_VOC_VOICES; i++)
	{
		tRamp_init(&polyRamp[i], 10.0f, 1);
	}




	tRamp_init(&nearWetRamp, 10.0f, 1);
	tRamp_init(&nearDryRamp, 10.0f, 1);
	tRamp_init(&comp, 10.0f, 1);
}

///1 vocoder internal poly

void SFXDattorroAlloc()
{

	tDattorroReverb_init(&reverb);
	tDattorroReverb_setMix(&reverb, 1.0f);
}

void SFXDattorroFrame()
{


}

void SFXDattorroTick(float audioIn)
{
	float stereo[2];

		if (buttonPressed[1])
		{
			if (freeze == 0)
			{
				freeze = 1;
				tDattorroReverb_setFreeze(&reverb, 1);
				setLED_B(255);
				buttonPressed[1] = 0;
			}
			else
			{
				freeze = 0;
				tDattorroReverb_setFreeze(&reverb, 0);
				setLED_B(0);
				buttonPressed[1] = 0;
			}

		}



		float tempFloat = LEAF_clip(0.0f,(smoothedADC[0] + smoothedADC[8]), 1.0f);
		uiParams[0] = tempFloat; //size

		tempFloat = LEAF_clip(0.0f, (smoothedADC[1] + smoothedADC[9]), 1.0f);
		uiParams[1] = tempFloat; //feedback

		tempFloat = LEAF_clip(0.0f,(smoothedADC[2] + smoothedADC[10]) * 123.0f,123.0f);
		uiParams[2] = faster_mtof(tempFloat); //feedback highpass

		tempFloat = LEAF_clip(0.0f,(smoothedADC[3] + smoothedADC[11]) * 130.0f, 130.0f);
		uiParams[3] = faster_mtof(tempFloat); //feedback lowpass

		tempFloat = (smoothedADC[4]) * 130.0f;
		uiParams[4] = faster_mtof(tempFloat); //input lowpass

		uiParams[5] = (smoothedADC[5] * 200.f); //predelay


		tDattorroReverb_setInputFilter(&reverb, uiParams[4]);
		tDattorroReverb_setHP(&reverb, uiParams[2]);
		tDattorroReverb_setFeedbackFilter(&reverb, uiParams[3]);

		tDattorroReverb_setInputDelay(&reverb, uiParams[5]);
		audioIn *= 1.0f;
		tDattorroReverb_setSize(&reverb, uiParams[0]);
		tDattorroReverb_setFeedbackGain(&reverb, uiParams[1]);
		tDattorroReverb_tickStereo(&reverb, audioIn, stereo);

		sample = tanhf(stereo[0]) * 0.99f;
		rightOut = tanhf(stereo[1]) * 0.99f;
}

void SFXDattorroFree(void)
{
	tDattorroReverb_free(&reverb);
}



//17 Living String
void SFXLivingStringAlloc()
{
	for (int i = 0; i < NUM_STRINGS; i++)
	{
		myFreq = (randomNumber() * 300.0f) + 60.0f;
		myDetune[i] = (randomNumber() * 0.3f) - 0.15f;
		//tLivingString_init(&theString[i],  myFreq, 0.4f, 0.0f, 16000.0f, .999f, .5f, .5f, 0.1f, 0);
		tLivingString_init(&theString[i], 440.f, 0.2f, 0.f, 9000.f, 1.0f, 0.3f, 0.01f, 0.125f, 0);
	}
}

void SFXLivingStringFrame()
{
	uiParams[0] = LEAF_clip(10.0f, mtof(((smoothedADC[0] + (smoothedADC[8] * smoothedADC[5])) * 135.0f)), 19000.0f); //freq
	uiParams[1] = smoothedADC[0] + smoothedADC[9]; //detune
	uiParams[2] = LEAF_clip(0.9f, (((smoothedADC[2] + smoothedADC[9]) * 0.09999999f) + 0.9f), 0.999999999f); //decay
	uiParams[3] = LEAF_clip(10.0f, mtof(((smoothedADC[3]+ smoothedADC[10])* 130.0f)+12.0f), 19000.0f); //lowpass
	uiParams[4] = LEAF_clip(0.001f,((smoothedADC[4] + smoothedADC[11])* 0.5) + 0.02f, 1.0f);//pickPos
	for (int i = 0; i < NUM_STRINGS; i++)
	{
		tLivingString_setFreq(&theString[i], (i + (1.0f+(myDetune[i] * uiParams[1]))) * uiParams[0]);
		tLivingString_setDecay(&theString[i], uiParams[2]);
		tLivingString_setDampFreq(&theString[i], uiParams[3]);
		tLivingString_setPickPos(&theString[i], uiParams[4]);
	}

}


void SFXLivingStringTick(float audioIn)
{
	for (int i = 0; i < NUM_STRINGS; i++)
	{
		sample += tLivingString_tick(&theString[i], audioIn);
	}
	sample *= 0.0625f;
	rightOut = sample;


}

void SFXLivingStringFree(void)
{
	for (int i = 0; i < NUM_STRINGS; i++)
	{
		tLivingString_free(&theString[i]);
	}
}



//Kick and Snare

void SFXKickAndSnareAlloc()
{
	t808Snare_init(&mySnare);
	t808Kick_init(&myKick);
	tEnvelopeFollower_init(&myAtkDtk[0], 0.1f, .99f);
	tEnvelopeFollower_init(&myAtkDtk[1], 0.1f, .99f);
}

void SFXKickAndSnareFrame()
{


}

uint8_t kickTriggered = 0;
uint8_t snareTriggered = 0;
void SFXKickAndSnareTick(float audioIn)
{
	uiParams[0] = LEAF_clip(0.0f, smoothedADC[0] + smoothedADC[8], 2.0f);

	//if digital input on jack 5, then trigger snare

	float leftEnvelope = tEnvelopeFollower_tick(&myAtkDtk[0], audioIn);
	float rightEnvelope = tEnvelopeFollower_tick(&myAtkDtk[1], rightIn);

	if (leftEnvelope > 0.5f)
	{
		if (snareTriggered == 0)
		{
			t808Snare_on(&mySnare, uiParams[0]);
			snareTriggered = 1;
		}
	}
	else if (leftEnvelope < 0.05f)
	{
		snareTriggered = 0;
	}

	//if digital input on jack 6, then trigger kick
	if (rightEnvelope > 0.5f)
	{
		if (kickTriggered == 0)
		{
			t808Kick_on(&myKick, uiParams[0]);
			kickTriggered = 1;
		}
	}
	else if (rightEnvelope < 0.05f)
	{
		kickTriggered = 0;
	}

	//OK, now some audio stuff
	uiParams[1] = LEAF_clip(0.0f, LEAF_midiToFrequency((smoothedADC[2] * 15.0f + smoothedADC[9] * 15.0f) + 30.0f), 24000.0f);
	t808Snare_setToneNoiseMix(&mySnare, smoothedADC[1]);//knob 2 sets noise mix
	t808Snare_setTone1Decay(&mySnare, (smoothedADC[3] * 100.0f) + (smoothedADC[10] * 100.0f)); //knob 4 sets snare tone1 decay time (added with jack 3 CV input)
	t808Snare_setTone2Decay(&mySnare, (smoothedADC[3] * 150.0f) + (smoothedADC[10] * 150.0f)); //knob 4 sets snare tone2 decay time (added with jack 3 CV input)
	t808Snare_setNoiseDecay(&mySnare, (smoothedADC[3] * 100.0f) + (smoothedADC[10] * 100.0f)); //knob 4 sets snare noise decay time (added with jack 3 CV input)
	t808Snare_setTone1Freq(&mySnare, uiParams[1]);
	t808Snare_setTone2Freq(&mySnare, uiParams[1] * 1.5f);
	rightOut = t808Snare_tick(&mySnare);//
	LEAF_shaper(sample, 1.0f);


	t808Kick_setToneFreq(&myKick, LEAF_midiToFrequency(((smoothedADC[4]* 30.0f) + smoothedADC[10] * 30.0f)+ 14.0f)); //knob 5 sets kick freq
	t808Kick_setToneDecay(&myKick, (smoothedADC[5] * 1000.0f) + (smoothedADC[11] * 1000.0f)); //knob 6 set kick decay (added to jack 4 CV in)
	rightOut = LEAF_shaper(t808Kick_tick(&myKick), 1.0f);

}

void SFXKickAndSnareFree(void)
{
	t808Snare_free(&mySnare);
	t808Kick_free(&myKick);
	tEnvelopeFollower_free(&myAtkDtk[0]);
	tEnvelopeFollower_free(&myAtkDtk[1]);
}

// midi functions


void calculateFreq(int voice)
{
	float tempNote = tPoly_getPitch(&poly, voice);
	float tempPitchClass = ((((int)tempNote) - keyCenter) % 12 );
	float tunedNote = tempNote + centsDeviation[(int)tempPitchClass];
	freq[voice] = LEAF_midiToFrequency(tunedNote);
}

void noteOn(int key, int velocity)
{
	if (!velocity)
	{
		if (chordArray[key%12] > 0) chordArray[key%12]--;

		int voice = tPoly_noteOff(&poly, key);
		if (voice >= 0) tRamp_setDest(&polyRamp[voice], 0.0f);

		for (int i = 0; i < tPoly_getNumVoices(&poly); i++)
		{
			if (tPoly_isOn(&poly, i) == 1)
			{
				tRamp_setDest(&polyRamp[i], 1.0f);
				calculateFreq(i);
			}
		}

	}
	else
	{
		chordArray[key%12]++;

		tPoly_noteOn(&poly, key, velocity);

		for (int i = 0; i < tPoly_getNumVoices(&poly); i++)
		{
			if (tPoly_isOn(&poly, i) == 1)
			{
				tRamp_setDest(&polyRamp[i], 1.0f);
				calculateFreq(i);
			}
		}

	}
}

void noteOff(int key, int velocity)
{
	if (chordArray[key%12] > 0) chordArray[key%12]--;

	int voice = tPoly_noteOff(&poly, key);
	if (voice >= 0) tRamp_setDest(&polyRamp[voice], 0.0f);

	for (int i = 0; i < tPoly_getNumVoices(&poly); i++)
	{
		if (tPoly_isOn(&poly, i) == 1)
		{
			tRamp_setDest(&polyRamp[i], 1.0f);
			calculateFreq(i);
		}
	}
}

void sustainOff()
{

}

void sustainOn()
{

}

void toggleBypass()
{

}

void toggleSustain()
{

}

void ctrlInput(int ctrl, int value)
{

}

