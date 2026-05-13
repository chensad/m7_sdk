/*
 * RoboBase safety input abstraction.
 *
 * The first implementation is a mock backend so the M7 safety state machine can
 * be validated before real GPIO wiring is connected.
 */

#ifndef ROBOBASE_SAFETY_INPUTS_H
#define ROBOBASE_SAFETY_INPUTS_H

#include <stdint.h>

#define ROBOBASE_SAFETY_ESTOP_J25_PIN (23U)
#define ROBOBASE_SAFETY_ESTOP_GPIO_BANK (5U)
#define ROBOBASE_SAFETY_ESTOP_GPIO_INDEX (10U)

#define ROBOBASE_SAFETY_BUMPER_J25_PIN (21U)
#define ROBOBASE_SAFETY_BUMPER_GPIO_BANK (5U)
#define ROBOBASE_SAFETY_BUMPER_GPIO_INDEX (12U)

typedef struct _robobase_safety_inputs
{
    uint8_t estop_nc_closed;
    uint8_t bumper_nc_closed;
} robobase_safety_inputs_t;

void robobase_safety_inputs_init(void);
void robobase_safety_inputs_sample(robobase_safety_inputs_t *inputs);
void robobase_safety_inputs_set_debug_gpio_level(uint8_t estop_gpio_level, uint8_t bumper_gpio_level,
                                                 uint32_t valid_mask);

#endif /* ROBOBASE_SAFETY_INPUTS_H */
