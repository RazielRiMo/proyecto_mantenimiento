#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <stdio.h>
#include <string.h>

#define WIFI_SSID "TU_NOMBRE_DE_RED"
#define WIFI_PSK "TU_CONTRASEÑA"
#define PC_IP_ADDR "192.168.1.100" 
#define PC_PORT 5000
#define UDP_PEER_PORT 5001
#define MAX_JSON_LEN 256

/* --- Variables Globales de Red --- */
int tcp_sock, udp_sock;
volatile bool network_ready = false;
struct sockaddr_in udp_bcast_addr;

/* Referencia al UART */
const struct device *uart_ext = DEVICE_DT_GET(DT_ALIAS(uart_ext));

/* =========================================================
 * 1. RECURSOS DE SINCRONIZACIÓN (Semáforos Binarios)
 * ========================================================= */
K_SEM_DEFINE(uart_rx_sem, 0, 1);
K_SEM_DEFINE(tcp_rx_sem, 0, 1);
K_SEM_DEFINE(udp_rx_sem, 0, 1);

/* =========================================================
 * 2. BUFFERS Y COLAS DE TRANSMISIÓN
 * ========================================================= */
char uart_rx_buf[MAX_JSON_LEN];
char tcp_rx_buf[MAX_JSON_LEN];
char udp_rx_buf[MAX_JSON_LEN];

// Colas independientes para aislar las tareas de envío (TX)
K_MSGQ_DEFINE(uart_tx_q, MAX_JSON_LEN, 10, 4);
K_MSGQ_DEFINE(tcp_tx_q, MAX_JSON_LEN, 10, 4);
K_MSGQ_DEFINE(udp_tx_q, MAX_JSON_LEN, 10, 4);

/* =========================================================
 * 3. INTERRUPCIONES (Productores de Eventos)
 * ========================================================= */

// --- A. Interrupción de Hardware Real (UART) ---
static void uart_isr(const struct device *dev, void *user_data) {
    static int rx_ptr = 0;
    
    uart_irq_update(dev);
    if (uart_irq_rx_ready(dev)) {
        unsigned char c;
        while (uart_fifo_read(dev, &c, 1) == 1) {
            if (c == '\n' || c == '\r') {
                uart_rx_buf[rx_ptr] = '\0';
                if (rx_ptr > 0) {
                    // ACTIVA EL SEMÁFORO BINARIO
                    k_sem_give(&uart_rx_sem); 
                }
                rx_ptr = 0;
            } else if (rx_ptr < MAX_JSON_LEN - 1) {
                uart_rx_buf[rx_ptr++] = c;
            }
        }
    }
}

/// --- B. Tarea Disparadora de Red (Simula una interrupción) ---
void net_listener_thread(void *p1, void *p2, void *p3) {
    while (!network_ready) k_sleep(K_MSEC(100)); // Esperar inicialización

    struct zsock_pollfd fds[2] = {
        {.fd = tcp_sock, .events = ZSOCK_POLLIN},
        {.fd = udp_sock, .events = ZSOCK_POLLIN}
    };

    while (1) {
        // Bloquea el hilo hasta que el hardware Wi-Fi reciba algo
        // El parámetro -1 indica espera infinita (K_FOREVER de POSIX)
        if (zsock_poll(fds, 2, -1) > 0) {
            
            // ¿Llegó paquete por TCP?
            if (fds[0].revents & ZSOCK_POLLIN) {
                int len = zsock_recv(tcp_sock, tcp_rx_buf, MAX_JSON_LEN - 1, 0);
                if (len > 0) {
                    tcp_rx_buf[len] = '\0';
                    k_sem_give(&tcp_rx_sem); // ACTIVA EL SEMÁFORO BINARIO
                }
            }
            
            // ¿Llegó paquete por UDP?
            if (fds[1].revents & ZSOCK_POLLIN) {
                int len = zsock_recv(udp_sock, udp_rx_buf, MAX_JSON_LEN - 1, 0);
                if (len > 0) {
                    udp_rx_buf[len] = '\0';
                    k_sem_give(&udp_rx_sem); // ACTIVA EL SEMÁFORO BINARIO
                }
            }
        }
    }
}
/* =========================================================
 * 4. TAREAS DE RECEPCIÓN (Consumidores de Semáforos)
 * ========================================================= */
void uart_rx_task(void *p1, void *p2, void *p3) {
    while (1) {
        // Hilo suspendido (0% CPU) hasta que la interrupción libere el semáforo
        k_sem_take(&uart_rx_sem, K_FOREVER);
        
        // Procesar JSON
        if (strchr(uart_rx_buf, '{') != NULL && strchr(uart_rx_buf, '}') != NULL) {
            // Enrutar hacia las colas de envío de red
            k_msgq_put(&tcp_tx_q, uart_rx_buf, K_NO_WAIT);
            k_msgq_put(&udp_tx_q, uart_rx_buf, K_NO_WAIT);
        }
    }
}

void tcp_rx_task(void *p1, void *p2, void *p3) {
    while (1) {
        k_sem_take(&tcp_rx_sem, K_FOREVER);
        if (strchr(tcp_rx_buf, '{') != NULL && strchr(tcp_rx_buf, '}') != NULL) {
            k_msgq_put(&uart_tx_q, tcp_rx_buf, K_NO_WAIT); // Enrutar al UART
        }
    }
}

void udp_rx_task(void *p1, void *p2, void *p3) {
    while (1) {
        k_sem_take(&udp_rx_sem, K_FOREVER);
        if (strchr(udp_rx_buf, '{') != NULL && strchr(udp_rx_buf, '}') != NULL) {
            k_msgq_put(&uart_tx_q, udp_rx_buf, K_NO_WAIT); // Enrutar al UART
        }
    }
}

/* =========================================================
 * 5. TAREAS DE TRANSMISIÓN INDEPENDIENTES
 * ========================================================= */
void uart_tx_task(void *p1, void *p2, void *p3) {
    char tx_buf[MAX_JSON_LEN];
    while (1) {
        k_msgq_get(&uart_tx_q, &tx_buf, K_FOREVER);
        for (int i = 0; i < strlen(tx_buf); i++) {
            uart_poll_out(uart_ext, tx_buf[i]);
        }
        uart_poll_out(uart_ext, '\n');
    }
}

void tcp_tx_task(void *p1, void *p2, void *p3) {
    char tx_buf[MAX_JSON_LEN];
    while (!network_ready) k_sleep(K_MSEC(100));

    while (1) {
        k_msgq_get(&tcp_tx_q, &tx_buf, K_FOREVER);
        zsock_send(tcp_sock, tx_buf, strlen(tx_buf), 0);
        zsock_send(tcp_sock, "\n", 1, 0);
    }
}

void udp_tx_task(void *p1, void *p2, void *p3) {
    char tx_buf[MAX_JSON_LEN];
    while (!network_ready) k_sleep(K_MSEC(100));

    while (1) {
        k_msgq_get(&udp_tx_q, &tx_buf, K_FOREVER);
        zsock_sendto(udp_sock, tx_buf, strlen(tx_buf), 0, 
                     (struct sockaddr *)&udp_bcast_addr, sizeof(udp_bcast_addr));
    }
}

/* =========================================================
 * DEFINICIÓN E INICIALIZACIÓN DE HILOS
 * ========================================================= */
// Tareas RX (Prioridad 5)
K_THREAD_DEFINE(uart_rx_id, 2048, uart_rx_task, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(tcp_rx_id, 2048, tcp_rx_task, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(udp_rx_id, 2048, udp_rx_task, NULL, NULL, NULL, 5, 0, 0);

// Tareas TX (Prioridad 6 - Ligeramente menor para asegurar procesamiento antes de envío)
K_THREAD_DEFINE(uart_tx_id, 2048, uart_tx_task, NULL, NULL, NULL, 6, 0, 0);
K_THREAD_DEFINE(tcp_tx_id, 2048, tcp_tx_task, NULL, NULL, NULL, 6, 0, 0);
K_THREAD_DEFINE(udp_tx_id, 2048, udp_tx_task, NULL, NULL, NULL, 6, 0, 0);

// Listener de Red (Prioridad 4 - Alta, equivalente a una ISR de hardware)
K_THREAD_DEFINE(net_list_id, 2048, net_listener_thread, NULL, NULL, NULL, 4, 0, 0);

/* =========================================================
 * FUNCIÓN MAIN: Configuración de Hardware y Red
 * ========================================================= */
int main(void) {
    printk("Iniciando Firmware Multitarea Orientado a Eventos\n");

    // 1. Configurar Interrupción UART
    if (device_is_ready(uart_ext)) {
        uart_irq_callback_set(uart_ext, uart_isr);
        uart_irq_rx_enable(uart_ext);
    }

    // 2. Conectar Wi-Fi
    struct net_if *iface = net_if_get_default();
    struct wifi_connect_req_params wifi_params = {
        .ssid = WIFI_SSID, .ssid_length = strlen(WIFI_SSID),
        .psk = WIFI_PSK, .psk_length = strlen(WIFI_PSK),
        .channel = WIFI_CHANNEL_ANY, .security = WIFI_SECURITY_TYPE_PSK
    };
    net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params, sizeof(wifi_params));
    
    k_sleep(K_SECONDS(8)); // Esperar asignación DHCP

    // 3. Crear Sockets
    tcp_sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in pc_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PC_PORT),
    };
    zsock_inet_pton(AF_INET, PC_IP_ADDR, &pc_addr.sin_addr);
    
    udp_sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in udp_bind_addr = {
        .sin_family = AF_INET, .sin_port = htons(UDP_PEER_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    int optval = 1;
    zsock_setsockopt(udp_sock, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
    zsock_bind(udp_sock, (struct sockaddr *)&udp_bind_addr, sizeof(udp_bind_addr));

    udp_bcast_addr.sin_family = AF_INET;
    udp_bcast_addr.sin_port = htons(UDP_PEER_PORT);
    zsock_inet_pton(AF_INET, "255.255.255.255", &udp_bcast_addr.sin_addr);

    // Conectar TCP
    while (zsock_connect(tcp_sock, (struct sockaddr *)&pc_addr, sizeof(pc_addr)) < 0) {
        k_sleep(K_SECONDS(2));
    }
    
    // 4. Liberar ejecución de tareas
    network_ready = true;
    printk("Sistema Inicializado Exitosamente.\n");

    k_sleep(K_FOREVER);
    return 0;
}