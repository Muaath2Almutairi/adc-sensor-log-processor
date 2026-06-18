#ifndef ADC_H
#define ADC_H

/* adc.h
 * 
 * Main definitions for ADC data. Holds the structs for samples
 * and channel statistics, plus ADC constants like Vref.
 */

#include <stdint.h>   /* uint8_t, uint16_t, ... */
#include <stddef.h>   /* size_t */

/* ADC constants */

#define ADC_VREF        3.3      /* 3.3V reference voltage */

/* 12-bit ADC max value. Used for converting raw code to volts. */
#define ADC_FULL_SCALE  4095.0

/* expected file header values for validation */
#define ADC_MAGIC          0xADC1BEEFu
#define ADC_EXPECT_VERSION 1
#define ADC_EXPECT_CHANS   4
#define ADC_EXPECT_RATE    1000

/* fault limits in volts */
#define ADC_OVERVOLT   3.0
#define ADC_UNDERVOLT  0.3

/* status flag bitmasks */
#define ADC_FLAG_SENSOR_FAULT  0x01u   /* bit 0 */
#define ADC_FLAG_OUT_OF_RANGE  0x02u   /* bit 1 */

#define ADC_TEMP_LIMIT_C  60.0   /* flag temps above this */

/* In-memory sample structure.
 * Differs from on-disk layout to include computed voltage
 * and exclude unused padding bytes. */
typedef struct {
    float    timestamp;
    uint8_t  channel_id;
    uint16_t raw_value;
    float    voltage;
    int16_t  temperature;
    uint8_t  status_flags;
    uint32_t sequence_number;
} ADCSample;

/* calculated statistics for a single channel */
typedef struct {
    uint8_t channel;
    size_t  count;          /* how many samples this channel had */

    double  mean_v;
    double  min_v;
    double  max_v;
    double  stddev_v;

    size_t  overvoltage;    /* > 3.0 V */
    size_t  undervoltage;   /* < 0.3 V */
    size_t  sensor_fault;   /* status bit 0 set */
    size_t  fault_total;    /* records that hit at least one of the above */

    double  temp_min_c;
    double  temp_mean_c;
    double  temp_max_c;
    size_t  temp_over_limit;
} ChannelStats;

/* stores info about a missing sequence of records */
typedef struct {
    uint32_t prev_seq;
    uint32_t curr_seq;
    uint32_t missing;       /* how many got lost in this gap */
} SequenceGap;

/* core analysis functions */

float adc_raw_to_voltage(uint16_t raw);

void adc_convert_all(ADCSample *samples, size_t count);

void adc_channel_stats(const ADCSample *samples, size_t count,
                       uint8_t channel, ChannelStats *out);

/* finds sequence gaps, returns total gaps found */
size_t adc_check_integrity(const ADCSample *samples, size_t count,
                           SequenceGap *gaps, size_t max_gaps,
                           uint32_t *out_total_missing);

/* extension functions */

/* calculates voltage histogram with 10 bins */
void adc_histogram(const ADCSample *samples, size_t count,
                   uint8_t channel, size_t bins[10]);

/* computes rolling average over specified window. caller must free result */
double *adc_sliding_mean(const ADCSample *samples, size_t count,
                         uint8_t channel, size_t window, size_t *out_len);

int adc_sample_is_faulted(const ADCSample *s);

#endif /* ADC_H */
