#include "robobase_safety_inputs.h"

#include "fsl_gpio.h"
#include "fsl_iomuxc.h"
#include "robobase/rb_safety_proto.h"

#define ROBOBASE_GPIO_LOW (0U)
#define ROBOBASE_GPIO_HIGH (1U)

#define ROBOBASE_SAFETY_INPUT_PAD_PULL_DOWN \
    (IOMUXC_SW_PAD_CTL_PAD_PE_MASK | IOMUXC_SW_PAD_CTL_PAD_HYS_MASK | IOMUXC_SW_PAD_CTL_PAD_DSE(1U))
#define ROBOBASE_SAFETY_INPUT_PAD_PULL_UP \
    (ROBOBASE_SAFETY_INPUT_PAD_PULL_DOWN | IOMUXC_SW_PAD_CTL_PAD_PUE_MASK)

static uint8_t s_estop_debug_pull_level = ROBOBASE_GPIO_HIGH;
static uint8_t s_bumper_debug_pull_level = ROBOBASE_GPIO_HIGH;

/*
 * Real M7 GPIO input backend.
 *
 * Hardware mapping:
 * - E-stop NC auxiliary contact: J25 pin 23, ECSPI2_SCLK_3V3, GPIO5_IO10.
 * - Bumper/microswitch NC contact: J25 pin 21, ECSPI2_MISO_3V3, GPIO5_IO12.
 *
 * Both contacts are active-safe NC inputs. With the normal internal pull-up
 * policy, closed contact reads 0 and open contact reads 1.
 * For no-device bench testing, the RPMsg debug command changes the internal pad
 * bias and the safety layer still consumes the actual GPIO pad status.
 */

static void robobase_configure_estop_pad(uint8_t pull_level)
{
    uint32_t pad_config = (pull_level == ROBOBASE_GPIO_HIGH) ? ROBOBASE_SAFETY_INPUT_PAD_PULL_UP :
                                                               ROBOBASE_SAFETY_INPUT_PAD_PULL_DOWN;

    IOMUXC_SetPinMux(IOMUXC_ECSPI2_SCLK_GPIO5_IO10, 1U);
    IOMUXC_SetPinConfig(IOMUXC_ECSPI2_SCLK_GPIO5_IO10, pad_config);
}

static void robobase_configure_bumper_pad(uint8_t pull_level)
{
    uint32_t pad_config = (pull_level == ROBOBASE_GPIO_HIGH) ? ROBOBASE_SAFETY_INPUT_PAD_PULL_UP :
                                                               ROBOBASE_SAFETY_INPUT_PAD_PULL_DOWN;

    IOMUXC_SetPinMux(IOMUXC_ECSPI2_MISO_GPIO5_IO12, 1U);
    IOMUXC_SetPinConfig(IOMUXC_ECSPI2_MISO_GPIO5_IO12, pad_config);
}

void robobase_safety_inputs_init(void)
{
    robobase_configure_estop_pad(s_estop_debug_pull_level);
    robobase_configure_bumper_pad(s_bumper_debug_pull_level);

    GPIO5->IMR &= ~((1UL << ROBOBASE_SAFETY_ESTOP_GPIO_INDEX) | (1UL << ROBOBASE_SAFETY_BUMPER_GPIO_INDEX));
    GPIO5->GDIR &= ~((1UL << ROBOBASE_SAFETY_ESTOP_GPIO_INDEX) | (1UL << ROBOBASE_SAFETY_BUMPER_GPIO_INDEX));
}

void robobase_safety_inputs_sample(robobase_safety_inputs_t *inputs)
{
    if (inputs == 0)
    {
        return;
    }

    inputs->estop_nc_closed =
        (GPIO_PinReadPadStatus(GPIO5, ROBOBASE_SAFETY_ESTOP_GPIO_INDEX) == ROBOBASE_GPIO_LOW) ? 1U : 0U;
    inputs->bumper_nc_closed =
        (GPIO_PinReadPadStatus(GPIO5, ROBOBASE_SAFETY_BUMPER_GPIO_INDEX) == ROBOBASE_GPIO_LOW) ? 1U : 0U;

}

void robobase_safety_inputs_set_debug_gpio_level(uint8_t estop_gpio_level, uint8_t bumper_gpio_level,
                                                 uint32_t valid_mask)
{
    if ((valid_mask & RB_SAFE_DEBUG_INPUT_ESTOP) != 0U)
    {
        s_estop_debug_pull_level = (estop_gpio_level == 0U) ? ROBOBASE_GPIO_LOW : ROBOBASE_GPIO_HIGH;
        robobase_configure_estop_pad(s_estop_debug_pull_level);
    }

    if ((valid_mask & RB_SAFE_DEBUG_INPUT_BUMPER) != 0U)
    {
        s_bumper_debug_pull_level = (bumper_gpio_level == 0U) ? ROBOBASE_GPIO_LOW : ROBOBASE_GPIO_HIGH;
        robobase_configure_bumper_pad(s_bumper_debug_pull_level);
    }
}
