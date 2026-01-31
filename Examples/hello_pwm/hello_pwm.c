/*!
  \brief 以 pico 的 pwm 方式點亮 LED
  \author kalvinchiang@gmail.com
  \date 2026-01-31
 */

#include "pico/stdlib.h"
#include "hardware/pwm.h"

// ------------------------------------
// 同一個 PWM Slice 底下的兩個 Channel
// ------------------------------------
#define LED_1       6   //<! 第一顆 LED
#define LED_2       7   //<! 第二顆 LED

// ------------------------------------
// 同一個 PWM Slice 底下的兩個 Channel
// ------------------------------------
#define LED_3       8   //<! 第三顆 LED
#define LED_4       9   //<! 第四顆 LED

int main() 
{
    // 指派 PWM 功能給 GPIO 6(LED_1)
    gpio_set_function(LED_1, GPIO_FUNC_PWM);
    gpio_set_function(LED_2, GPIO_FUNC_PWM);

    /** 查詢 GPIO 6(LED_1) 屬於哪個 Slice
     * 每個 Slice 裡面有 2 個頻道 (Channel A, Channel B)
     * Slice 0	GPIO 0	GPIO 1
     * Slice 1	GPIO 2	GPIO 3
     * ...
     * 設定 PWM 的「頻率 (Frequency)」時，是對 整個 Slice 設定的，而不是對單一腳位。
     * 所以同一個 Slice 底下的兩個 Channel (A、B) 會共用同一個頻率設定。
     * 但兩個 Channel 的「佔空比 (Duty Cycle)」可以獨立設定。
     */
    uint slice_num = pwm_gpio_to_slice_num(LED_1);

    /** 決定 PWM 的「週期長度」(也可以理解為解析度)
     * 例如 Wrap = 65535 (16-bit)，你可以把亮度調成 1/65535 的微光
     * 若 Wrap = 9，你只有 0~9 共 10 個亮度等級可選。調一點點亮度就跳很大
     */
    pwm_set_wrap(slice_num, 1023);

    /** 與 pwm_set_wrap 配合使用，設定 Channel A 的「佔空比」(Duty Cycle)
     * 如果你設 wrap = 100，然後設 level = 50 -> 輸出 50% Duty Cycle。
     * 如果你設 wrap = 1000，然後設 level = 50 -> 輸出 5% Duty Cycle。
     * 
     * Level = (Wrap + 1) * DutyCycle
     */
    pwm_set_chan_level(slice_num, PWM_CHAN_A, 100);     // 10% 亮度
    pwm_set_chan_level(slice_num, PWM_CHAN_B, 1024);    // 100% 亮度

    // 啟動 PWM
    pwm_set_enabled(slice_num, true);
}
