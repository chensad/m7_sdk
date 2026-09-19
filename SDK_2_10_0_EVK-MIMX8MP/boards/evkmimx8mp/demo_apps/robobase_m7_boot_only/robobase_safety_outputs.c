#include "robobase_safety_outputs.h"

#include "fsl_device_registers.h"
#include "fsl_iomuxc.h"

#define ROBOBASE_SAFETY_ALLOW_MASK (1UL << ROBOBASE_SAFETY_ALLOW_GPIO_INDEX)

#define ROBOBASE_SAFETY_OUTPUT_PAD_CONFIG (IOMUXC_SW_PAD_CTL_PAD_DSE(1U))

void robobase_safety_outputs_init(void)
{
    IOMUXC_SetPinMux(IOMUXC_ECSPI2_MOSI_GPIO5_IO11, 0U);
    IOMUXC_SetPinConfig(IOMUXC_ECSPI2_MOSI_GPIO5_IO11, ROBOBASE_SAFETY_OUTPUT_PAD_CONFIG);

    GPIO5->IMR &= ~ROBOBASE_SAFETY_ALLOW_MASK;
    GPIO5->DR &= ~ROBOBASE_SAFETY_ALLOW_MASK;
    GPIO5->GDIR |= ROBOBASE_SAFETY_ALLOW_MASK;
}

void robobase_safety_outputs_set_safety_allow(uint8_t allow)
{
    if (allow == 0U)
    {
        GPIO5->DR &= ~ROBOBASE_SAFETY_ALLOW_MASK;
    }
    else
    {
        GPIO5->DR |= ROBOBASE_SAFETY_ALLOW_MASK;
    }
}
