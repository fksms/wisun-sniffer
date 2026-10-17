#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <uhd.h>

#include "brb.h"
#include "channelizer.h"
#include "channelizer_test.h"
#include "demodulator.h"
#include "lfrb.h"
#include "packet.h"
#include "usrp.h"

// 送信テスト時は以下をコメントアウト
// #define TX_TEST

// ---------------------Status---------------------
_Atomic bool running = true;
// ------------------------------------------------

// ---------------------Buffer---------------------
// USRP -> Channelizer のリングバッファ
LockFreeRingBuffer lfrb;
// Channelizer -> Demod のリングバッファ
BlockingRingBuffer brb;
// ------------------------------------------------

void print_help(void) {
    fprintf(stderr, "XXXXXX\n\n"

                    "Options:\n"
                    "    -a (RX antenna)\n"
                    "    -c (RX channel)\n"
                    "    -f (RX frequency in Hz)\n"
                    "    -g (RX gain)\n"
                    "    -w (write live PCAP data to a file or FIFO)\n"
                    "    -k (keep packets with invalid FCS when using -w)\n"
                    "    -m (run FSK/GFSK modem loopback self-test on channel and exit)\n"
                    "    -t (run channelizer self-test and exit)\n"
                    "    -h (print this help message)\n");
}

void handle_sigint(int sig) { atomic_store(&running, false); }

int main(int argc, char *argv[]) {

    // Register signal handler for SIGINT (Ctrl+C)
    signal(SIGINT, handle_sigint);

    int option = 0;

    // ------------------For USRP RX-------------------
    double rx_freq = 924.3e6; // 42chと43chの間
    double rx_gain = 30.0;
    size_t rx_channel = 0;
    char *rx_antenna = "RX2";
    const char *pcap_output_path = NULL;
    bool keep_bad_fcs = false;
    // ------------------------------------------------

#ifdef TX_TEST
    // ------------------For USRP TX-------------------
    double tx_freq = 922.3e6;
    double tx_gain = 30.0;
    size_t tx_channel = 1;
    char *tx_antenna = "TX/RX";
    // ------------------------------------------------
#endif // TX_TEST

    // --------------------For Test--------------------
    bool run_channelizer_single_tone_test = false;
    int run_modem_loopback_channel = -1;
    // ------------------------------------------------

    // Process options
    while ((option = getopt(argc, argv, "a:c:f:g:w:km:th")) != -1) {
        switch (option) {

        case 'a':
            rx_antenna = strdup(optarg);
            break;

        case 'c':
            rx_channel = atoi(optarg);
            break;

        case 'f':
            rx_freq = atof(optarg);
            break;

        case 'g':
            rx_gain = atof(optarg);
            break;

        case 'w':
            pcap_output_path = optarg;
            break;

        case 'k':
            keep_bad_fcs = true;
            break;

        case 'm':
            run_modem_loopback_channel = atoi(optarg);
            break;

        case 't':
            run_channelizer_single_tone_test = true;
            break;

        case 'h':
            print_help();
            return 0;

        default:
            print_help();
            return 0;
        }
    }

    // ----------------------------Test----------------------------
    if (run_channelizer_single_tone_test || run_modem_loopback_channel >= 0) {
        channelizer_handle channelizer;
        if (channelizer_setup(&channelizer)) {
            fprintf(stderr, "Setup channelizer failed\n");
            return -1;
        }

        int rc;
        if (run_channelizer_single_tone_test) {
            rc = channelizer_run_single_tone_test(&channelizer, stdout);
        } else {
            rc = channelizer_run_modem_loopback_test(&channelizer, run_modem_loopback_channel, stdout);
        }
        channelizer_close(&channelizer);
        return (rc == 0) ? 0 : -1;
    }
    // ------------------------------------------------------------

    if (pcap_output_path != NULL && packet_pcap_open(pcap_output_path, keep_bad_fcs) != 0) {
        return -1;
    }

    // Init the ring buffer
    lfrb_init(&lfrb);
    if (!brb_init(&brb)) {
        fprintf(stderr, "Init Blocking Ring Buffer failed\n");
        return -1;
    }

    // ----------------------------Setup---------------------------
    // USRP handle
    uhd_usrp_handle usrp;

    // Setup USRP
    if (usrp_setup(&usrp)) {
        fprintf(stderr, "Setup USRP failed\n");
        return -1;
    }

    // USRP RX handle
    uhd_usrp_rx_handle usrp_rx = {
        .rx_freq = rx_freq,
        .rx_gain = rx_gain,
        .rx_samp_rate = RX_SAMP_RATE,
        .rx_channel = rx_channel,
        .rx_antenna = rx_antenna,
        .usrp = usrp,
        .rx_streamer = NULL,
        .rx_metadata = NULL,
    };

    // Setup USRP RX
    if (usrp_rx_setup(&usrp_rx)) {
        fprintf(stderr, "Setup USRP RX failed\n");
        return -1;
    }

#ifdef TX_TEST
    // USRP TX handle
    uhd_usrp_tx_handle usrp_tx = {
        .tx_freq = tx_freq,
        .tx_gain = tx_gain,
        .tx_samp_rate = TX_SAMP_RATE,
        .tx_channel = tx_channel,
        .tx_antenna = tx_antenna,
        .usrp = usrp,
        .tx_streamer = NULL,
        .tx_metadata = NULL,
    };

    // Setup USRP TX
    if (usrp_tx_setup(&usrp_tx)) {
        fprintf(stderr, "Setup USRP TX failed\n");
        return -1;
    }
#endif // TX_TEST

    // Setup channelizer
    channelizer_handle channelizer;
    if (channelizer_setup(&channelizer)) {
        fprintf(stderr, "Setup channelizer failed\n");
        return -1;
    }
    // ------------------------------------------------------------

    // -----------------Setting pthread attributes-----------------
    pthread_attr_t attr;
    struct sched_param param;

    // Initialize pthread attributes
    if (pthread_attr_init(&attr)) {
        fprintf(stderr, "Init pthread attributes failed\n");
        return -1;
    }

    // Set scheduler policy and priority of pthread
    if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO)) {
        fprintf(stderr, "Pthread setschedpolicy failed\n");
        return -1;
    }

    param.sched_priority = 99;

    if (pthread_attr_setschedparam(&attr, &param)) {
        fprintf(stderr, "Pthread setschedparam failed\n");
        return -1;
    }

    // Use scheduling parameters of attr
    if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) {
        fprintf(stderr, "Pthread setinheritsched failed\n");
        return -1;
    }
    // ------------------------------------------------------------

    // ------------------------Create thread-----------------------
    // Create demodulator thread
    pthread_t demodulatorThread;
    if (pthread_create(&demodulatorThread, &attr, demodulator_thread, NULL)) {
        fprintf(stderr, "Create demodulator thread failed\n");
        return -1;
    }

    // Create channelizer thread
    pthread_t channelizerThread;
    if (pthread_create(&channelizerThread, &attr, channelizer_thread, (void *)&channelizer)) {
        fprintf(stderr, "Create channelizer thread failed\n");
        return -1;
    }

    // Create USRP RX thread
    pthread_t usrpRxThread;
    if (pthread_create(&usrpRxThread, &attr, usrp_rx_thread, (void *)&usrp_rx)) {
        fprintf(stderr, "Create USRP RX thread failed\n");
        return -1;
    }

#ifdef TX_TEST
    // Create USRP TX thread
    pthread_t usrpTxThread;
    if (pthread_create(&usrpTxThread, &attr, usrp_tx_thread, (void *)&usrp_tx)) {
        fprintf(stderr, "Create USRP TX thread failed\n");
        return -1;
    }
#endif // TX_TEST
    // ------------------------------------------------------------

    // Main thread waits until SIGINT is received
    while (atomic_load(&running)) {
        sleep(1);
    }
    if (!brb_stop(&brb)) {
        fprintf(stderr, "Stop Blocking Ring Buffer failed\n");
        return -1;
    }

    // -------------------------Join thread------------------------
#ifdef TX_TEST
    // Join USRP TX thread
    if (pthread_join(usrpTxThread, NULL)) {
        fprintf(stderr, "Join USRP TX thread failed\n");
        return -1;
    }
#endif // TX_TEST

    // Join USRP RX thread
    if (pthread_join(usrpRxThread, NULL)) {
        fprintf(stderr, "Join USRP RX thread failed\n");
        return -1;
    }

    // Join channelizer thread
    if (pthread_join(channelizerThread, NULL)) {
        fprintf(stderr, "Join channelizer thread failed\n");
        return -1;
    }

    // Join demodulator thread
    if (pthread_join(demodulatorThread, NULL)) {
        fprintf(stderr, "Join demodulator thread failed\n");
        return -1;
    }
    // ------------------------------------------------------------

    if (packet_pcap_close() != 0) {
        return -1;
    }

    // ----------------------------Close---------------------------
    // Close channelizer
    if (channelizer_close(&channelizer)) {
        fprintf(stderr, "Close channelizer failed\n");
        return -1;
    }

#ifdef TX_TEST
    // Close USRP TX
    if (usrp_tx_close(&usrp_tx)) {
        fprintf(stderr, "Close USRP TX failed\n");
        return -1;
    }
#endif // TX_TEST

    // Close USRP RX
    if (usrp_rx_close(&usrp_rx)) {
        fprintf(stderr, "Close USRP RX failed\n");
        return -1;
    }

    // Close USRP
    if (usrp_close(&usrp)) {
        fprintf(stderr, "Close USRP failed\n");
        return -1;
    }
    // ------------------------------------------------------------

    // Destroy the ring buffer
    if (!brb_destroy(&brb)) {
        fprintf(stderr, "Destroy Blocking Ring Buffer failed\n");
        return -1;
    }

    return 0;
}
