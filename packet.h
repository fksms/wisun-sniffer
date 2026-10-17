#ifndef __PACKET_H__
#define __PACKET_H__

#include <stdbool.h>
#include <stdint.h>

int packet_pcap_open(const char *path, bool keep_bad_fcs);
int packet_pcap_close(void);
void analyze_packet(const uint8_t *rx_bits, int n_rx_bits);

#endif // __PACKET_H__
