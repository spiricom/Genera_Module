/*
 * audiostream.c
 *
 *  Created on: Aug 30, 2019
 *      Author: jeffsnyder
 */


/* Includes ------------------------------------------------------------------*/
#include "audiostream.h"
#include "main.h"
#include "leaf.h"
#include "codec.h"
#include "ui.h"
#include "tunings.h"
#include "i2c.h"
#include "gpio.h"
#include "sfx.h"

//the audio buffers are put in the D2 RAM area because that is a memory location that the DMA has access to.
int32_t audioOutBuffer[AUDIO_BUFFER_SIZE] __ATTR_RAM_D2;
int32_t audioInBuffer[AUDIO_BUFFER_SIZE] __ATTR_RAM_D2;

#define SMALL_MEM_SIZE 8192
#define MED_MEM_SIZE 500000
#define LARGE_MEM_SIZE 33554432 //32 MBytes - size of SDRAM IC
char small_memory[SMALL_MEM_SIZE];
char medium_memory[MED_MEM_SIZE]__ATTR_RAM_D1;
char large_memory[LARGE_MEM_SIZE] __ATTR_SDRAM;

void audioFrame(uint16_t buffer_offset);
float audioTickL(float audioIn);
float audioTickR(float audioIn);
void buttonCheck(void);

HAL_StatusTypeDef transmit_status;
HAL_StatusTypeDef receive_status;

uint8_t codecReady = 0;

uint16_t frameCounter = 0;

tMempool smallPool;
tMempool largePool;

tRamp adc[12];

float smoothedADC[12];

uint32_t clipCounter[4] = {0,0,0,0};
uint8_t clipped[4] = {0,0,0,0};


float rightIn = 0.0f;
float rightOut = 0.0f;
float sample = 0.0f;

BOOL frameCompleted = TRUE;

BOOL bufferCleared = TRUE;

int numBuffersToClearOnLoad = 2;
int numBuffersCleared = 0;

/**********************************************/


void (*allocFunctions[PresetNil])(void);
void (*frameFunctions[PresetNil])(void);
void (*tickFunctions[PresetNil])(float);
void (*freeFunctions[PresetNil])(void);

void audioInit(I2C_HandleTypeDef* hi2c, SAI_HandleTypeDef* hsaiOut, SAI_HandleTypeDef* hsaiIn)
{
	// Initialize LEAF.

	LEAF_init(SAMPLE_RATE, AUDIO_FRAME_SIZE, medium_memory, MED_MEM_SIZE, &randomNumber);

	tMempool_init (&smallPool, small_memory, SMALL_MEM_SIZE);
	tMempool_init (&largePool, large_memory, LARGE_MEM_SIZE);

	initFunctionPointers();

	//ramps to smooth the knobs
	for (int i = 0; i < 12; i++)
	{
		tRamp_init(&adc[i], 5.0f, 1); //set all ramps for knobs to be 10ms ramp time and let the init function know they will be ticked every sample
	}

	initGlobalSFXObjects();

	allocFunctions[currentPreset]();

	displayColorsForCurrentPreset();

	HAL_Delay(10);

	for (int i = 0; i < AUDIO_BUFFER_SIZE; i++)
	{
		audioOutBuffer[i] = 0;
	}

	HAL_Delay(1);

	// set up the I2S driver to send audio data to the codec (and retrieve input as well)
	transmit_status = HAL_SAI_Transmit_DMA(hsaiOut, (uint8_t *)&audioOutBuffer[0], AUDIO_BUFFER_SIZE);
	receive_status = HAL_SAI_Receive_DMA(hsaiIn, (uint8_t *)&audioInBuffer[0], AUDIO_BUFFER_SIZE);


	// with the CS4271 codec IC, the SAI Transmit and Receive must be happening before the chip will respond to
	// I2C setup messages (it seems to use the masterclock input as it's own internal clock for i2c data, etc)
	// so while we used to set up codec before starting SAI, now we need to set up codec afterwards, and set a flag to make sure it's ready


	//now to send all the necessary messages to the codec
	AudioCodec_init(hi2c);
	HAL_Delay(1);
}

void audioFrame(uint16_t buffer_offset)
{
	frameCompleted = FALSE;

	int i;
	int32_t current_sample;

	buttonCheck();


	//read the analog inputs and smooth them with ramps
	for (i = 0; i < 12; i++)
	{
		//this drops them down to 10 bits, useful for the knobs but likely not for the CV inputs - probably should keep those as 16 bit
		tRamp_setDest(&adc[i],(float) (ADC_values[i]>>6) * INV_TWO_TO_10);
	}

	if (!loadingPreset)
	{

		frameFunctions[currentPreset]();
	}

	//if the codec isn't ready, keep the buffer as all zeros
	//otherwise, start computing audio!

	bufferCleared = TRUE;

	if (codecReady)
	{
		for (i = 0; i < (HALF_BUFFER_SIZE); i++)
		{
			if ((i & 1) == 0)
			{
				current_sample = (int32_t)(audioTickL((float) (audioInBuffer[buffer_offset + i] << 8) * INV_TWO_TO_31) * TWO_TO_23);
			}
			else
			{
				current_sample = (int32_t)(audioTickR((float) (audioInBuffer[buffer_offset + i] << 8) * INV_TWO_TO_31) * TWO_TO_23);
			}

			audioOutBuffer[buffer_offset + i] = current_sample;
		}

	}

	if (bufferCleared)
	{
		numBuffersCleared++;
		if (numBuffersCleared >= numBuffersToClearOnLoad)
		{
			numBuffersCleared = numBuffersToClearOnLoad;
			if (loadingPreset)
			{
				if ((previousPreset < PresetNil))
				{
					freeFunctions[previousPreset]();
				}
				allocFunctions[currentPreset]();
				loadingPreset = 0;
			}
		}
	}
	else numBuffersCleared = 0;

	frameCompleted = TRUE;

}




float audioTickL(float audioIn)
{
	sample = 0.0f;
	rightOut = 0.0f;
	//knobs are hooked up backwards
	for (int i = 0; i < 8; i++)
	{
		smoothedADC[i] = 1.0f - tRamp_tick(&adc[i]);
	}

	//jacks are correct
	for (int i = 8; i < 12; i++)
	{
		smoothedADC[i] = tRamp_tick(&adc[i]);
	}

	if (loadingPreset) return sample;

	bufferCleared = FALSE;

	/*
	if ((audioIn >= 0.999999f) || (audioIn <= -0.999999f))
	{
		setLED_C(255);
		clipCounter[0] = 10000;
		clipped[0] = 1;
	}
	if ((clipCounter[0] > 0) && (clipped[0] == 1))
	{
		clipCounter[0]--;
	}
	else if ((clipCounter[0] == 0) && (clipped[0] == 1))
	{
		setLED_C(0);
		clipped[0] = 0;
	}
*/


	tickFunctions[currentPreset](audioIn);
/*
	if ((sample >= 0.999999f) || (sample <= -0.999999f))
	{
		setLED_D(255);
		clipCounter[2] = 10000;
		clipped[2] = 1;
	}
	if ((clipCounter[2] > 0) && (clipped[2] == 1))
	{
		clipCounter[2]--;
	}
	else if ((clipCounter[2] == 0) && (clipped[2] == 1))
	{
		setLED_D(0);
		clipped[2] = 0;
	}
	*/
	return sample;
}



float audioTickR(float audioIn)
{
	rightIn = audioIn;
	return rightOut;
}


static void initFunctionPointers(void)
{
	allocFunctions[DattorroReverb] = SFXDattorroAlloc;
	frameFunctions[DattorroReverb] = SFXDattorroFrame;
	tickFunctions[DattorroReverb] = SFXDattorroTick;
	freeFunctions[DattorroReverb] = SFXDattorroFree;

	allocFunctions[LivingString] = SFXLivingStringAlloc;
	frameFunctions[LivingString] = SFXLivingStringFrame;
	tickFunctions[LivingString] = SFXLivingStringTick;
	freeFunctions[LivingString] = SFXLivingStringFree;

	allocFunctions[KickAndSnare] = SFXKickAndSnareAlloc;
	frameFunctions[KickAndSnare] = SFXKickAndSnareFrame;
	tickFunctions[KickAndSnare] = SFXKickAndSnareTick;
	freeFunctions[KickAndSnare] = SFXKickAndSnareFree;

	allocFunctions[Hihat] = SFXHihatAlloc;
	frameFunctions[Hihat] = SFXHihatFrame;
	tickFunctions[Hihat] = SFXHihatTick;
	freeFunctions[Hihat] = SFXHihatFree;
}



void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{

}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{

}

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{

}


void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
	audioFrame(HALF_BUFFER_SIZE);
}

void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
	audioFrame(0);
}
