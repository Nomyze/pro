#ifndef MCAP_FORMAT_H
#define MCAP_FORMAT_H

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
#define MCAP_MAGIC 0x4d434150 // MCAP -> ascii, also defines the endianness -> le, 
struct mcap_file_header {
    uint32_t magic;
    uint16_t version_major;
    uint16_t version_minor;
    uint64_t start_time;
    uint32_t metadata_len;
} __attribute__((packed));

/* followed by metadata entries ie process name */
struct mcap_meta_entry_header {
    uint16_t type;
    uint16_t length;
} __attribute__((packed));

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
    MCAP_WRITE_FULL = 4,
    MCAP_WRITE_DIFF = 5
};

struct mcap_event_header {
    uint64_t timestamp;
    uint32_t type;
    uint32_t length; // size of struct and any data following it (mcap_write)
} __attribute__((packed));

struct mcap_alloc {
    uint64_t addr;
    uint64_t size;
    uint32_t tid;
    uint32_t type_pad; // malloc - 0, calloc - 1 + padding
} __attribute__((packed));
struct mcap_realloc {
    uint64_t addr_source;
    uint64_t addr_dest;
    uint64_t size;
    uint32_t tid;
} __attribute__((packed));

struct mcap_free {
    uint64_t addr;
    uint32_t tid;
} __attribute__((packed));

struct mcap_write_batch {
    uint64_t addr;
    uint64_t snapshot_num;
    uint32_t tid;
    uint32_t batches;
} __attribute__((packed)); // followed by batches mcap_write

struct mcap_write {
    uint64_t offset;
    uint64_t size;
} __attribute__((packed)); // followed by size bytes of data

#endif
