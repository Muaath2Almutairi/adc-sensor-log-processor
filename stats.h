#ifndef STATS_H
#define STATS_H

/* stats.h
 *
 * Math functions for arrays (mean, min, max, standard deviation).
 * These are generic and can be used independently of the ADC logic.
 */

#include <stddef.h>   /* size_t */

double stats_mean(const double *values, size_t n);
double stats_min(const double *values, size_t n);
double stats_max(const double *values, size_t n);

/* computes standard deviation using a pre-calculated mean */
double stats_stddev(const double *values, size_t n, double mean);

#endif /* STATS_H */
