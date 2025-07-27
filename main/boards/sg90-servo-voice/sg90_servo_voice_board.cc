#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "led/single_led.h"
#include "display/oled_display.h"
#include "display/display.h"
#include "servo_controller.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#define TAG "SG90ServoVoice"

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

class SG90ServoVoiceBoard : public WifiBoard {
private:
    Button boot_button_;
    Button touch_button_;
    Button asr_button_;
    
    ServoController* servo_controller_;
    
    // 显示相关（可选）
    i2c_master_bus_handle_t display_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    bool display_enabled_ = false;

    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        
        esp_err_t ret = i2c_new_master_bus(&bus_config, &display_i2c_bus_);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "I2C总线初始化失败，禁用显示屏: %s", esp_err_to_name(ret));
            display_enabled_ = false;
            return;
        }
        display_enabled_ = true;
    }

    void InitializeSsd1306Display() {
        if (!display_enabled_) {
            display_ = new NoDisplay();
            return;
        }
        
        // SSD1306 config
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
            .scl_speed_hz = 400 * 1000,
        };

        esp_err_t ret = esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "显示屏IO初始化失败，使用无显示模式: %s", esp_err_to_name(ret));
            display_ = new NoDisplay();
            return;
        }

        ESP_LOGI(TAG, "安装SSD1306驱动");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

        ret = esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "SSD1306面板创建失败，使用无显示模式: %s", esp_err_to_name(ret));
            display_ = new NoDisplay();
            return;
        }

        ESP_LOGI(TAG, "SSD1306驱动安装成功");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "显示屏初始化失败，使用无显示模式");
            display_ = new NoDisplay();
            return;
        }

        // Set the display to on
        ESP_LOGI(TAG, "开启显示屏");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, 
                                   DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                   {&font_puhui_14_1, &font_awesome_14_1});
    }

    void InitializeButtons() {
        // 配置内置LED GPIO
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << BUILTIN_LED_GPIO,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf);

        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && 
                !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            gpio_set_level(BUILTIN_LED_GPIO, 1);
            app.ToggleChatState();
        });

        asr_button_.OnClick([this]() {
            std::string wake_word = "你好小智";
            Application::GetInstance().WakeWordInvoke(wake_word);
        });

        touch_button_.OnPressDown([this]() {
            gpio_set_level(BUILTIN_LED_GPIO, 1);
            Application::GetInstance().StartListening();
        });
        
        touch_button_.OnPressUp([this]() {
            gpio_set_level(BUILTIN_LED_GPIO, 0);
            Application::GetInstance().StopListening();
        });
    }

    void InitializeServoController() {
        ESP_LOGI(TAG, "初始化SG90舵机控制器");
        servo_controller_ = new ServoController(SERVO_GPIO);
        
        if (!servo_controller_->Initialize()) {
            ESP_LOGE(TAG, "舵机控制器初始化失败");
            delete servo_controller_;
            servo_controller_ = nullptr;
            return;
        }
        
        // 设置运动完成回调（可选，用于状态指示）
        servo_controller_->SetOnMoveCompleteCallback([this]() {
            // 运动完成时闪烁LED
            gpio_set_level(BUILTIN_LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(BUILTIN_LED_GPIO, 0);
        });

        ESP_LOGI(TAG, "SG90舵机控制器初始化完成");
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeTools() {
        if (servo_controller_ != nullptr) {
            servo_controller_->InitializeTools();
        }
    }

public:
    SG90ServoVoiceBoard() 
        : boot_button_(BOOT_BUTTON_GPIO)
        , touch_button_(TOUCH_BUTTON_GPIO)
        , asr_button_(ASR_BUTTON_GPIO)
        , servo_controller_(nullptr) {
        
        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        InitializeServoController();
        InitializeTools();
    }

    virtual ~SG90ServoVoiceBoard() {
        if (servo_controller_) {
            delete servo_controller_;
        }
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, 
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(SG90ServoVoiceBoard);
