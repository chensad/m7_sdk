#ifndef ROBOBASE_SAFETY_OUTPUTS_H
#define ROBOBASE_SAFETY_OUTPUTS_H

#include <stdint.h>

/*
 * M7 safety_allow output.
 *
 * Hardware mapping:
 * - safety_allow: J25 pin 19, ECSPI2_MOSI_3V3, GPIO5_IO11.
 *
 * This signal drives the external AND gate input. The external gate combines:
 *
 *   EN_NODE = HW_OK && safety_allow
 *
 * where HW_OK is the hardwired E-stop/Bumper NC chain. The output is active
 * high and must default low.
 */
#define ROBOBASE_SAFETY_ALLOW_J25_PIN (19U)
#define ROBOBASE_SAFETY_ALLOW_GPIO_INDEX (11U)

void robobase_safety_outputs_init(void);
void robobase_safety_outputs_set_safety_allow(uint8_t allow);

#endif /* ROBOBASE_SAFETY_OUTPUTS_H */
