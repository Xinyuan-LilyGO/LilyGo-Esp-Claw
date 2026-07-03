/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "captive_dns.h"

#include <errno.h>
#include <string.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/ip4_addr.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

static const char *TAG = "captive_dns";

#define DNS_PORT          53
#define DNS_BUF_SIZE      512
#define DHCPS_OFFER_DNS   0x02

static TaskHandle_t s_dns_task;
static volatile bool s_running;

static esp_err_t captive_dns_configure_dhcp_dns(void)
{
    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap) {
        ESP_LOGW(TAG, "AP netif not found, skipping DHCP DNS option");
        return ESP_ERR_INVALID_STATE;
    }

    esp_netif_ip_info_t ip_info = {0};
    esp_netif_get_ip_info(ap, &ip_info);
    if (ip_info.ip.addr == 0) {
        IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    }

    esp_netif_dns_info_t dns = {0};
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ip_info.ip.addr;

    esp_netif_dhcp_status_t status = ESP_NETIF_DHCP_INIT;
    esp_netif_dhcps_get_status(ap, &status);
    const bool was_started = (status == ESP_NETIF_DHCP_STARTED);
    if (was_started) {
        esp_netif_dhcps_stop(ap);
    }

    uint8_t offer_dns = DHCPS_OFFER_DNS;
    esp_err_t err = esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET,
                                           ESP_NETIF_DOMAIN_NAME_SERVER,
                                           &offer_dns, sizeof(offer_dns));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set DHCP DOMAIN_NAME_SERVER option failed: %s",
                 esp_err_to_name(err));
    } else {
        err = esp_netif_set_dns_info(ap, ESP_NETIF_DNS_MAIN, &dns);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "set AP DNS info failed: %s", esp_err_to_name(err));
        }
    }

    if (was_started) {
        esp_err_t start_err = esp_netif_dhcps_start(ap);
        if (start_err != ESP_OK) {
            ESP_LOGE(TAG, "restart DHCP server failed: %s",
                     esp_err_to_name(start_err));
        }
    }

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "DHCP server will advertise DNS=" IPSTR " to clients",
                 IP2STR(&ip_info.ip));
    }
    return err;
}

static int dns_build_response(const uint8_t *req, int req_len,
                              uint8_t *resp, int resp_max,
                              uint32_t redirect_ip)
{
    if (req_len < 12 || resp_max < req_len + 16) {
        return -1;
    }

    memcpy(resp, req, req_len);
    resp[2] = 0x81;
    resp[3] = 0x80;
    resp[6] = 0x00; resp[7] = 0x01;

    int pos = req_len;
    resp[pos++] = 0xC0; resp[pos++] = 0x0C;
    resp[pos++] = 0x00; resp[pos++] = 0x01;
    resp[pos++] = 0x00; resp[pos++] = 0x01;
    resp[pos++] = 0x00; resp[pos++] = 0x00;
    resp[pos++] = 0x00; resp[pos++] = 0x3C;
    resp[pos++] = 0x00; resp[pos++] = 0x04;
    resp[pos++] = (redirect_ip >> 24) & 0xFF;
    resp[pos++] = (redirect_ip >> 16) & 0xFF;
    resp[pos++] = (redirect_ip >>  8) & 0xFF;
    resp[pos++] = (redirect_ip      ) & 0xFF;
    return pos;
}

static void dns_task(void *arg)
{
    (void)arg;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create DNS socket: errno=%d", errno);
        s_running = false;
        s_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "DNS bind failed: errno=%d", errno);
        close(sock);
        s_running = false;
        s_dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint32_t redirect_ip_host = 0xC0A80401;
    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_ip_info_t ip_info = {0};
    if (ap && esp_netif_get_ip_info(ap, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
        redirect_ip_host = ntohl(ip_info.ip.addr);
    }
    ESP_LOGI(TAG, "Captive DNS up on :%d -> %lu.%lu.%lu.%lu", DNS_PORT,
             (redirect_ip_host >> 24) & 0xFFu,
             (redirect_ip_host >> 16) & 0xFFu,
             (redirect_ip_host >>  8) & 0xFFu,
             redirect_ip_host & 0xFFu);

    static uint8_t buf[DNS_BUF_SIZE];
    static uint8_t resp[DNS_BUF_SIZE + 16];

    while (s_running) {
        struct sockaddr_in src;
        socklen_t src_len = sizeof(src);
        int len = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr *)&src, &src_len);
        if (len < 0) {
            continue;
        }
        int resp_len = dns_build_response(buf, len, resp, sizeof(resp), redirect_ip_host);
        if (resp_len > 0) {
            sendto(sock, resp, resp_len, 0, (struct sockaddr *)&src, src_len);
        }
    }

    close(sock);
    ESP_LOGI(TAG, "Captive DNS stopped");
    s_dns_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t captive_dns_start(void)
{
    if (s_dns_task != NULL) {
        return ESP_OK;
    }

    (void)captive_dns_configure_dhcp_dns();

    s_running = true;
    BaseType_t ret = xTaskCreate(dns_task, "captive_dns", 3072, NULL, 5, &s_dns_task);
    if (ret != pdPASS) {
        s_running = false;
        s_dns_task = NULL;
        ESP_LOGE(TAG, "Failed to create captive DNS task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void captive_dns_stop(void)
{
    s_running = false;
}
