#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h" // Para acessar netif_default e IP

// Configurações de Wi-Fi
#define WIFI_SSID "Roteadorzin"
#define WIFI_PASSWORD "ENZOMELO1000"

// Definição dos botões
#define BUTTON_A 5
#define BUTTON_B 6

/**
 * @brief Inicializa o ADC no GPIO 28
 * @details Configura o canal ADC2, que corresponde ao GPIO 28, como entrada analógica.
 */
void tmp_init(){
    adc_init();
    adc_gpio_init(28);
    adc_select_input(2);
};

/**
 * @brief Realiza a leitura de temperatura no GPIO 28
 * @return Temperatura estimada em graus Celsius
 * @details Realiza uma média de 10 leituras para maior estabilidade e aplica um fator de escala.
 */
float get_temp() {
    int num_readings = 10;
    float sum = 0.0f;

    for (int i = 0; i < num_readings; i++) {
        uint16_t raw_value = adc_read();
        float voltage = (raw_value * 3.3f) / (1 << 12);
        sum += voltage;
        sleep_ms(10);
    }

    float avg_voltage = sum / num_readings;
    float temperature = (avg_voltage - 0.5f) / (0.02f * 2);

    return temperature;
}

/**
 * @brief Callback para processar requisições HTTP
 * @param arg Argumento genérico (não utilizado)
 * @param tpcb Ponteiro para a conexão TCP
 * @param p Buffer de pacote recebido
 * @param err Código de erro
 * @return ERR_OK em caso de sucesso
 * @details Responde com uma página HTML ou dados JSON dependendo da URL requisitada.
 */
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *request = malloc(p->tot_len + 1);
    if (!request) {
        pbuf_free(p);
        return ERR_MEM;
    }
    memcpy(request, p->payload, p->tot_len);
    request[p->tot_len] = '\0';

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);

    uint8_t btn_a = !gpio_get(BUTTON_A);
    uint8_t btn_b = !gpio_get(BUTTON_B);

    adc_select_input(2);
    float temperatura = get_temp();

    bool is_data = (strstr(request, "GET /data") != NULL);

    static char response_buffer[1024];

    if (is_data) {
        char json_body[256];
        int n = snprintf(json_body, sizeof(json_body),
            "{\"temperatura\":%.2f,\"btn_a\":%d,\"btn_b\":%d}",
            temperatura, btn_a, btn_b);

        int header_len = snprintf(response_buffer, sizeof(response_buffer),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n",
            n);

        memcpy(response_buffer + header_len, json_body, n);
    } else {
        const char *html_body =
            "<!DOCTYPE html><html><head>"
            "<meta charset=\"utf-8\">"
            "<title>Monitoramento Pico W</title>"
            "<style>"
              "body{font-family:Arial,sans-serif;text-align:center;margin:20px;}"
              ".data{font-size:1.2em;margin:8px;}"
            "</style>"
            "<script>"
              "function update(){"
                "fetch('/data').then(r=>r.json()).then(d=>{"
                  "document.getElementById('temp').textContent=d.temperatura.toFixed(2) + ' °C';"
                  "document.getElementById('a').textContent=d.btn_a?'ON':'OFF';"
                  "document.getElementById('b').textContent=d.btn_b?'ON':'OFF';"
                "});"
              "}"
              "setInterval(update,1000);window.onload=update;"
            "</script>"
            "</head><body>"
            "<h1>Monitor de Botões e Temperatura</h1>"
            "<div class=\"data\">Temperatura: <span id=\"temp\">-</span></div>"
            "<div class=\"data\">Botão A: <span id=\"a\">-</span></div>"
            "<div class=\"data\">Botão B: <span id=\"b\">-</span></div>"
            "</body></html>";

        int body_len = strlen(html_body);
        int header_len = snprintf(response_buffer, sizeof(response_buffer),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n",
            body_len);
        memcpy(response_buffer + header_len, html_body, body_len);
    }

    tcp_write(tpcb, response_buffer, strlen(response_buffer), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    free(request);
    return ERR_OK;
}

/**
 * @brief Callback ao aceitar conexões TCP
 * @param arg Argumento não utilizado
 * @param newpcb Estrutura da nova conexão
 * @param err Código de erro
 * @return ERR_OK
 * @details Define a função de recebimento para a nova conexão.
 */
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

/**
 * @brief Função principal do programa
 * @return 0 se sucesso, -1 se erro
 * @details Inicializa GPIOs, Wi-Fi e inicia servidor TCP escutando na porta 80.
 */
int main()
{
    stdio_init_all();
    sleep_ms(2000);

    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    adc_init();
    tmp_init();

    if (cyw43_arch_init()) {
        printf("Falha ao inicializar Wi-Fi\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return -1;
    }

    printf("Conectado ao Wi-Fi\n");

    if (netif_default) {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    struct tcp_pcb *server = tcp_new();
    if (!server) {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    server = tcp_listen(server);
    tcp_accept(server, tcp_server_accept);

    printf("Servidor ouvindo na porta 80\n");

    while (true) {
        cyw43_arch_poll();
    }

    cyw43_arch_deinit();
    return 0;
}