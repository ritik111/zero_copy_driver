#ifndef SHARED_H
#define SHARED_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#define BUFFER_ORDER 4    //2^4 = 16pages = 64KB
#define PAGE_SIZE_VAL 4096 
#define BUFFER_SIZE (PAGE_SIZE_VAL << BUFFER_ORDER)

struct rb_metadata {
    unsigned int head;
    unsigned int tail;
    unsigned long total_bytes_produced;
    unsigned long overflow_count;
    unsigned long total_bytes_consumed;
    char data[];    //Flexible Array Member
};

#define DATA_SIZE (BUFFER_SIZE - sizeof(struct rb_metadata))

#endif
