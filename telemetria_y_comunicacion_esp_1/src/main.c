#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/data/json.h>
#include <stdio.h>
#include <string.h>

#define MAX_JSON_LEN 256
#define PC_IP_ADDR "192.168.1.100" 
#define PC_PORT 5000
#define UDP_PEER_PORT 5001 // Puerto para comunicación ESP32 a ESP32

/* --- Colas de Mensajes IPC --- */
K_MSGQ_DEFINE(uart_to_wifi_msgq, MAX_JSON_LEN, 10, 4);
K_MSGQ_DEFINE(wifi_to_uart_msgq, MAX_JSON_LEN, 10, 4);

const struct device *uart_ext = DEVICE_DT_GET(DT_ALIAS(uart_ext));

/* --- Estructuras JSON Nativa de Zephyr --- */
struct heartbeat_data {
    const char *status;
    int core0_ticks;
};

static const struct json_obj_descr heartbeat_descr[] = {
    JSON_OBJ_DESCR_PRIM(struct heartbeat_data, status, JSON_TOK_STRING),
    JSON_OBJ_DESCR_PRIM(struct heartbeat_data, core0_ticks, JSON_TOK_NUMBER),
};

/* =========================================================
 * THREAD 0: Wi-Fi (TCP RX/TX al PC y UDP RX/TX a otra ESP32)
 * ========================================================= */
void wifi_thread_entry(void *p1, void *p2, void *p3) {
    // 1. Socket TCP (Hacia el PC)
    int tcp_sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in pc_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PC_PORT),
    };
    zsock_inet_pton(AF_INET, PC_IP_ADDR, &pc_addr.sin_addr);
    
    // 2. Socket UDP (Hacia otras placas ESP32 - Reemplazo de ESP-NOW)
    int udp_sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in udp_bind_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(UDP_PEER_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    // Habilitar permisos de Broadcast
    int optval = 1;
    zsock_setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
    zsock_bind(udp_sock, (struct sockaddr *)&udp_bind_addr, sizeof(udp_bind_addr));

    // Dirección destino para lanzar los paquetes UDP "al aire"
    struct sockaddr_in udp_bcast_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(UDP_PEER_PORT),
    };
    zsock_inet_pton(AF_INET, "255.255.255.255", &udp_bcast_addr.sin_addr);
    
    printk("Conectando al servidor TCP en el PC...\n");
    while (zsock_connect(tcp_sock, (struct sockaddr *)&pc_addr, sizeof(pc_addr)) < 0) {
        k_sleep(K_SECONDS(2));
    }
    printk("Conexiones de red establecidas!\n");

    char ipc_buffer[MAX_JSON_LEN];
    char rx_buf[MAX_JSON_LEN];

    while (1) {
        // TRANSMISIÓN: UART -> TCP(PC) y UDP(Otras ESP32)
        if (k_msgq_get(&uart_to_wifi_msgq, &ipc_buffer, K_NO_WAIT) == 0) {
            // Enviar al PC
            zsock_send(tcp_sock, ipc_buffer, strlen(ipc_buffer), 0);
            zsock_send(tcp_sock, "\n", 1, 0); 
            // Enviar a otras placas ESP32 (Broadcast)
            zsock_sendto(udp_sock, ipc_buffer, strlen(ipc_buffer), 0, (struct sockaddr *)&udp_bcast_addr, sizeof(udp_bcast_addr));
        }

        // RECEPCIÓN 1: Desde el PC (TCP)
        int rx_len = zsock_recv(tcp_sock, rx_buf, sizeof(rx_buf) - 1, ZSOCK_MSG_DONTWAIT);
        if (rx_len > 0) {
            rx_buf[rx_len] = '\0';
            if (strchr(rx_buf, '{') != NULL && strchr(rx_buf, '}') != NULL) {
                k_msgq_put(&wifi_to_uart_msgq, rx_buf, K_NO_WAIT);
            }
        }

        // RECEPCIÓN 2: Desde otras ESP32 (UDP)
        rx_len = zsock_recv(udp_sock, rx_buf, sizeof(rx_buf) - 1, ZSOCK_MSG_DONTWAIT);
        if (rx_len > 0) {
            rx_buf[rx_len] = '\0';
            if (strchr(rx_buf, '{') != NULL && strchr(rx_buf, '}') != NULL) {
                k_msgq_put(&wifi_to_uart_msgq, rx_buf, K_NO_WAIT);
            }
        }

        // HEARTBEAT PERIÓDICO AL PC
        static int ticks = 0;
        if (++ticks >= 500) { 
            struct heartbeat_data hb = {.status = "alive", .core0_ticks = ticks};
            char json_str[128];
            if (json_obj_encode_buf(heartbeat_descr, ARRAY_SIZE(heartbeat_descr), &hb, json_str, sizeof(json_str)) == 0) {
                zsock_send(tcp_sock, json_str, strlen(json_str), 0);
                zsock_send(tcp_sock, "\n", 1, 0);
            }
            ticks = 0;
        }

        k_sleep(K_MSEC(10)); 
    }
}

/* =========================================================
 * THREAD 1: Gestión de UART1 (Recepción y Transmisión)
 * ========================================================= */
void uart_thread_entry(void *p1, void *p2, void *p3) {
    if (!device_is_ready(uart_ext)) return;

    char rx_buf[MAX_JSON_LEN];
    int rx_ptr = 0;
    char tx_buf[MAX_JSON_LEN];

    while (1) {
        unsigned char c;
        if (uart_poll_in(uart_ext, &c) == 0) {
            if (c == '\n' || c == '\r') {
                rx_buf[rx_ptr] = '\0'; 
                if (rx_ptr > 0) {
                    if (strchr(rx_buf, '{') != NULL && strchr(rx_buf, '}') != NULL) {
                        k_msgq_put(&uart_to_wifi_msgq, rx_buf, K_NO_WAIT);
                    }
                }
                rx_ptr = 0; 
            } else if (rx_ptr < MAX_JSON_LEN - 1) {
                rx_buf[rx_ptr++] = c; 
            }
        }

        if (k_msgq_get(&wifi_to_uart_msgq, &tx_buf, K_NO_WAIT) == 0) {
            for (int i = 0; i < strlen(tx_buf); i++) {
                uart_poll_out(uart_ext, tx_buf[i]);
            }
            uart_poll_out(uart_ext, '\n'); 
        }

        k_sleep(K_MSEC(10)); 
    }
}

K_THREAD_DEFINE(wifi_thread_id, 4096, wifi_thread_entry, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(uart_thread_id, 4096, uart_thread_entry, NULL, NULL, NULL, 5, 0, 0);

int main(void) {
    printk("Iniciando Firmware Bidireccional (TCP/UDP)\n");
    k_sleep(K_FOREVER);
    return 0;
}