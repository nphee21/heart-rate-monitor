#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "driver/adc.h"
#include "driver/gpio.h"

#include "lvgl.h"
#include "ili9341_lcd.h"

#include "ui.h"
#include "screens.h"

static const char *TAG = "Heartrate";

// CONFIG 
#define NUMBER_OF_POINTS 83

// Pulse sensor OUT -> GPIO34 (ADC1_CH6)
#define PULSE_ADC_CH        ADC1_CHANNEL_6
#define PULSE_ADC_ATTEN     ADC_ATTEN_DB_11

#define FINGER_THRESHOLD    200
#define FINGER_STABLE_MS    800

// Buzzer + LED
#define BUZZER_GPIO   26
#define LED_GPIO      25
#define BUZZER_ON_MS  100
#define LED_ON_MS     100

static bool     buzzer_on      = false;
static bool     led_on         = false;
static uint32_t buzzer_on_time = 0;
static uint32_t led_on_time    = 0;

// LVGL tick
static uint32_t lastTick = 0;

// Chart series
static lv_chart_series_t *ser_HB = NULL;

// BPM VARS
int pulse_Signal;
int highest_Signal, lowest_Signal;
int heartbeat_Detc_Val;
int highest_lowest_Tolerance_Val = 0;
bool detection_Sta = true;

uint32_t previousMillis_HB_Detection = 0;
const uint32_t interval_HB_Detection = 20;    // ~50 Hz

uint32_t previousMillis_Get_BPM = 0;
const uint32_t interval_Get_BPM = 1000;
int timer_Get_BPM = 0;
int ts_Hour = 0;
int ts_Minute = 0;
int ts_Second = 0;

bool get_BPM = false;
int cnt_HB = 0;
int BPM_Val = 0;

// BPM từ khoảng thời gian giữa 2 beat
#define BPM_BUF_SIZE 8
uint32_t lastBeatMs = 0;
float bpmBuffer[BPM_BUF_SIZE];
int bpmIndex = 0;
int bpmValid = 0;

#define MA_WINDOW 5
int   maBuf[MA_WINDOW];
long  maSum       = 0;
int   maIndex     = 0;
int   maCount     = 0;

static inline uint32_t millis(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static int map_int(int x, int in_min, int in_max, int out_min, int out_max) {
    if (in_max == in_min) return out_min;
    int32_t num = (int32_t)(x - in_min) * (out_max - out_min);
    int32_t den = (in_max - in_min);
    return out_min + (int)(num / den);
}

static int read_pulse_raw_10bit(void) {
    int raw12 = adc1_get_raw(PULSE_ADC_CH); // 0..4095
    if (raw12 < 0) raw12 = 0;
    if (raw12 > 4095) raw12 = 4095;
    return raw12 >> 2; // 12-bit -> 10-bit
}

static int filterPulse(int newSample) {
    maSum -= maBuf[maIndex];
    maBuf[maIndex] = newSample;
    maSum += newSample;

    maIndex++;
    if (maIndex >= MA_WINDOW) maIndex = 0;

    if (maCount < MA_WINDOW) maCount++;

    return (int)(maSum / maCount);
}

// LVGL + UI tick
static void ui_update(void) {
    uint32_t now = millis();
    uint32_t diff = now - lastTick;
    lv_tick_inc(diff);
    lastTick = now;

    lv_timer_handler();
    ui_tick();  // tick EEZ UI
}

// GPIO buzzer + led
static void init_gpio_beep_led(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO) | (1ULL << LED_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(BUZZER_GPIO, 0);
    gpio_set_level(LED_GPIO, 0);
}

//  SETUP CHART & LABEL
static void set_initial_display(void) {
    // BPM label
    if (objects.label_bpm) {
        lv_label_set_text(objects.label_bpm, "000");
    }

    // Chart
    if (objects.obj1) {
        lv_chart_set_type(objects.obj1, LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(objects.obj1, NUMBER_OF_POINTS);
        lv_chart_set_range(objects.obj1, LV_CHART_AXIS_PRIMARY_Y, 0, 80);
        lv_obj_set_style_size(objects.obj1, 0, 0, LV_PART_INDICATOR);

        ser_HB = lv_chart_add_series(objects.obj1,
                                     lv_color_hex(0xffff696a),
                                     LV_CHART_AXIS_PRIMARY_Y);

        for (int i = 0; i < NUMBER_OF_POINTS; i++) {
            lv_chart_set_next_value(objects.obj1, ser_HB, 0);
        }
        lv_chart_refresh(objects.obj1);
    }

    ui_update();
}

// CALIBRATION 10s
static void pulse_sensor_calibration(void) {
    for (int i = 0; i < 20; i++) {
        ui_update();
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    // Reset chart
    if (objects.obj1 && ser_HB) {
        for (int i = 0; i < NUMBER_OF_POINTS; i++) {
            lv_chart_set_next_value(objects.obj1, ser_HB, 0);
        }
        lv_chart_refresh(objects.obj1);
        ui_update();
    }

    // 1. Chờ đặt tay
    if (objects.label_header) {
        lv_label_set_text(objects.label_header, "Touch the sensor to start");
        ui_update();
    }

    uint32_t stableStart = 0;

    while (true) {
        uint32_t now = millis();
        int rawPulse = read_pulse_raw_10bit();

        if (rawPulse > FINGER_THRESHOLD) {
            if (stableStart == 0) {
                stableStart = now;
            } else {
                if (now - stableStart >= FINGER_STABLE_MS) {
                    break; // giữ tay đủ lâu
                }
            }
        } else {
            stableStart = 0;
        }

        int sig = map_int(rawPulse, 0, 1023, 0, 80);
        if (objects.obj1 && ser_HB) {
            lv_chart_set_next_value(objects.obj1, ser_HB, sig);
            lv_chart_refresh(objects.obj1);
        }

        ui_update();
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    // 2. Đếm ngược 4--->0
    if (objects.label_header) {
        char msg[64];
        for (int i = 4; i >= 0; i--) {
            snprintf(msg, sizeof(msg), "Touch the pulse sensor %d", i);
            lv_label_set_text(objects.label_header, msg);
            ui_update();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        lv_label_set_text(objects.label_header, "Calibration in progress...");
        ui_update();
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // 3. Calib 10s
    highest_Signal = 0;
    lowest_Signal  = 1023;

    const uint32_t interval_Get_Signal = 20;
    const uint32_t interval_Cal_Duration = 1000;

    uint32_t previousMillis_Get_Signal = 0;
    uint32_t previousMillis_Cal_Duration = 0;
    int cal_Duration = 10;

    while (true) {
        uint32_t now = millis();

        // Lấy mẫu
        if (now - previousMillis_Get_Signal >= interval_Get_Signal) {
            previousMillis_Get_Signal = now;

            int rawPulse = read_pulse_raw_10bit();

            if (rawPulse > highest_Signal) highest_Signal = rawPulse;
            if (rawPulse < lowest_Signal)  lowest_Signal  = rawPulse;

            int signal_HB = map_int(rawPulse, 0, 1023, 0, 80);

            if (objects.obj1 && ser_HB) {
                lv_chart_set_next_value(objects.obj1, ser_HB, signal_HB);
                lv_chart_refresh(objects.obj1);
            }

            ui_update();
        }

        // Đếm thời gian calib
        if (now - previousMillis_Cal_Duration >= interval_Cal_Duration) {
            previousMillis_Cal_Duration = now;

            cal_Duration--;

            if (objects.label_header) {
                char str[64];
                snprintf(str, sizeof(str), "Calibration in progress... %d", cal_Duration);
                lv_label_set_text(objects.label_header, str);
                ui_update();
            }

            if (cal_Duration < 1) {
                heartbeat_Detc_Val = lowest_Signal +
                                     (highest_Signal - lowest_Signal) / 2;
                highest_lowest_Tolerance_Val =
                    (highest_Signal - lowest_Signal) / 6;
                break;
            }
        }
    }

    // 4. Kết thúc calib
    if (objects.label_header) {
        lv_label_set_text(objects.label_header, "HEART RATE");
        ui_update();
    }

    BPM_Val = 0;
    lastBeatMs = 0;
    bpmIndex = 0;
    bpmValid = 0;
    for (int i = 0; i < BPM_BUF_SIZE; i++) bpmBuffer[i] = 0;

    if (objects.label_bpm) {
        lv_label_set_text(objects.label_bpm, "000");
    }
}

// Main
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());

    // ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(PULSE_ADC_CH, PULSE_ADC_ATTEN);

    // LED + buzzer
    init_gpio_beep_led();

    // LVGL
    lv_init();

    // LCD + display LVGL mặc định
    lv_display_t *disp = ili9341_lcd_init();
    (void)disp;

    lastTick = millis();

    // Init UI EEZ
    ui_init();

    // Chart + BPM label theo UI của bạn
    set_initial_display();

    // Calibration 10s
    pulse_sensor_calibration();

    // Reset BPM logic
    timer_Get_BPM = 0;
    cnt_HB = 0;
    BPM_Val = 0;
    ts_Second = ts_Minute = ts_Hour = 0;
    get_BPM = true;

    previousMillis_HB_Detection = millis();
    previousMillis_Get_BPM      = millis();

    ESP_LOGI(TAG, "Start loop");

    while (1) {
        uint32_t now = millis();

        // Đọc ADC + vẽ chart + detect nhịp
        if (now - previousMillis_HB_Detection >= interval_HB_Detection) {
            previousMillis_HB_Detection = now;

            int rawPulse = read_pulse_raw_10bit();
            pulse_Signal = filterPulse(rawPulse);

            // Detect beat
            if (pulse_Signal > heartbeat_Detc_Val && detection_Sta == true) {
                detection_Sta = false;

                // Bật buzzer + LED
                gpio_set_level(BUZZER_GPIO, 1);
                buzzer_on      = true;
                buzzer_on_time = now;

                gpio_set_level(LED_GPIO, 1);
                led_on      = true;
                led_on_time = now;

                if (get_BPM) {
                    cnt_HB++;

                    if (lastBeatMs != 0) {
                        uint32_t interval = now - lastBeatMs;

                        if (interval > 300 && interval < 2000) {
                            float bpm_inst = 60000.0f / (float)interval;

                            if (bpm_inst >= 50.0f && bpm_inst <= 130.0f) {
                                bpmBuffer[bpmIndex] = bpm_inst;
                                bpmIndex = (bpmIndex + 1) % BPM_BUF_SIZE;
                                if (bpmValid < BPM_BUF_SIZE) bpmValid++;

                                float sum = 0;
                                for (int i = 0; i < bpmValid; i++) sum += bpmBuffer[i];
                                BPM_Val = (int)(sum / bpmValid + 0.5f);

                                char s[8];
                                snprintf(s, sizeof(s), "%d", BPM_Val);
                                if (objects.label_bpm) {
                                    lv_label_set_text(objects.label_bpm, s);
                                }
                            }
                        }
                    }
                    lastBeatMs = now;
                }
            }
            if (pulse_Signal < heartbeat_Detc_Val) {
                detection_Sta = true;
            }

            // Giới hạn theo tolerance
            if (pulse_Signal > highest_Signal + highest_lowest_Tolerance_Val)
                pulse_Signal = highest_Signal + highest_lowest_Tolerance_Val;
            if (pulse_Signal < lowest_Signal - highest_lowest_Tolerance_Val)
                pulse_Signal = lowest_Signal - highest_lowest_Tolerance_Val;

            int signal_HB = map_int(
                pulse_Signal,
                lowest_Signal - highest_lowest_Tolerance_Val,
                highest_Signal + highest_lowest_Tolerance_Val,
                0, 80
            );

            if (objects.obj1 && ser_HB) {
                lv_chart_set_next_value(objects.obj1, ser_HB, signal_HB);
                lv_chart_refresh(objects.obj1);
            }

            ui_update();
        }

        // Đếm thời gian & log mỗi 10s
        if (now - previousMillis_Get_BPM >= interval_Get_BPM) {
            previousMillis_Get_BPM = now;

            if (get_BPM) {
                ts_Second++;
                if (ts_Second > 59) {
                    ts_Second = 0;
                    ts_Minute++;
                    if (ts_Minute > 59) {
                        ts_Minute = 0;
                        ts_Hour++;
                    }
                }

                timer_Get_BPM++;
                if (timer_Get_BPM > 9) {
                    timer_Get_BPM = 0;
                    ESP_LOGI(TAG, "Time %02d:%02d:%02d  BPM_avg=%d",
                             ts_Hour, ts_Minute, ts_Second, BPM_Val);
                    cnt_HB = 0;
                }
            }
        }

        // Tắt buzzer
        if (buzzer_on && (millis() - buzzer_on_time >= BUZZER_ON_MS)) {
            gpio_set_level(BUZZER_GPIO, 0);
            buzzer_on = false;
        }

        // Tắt LED
        if (led_on && (millis() - led_on_time >= LED_ON_MS)) {
            gpio_set_level(LED_GPIO, 0);
            led_on = false;
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
