/*!
  \brief 用最簡單的 GPIO 方式控制 LED 閃爍
  \author kalvinchiang@gmail.com
  \date 2026-01-31
 */
#include <stdio.h>
#include "pico/stdlib.h"

#define LED_1       6   //<! 第一顆 LED
#define LED_2       7   //<! 第二顆 LED
#define LED_3       8   //<! 第三顆 LED
#define LED_4       9   //<! 第四顆 LED

/*!
  \brief 以 1Hz 頻率和指定的佔空比閃爍 LED
  \param pin  要閃爍的 LED 腳位
  \param duty 佔空比 (0.0 ~ 1.0)
  \note 1Hz 閃爍，即每秒閃爍一次
        - 佔空比 50%，即亮 0.5 秒，滅 0.5 秒
        - 佔空比 75%，即亮 0.75 秒，滅 0.25 秒
        - 佔空比 25%，即亮 0.25 秒，滅 0.75 秒
        - 佔空比 10%，即亮 0.1 秒，滅 0.9 秒
 */
void blink_1hz(uint pin, float duty)
{
    absolute_time_t start_time = get_absolute_time();

    uint32_t high_time = 1000 * duty;
    uint32_t low_time = 1000 * (1 - duty);

    while(1)
    {
        gpio_put(pin, 0);
        sleep_ms(low_time);
        gpio_put(pin, 1);
        sleep_ms(high_time);

        if (absolute_time_diff_us(start_time, get_absolute_time()) >= 3000000)
        {
            gpio_put(pin, 0);
            break;
        }
    }
}

void init_gpio()
{
    // ------------------------------------
    // 使用 GPIO 輸出的起手式
    // 初始化、方向設定、初始狀態設定
    // ------------------------------------

    gpio_init(LED_1);
    gpio_init(LED_2);
    gpio_init(LED_3);
    gpio_init(LED_4);

    gpio_set_dir(LED_1, GPIO_OUT);
    gpio_set_dir(LED_2, GPIO_OUT);
    gpio_set_dir(LED_3, GPIO_OUT);
    gpio_set_dir(LED_4, GPIO_OUT);

    gpio_put(LED_1, 0);
    gpio_put(LED_2, 0);
    gpio_put(LED_3, 0);
    gpio_put(LED_4, 0);
}

int main()
{
    stdio_init_all();

    init_gpio();
    
    while (true) 
    {
        blink_1hz(LED_1, 0.5);
        blink_1hz(LED_2, 0.75);
        blink_1hz(LED_3, 0.25);
        blink_1hz(LED_4, 0.1);
    }
}