#include "enclave/scalable_oblivious_join.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common/elem_t.h"
#include "common/code_conf.h"

#ifndef DISTRIBUTED_SGX_SORT_HOSTONLY
#include <openenclave/enclave.h>
#include "enclave/parallel_t.h"
#endif

static int number_threads;

typedef struct join_row {
    elem_t row;
    int original_index;
} join_row_t;

static int my_len(const char *data) {
    int i = 0;

    while ((data[i] != '\0') && (i < DATA_LENGTH)) {
        i++;
    }

    return i;
}

static int compare_join_row(const void *lhs, const void *rhs) {
    const join_row_t *left = lhs;
    const join_row_t *right = rhs;

    if (left->row.key < right->row.key) {
        return -1;
    }
    if (left->row.key > right->row.key) {
        return 1;
    }
    if (left->row.table_0 != right->row.table_0) {
        return left->row.table_0 ? -1 : 1;
    }

    int data_cmp = memcmp(left->row.data, right->row.data, DATA_LENGTH);
    if (data_cmp != 0) {
        return data_cmp;
    }
    if (left->original_index < right->original_index) {
        return -1;
    }
    if (left->original_index > right->original_index) {
        return 1;
    }

    return 0;
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

    int length = length1 + length2;
    join_row_t *rows = calloc((size_t) length, sizeof(*rows));
    elem_t *left_matches = calloc((size_t) length, sizeof(*left_matches));
    elem_t *right_matches = calloc((size_t) length, sizeof(*right_matches));
    int match_count = 0;
    elem_t current_left;
    bool have_left = false;

    for (int i = 0; i < length; i++) {
        rows[i].row = arr[i];
        rows[i].original_index = i;
    }

    init_time2();
    qsort(rows, (size_t) length, sizeof(*rows), compare_join_row);

    for (int i = 0; i < length; i++) {
        if (rows[i].row.table_0) {
            current_left = rows[i].row;
            have_left = true;
            continue;
        }

        if (have_left && current_left.key == rows[i].row.key) {
            left_matches[match_count] = current_left;
            right_matches[match_count] = rows[i].row;
            match_count++;
        }
    }

    get_time2(true);

    char *char_current = output_path;
    for (int i = 0; i < match_count; i++) {
        int left_len = my_len(left_matches[i].data);
        int right_len = my_len(right_matches[i].data);

        strncpy(char_current, left_matches[i].data, left_len);
        char_current += left_len;
        char_current[0] = '@';
        char_current[1] = '$';
        char_current += 2;

        strncpy(char_current, right_matches[i].data, right_len);
        char_current += right_len;
        char_current[0] = '\n';
        char_current += 1;
    }
    char_current[0] = '\0';

    free(right_matches);
    free(left_matches);
    free(rows);
}
