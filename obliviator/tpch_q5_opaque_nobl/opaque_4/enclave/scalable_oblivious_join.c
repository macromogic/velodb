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

typedef struct agg_row {
    elem_t row;
    int original_index;
} agg_row_t;

static int my_len(const char *data) {
    int i = 0;

    while ((data[i] != '\0') && (i < DATA_LENGTH)) {
        i++;
    }

    return i;
}

static int compare_group_key(const void *lhs, const void *rhs) {
    const agg_row_t *left = lhs;
    const agg_row_t *right = rhs;

    if (left->row.key < right->row.key) {
        return -1;
    }
    if (left->row.key > right->row.key) {
        return 1;
    }
    if (left->original_index < right->original_index) {
        return -1;
    }
    if (left->original_index > right->original_index) {
        return 1;
    }

    return 0;
}

static int compare_final_rows(const void *lhs, const void *rhs) {
    const elem_t *left = lhs;
    const elem_t *right = rhs;

    if (left->sum_adrevenue > right->sum_adrevenue) {
        return -1;
    }
    if (left->sum_adrevenue < right->sum_adrevenue) {
        return 1;
    }
    if (left->key < right->key) {
        return -1;
    }
    if (left->key > right->key) {
        return 1;
    }
    if (left->adrevenue < right->adrevenue) {
        return -1;
    }
    if (left->adrevenue > right->adrevenue) {
        return 1;
    }
    if (left->discount < right->discount) {
        return -1;
    }
    if (left->discount > right->discount) {
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
    (void) length2;

    if (length1 <= 0) {
        output_path[0] = '\0';
        return;
    }

    agg_row_t *rows = calloc((size_t) length1, sizeof(*rows));
    int result_count = 0;

    for (int i = 0; i < length1; i++) {
        rows[i].row = arr[i];
        rows[i].original_index = i;
    }

    init_time2();
    qsort(rows, (size_t) length1, sizeof(*rows), compare_group_key);

    for (int i = 0; i < length1;) {
        int j = i;
        float sum_adrevenue = 0.0f;

        while ((j < length1) && (rows[j].row.key == rows[i].row.key)) {
            sum_adrevenue += rows[j].row.adrevenue * (1.0f - rows[j].row.discount);
            j++;
        }

        arr[result_count] = rows[i].row;
        arr[result_count].sum_adrevenue = sum_adrevenue;
        result_count++;
        i = j;
    }

    qsort(arr, (size_t) result_count, sizeof(*arr), compare_final_rows);
    get_time2(true);

    char *char_current = output_path;
    for (int i = 0; i < result_count; i++) {
        char nation[20];
        int nation_len;
        sprintf(nation, "%lu", (unsigned long) arr[i].key);
        nation_len = my_len(nation);

        char revenue[20];
        int revenue_len;
        sprintf(revenue, "%f", arr[i].sum_adrevenue);
        revenue_len = my_len(revenue);

        strncpy(char_current, nation, nation_len);
        char_current += nation_len;
        char_current[0] = ' ';
        char_current += 1;
        strncpy(char_current, revenue, revenue_len);
        char_current += revenue_len;
        char_current[0] = '\n';
        char_current += 1;
    }
    char_current[0] = '\0';

    free(rows);
}
