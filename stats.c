/* stats.c
 * 
 * Mathematical helper functions.
 * Uses pointer arithmetic to traverse arrays.
 */

#include "stats.h"
#include <math.h>   /* sqrt */

double stats_mean(const double *values, size_t n)
{
    if (n == 0) return 0.0;            /* guard against division by zero */

    double sum = 0.0;
    const double *p;
    for (p = values; p < values + n; ++p)
        sum += *p;

    return sum / (double) n;
}

double stats_min(const double *values, size_t n)
{
    if (n == 0) return 0.0;

    double m = *values;                /* assume first element is minimum */
    const double *p;
    for (p = values + 1; p < values + n; ++p)
        if (*p < m) m = *p;

    return m;
}

double stats_max(const double *values, size_t n)
{
    if (n == 0) return 0.0;

    double m = *values;
    const double *p;
    for (p = values + 1; p < values + n; ++p)
        if (*p > m) m = *p;

    return m;
}

double stats_stddev(const double *values, size_t n, double mean)
{
    if (n == 0) return 0.0;

    /* second pass: sum squared differences from the mean */
    double sum_sq = 0.0;
    const double *p;
    for (p = values; p < values + n; ++p) {
        double d = *p - mean;
        sum_sq += d * d;
    }
    return sqrt(sum_sq / (double) n);
}