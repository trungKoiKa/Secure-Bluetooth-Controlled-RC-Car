#ifndef __CAR_H
#define __CAR_H
#include "stm32f1xx.h"


typedef enum
{
	CAR_STOP_STATE,
	CAR_FORWARD_STATE,
	CAR_BACKWARD_STATE,
	CAR_TURNLEFT_STATE,
	CAR_TURNRIGHT_STATE,
}Car_State;


void car_control(Car_State state, uint8_t speed);
void car_init(TIM_HandleTypeDef *htim);

#endif

