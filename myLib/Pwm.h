#ifndef __PWM_H
#define __PWM_H
#include "stm32f1xx.h"

void pwm_set_duty(TIM_HandleTypeDef *tim, uint32_t channel, uint8_t duty);

#endif

