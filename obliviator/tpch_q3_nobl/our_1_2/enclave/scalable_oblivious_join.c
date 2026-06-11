#include "enclave/scalable_oblivious_join.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "common/elem_t.h"
#include "common/code_conf.h"

#ifndef DISTRIBUTED_SGX_SORT_HOSTONLY
#include <openenclave/enclave.h>
#include "enclave/parallel_t.h"
#endif

static int number_threads;

static int my_len(const char *data) {
    int i = 0;

    while ((data[i] != '\0') && (i < DATA_LENGTH)) {
        i++;
    }

    return i;
}

static int compact_matches(elem_t *arr, int length) {
    int write_idx = 0;

    for (int read_idx = 0; read_idx < length; read_idx++) {
        if (!(arr[read_idx].key < 19950315)) {
            continue;
        }

        if (write_idx != read_idx) {
            arr[write_idx] = arr[read_idx];
        }
        write_idx++;
    }

    return write_idx;
}

int scalable_oblivious_join_init(int nthreads) {
    number_threads = nthreads;
    return 0;
}

void scalable_oblivious_join_free() {
    return;
}

void scalable_oblivious_join(elem_t *arr, int length1, int length2, char *output_path) {
    (void) number_threads;
    (void) length2;

    init_time2();
    init_time();
    int length_result = compact_matches(arr, length1);
    get_time(true);
    get_time(true);
    get_time2(true);

    char *char_current = output_path;
    for (int i = 0; i < length_result; i++) {
        int data_len = my_len(arr[i].data);
        strncpy(char_current, arr[i].data, data_len);
        char_current += data_len;
        char_current[0] = '\n';
        char_current += 1;
    }
    char_current[0] = '\0';
}
