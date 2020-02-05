/*
 * ui.h
 *
 *  Created on: Aug 30, 2019
 *      Author: jeffsnyder
 */

#ifndef UI_H_
#define UI_H_
#define NUM_ADC_CHANNELS 12
#define NUM_BUTTONS 4

extern uint16_t ADC_values[NUM_ADC_CHANNELS];


//PresetNil is used as a counter for the size of the enum
typedef enum _GeneraPreset
{
	DattorroReverb = 0,
	LivingString,
	KickAndSnare,
	Hihat,
	PresetNil
} GeneraPreset;

extern uint8_t buttonValues[NUM_BUTTONS];
extern uint8_t buttonPressed[NUM_BUTTONS];
extern uint8_t buttonReleased[NUM_BUTTONS];

extern GeneraPreset currentPreset;
extern GeneraPreset previousPreset;
extern uint8_t loadingPreset;
// Display values
extern float uiParams[12];

void displayColorsForCurrentPreset(void);

void RGB_LED_setColor(uint8_t Red, uint8_t Green, uint8_t Blue); //inputs between 0-255

void writeCurrentPresetToFlash(void);

void setLED_B(uint8_t brightness); // 0-255

void setLED_C(uint8_t brightness); // 0-255

void setLED_D(uint8_t brightness); // 0-255

void buttonCheck(void);



#endif /* UI_H_ */

