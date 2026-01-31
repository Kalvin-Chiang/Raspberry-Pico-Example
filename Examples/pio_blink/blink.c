/*!
  \brief 以 PIO 狀態機控制多個 LED 閃爍的範例程式。
  \author kalvinchiang@gmail.com
  \date 2026-01-31
 */

#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "blink.pio.h"

#define LED_1       6   //<! 第一顆 LED
#define LED_2       7   //<! 第二顆 LED
#define LED_3       8   //<! 第三顆 LED
#define LED_4       9   //<! 第四顆 LED

/*!
  \brief 讓指定的 PIO 狀態機控制指定的腳位以指定頻率閃爍
  \param pio 指定的 PIO 執行個體
  \param sm 指定的 PIO 狀態機編號
  \param offset 指定的 PIO 程式偏移量
  \param pin 要閃爍的 GPIO 腳位編號
  \param freq 閃爍頻率 (Hz)
 */
void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq);

PIO pio[2];     //<! 宣告二個 PIO 執行個體
uint sm[2];     //<! 宣告二個狀態機
uint offset[2]; //<! 宣告二個 PIO 程式偏移量

int main() 
{
    stdio_init_all();

    //! 找一個空閒的 PIO 狀態機，並載入 PIO 程式
    bool rc = pio_claim_free_sm_and_add_program_for_gpio_range(
        &blink_program,         //<! 要載入的 .pio 程式碼
        &pio[0],                //<! 回傳系統幫你找到的那個 PIO (可能是 PIO0, PIO1 或 PIO2)
        &sm[0],                 //<! 回傳幫你配對到的狀態機編號
        &offset[0],             //<! 回傳程式碼在記憶體中的位置
        LED_1,                  //<! 你的程式會用到的最小 GPIO 編號 
        4,                      //<! 你會用到幾根腳位(連續的)
        true                    //<! 這個有點複雜，先寫 true 就對了
    );
    
    /**
     * 如果發生錯誤，停止運作：CPU 核心停止執行主程式
     * 和 assert 不同，hard_assert 在 release 版本也會生效
     */
    hard_assert(rc);
    printf("Loaded program at %u on pio %u\n", offset[0], PIO_NUM(pio[0]));

    // ------------------------------------
    // 讓 LED_1 閃爍，頻率 4 Hz
    // ------------------------------------
    blink_pin_forever(pio[0], sm[0], offset[0], LED_1, 4);

    // ------------------------------------
    // 尋找下一個可用狀態機來控制 LED_2
    // 讓 LED_2 閃爍，頻率 3 Hz
    // ------------------------------------
    pio_sm_claim(pio[0], sm[0] + 1);
    blink_pin_forever(pio[0], sm[0] + 1, offset[0], LED_2, 3);

    // pico example 的程式考慮比較多，比較複雜，這裡先簡化掉

    pio[1] = pio[0];        //<! 使用同一個 PIO 執行個體
    sm[1] = sm[0] + 2;      //<! 每個 PIO 執行個體有 4 個狀態機，前面已經用掉 0,1，接下來用 2,3
    offset[1] = offset[0];  //<! 程式碼位置相同

    // ------------------------------------
    // 尋找下一個可用狀態機來控制 LED_3
    // 讓 LED_3 閃爍，頻率 2 Hz
    // ------------------------------------
    pio_sm_claim(pio[1], sm[1]);
    blink_pin_forever(pio[1], sm[1], offset[1], LED_3, 2);

    // ------------------------------------
    // 尋找下一個可用狀態機來控制 LED_4
    // 讓 LED_3 閃爍，頻率 1 Hz
    // ------------------------------------
    pio_sm_claim(pio[1], sm[1] + 1);
    blink_pin_forever(pio[1], sm[1] + 1, offset[1], LED_4, 1);

    // ------------------------------------
    // 釋放狀態機資源(這裡其實有沒有釋放也沒關係，因為程式不會結束)
    // 但是如果你的程式會動態載入/卸載 PIO 程式碼，就需要釋放
    // ------------------------------------
    pio_sm_unclaim(pio[1], sm[1] + 1);
    pio_sm_unclaim(pio[1], sm[1]);
    pio_sm_unclaim(pio[0], sm[0] + 1);
    pio_remove_program_and_unclaim_sm(&blink_program, pio[0], sm[0], offset[0]);
    
    // 利用了 PIO 程式碼的特性，即使沒有無窗迴圈，PIO 狀態機仍會持續運作
    printf("All leds should be flashing\n");
}

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) 
{
    // 這是由 CMake 自動產生的函式，位於 build/blink.pio.h
    blink_program_init(pio, sm, offset, pin);

    // 啟動 PIO 狀態機
    pio_sm_set_enabled(pio, sm, true);

    printf("Blinking pin %d at %d Hz\n", pin, freq);

    /**
     * 這裡傳入的值就是要給 PIO 程式中，x 暫存器使用的延遲計數值
     * 但是要拆成兩部分來看：「總目標週期數」與「扣除的開銷」
     * 
     * 總目標週期數 (Total Cycles) = 系統時鐘頻率 / (2 * 閃爍頻率)
     * 假設你要讓 LED 每秒閃爍 freq 次 (例如 1 Hz)
     * 那麼一個週期 (亮+滅) 的時間是 1/freq 秒
     * 因為是對稱方波 (亮滅各一半)，所以半個週期 (亮或滅) 的時間是 1/(2*freq)。
     * 將時間乘上 CPU 時脈速度 (clk_sys)，就是「這段亮燈時間，總共需要經過多少個時脈週期 (Cycles)」。
     * 假設算出來是 1000 個週期，代表我們希望 LED 亮著的這段時間，硬體總共消耗掉 1000 cycles。
     * 
     * N = 1000, 為什麼要 -3? 
     * 因為 PIO 程式在執行時，有一些指令會消耗掉額外的週期數
     * mov x, y        ; 1. 花費 1 cycle (把計數值載入 X)
     * set pins, 1     ; 2. 花費 1 cycle (點亮 LED)
     * jmp x-- lp1     ; 3. 這個迴圈總共花費 (N + 1) cycles
     *
     * 所以為了精確的計時，我們需要從總目標週期數中扣除這些額外的開銷
     */
    
    pio->txf[sm] = (clock_get_hz(clk_sys) / (2 * freq)) - 3;
}
