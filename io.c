/* io.c
 * 
 * Handles reading the binary file and writing output reports.
 * Uses a packed struct to correctly read the 16-byte binary records
 * without compiler padding issues.
 */

#include "io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* packed on-disk record structure (exactly 16 bytes) */
#pragma pack(push, 1)
typedef struct {
    float    timestamp;
    uint8_t  channel_id;
    uint16_t raw_value;
    int16_t  temperature;
    uint8_t  status_flags;
    uint32_t sequence_number;
    uint8_t  reserved[2];
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((packed))
#endif
ADCRecordRaw;
#pragma pack(pop)

/* compile-time checks to ensure structs match expected sizes */
typedef char assert_record_is_16[(sizeof(ADCRecordRaw) == 16) ? 1 : -1];
typedef char assert_header_is_24[(sizeof(ADCFileHeader) == 24) ? 1 : -1];

/* turn an error code into something readable for the user */
const char *io_status_str(IoStatus s)
{
    switch (s) {
        case IO_OK:            return "ok";
        case IO_ERR_OPEN:      return "could not open input file";
        case IO_ERR_HEADER:    return "could not read 24-byte header";
        case IO_ERR_MAGIC:     return "bad magic number (not an ADC log)";
        case IO_ERR_VERSION:   return "unsupported file version";
        case IO_ERR_TRUNCATED: return "file is truncated (missing records)";
        case IO_ERR_ALLOC:     return "out of memory";
        case IO_ERR_WRITE:     return "could not open output file for writing";
        default:               return "unknown error";
    }
}

IoStatus io_read_file(const char *path,
                      ADCFileHeader *header,
                      ADCSample **out_samples,
                      size_t *out_count)
{
    *out_samples = NULL;
    *out_count   = 0;

    /* open in binary read mode */
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return IO_ERR_OPEN;

    /* read 24-byte header */
    if (fread(header, sizeof(*header), 1, fp) != 1) {
        fclose(fp);
        return IO_ERR_HEADER;
    }

    /* validate file format via magic number and version */
    if (header->magic != ADC_MAGIC) { fclose(fp); return IO_ERR_MAGIC; }
    if (header->version != ADC_EXPECT_VERSION) { fclose(fp); return IO_ERR_VERSION; }

    size_t n = header->record_count;

    /* allocate memory for samples */
    ADCSample *samples = malloc(n * sizeof(ADCSample));
    if (samples == NULL && n > 0) { fclose(fp); return IO_ERR_ALLOC; }

    /* read each record and copy it across into an ADCSample */
    size_t i;
    for (i = 0; i < n; ++i) {
        ADCRecordRaw raw;
        if (fread(&raw, sizeof(raw), 1, fp) != 1) {
            /* file truncated */
            free(samples);
            fclose(fp);
            return IO_ERR_TRUNCATED;
        }
        ADCSample *s = samples + i;            /* where this one goes */
        s->timestamp       = raw.timestamp;
        s->channel_id      = raw.channel_id;
        s->raw_value       = raw.raw_value;
        s->temperature     = raw.temperature;
        s->status_flags    = raw.status_flags;
        s->sequence_number = raw.sequence_number;
        s->voltage         = adc_raw_to_voltage(raw.raw_value);  /* work out volts now */
    }

    fclose(fp);
    *out_samples = samples;
    *out_count   = n;
    return IO_OK;
}

IoStatus io_write_results(const char *path,
                          const ADCFileHeader *header,
                          const ADCSample *samples, size_t count)
{
    /* open file for writing results */
    FILE *out = fopen(path, "w");
    if (out == NULL) return IO_ERR_WRITE;

    fprintf(out, "ADC Sensor Log - Analysis Results\n");
    fprintf(out, "=================================\n\n");

    fprintf(out, "File header:\n");
    fprintf(out, "  magic        0x%08X\n", header->magic);
    fprintf(out, "  version      %u\n", header->version);
    fprintf(out, "  channels     %u\n", header->channel_count);
    fprintf(out, "  records      %u\n", header->record_count);
    fprintf(out, "  sample rate  %u Hz\n\n", header->sample_rate_hz);

    fprintf(out, "Voltage stats per channel (Vref = %.1f V):\n", ADC_VREF);
    fprintf(out, "  %-4s %8s %8s %8s %8s %7s\n",
            "ch", "mean(V)", "min(V)", "max(V)", "stddev", "samples");

    uint8_t ch;
    for (ch = 0; ch < header->channel_count; ++ch) {
        ChannelStats st;
        adc_channel_stats(samples, count, ch, &st);
        fprintf(out, "  %-4u %8.4f %8.4f %8.4f %8.4f %7zu\n",
                ch, st.mean_v, st.min_v, st.max_v, st.stddev_v, st.count);
    }

    fprintf(out, "\nFaults per channel:\n");
    fprintf(out, "  %-4s %12s %12s %12s %8s\n",
            "ch", "overvolt", "undervolt", "sensorflag", "total");
    for (ch = 0; ch < header->channel_count; ++ch) {
        ChannelStats st;
        adc_channel_stats(samples, count, ch, &st);
        fprintf(out, "  %-4u %12zu %12zu %12zu %8zu\n",
                ch, st.overvoltage, st.undervoltage, st.sensor_fault, st.fault_total);
    }

    fprintf(out, "\nTemperature per channel in deg C (flagging over %.0f C):\n", ADC_TEMP_LIMIT_C);
    fprintf(out, "  %-4s %8s %8s %8s %10s\n",
            "ch", "min", "mean", "max", "over-limit");
    for (ch = 0; ch < header->channel_count; ++ch) {
        ChannelStats st;
        adc_channel_stats(samples, count, ch, &st);
        fprintf(out, "  %-4u %8.1f %8.1f %8.1f %10zu\n",
                ch, st.temp_min_c, st.temp_mean_c, st.temp_max_c, st.temp_over_limit);
    }

    fprintf(out, "\nVoltage histogram (10 bins, 0 to %.1f V):\n", ADC_VREF);
    for (ch = 0; ch < header->channel_count; ++ch) {
        size_t bins[10];
        adc_histogram(samples, count, ch, bins);
        fprintf(out, "  ch%u:", ch);
        int b;
        for (b = 0; b < 10; ++b) fprintf(out, " %4zu", bins[b]);
        fprintf(out, "\n");
    }

    fprintf(out, "\nSequence check:\n");
    SequenceGap gaps[64];
    uint32_t missing = 0;
    size_t ngaps = adc_check_integrity(samples, count, gaps, 64, &missing);
    if (ngaps == 0) {
        fprintf(out, "  all good - sequence numbers are consecutive, nothing missing.\n");
    } else {
        fprintf(out, "  %zu gap(s), %u record(s) missing in total:\n", ngaps, missing);
        size_t g;
        size_t shown = (ngaps < 64) ? ngaps : 64;
        for (g = 0; g < shown; ++g)
            fprintf(out, "    seq %u jumps to %u  (%u missing)\n",
                    gaps[g].prev_seq, gaps[g].curr_seq, gaps[g].missing);
    }

    fclose(out);
    return IO_OK;
}

IoStatus io_write_fault_report(const char *path,
                               const ADCSample *samples, size_t count)
{
    FILE *out = fopen(path, "w");
    if (out == NULL) return IO_ERR_WRITE;

    fprintf(out, "Fault report - one line per dodgy record\n");
    fprintf(out, "%10s %4s %8s %10s  %s\n",
            "time(s)", "ch", "raw", "volt(V)", "what's wrong");

    const ADCSample *p;
    for (p = samples; p < samples + count; ++p) {
        if (!adc_sample_is_faulted(p)) continue;

        /* append reasons for fault */
        char types[64] = "";
        if (p->voltage > ADC_OVERVOLT)  strcat(types, "OVERVOLT ");
        if (p->voltage < ADC_UNDERVOLT) strcat(types, "UNDERVOLT ");
        if (p->status_flags & ADC_FLAG_SENSOR_FAULT) strcat(types, "SENSOR-FLAG ");

        fprintf(out, "%10.4f %4u %8u %10.4f  %s\n",
                p->timestamp, p->channel_id, p->raw_value, p->voltage, types);
    }

    fclose(out);
    return IO_OK;
}