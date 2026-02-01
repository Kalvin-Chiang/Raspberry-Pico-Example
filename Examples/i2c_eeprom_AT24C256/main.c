/*!
  \brief ATMEL AT24C256 I2C EEPROM 範例
  \author kalvinchiang@gmail.com
  \date 2026-01-31
 */
#include <hardware/i2c.h>
#include <pico/i2c_slave.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include <string.h>

/** AT24C256C Spec
 * 
 * 32,768 x 8 (256Kb) = 32KB
 * VCC = 1.7V to 5.5V
 * 
 * The AT24C256C is internally organized as 512 pages of 64 bytes each.
 * 
 * 100 kHz Standard mode, 1.7V to 5.5V
 * 400 kHz Fast mode, 1.7V to 5.5V
 * MHz Fast Mode Plus (FM+), 2.5V to 5.5V
 * 
 * I2C 位址由固定高 4 位元 1010 和 3 位元硬體設定位元 A2 A1 A0 組成，範圍為 0x50 至 0x57
 * 若全部接地，預設位址為 0x50，最後一位元為讀寫控制位元
 * 
 */

//! 使用官方 C SDK，用 7-bit 位址就可以了
static const uint AT24C256_ADDRESS = 0x50;

//! I2C 通訊速率 100 kHz
static const uint I2C_BAUDRATE = 100000;

#define PAGE_SIZE 64 //<! 每頁 64 Bytes

#define I2C_PORT    i2c_default                 //<! 使用預設 I2C 埠 (i2c0 或 i2c1)
#define I2C_SDA     PICO_DEFAULT_I2C_SDA_PIN    //<! GP4
#define I2C_SCL     PICO_DEFAULT_I2C_SCL_PIN    //<! GP5

// -----------------------------------------------------------------------------
// I2C 初始化
// -----------------------------------------------------------------------------
void setup_i2c() 
{
    // ------------------------------------
    // 使用 GPIO 輸出的起手式
    // 初始化、方向設定、功能設定、初始狀態設定
    // ------------------------------------

    gpio_init(I2C_SDA);
    gpio_init(I2C_SCL);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);

    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    i2c_init(I2C_PORT, I2C_BAUDRATE);
}

// -----------------------------------------------------------------------------
// EEPROM API
// -----------------------------------------------------------------------------

//! ACK 查詢
void eeprom_wait_ready();
//! 連續讀取多個 Byte
void eeprom_read_buffer(uint16_t addr, uint8_t *buf, size_t len);
//! 智慧型寫入 (自動分頁)
void eeprom_write_buffer(uint16_t addr, const uint8_t *data, size_t len);
//! 寫入之前，先讀取該位址的值。只有當新值跟舊值不一樣時，才執行寫入。
void eeprom_update_byte(uint16_t addr, uint8_t new_val);

void eeprom_wait_ready() 
{
    uint8_t dummy;
    int ret;
    // 設定一個較短的 timeout，例如 1ms
    // 不斷嘗試讀取，直到收到 ACK (ret >= 0)
    do 
    {
        // 嘗試讀取 1 個 byte
        // 這裡單純用 write 測試 address 是否有 ACK
        ret = i2c_write_blocking(I2C_PORT, AT24C256_ADDRESS, &dummy, 1, false);
        if (ret < 0) {
            sleep_us(100); // 稍微等一下再試，避免佔用太多 Bus 頻寬
        }
    } while (ret < 0);
}

void _eeprom_write_page_raw(uint16_t mem_addr, const uint8_t *data, size_t len) 
{
    // 準備 I2C Buffer: 2 bytes Address + Data
    // 這裡用 stack 宣告陣列，大小為 Page Size + 2
    uint8_t buf[PAGE_SIZE + 2]; 
    
    buf[0] = (mem_addr >> 8) & 0xFF; // High Byte
    buf[1] = mem_addr & 0xFF;        // Low Byte
    memcpy(&buf[2], data, len);      // 複製數據

    // 發送 (Address + Data)
    i2c_write_blocking(I2C_PORT, AT24C256_ADDRESS, buf, len + 2, false);
    
    // 等待 EEPROM 寫入完成
    eeprom_wait_ready(); 
}

void eeprom_write_buffer(uint16_t addr, const uint8_t *data, size_t len) 
{
    size_t remaining_len = len;
    size_t offset = 0;
    
    while (remaining_len > 0) 
    {
        // 計算這一頁還剩多少空間
        // (例如 addr=60, 64 - (60%64) = 4 bytes)
        size_t space_in_page = PAGE_SIZE - (addr % PAGE_SIZE);
        
        // 這次要寫多少？取「剩餘資料長度」和「頁面剩餘空間」的最小值
        size_t chunk_size = (remaining_len < space_in_page) ? remaining_len : space_in_page;
        
        // 執行底層寫入
        _eeprom_write_page_raw(addr, &data[offset], chunk_size);
        
        // 更新指標與計數
        addr += chunk_size;
        offset += chunk_size;
        remaining_len -= chunk_size;
    }
}

void eeprom_read_buffer(uint16_t addr, uint8_t *buf, size_t len) 
{
    // 先寫入要讀取的起始位址 (Dummy Write)
    uint8_t reg_addr[2];
    reg_addr[0] = (addr >> 8) & 0xFF;
    reg_addr[1] = addr & 0xFF;
    
    // nostop = true (Repeated Start)
    i2c_write_blocking(I2C_PORT, AT24C256_ADDRESS, reg_addr, 2, true);
    
    // 一口氣讀取所有 bytes
    // I2C controller 會自動處理 ACK/NACK
    i2c_read_blocking(I2C_PORT, AT24C256_ADDRESS, buf, len, false);
}

/*! 寫入一個 Byte
  \brief 特別注意：AT24C256 的「位址」是 16-bit 跟小的 EEPROM\n
   (如 AT24C02) 不同，AT24C256 容量大，所以寫入數據時，需要發送 2 個\n
    byte 的記憶體位址 (High Byte + Low Byte)。
 */
void eeprom_write_byte(uint16_t mem_addr, uint8_t data) 
{
    uint8_t buf[3];

    // AT24C256 需要 16-bit 記憶體位址(大端序)
    // 假設 mem_addr = 0x1234 (16-bit)
    // 高位元組 (High Byte/MSB) = 0x12
    // 低位元組 (Low Byte/LSB)  = 0x34
    // 發送順序： 先送 buf[0] (0x12)，再送 buf[1] (0x34)
    // 意義：先送 高位 (High Byte)，後送低位 (Low Byte)。

    // 大端序 (Big-Endian)：高位元組 (MSB) 存放在記憶體低位址，或是在通訊中先被發送。
    // 就像我們寫阿拉伯數字 "1234"，先寫千位數 (1)，最後寫個位數 (4)。
    // 小端序 (Little-Endian)： 低位元組 (LSB) 先被發送。

    buf[0] = (mem_addr >> 8) & 0xFF; // 取出 0x12(High)，放入陣列第 0 格
    buf[1] = mem_addr & 0xFF;        // 取出 0x34(Low)，放入陣列第 1 格
    buf[2] = data;                   // Data

    // 寫入數據
    // 注意：nostop = false，表示傳完這 3 個 byte 後發送 STOP 訊號
    // 這樣 EEPROM 才會開始內部的寫入週期
    i2c_write_blocking(I2C_PORT, AT24C256_ADDRESS, buf, 3, false);
    
    // 【重要】EEPROM 寫入需要時間 (約 5ms)
    // 如果不加這行，馬上讀取會失敗
    eeprom_wait_ready();
}

/*! 讀取一個 Byte
  \brief 特別注意：AT24C256 的「位址」是 16-bit 跟小的 EEPROM\n
   (如 AT24C02) 不同，AT24C256 容量大，所以讀取數據時，需要先發送 2 個\n
    byte 的記憶體位址 (High Byte + Low Byte)，然後再讀取數據。
 */
uint8_t eeprom_read_byte(uint16_t mem_addr) 
{
    uint8_t reg_addr[2];
    uint8_t rx_data = 0;

    reg_addr[0] = (mem_addr >> 8) & 0xFF;
    reg_addr[1] = mem_addr & 0xFF;

    // 先寫入我們要讀的記憶體位址 (Dummy Write)
    // nostop = true，表示先不放手，緊接著要讀取 (Repeated Start)
    i2c_write_blocking(I2C_PORT, AT24C256_ADDRESS, reg_addr, 2, true);

    // 讀取數據
    i2c_read_blocking(I2C_PORT, AT24C256_ADDRESS, &rx_data, 1, false);

    return rx_data;
}

void eeprom_update_byte(uint16_t addr, uint8_t new_val) 
{
    uint8_t old_val = eeprom_read_byte(addr);
    if (old_val != new_val) {
        eeprom_write_byte(addr, new_val); // 只有變更時才寫，節省壽命
    }
}

// -----------------------------------------------------------------------------
// 工具函式：I2C Bus 掃描
// -----------------------------------------------------------------------------

//! 工具函式，有些位址是 I2C 協定保留的，掃描時通常會標記出來
bool reserved_addr(uint8_t addr) 
{
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

//! 工具函式，掃描 I2C Bus 上的設備
void scan_i2c_bus()
{
    printf("\nScanning I2C Bus...\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    for (int addr = 0; addr < (1 << 7); ++addr) 
    {
        if (addr % 16 == 0) {
            printf("%02x ", addr);
        }

        // 核心邏輯：嘗試從該位址讀取 1 byte
        // 我們不需要真正的數據，只需要知道對方是否 ACK
        int ret;
        uint8_t rxdata;
        if (reserved_addr(addr)) 
        {
            ret = PICO_ERROR_GENERIC; // 忽略保留位址
        } 
        else 
        {
            ret = i2c_read_blocking(I2C_PORT, addr, &rxdata, 1, false);
            //ret = i2c_read_timeout_us(I2C_PORT, addr, &rxdata, 1, false, 10000);
        }

        if (ret >= 0) 
        {
            // 找到設備！顯示位址
            printf("@ "); 
        } 
        else 
        {
            // 無回應
            printf(". ");
        }
        // 格式化輸出
        printf(addr % 16 == 15 ? "\n" : "");
    }
    
    printf("Scan complete.\n");
}

// -----------------------------------------------------------------------------
// 使用範例
// -----------------------------------------------------------------------------

#define SETTINGS_ADDR   0x0000    //<! 系統設定儲存位址
#define MAGIC_CODE      0xA55A    //<! 魔術數字，用來判斷資料有效性

/*! 系統設定結構體範例(共 40 bytes)
  \brief 包含馬達位置校正值、WiFi SSID、音量設定等
  \note 結構體大小需注意對齊 (padding) 問題，建議使用 sizeof() 檢查\n
        實際大小是否符合預期。最好是把結構體成員依照大小順序排列，\n
        減少對齊浪費的空間。
 */
typedef struct 
{
    int32_t  motor_offset;  //<! 馬達位置校正值
    uint16_t magic;         //<! 魔術數字，用來判斷資料是否有效
    uint8_t  wifi_ssid[32]; //<! WiFi SSID
    uint8_t  volume;        //<! 音量設定 (0-100)
    uint8_t  checksum;      //<! 簡單的校驗和
} SystemSettings;

//! 全域系統設定變數
SystemSettings current_settings;

//! 計算 Checksum (簡單累加法)
uint8_t calc_checksum(SystemSettings *s) 
{
    uint8_t sum = 0;
    uint8_t *ptr = (uint8_t*)s;
    // 計算除了最後一個 byte (checksum 本身) 以外的所有 bytes
    for (size_t i = 0; i < sizeof(SystemSettings) - 1; i++) 
    {
        sum += ptr[i];
    }
    return sum;
}

// 儲存設定 (包含 Update 機制，這裡簡化為全寫，建議配合上面的 Update 邏輯)
void settings_save() 
{
    // 更新 Checksum
    current_settings.checksum = calc_checksum(&current_settings);
    
    // 寫入 EEPROM
    eeprom_write_buffer(SETTINGS_ADDR, (uint8_t*)&current_settings, sizeof(SystemSettings));
    
    printf("設定已儲存。\n");
}

// 初始化設定
void settings_init() 
{
    // 從 EEPROM 讀取整個結構體
    eeprom_read_buffer(SETTINGS_ADDR, (uint8_t*)&current_settings, sizeof(SystemSettings));

    // 檢查 Magic Number 和 Checksum
    uint8_t calced_sum = calc_checksum(&current_settings);
    
    if (current_settings.magic != MAGIC_CODE || current_settings.checksum != calced_sum) 
    {
        printf("EEPROM 空白或資料損毀，載入預設值...\n");
        
        // 載入預設值
        current_settings.magic = MAGIC_CODE;
        strcpy((char*)current_settings.wifi_ssid, "MyWifi");
        current_settings.volume = 50;
        current_settings.motor_offset = 0;
        
        // 寫入 EEPROM (格式化)
        settings_save(); 
    } 
    else 
    {
        printf("設定載入成功！Wifi: %s\n", current_settings.wifi_ssid);
    }
}

int main() 
{
    // 測試從 EEPROM 讀取的設定
    SystemSettings test_read_settings;

    stdio_init_all();

    setup_i2c();

    #if !defined(i2c_default) || !defined(PICO_DEFAULT_I2C_SDA_PIN) || !defined(PICO_DEFAULT_I2C_SCL_PIN)
        printf("錯誤: 未定義預設 I2C 引腳 (請檢查 board 設定)\n");
        return 0;
    #else
        scan_i2c_bus();

        printf("SystemSettings size=%d bytes\n", sizeof(SystemSettings));

        printf("開始 EEPROM 測試...\n");

        // 一般應用的做法是先 current_settings 初始化設定，之後都透過 current_settings 操作
        // 直到需要儲存時，才呼叫 settings_save() 寫回 EEPROM
        settings_init();

        // 這裡為了測試，所以直接讀取 EEPROM 的內容到另一個變數
        eeprom_read_buffer(SETTINGS_ADDR, (uint8_t*)&test_read_settings, sizeof(SystemSettings));

        printf("讀取到的設定：\n");
        printf("  Magic: 0x%04X\n", test_read_settings.magic);
        printf("  Motor Offset: %d\n", test_read_settings.motor_offset);
        printf("  WiFi SSID: %s\n", test_read_settings.wifi_ssid);
        printf("  Volume: %d\n", test_read_settings.volume);
        printf("  Checksum: %d\n", test_read_settings.checksum);

        // 測試寫入單一 byte
        #define NEXT_BYTE_ADDR (SETTINGS_ADDR + sizeof(SystemSettings))
        uint8_t write_val = 0xAB;      // 測試寫入值

        // 寫入
        printf("\n單一 Byte 寫入測試...\n");
        printf("Writing 0x%02X to address 0x%04X...\n", write_val, NEXT_BYTE_ADDR);
        eeprom_write_byte(NEXT_BYTE_ADDR, write_val);

        // 讀取
        uint8_t read_val = eeprom_read_byte(NEXT_BYTE_ADDR);
        printf("Read back: 0x%02X\n", read_val);

        if (write_val == read_val) 
        {
            printf("測試成功！\n");
        } 
        else 
        {
            printf("測試失敗，讀到的值不對。\n");
        }

        while (1) 
        {
            tight_loop_contents();
        }

    #endif

    return 0;
}
