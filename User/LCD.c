#include "main.h"
#include "spi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "ADC_DATA.h"
#include "SD.h"
#include "LCD.h"
#include "lvgl.h"
#include <string.h>

#define LCD_CS              GPIO_PIN_11
#define LCD_DC              GPIO_PIN_12
#define LCD_RST             GPIO_PIN_13
#define LCD_BL              GPIO_PIN_14
#define LCD_SIZE            240U
#define LCD_BUFFER_ROWS     20U
#define LCD_SPI_TIMEOUT     50U
#define LCD_SAMPLE_TIMEOUT  3000U

LV_FONT_DECLARE(LCD_Font);
extern QueueHandle_t CLI_DataHandle;

static TaskHandle_t LCD_Handle;
static bool LCD_SPI_Failed;
static uint16_t LCD_Buffer[LCD_SIZE * LCD_BUFFER_ROWS] __attribute__((aligned(4)));

static HAL_StatusTypeDef LCD_WriteCommand(const uint8_t *Command, size_t Length)
{
    HAL_GPIO_WritePin(GPIOG, LCD_CS | LCD_DC, GPIO_PIN_RESET);
    HAL_StatusTypeDef Result = HAL_SPI_Transmit(&hspi3, Command, (uint16_t)Length, LCD_SPI_TIMEOUT);
    HAL_GPIO_WritePin(GPIOG, LCD_DC, GPIO_PIN_SET);
    return Result;
}

static void LCD_SendCommand(lv_display_t *Display, const uint8_t *Command, size_t CommandSize,
                            const uint8_t *Data, size_t DataSize)
{
    (void)Display;
    if (!LCD_SPI_Failed) {
        LCD_SPI_Failed = (LCD_WriteCommand(Command, CommandSize) != HAL_OK) ||
                         (DataSize != 0U && HAL_SPI_Transmit(&hspi3, Data, (uint16_t)DataSize,
                                                           LCD_SPI_TIMEOUT) != HAL_OK);
        HAL_GPIO_WritePin(GPIOG, LCD_CS, GPIO_PIN_SET);
    }
}

static void LCD_SendColor(lv_display_t *Display, const uint8_t *Command, size_t CommandSize,
                          uint8_t *Pixels, size_t Size)
{
    if (!LCD_SPI_Failed) {
        HAL_StatusTypeDef Result = LCD_WriteCommand(Command, CommandSize);
        if (Result == HAL_OK) {
            /* ST7789 receives the high byte of each RGB565 pixel first. */
            lv_draw_sw_rgb565_swap(Pixels, (uint32_t)(Size / 2U));
            Result = HAL_SPI_Transmit_DMA(&hspi3, Pixels, (uint16_t)Size);
            if (Result != HAL_OK || ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(LCD_SPI_TIMEOUT)) == 0U ||
                HAL_SPI_GetError(&hspi3) != HAL_SPI_ERROR_NONE) {
                (void)HAL_SPI_Abort(&hspi3);
                Result = HAL_ERROR;
            }
        }
        HAL_GPIO_WritePin(GPIOG, LCD_CS, GPIO_PIN_SET);
        LCD_SPI_Failed = (Result != HAL_OK);
    }
    lv_display_flush_ready(Display);
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI3) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(LCD_Handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    HAL_SPI_TxCpltCallback(hspi);
}

static lv_obj_t *LCD_Label(lv_obj_t *Parent, const char *Text, const lv_font_t *Font,
                          lv_align_t Align, int32_t X, int32_t Y, lv_color_t Color)
{
    lv_obj_t *Label = lv_label_create(Parent);
    lv_label_set_text(Label, Text);
    lv_obj_set_style_text_font(Label, Font, 0);
    lv_obj_set_style_text_color(Label, Color, 0);
    lv_obj_align(Label, Align, X, Y);
    return Label;
}

void LCD_Task(void *arg)
{
    static const char *const Names[] = {"转速", "温度", "电流", "电压"};
    static const char *const Units[] = {"rpm", "°C", "A", "V"};
    static const uint8_t CardFaults[] = {ADC_FAULT_MOTOR_ABNORMAL | ADC_FAULT_MOTOR_SPEED_STALL,
                                        ADC_FAULT_OVERTEMP, ADC_FAULT_OVERCURRENT, ADC_FAULT_UNDERVOLT};
    static const char *const FaultNames[] = {" 过流", " 温度", " 电压", " 转速", " 堵转"};
    const lv_color_t TextColor = lv_color_hex(0xF0F4F3);
    const lv_color_t MutedColor = lv_color_hex(0xAEBAB7);
    const lv_color_t NormalColor = lv_color_hex(0x83D8B0);
    const lv_color_t FaultColor = lv_color_hex(0xFF9686);
    lv_obj_t *Values[4];
    Motor_Data_t Data;
    uint32_t Sequence = 0;
    uint32_t LastSampleTick = 0;
    bool HasSample = false;
    bool WasFresh = false;
    uint8_t SD_State = SD_Online;
    (void)arg;

    LCD_Handle = xTaskGetCurrentTaskHandle();
    HAL_GPIO_WritePin(GPIOG, LCD_BL | LCD_RST, GPIO_PIN_RESET);
    vTaskDelay(pdMS_TO_TICKS(20));
    HAL_GPIO_WritePin(GPIOG, LCD_RST, GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(120));

    lv_init();
    lv_tick_set_cb(HAL_GetTick);
    lv_delay_set_cb(vTaskDelay); /* FreeRTOS tick: 1 ms. */
    lv_display_t *Display = lv_st7789_create(LCD_SIZE, LCD_SIZE, LV_LCD_FLAG_NONE,
                                             LCD_SendCommand, LCD_SendColor);
    lv_st7789_set_gap(Display, 0, 0);
    lv_st7789_set_invert(Display, true);
    lv_display_set_buffers(Display, LCD_Buffer, NULL, sizeof(LCD_Buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_obj_t *Screen = lv_display_get_screen_active(Display);
    lv_obj_remove_flag(Screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(Screen, lv_color_hex(0x14191A), 0);
    lv_obj_set_style_bg_opa(Screen, LV_OPA_COVER, 0);

    LCD_Label(Screen, "电机监测", &LCD_Font, LV_ALIGN_TOP_LEFT, 10, 9, TextColor);
    lv_obj_t *Time = LCD_Label(Screen, "---------- --:--:--", &lv_font_montserrat_12,
                               LV_ALIGN_TOP_RIGHT, -10, 12, MutedColor);
    lv_obj_t *Status = LCD_Label(Screen, "等待采样", &LCD_Font, LV_ALIGN_TOP_LEFT, 10, 35, MutedColor);

    for (uint8_t i = 0; i < 4U; i++) {
        lv_obj_t *Card = lv_obj_create(Screen);
        lv_obj_remove_style_all(Card);
        lv_obj_remove_flag(Card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(Card, 10 + (i % 2U) * 114, 63 + (i / 2U) * 73);
        lv_obj_set_size(Card, 106, 66);
        lv_obj_set_style_radius(Card, 6, 0);
        lv_obj_set_style_bg_color(Card, lv_color_hex(0x222928), 0);
        lv_obj_set_style_bg_opa(Card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(Card, 1, 0);
        lv_obj_set_style_border_color(Card, lv_color_hex(0x394441), 0);

        LCD_Label(Card, Names[i], &LCD_Font, LV_ALIGN_TOP_LEFT, 8, 6, MutedColor);
        LCD_Label(Card, Units[i], &lv_font_montserrat_12, LV_ALIGN_TOP_RIGHT, -8, 9, MutedColor);
        Values[i] = LCD_Label(Card, "--", &lv_font_montserrat_24, LV_ALIGN_BOTTOM_LEFT, 8, -5, TextColor);
    }

    lv_obj_t *Sample = LCD_Label(Screen, "采样未就绪", &LCD_Font, LV_ALIGN_BOTTOM_LEFT, 10, -9, MutedColor);
    lv_obj_t *SD = LCD_Label(Screen, SD_State ? "SD 就绪" : "SD 离线", &LCD_Font,
                             LV_ALIGN_BOTTOM_RIGHT, -10, -9, SD_State ? NormalColor : MutedColor);
    lv_refr_now(Display);
    if (!LCD_SPI_Failed) {
        HAL_GPIO_WritePin(GPIOG, LCD_BL, GPIO_PIN_SET);
    }

    while (!LCD_SPI_Failed)
    {
        bool NewSample = CLI_DataHandle != NULL && xQueuePeek(CLI_DataHandle, &Data, 0) == pdPASS &&
                         (!HasSample || Data.sequence != Sequence);
        if (NewSample) {
            HasSample = true;
            Sequence = Data.sequence;
            LastSampleTick = HAL_GetTick();
            lv_label_set_text_fmt(Time, "%04u-%02u-%02u %02u:%02u:%02u", (unsigned)Data.DataTime.year,
                                  (unsigned)Data.DataTime.month, (unsigned)Data.DataTime.day,
                                  (unsigned)Data.DataTime.hours, (unsigned)Data.DataTime.minutes,
                                  (unsigned)Data.DataTime.seconds);
            lv_label_set_text_fmt(Values[0], "%u", (unsigned)Data.Motor_rpm);
            lv_label_set_text_fmt(Values[1], "%.1f", (double)Data.Temp_val);
            lv_label_set_text_fmt(Values[2], "%.2f", (double)Data.Curr_val);
            lv_label_set_text_fmt(Values[3], "%.2f", (double)Data.Volt_val);
        }

        bool Fresh = HasSample && HAL_GetTick() - LastSampleTick < LCD_SAMPLE_TIMEOUT;
        if (NewSample || Fresh != WasFresh) {
            if (Fresh && Data.fault_code != 0U) {
                char FaultText[48] = "告警";
                for (uint8_t i = 0; i < FAULT_COUNT; i++) {
                    if (Data.fault_code & (1U << i)) {
                        strcat(FaultText, FaultNames[i]);
                    }
                }
                lv_label_set_text(Status, FaultText);
                lv_obj_set_style_text_color(Status, FaultColor, 0);
            } else {
                lv_label_set_text(Status, Fresh ? "运行正常" : "数据超时");
                lv_obj_set_style_text_color(Status, Fresh ? NormalColor : FaultColor, 0);
            }
            lv_label_set_text(Sample, Fresh ? "采样正常" : "采样中断");
            for (uint8_t i = 0; i < 4U; i++) {
                bool Alarm = Fresh && (Data.fault_code & CardFaults[i]);
                lv_obj_set_style_text_color(Values[i], Alarm ? FaultColor : (Fresh ? TextColor : MutedColor), 0);
            }
            WasFresh = Fresh;
        }

        uint8_t SD_Current = SD_Online;
        if (SD_Current != SD_State) {
            SD_State = SD_Current;
            lv_label_set_text(SD, SD_State ? "SD 就绪" : "SD 离线");
            lv_obj_set_style_text_color(SD, SD_State ? NormalColor : MutedColor, 0);
        }

        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    HAL_GPIO_WritePin(GPIOG, LCD_BL, GPIO_PIN_RESET);
    vTaskSuspend(NULL);
}
