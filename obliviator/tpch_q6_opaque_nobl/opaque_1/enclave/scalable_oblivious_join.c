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

static bool keep_row(const elem_t *row) {
    return (19940101 <= row->date) &&
        (row->date < 19950101) &&
        (0.05f <= row->discount) &&
        (row->discount <= 0.07f) &&
        (row->quantity < 24);
}

static int compact_matches(elem_t *arr, int length) {
    int write_idx = 0;

    for (int read_idx = 0; read_idx < length; read_idx++) {
        if (!keep_row(&arr[read_idx])) {
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
    int length_result = compact_matches(arr, length1);
    get_time2(true);

    char *char_current = output_path;
    for (int i = 0; i < length_result; i++) {
        char price[20];
        int price_len;
        sprintf(price, "%.2f", arr[i].price);
        price_len = my_len(price);

        char discount[20];
        int discount_len;
        sprintf(discount, "%.2f", arr[i].discount);
        discount_len = my_len(discount);

        strncpy(char_current, price, price_len);
        char_current += price_len;
        char_current[0] = ' ';
        char_current += 1;
        strncpy(char_current, discount, discount_len);
        char_current += discount_len;
        char_current[0] = '\n';
        char_current += 1;
    }
    char_current[0] = '\0';
}
