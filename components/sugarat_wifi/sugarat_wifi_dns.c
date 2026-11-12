/* SPDX-License-Identifier: MIT
 * Raw UDP callbacks are owned by lwIP; start/stop synchronize with its TCP/IP thread. */
#include <stdbool.h>
#include <stdlib.h>
#include "sugarat_wifi_dns.h"
#include "sugarat_wifi_dns_packet.h"
#include "sugarat_wifi_peer.h"
#include "esp_netif_net_stack.h"
#include "esp_log.h"
#include "dhcpserver/dhcpserver.h"
#include "lwip/udp.h"

struct sugarat_wifi_dns {
    esp_netif_t *ap;
    esp_netif_ip_info_t info;
    struct udp_pcb *pcb;
};
static void receive(void *context, struct udp_pcb *pcb, struct pbuf *request, const ip_addr_t *peer, u16_t port) {
    sugarat_wifi_dns_t *dns = context;
    if (!request) return;
    uint8_t packet[512]; size_t bytes = request->tot_len;
    bool local = IP_IS_V4(peer) && sugarat_wifi_peer_on_subnet((const uint8_t *)&ip_2_ip4(peer)->addr, 4, dns->info.ip.addr, dns->info.netmask.addr);
    if (!local || bytes > sizeof(packet)) { pbuf_free(request); return; }
    ip_addr_t destination = *peer;
    pbuf_copy_partial(request, packet, bytes, 0); pbuf_free(request);
    bytes = sugarat_wifi_dns_reply(packet, bytes, sizeof(packet), dns->info.ip.addr);
    if (!bytes) return;
    struct pbuf *response = pbuf_alloc(PBUF_TRANSPORT, bytes, PBUF_RAM);
    if (!response) return;
    if (pbuf_take(response, packet, bytes) == ERR_OK) udp_sendto(pcb, response, &destination, port);
    pbuf_free(response);
}
static esp_err_t open_receiver(void *context) {
    sugarat_wifi_dns_t *dns = context;
    struct netif *interface = esp_netif_get_netif_impl(dns->ap);
    if (!interface) return ESP_ERR_INVALID_STATE;
    dns->pcb = udp_new_ip_type(IPADDR_TYPE_V4);
    if (!dns->pcb) return ESP_ERR_NO_MEM;
    ip_addr_t address = IPADDR4_INIT(dns->info.ip.addr);
    udp_bind_netif(dns->pcb, interface);
    if (udp_bind(dns->pcb, &address, 53) != ERR_OK) { udp_remove(dns->pcb); dns->pcb = NULL; return ESP_FAIL; }
    udp_recv(dns->pcb, receive, dns);
    return ESP_OK;
}
static esp_err_t close_receiver(void *context) {
    sugarat_wifi_dns_t *dns = context;
    if (dns->pcb) { udp_remove(dns->pcb); dns->pcb = NULL; }
    return ESP_OK;
}
void sugarat_wifi_dns_stop(sugarat_wifi_dns_t *dns) {
    if (!dns) return;
    ESP_ERROR_CHECK(esp_netif_tcpip_exec(close_receiver, dns));
    free(dns);
}
esp_err_t sugarat_wifi_dns_start(esp_netif_t *ap, sugarat_wifi_dns_t **out) {
    *out = NULL;
    sugarat_wifi_dns_t *dns = calloc(1, sizeof(*dns));
    if (!dns) return ESP_ERR_NO_MEM;
    dns->ap = ap;
    esp_err_t err = esp_netif_get_ip_info(ap, &dns->info);
    if (err == ESP_OK) err = esp_netif_tcpip_exec(open_receiver, dns);
    if (err != ESP_OK) { sugarat_wifi_dns_stop(dns); return err; }
    err = esp_netif_dhcps_stop(ap);
    if (err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) err = ESP_OK;
    dhcps_offer_t offer = OFFER_DNS;
    esp_netif_dns_info_t info = {0}; info.ip.type = ESP_IPADDR_TYPE_V4; info.ip.u_addr.ip4 = dns->info.ip;
    if (err == ESP_OK) err = esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &offer, sizeof(offer));
    if (err == ESP_OK) err = esp_netif_set_dns_info(ap, ESP_NETIF_DNS_MAIN, &info);
    esp_err_t started = esp_netif_dhcps_start(ap);
    if (err == ESP_OK && started != ESP_OK && started != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) err = started;
    if (err != ESP_OK) { sugarat_wifi_dns_stop(dns); return err; }
    *out = dns;
    ESP_LOGI("sugarat_wifi", "captive_dns_ready");
    return ESP_OK;
}
