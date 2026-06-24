/* main.c
 * 
 * Main entry point. Handles arguments, coordinates file I/O,
 * and prints out the rolling average preview.
 * 
 * Usage: ./adc_processor <filename> [window]
 */

#include "adc.h"
#include "io.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    /* check arguments */
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <adc_sensor_log.bin> [window]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *in_path = argv[1];
    size_t window = (argc >= 3) ? (size_t) strtoul(argv[2], NULL, 10) : 8;

    /* load the file */
    ADCFileHeader header;
    ADCSample *samples = NULL;
    size_t count = 0;

    IoStatus rc = io_read_file(in_path, &header, &samples, &count);
    if (rc != IO_OK) {
        fprintf(stderr, "Error reading '%s': %s\n", in_path, io_status_str(rc));
        return EXIT_FAILURE;
    }

    printf("Loaded %zu records from '%s'\n", count, in_path);
    printf("Header: magic=0x%08X version=%u channels=%u rate=%u Hz\n\n",
           header.magic, header.version, header.channel_count, header.sample_rate_hz);

    /* main report */
    rc = io_write_results("results.txt", &header, samples, count);
    if (rc != IO_OK)
        fprintf(stderr, "Warning: %s\n", io_status_str(rc));
    else
        printf("Wrote results.txt\n");

    /* fault report */
    rc = io_write_fault_report("fault_report.txt", samples, count);
    if (rc == IO_OK) printf("Wrote fault_report.txt\n");

    /* print sliding-window mean preview */
    printf("\nSliding-window mean voltage (window = %zu), last 3 values/channel:\n", window);
    uint8_t ch;
    for (ch = 0; ch < header.channel_count; ++ch) {
        size_t len = 0;
        double *roll = adc_sliding_mean(samples, count, ch, window, &len);
        if (roll == NULL || len == 0) { free(roll); continue; }
        printf("  CH%u: ", ch);
        size_t start = (len >= 3) ? len - 3 : 0;
        size_t i;
        for (i = start; i < len; ++i) printf("%.4f ", roll[i]);
        printf("\n");
        free(roll);                 /* free each one as we go */
    }

    free(samples);                  /* done - hand the memory back */
    return EXIT_SUCCESS;
}