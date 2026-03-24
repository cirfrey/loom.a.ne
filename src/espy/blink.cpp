#include "espy/blink.hpp"

namespace espy
{
    blink_controller blink;
}

auto espy::create_blink_task(blink_task_params* bp) -> TaskHandle_t
{
    gpio_num_t pin = (gpio_num_t)bp->gpio_pin;
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    TaskHandle_t handle;
    xTaskCreate([](void* tparams){
        auto bp = (blink_task_params*)tparams;

        auto sw = stopwatch();
        sw.click();

        while(1)
        {
            vTaskDelay(pdMS_TO_TICKS(10));

            u32 dt = (u32)sw.click().last_segment<std::chrono::microseconds>();

            bool next_state = bp->bc.tick(dt);
            gpio_set_level((gpio_num_t)bp->gpio_pin, next_state);
        }
    }, "espy::blink", 2048, bp, 5, &handle);
    return handle;
}
