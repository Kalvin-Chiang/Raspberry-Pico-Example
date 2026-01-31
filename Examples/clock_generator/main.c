/*!
  \brief 單純使用 PIO 狀態機產生一個固定頻率的時鐘訊號
  \author kalvinchiang@gmail.com
  \date 2026-01-31
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "clk_gen.pio.h"

#define PIN_CLK     18
#define CLK_FREQ    4000000

PIO pio = pio0;     //<! PIO 模組 0
uint clk_sm = 0;    //<! PIO CLK 狀態機

void init_gpio()
{
    // -----------------------------------------------------------------------------
    // 剛上電時，腳位還是屬於 GPIO 模組管的，必須初始化才不會變成浮接有雜訊
    // ----------------------------------------------------------------------------- 

    // 初始化
    gpio_init(PIN_CLK);

    // 設定腳位的值
    gpio_put(PIN_CLK, 0); //< 要和 PIO 的行為一致!

    // 設定輸出入方向
    gpio_set_dir(PIN_CLK, GPIO_OUT);

    // 設定指定 GPIO 的驅動電流強度
    gpio_set_drive_strength(PIN_CLK, GPIO_DRIVE_STRENGTH_12MA);

    // 設定指定 GPIO 的轉換速率
    gpio_set_slew_rate(PIN_CLK, GPIO_SLEW_RATE_FAST);
}

void init_pio(uint pin_clk, float div)
{
    pio_gpio_init(pio, pin_clk);

    // 載入 PIO 程式
    uint offset = pio_add_program(pio, &clk_gen_program);

    // 取得預設設定
    pio_sm_config c = clk_gen_program_get_default_config(offset);

    // 設定 side-set pin
    sm_config_set_sideset_pins(&c, PIN_CLK);

    // 設定分頻
    sm_config_set_clkdiv(&c, div);

    // 設定方向
    pio_sm_set_consecutive_pindirs(pio, clk_sm, pin_clk, 1, true);

    // 初始化
    pio_sm_init(pio, clk_sm, offset, &c);

    // 啟動狀態機
    pio_sm_set_enabled(pio, clk_sm, true);
}

int main()
{
    stdio_init_all();

    init_gpio();

    // 計算分頻
    // 假設產生 20 MHz clock, 每個週期 2 個 PIO 指令
    // 需要 PIO 跑在 40 MHz
    // 150 MHz / 40 MHz = 3.75
    float div = (float)clock_get_hz(clk_sys) / (CLK_FREQ * 2);

    init_pio(PIN_CLK, div);

    printf("\n");
    printf(" ┌───────────────────────────────┐\n");
    printf(" │       PIO 頻率產生測試        │\n");
    printf(" └───────────────────────────────┘\n");
    printf("\n系統主頻: %d MHz\n", clock_get_hz(clk_sys) / 1000000);
    printf("目標頻率: %d MHz\n", CLK_FREQ / 1000000);
    printf("PIO 狀態機分頻值: %.4f\n", div);

    while(1);
}
