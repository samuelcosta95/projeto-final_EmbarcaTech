#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "song.h"


// Configurações de pinos
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;
const uint MIC_PIN = 28;     
const uint BUZZER_PIN = 21;   
const uint BUTTON_A_PIN = 5;  
const uint BUTTON_B_PIN = 6;  
const uint LED_RED_PIN = 13;  
const uint LED_GREEN_PIN = 11; 
const uint LED_BLUE_PIN = 12; 

// Wi-Fi
#define WIFI_SSID "PIPOCA"
#define WIFI_PASS "33221194"

// Configurações do ADC para detecção de som
const float SOUND_OFFSET = 1.65; 
const float SOUND_THRESHOLD = 0.25; 
const float ADC_REF = 3.3;         
const int ADC_RES = 4095;          

const uint DETECTION_DURATION_MS = 10000;  // 10 segundos
const uint SAMPLE_WINDOW_MS = 50;         // Janela de amostragem
const uint MIN_ACTIVE_SAMPLES = 40;

static uint32_t sample_buffer[200] = {0};  // Buffer circular para 10s
static uint sample_index = 0;
static uint active_samples_count = 0;

static absolute_time_t detection_start_time;
static uint sound_detection_count = 0;
static bool is_detecting = false;

// Estado do sistema
volatile bool system_active = false;
volatile bool melody_active = false;

// HTML
#define HTTP_RESPONSE "HTTP/1.1 200 OK\r\n" \
                      "Content-Type: text/html; charset=UTF-8\r\n\r\n" \
                      "<!DOCTYPE html>" \
                      "<html lang='pt-BR'>" \
                      "<head>" \
                      "  <meta charset='UTF-8'>" \
                      "  <style>" \
                      "    body { font-family: Arial, sans-serif; background-color: #f0f0f0; margin: 20px; }" \
                      "    .container { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); max-width: 400px; margin: 0 auto; }" \
                      "    h1 { color: #2c3e50; text-align: center; }" \
                      "    .btn { display: inline-block; padding: 10px 20px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; text-decoration: none; font-weight: bold; }" \
                      "    .btn-on { background: #27ae60; color: white; }" \
                      "    .btn-off { background: #c0392b; color: white; }" \
                      "    .btn:hover { opacity: 0.9; }" \
                      "  </style>" \
                      "</head>" \
                      "<body>" \
                      "  <div class='container'>" \
                      "    <h1>Babá Eletrônica</h1>" \
                      "    <div style='text-align: center;'>" \
                      "      <a href='/system/on' class='btn btn-on'>Ligar Sistema</a>" \
                      "      <a href='/system/off' class='btn btn-off'>Desligar Sistema</a>" \
                      "    </div>" \
                      "  </div>" \
                      "</body>" \
                      "</html>\r\n"

// Funções do Webserver
static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *request = (char *)p->payload;
    if (strstr(request, "GET /system/on")) {
        system_active = true;
    } else if (strstr(request, "GET /system/off")) {
        system_active = false;
        melody_active = false;
    }

    tcp_write(tpcb, HTTP_RESPONSE, strlen(HTTP_RESPONSE), TCP_WRITE_FLAG_COPY);
    pbuf_free(p);
    return ERR_OK;
}

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);
    return ERR_OK;
}

static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    tcp_bind(pcb, IP_ADDR_ANY, 80);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
}

// Inicialização do PWM para o buzzer
void pwm_init_buzzer(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f); 
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(pin, 0); 
}

// Configuração dos LEDs de estado
void configure_leds() {
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, 0);

    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, 0);

    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, 1); 
}

// Estado dos LEDs
void update_led_status(bool active, bool sound_detected) {
    if (!active) {
        gpio_put(LED_RED_PIN, 0);   
        gpio_put(LED_GREEN_PIN, 0);
        gpio_put(LED_BLUE_PIN, 1); 
    } else if (sound_detected) {
        gpio_put(LED_RED_PIN, 1);   
        gpio_put(LED_GREEN_PIN, 0); 
        gpio_put(LED_BLUE_PIN, 0);  
    } else {
        gpio_put(LED_RED_PIN, 0);  
        gpio_put(LED_GREEN_PIN, 1); 
        gpio_put(LED_BLUE_PIN, 0);  
    }
}

// Tocar uma nota
void play_tone(uint pin, uint frequency, uint duration_ms) {
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t top = clock_freq / frequency - 1;

    pwm_set_wrap(slice_num, top);
    pwm_set_gpio_level(pin, top / 2); 

    sleep_ms(duration_ms);
    pwm_set_gpio_level(pin, 0); 
    sleep_ms(30);
}

// Reproduzir melodia
void play_melody(uint pin) {
    melody_active = true;
    while (melody_active) {
        for (int i = 0; i < sizeof(melody_notes)/sizeof(melody_notes[0]); i++) {
            if (!melody_active) break;
            play_tone(pin, melody_notes[i], melody_durations[i]);
            if (gpio_get(BUTTON_B_PIN) == 0) {
                melody_active = false;
                break;
            }
        }
    }
}

int main() {
    stdio_init_all();
    adc_init();
    adc_gpio_init(MIC_PIN);
    adc_select_input(2); 

    // Inicializa hardware
    pwm_init_buzzer(BUZZER_PIN);
    configure_leds();
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init();

    // Botões
    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);
    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_B_PIN);

    // Display
    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&frame_area);
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);
    ssd1306_draw_string(ssd, 0, 0, "Baba Eletronica");
    ssd1306_draw_string(ssd, 0, 16, "Aguardando ativacao...");
    render_on_display(ssd, &frame_area);

    // Wi-Fi
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o Wi-Fi\n");
        
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)){
        printf("Falha ao conectar ao Wi-Fi\n");
        
    } 
    printf("Conectado!\nIP: %d.%d.%d.%d\n", 
        (int)(cyw43_state.netif[0].ip_addr.addr & 0xFF),
        (int)((cyw43_state.netif[0].ip_addr.addr >> 8) & 0xFF),
        (int)((cyw43_state.netif[0].ip_addr.addr >> 16) & 0xFF),
        (int)((cyw43_state.netif[0].ip_addr.addr >> 24) & 0xFF));
    
    printf("Wi-Fi conectado!\n");
    start_http_server();

    // Loop principal
    bool previous_state = system_active;
    while (true) {
        // Botões
        if (gpio_get(BUTTON_A_PIN) == 0) {
            system_active = true;
            sleep_ms(200);
        }
        if (gpio_get(BUTTON_B_PIN) == 0) {
            system_active = false;
            melody_active = false;
            sleep_ms(200);
        }

        // Atualiza display se estado mudar
        if (system_active != previous_state) {
            update_led_status(system_active, false);
            ssd1306_draw_string(ssd, 0, 16, system_active ? "Sistema ativado    " : "Sistema desativado ");
            render_on_display(ssd, &frame_area);
            previous_state = system_active;
        }

        // Detecção de som
        if (system_active) {
            uint16_t raw_adc = adc_read();
            float voltage = (raw_adc * ADC_REF) / ADC_RES;
            float sound_level = fabs(voltage - SOUND_OFFSET);

            if (system_active && !melody_active) {
                static absolute_time_t last_sample;
                
                if (absolute_time_diff_us(last_sample, get_absolute_time()) >= SAMPLE_WINDOW_MS * 1000) {
                    last_sample = get_absolute_time();
                    
                    // Leitura e processamento do ADC
                    uint16_t raw_adc = adc_read();
                    float voltage = (raw_adc * ADC_REF) / ADC_RES;
                    float sound_level = fabs(voltage - SOUND_OFFSET);
                    
                    // Atualização do buffer circular
                    uint32_t sample_value = (sound_level > SOUND_THRESHOLD) ? 1 : 0;
                    
                    // Subtrai a amostra mais antiga
                    active_samples_count -= sample_buffer[sample_index];
                    
                    // Adiciona nova amostra
                    sample_buffer[sample_index] = sample_value;
                    active_samples_count += sample_value;
                    
                    // Atualiza índice circular
                    sample_index = (sample_index + 1) % (DETECTION_DURATION_MS / SAMPLE_WINDOW_MS);
                    
                    // Cálculo do percentual de atividade
                    float activity_percent = (active_samples_count * 100.0f) / (DETECTION_DURATION_MS / SAMPLE_WINDOW_MS);
                    
                    // Atualização do display
                    char status[32];
                    snprintf(status, sizeof(status), "Atividade: %d%%", (int)activity_percent);
                    ssd1306_draw_string(ssd, 0, 32, status);
                    render_on_display(ssd, &frame_area);
                    
                    // Debug no terminal
                    printf("Nível: %.2f V | Amostras Ativas: %d/%d\n", 
                          sound_level, active_samples_count, MIN_ACTIVE_SAMPLES);
                    
                    // Condição de disparo
                    if (active_samples_count >= MIN_ACTIVE_SAMPLES) {
                        printf("Choro detectado!\n");
                        melody_active = true;
                        update_led_status(true, true);
                        ssd1306_draw_string(ssd, 0, 32, "Choro detectado!  ");
                        render_on_display(ssd, &frame_area);
                        play_melody(BUZZER_PIN);
                        
                        // Reset do buffer após detecção
                        memset(sample_buffer, 0, sizeof(sample_buffer));
                        active_samples_count = 0;
                    }
                }
            }
        }

        // Mantém Wi-Fi ativo
        cyw43_arch_poll();
        sleep_ms(10);
    }

    cyw43_arch_deinit();
    return 0;
}