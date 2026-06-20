/* adc.c
 * 
 * Performs engineering analysis on ADC samples.
 * Calculates voltages, statistics, and detects faults.
 */

#include "adc.h"
#include "stats.h"
#include <stdlib.h>   /* malloc, free */

/* converts raw 12-bit code to voltage */
float adc_raw_to_voltage(uint16_t raw)
{
    return (float) ((raw / ADC_FULL_SCALE) * ADC_VREF);
}

/* go through every sample and fill in its voltage */
void adc_convert_all(ADCSample *samples, size_t count)
{
    ADCSample *p;
    for (p = samples; p < samples + count; ++p)
        p->voltage = adc_raw_to_voltage(p->raw_value);
}

/* is this one sample faulty for any reason? */
int adc_sample_is_faulted(const ADCSample *s)
{
    if (s->voltage > ADC_OVERVOLT)               return 1;
    if (s->voltage < ADC_UNDERVOLT)              return 1;
    if (s->status_flags & ADC_FLAG_SENSOR_FAULT) return 1;  /* check just bit 0 */
    return 0;
}

/* computes all statistics for a specific channel */
void adc_channel_stats(const ADCSample *samples, size_t count,
                       uint8_t channel, ChannelStats *out)
{
    /* allocate temporary arrays for stats functions */
    double *volt = malloc(count * sizeof(double));
    double *temp = malloc(count * sizeof(double));
    size_t n = 0;

    /* initialize stats to zero */
    out->channel        = channel;
    out->count          = 0;
    out->mean_v = out->min_v = out->max_v = out->stddev_v = 0.0;
    out->overvoltage = out->undervoltage = out->sensor_fault = out->fault_total = 0;
    out->temp_min_c = out->temp_mean_c = out->temp_max_c = 0.0;
    out->temp_over_limit = 0;

    if (volt == NULL || temp == NULL) {     /* malloc failed, get out cleanly */
        free(volt);
        free(temp);
        return;
    }

    const ADCSample *p;
    for (p = samples; p < samples + count; ++p) {
        if (p->channel_id != channel) continue;   /* skip other channels */

        volt[n] = p->voltage;
        temp[n] = p->temperature / 10.0;    /* stored as tenths of a degree */
        n++;

        /* count individual faults and track total faulty records */
        int faulted = 0;
        if (p->voltage > ADC_OVERVOLT)  { out->overvoltage++;  faulted = 1; }
        if (p->voltage < ADC_UNDERVOLT) { out->undervoltage++; faulted = 1; }
        if (p->status_flags & ADC_FLAG_SENSOR_FAULT) { out->sensor_fault++; faulted = 1; }
        if (faulted) out->fault_total++;
    }

    out->count = n;
    if (n > 0) {
        out->mean_v   = stats_mean(volt, n);
        out->min_v    = stats_min(volt, n);
        out->max_v    = stats_max(volt, n);
        out->stddev_v = stats_stddev(volt, n, out->mean_v);

        out->temp_mean_c = stats_mean(temp, n);
        out->temp_min_c  = stats_min(temp, n);
        out->temp_max_c  = stats_max(temp, n);

        size_t i;
        for (i = 0; i < n; ++i)
            if (temp[i] > ADC_TEMP_LIMIT_C) out->temp_over_limit++;
    }

    free(volt);
    free(temp);
}

/* finds sequence gaps and counts missing records */
size_t adc_check_integrity(const ADCSample *samples, size_t count,
                           SequenceGap *gaps, size_t max_gaps,
                           uint32_t *out_total_missing)
{
    size_t found = 0;
    uint32_t total_missing = 0;

    const ADCSample *p;
    for (p = samples + 1; p < samples + count; ++p) {
        uint32_t prev = (p - 1)->sequence_number;   /* the one before this */
        uint32_t curr = p->sequence_number;

        if (curr != prev + 1) {
            uint32_t missing = (curr > prev) ? (curr - prev - 1) : 0;
            total_missing += missing;
            if (found < max_gaps) {
                gaps[found].prev_seq = prev;
                gaps[found].curr_seq = curr;
                gaps[found].missing  = missing;
            }
            found++;
        }
    }

    if (out_total_missing) *out_total_missing = total_missing;
    return found;
}

/* calculates 10-bin voltage histogram */
void adc_histogram(const ADCSample *samples, size_t count,
                   uint8_t channel, size_t bins[10])
{
    const double width = ADC_VREF / 10.0;
    int i;
    for (i = 0; i < 10; ++i) bins[i] = 0;

    const ADCSample *p;
    for (p = samples; p < samples + count; ++p) {
        if (p->channel_id != channel) continue;
        int idx = (int) (p->voltage / width);
        if (idx < 0)  idx = 0;          /* anything weirdly negative -> bin 0 */
        if (idx > 9)  idx = 9;          /* exactly Vref or over -> last bin */
        bins[idx]++;
    }
}

/* rolling average over the last `window` readings on a channel */
double *adc_sliding_mean(const ADCSample *samples, size_t count,
                         uint8_t channel, size_t window, size_t *out_len)
{
    if (window == 0) window = 1;

    /* pull out this channel's voltages in order first */
    double *chan = malloc(count * sizeof(double));
    if (chan == NULL) { *out_len = 0; return NULL; }

    size_t n = 0;
    const ADCSample *p;
    for (p = samples; p < samples + count; ++p)
        if (p->channel_id == channel) chan[n++] = p->voltage;

    double *out = malloc((n ? n : 1) * sizeof(double));
    if (out == NULL) { free(chan); *out_len = 0; return NULL; }

    /* calculate average over preceding window */
    size_t i;
    for (i = 0; i < n; ++i) {
        size_t start = (i + 1 >= window) ? (i + 1 - window) : 0;
        double sum = 0.0;
        size_t j;
        for (j = start; j <= i; ++j) sum += chan[j];
        out[i] = sum / (double) (i - start + 1);
    }

    free(chan);
    *out_len = n;
    return out;
}