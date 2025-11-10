#include <stdint.h>
// Modeled after the pcap format

/* *-----------------*
 * |   file_header   |
 * *-----------------*
 * |   meta_hdr 1    |
 * *-----------------*
 * | meta_data_bytes |
 * *-----------------*
 * |   meta_hdr 2    |
 * *-----------------*
 * |      ...        |
 * *-----------------*
 * | event_header 1  |
 * *-----------------*
 * |   event_data 1  |
 * *-----------------*
 * |      ...        |
 * *-----------------*
 */

/* file begins with a general file header */
#define MCAP_MAGIC 0x4d434150 // MCAP -> ascii
struct mcap_file_header {
    uint32_t magic;
    uint16_t version_major;
    uint16_t version_minor;
    uint64_t start_time;
    uint32_t metadata_len;
};

/* followed by metadata entries ie process name */
struct mcap_meta_entry_header {
    uint16_t type;
    uint16_t length;
};

enum Metadata_type {
    MCAP_PROCESS_NAME = 1,
    MCAP_PID = 2,
    MCAP_COMMAND = 3,
    MCAP_STRING = 4
};

/* data of mcap_meta_entry_header.length length */


/* Start of memory events */

enum Event_types {
    MCAP_ALLOC = 1, // malloc, calloc
    MCAP_REALLOC = 2,
    MCAP_FREE = 3,
    MCAP_WRITE = 4
};

struct mcap_event_header {
    uint64_t timestamp;
    uint32_t type;
    uint32_t length; // size of struct and any data following it (mcap_write)
};

struct mcap_alloc {
    uint64_t addr;
    uint64_t size;
    uint32_t tid;
    uint32_t type_pad; // malloc - 0, calloc - 1 + padding
};
struct mcap_realloc {
    uint64_t addr_source;
    uint64_t addr_dest;
    uint64_t size;
    uint32_t tid;
};

struct mcap_free {
    uint64_t addr;
    uint32_t tid;
};

struct mcap_write {
    uint64_t addr;
    uint32_t tid;
    uint64_t bytes_written;
    uint64_t offset;
}; // followed by bytes_written bytes
