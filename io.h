#ifndef IO_H
#define IO_H

/* io.h
 *
 * Handles binary file reading and report writing.
 * Isolates file layout specifics from the main analysis.
 */

#include "adc.h"
#include <stdint.h>
#include <stddef.h>

/* file header struct, directly matches the 24-byte header in the binary */
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t channel_count;
    uint32_t record_count;
    uint32_t sample_rate_hz;
    uint8_t  reserved[8];
} ADCFileHeader;

/* return status codes for file operations */
typedef enum {
    IO_OK = 0,
    IO_ERR_OPEN      = -1,   /* couldn't open the file */
    IO_ERR_HEADER    = -2,   /* couldn't read the header */
    IO_ERR_MAGIC     = -3,   /* wrong magic number */
    IO_ERR_VERSION   = -4,   /* version we don't handle */
    IO_ERR_TRUNCATED = -5,   /* file ended before all the records did */
    IO_ERR_ALLOC     = -6,   /* malloc failed */
    IO_ERR_WRITE     = -7    /* couldn't open the output file */
} IoStatus;

/* reads and validates the log file. allocates memory for samples */
IoStatus io_read_file(const char *path,
                      ADCFileHeader *header,
                      ADCSample **out_samples,
                      size_t *out_count);

/* writes main analysis report */
IoStatus io_write_results(const char *path,
                          const ADCFileHeader *header,
                          const ADCSample *samples, size_t count);

/* writes detailed fault report */
IoStatus io_write_fault_report(const char *path,
                               const ADCSample *samples, size_t count);

/* error code -> message */
const char *io_status_str(IoStatus s);

#endif /* IO_H */
