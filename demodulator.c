#include <complex.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "brb.h"
#include "channelizer.h"
#include "fsk.h"
#include "packet.h"

// ---------------------Status---------------------
extern _Atomic bool running;
// ------------------------------------------------

// ---------------------Buffer---------------------
extern BlockingRingBuffer brb;
// ------------------------------------------------

void *demodulator_thread(void *arg) {

    // int counter = 0;

    // チャネライザから出力された信号のSPSを計算
    int sps = get_samples_per_symbol(get_channel_spacing_hz());

    // ガウスフィルタ係数の長さを計算
    int gauss_len = get_gaussian_filter_length(get_channel_spacing_hz());

    // ガウスフィルタ係数の構築
    double gauss_coef[gauss_len];
    if (build_gaussian_filter_for_rate(get_channel_spacing_hz(), gauss_coef, gauss_len) != 0) {
        fprintf(stderr, "Failed to build Gaussian filters\n");
        // ガウスフィルタの構築に失敗した場合は強制終了
        atomic_store(&running, false);
        return NULL;
    }

    while (atomic_load(&running)) {
        double complex *data = NULL;
        int rows = 0;
        int cols = 0;

        if (!brb_read(&brb, &data, &rows, &cols)) {
            fprintf(stderr, "Failed to read from ring buffer\n");
            // 読み取りに失敗した場合は強制終了
            atomic_store(&running, false);
            return NULL;
        }

        fprintf(stdout, "Demod thread: Received burst of %d samples for %d channels\n\n", cols, rows);

        fprintf(stdout, "===============start===============\n\n");

        for (int i = 0; i < rows; ++i) {

            // 受け取ったデータの長さから復調可能なビット数を計算
            int rx_bits_capacity = cols / sps;

            // 復調ビット列格納用バッファ
            uint8_t rx_bits[rx_bits_capacity];

            // 復調されたビット数
            int n_rx_bits = 0;

            // 1行分のチャネル出力を取得
            double complex *row_data = &data[i * cols];

            // ---------- ファイル出力（デバッグ用） ----------
            // char filename[32];
            // snprintf(filename, sizeof(filename), "output_%d.csv", counter + 1);

            // FILE *fp = fopen(filename, "w");
            // if (!fp) {
            //     perror("Failed to open output_X.csv");
            //     free(data);
            //     atomic_store(&running, false);
            //     return NULL;
            // }

            // for (int j = 0; j < cols; ++j) {
            //     fprintf(fp, "%lf,%lf\n", creal(row_data[j]), cimag(row_data[j]));
            // }

            // fclose(fp);
            // ----------------------------------------------

            // チャネライザの出力をFSK/GFSK復調してビット列を回復
            if (fsk_demodulate_at_rate(row_data, cols, get_channel_spacing_hz(), true, gauss_coef, gauss_len,
                                       rx_bits_capacity, rx_bits, &n_rx_bits) != 0) {
                fprintf(stderr, "Demodulation failed\n");
                // 復調に失敗した場合は強制終了
                atomic_store(&running, false);
                return NULL;
            }

            // 復調したビット列を表示
            // fprintf(stdout, "Demodulated bits (%d bits): ", n_rx_bits);
            // for (int j = 0; j < n_rx_bits; ++j) {
            //     fprintf(stdout, "%d", rx_bits[j]);
            // }
            // fprintf(stdout, "\n");

            // パケットの解析
            analyze_packet(rx_bits, n_rx_bits);

            fprintf(stdout, "\n");
        }

        fprintf(stdout, "================end================\n\n");

        // counter++;
        free(data);
    }

    // Stop
    atomic_store(&running, false);
    return NULL;
}
