#ifndef __KEYPAD_H
#define __KEYPAD_H
#include "stm32f1xx.h"
#include "Car.h"

// -----------------------keypad--------------------
#define KEYPAD_ROW 4
#define KEYPAD_COL 4


void Keypad_Init(void);
void Keypad_Handle(void);

#endif

