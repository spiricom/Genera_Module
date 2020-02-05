/*
 * sfx.h
 *
 *  Created on: Dec 23, 2019
 *      Author: josnyder
 */

#ifndef SFX_H_
#define SFX_H_

#define NUM_VOC_VOICES 8
#define NUM_VOC_OSC 1
#define INV_NUM_VOC_VOICES 0.125
#define INV_NUM_VOC_OSC 1
#define NUM_AUTOTUNE 2
#define NUM_RETUNE 1
#define OVERSAMPLER_RATIO 8
#define OVERSAMPLER_HQ FALSE


extern tPoly poly;
extern tRamp polyRamp[NUM_VOC_VOICES];
extern tSawtooth osc[NUM_VOC_VOICES];
extern int autotuneChromatic;



void initGlobalSFXObjects();


//1 vocoder internal poly
void SFXDattorroAlloc();
void SFXDattorroFrame();
void SFXDattorroTick(float audioIn);
void SFXDattorroFree(void);


//1 vocoder internal poly
void SFXLivingStringAlloc();
void SFXLivingStringFrame();
void SFXLivingStringTick(float audioIn);
void SFXLivingStringFree(void);

void SFXKickAndSnareAlloc();
void SFXKickAndSnareFrame();
void SFXKickAndSnareTick(float audioIn);
void SFXKickAndSnareFree(void);


void SFXHihatAlloc();
void SFXHihatFrame();
void SFXHihatTick(float audioIn);
void SFXHihatFree(void);

// MIDI FUNCTIONS
void noteOn(int key, int velocity);
void noteOff(int key, int velocity);
void sustainOn(void);
void sustainOff(void);
void toggleBypass(void);
void toggleSustain(void);

void calculateFreq(int voice);
void calculatePeriodArray(void);
float nearestPeriod(float period);

void clearNotes(void);

void ctrlInput(int ctrl, int value);


#endif /* SFX_H_ */
