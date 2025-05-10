

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

// Funções para leitura da temperatura 
void tmp_init(){
    adc_init(); // Inicializa o ADC
    adc_gpio_init(28); // Habilita o GPIO 28 como entrada analógica
    adc_select_input(2); // O GPIO 28 corresponde ao canal ADC2
};

float get_temp() {
    int num_readings = 10;  // Número de leituras para a média
    float sum = 0.0f;

    // Ler múltiplos valores e somá-los
    for (int i = 0; i < num_readings; i++) {
        uint16_t raw_value = adc_read();
        float voltage = (raw_value * 3.3f) / (1 << 12);
        sum += voltage;
        sleep_ms(10);  // Espera um pouco entre as leituras para evitar leituras muito rápidas
    }

    // Calcular a média das leituras
    float avg_voltage = sum / num_readings;

    // Aplicar um fator de escala para reduzir a sensibilidade
    float temperature = (avg_voltage - 0.5f) / (0.02f * 2); // Diminuir a sensibilidade multiplicando por 2

    return temperature;
}



// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    // Copia o payload completo para uma string terminada em '\0'
    char *request = malloc(p->tot_len + 1);
    if (!request) {
        pbuf_free(p);
        return ERR_MEM;
    }
    memcpy(request, p->payload, p->tot_len);
    request[p->tot_len] = '\0';

    // Informa ao stack LWIP que consumimos os bytes
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);

    // Lê sensores
    uint8_t btn_a = !gpio_get(BUTTON_A);
    uint8_t btn_b = !gpio_get(BUTTON_B);
   
    adc_select_input(2);
    float temperatura = get_temp();

    
    bool is_data = (strstr(request, "GET /data") != NULL);

    // Buffer de resposta único, em memória estática
    static char response_buffer[1024];

    if (is_data) {
        // monta JSON
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
        // copia o corpo JSON após o header
        memcpy(response_buffer + header_len, json_body, n);
    } else {
        // monta HTML
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

    // envia tudo de uma vez
    tcp_write(tpcb, response_buffer, strlen(response_buffer), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    free(request);
    return ERR_OK;
}

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}


// Função principal
int main()
{
    stdio_init_all();
    sleep_ms(2000); // Espera para o terminal conectar

    // Inicializa botões como entrada com pull-up
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);

    // Inicializa ADC para joystick
    adc_init();

    tmp_init();

    // Inicializa Wi-Fi
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

    // Configura o servidor TCP (porta 80)
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

    // Loop principal
    while (true) {
        cyw43_arch_poll(); // Necessário para manter Wi-Fi funcionando
    }

    cyw43_arch_deinit();
    return 0;
}
