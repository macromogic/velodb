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
    if (left->order_date < right->order_date) {
        return -1;
    }
    if (left->order_date > right->order_date) {
        return 1;
    }
    if (left->key < right->key) {
        return -1;
    }
    if (left->key > right->key) {
        return 1;
    }
    if (left->order_key < right->order_key) {
        return -1;
    }
    if (left->order_key > right->order_key) {
        return 1;
    }
    if (left->ship_priority < right->ship_priority) {
        return -1;
    }
    if (left->ship_priority > right->ship_priority) {
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
    for (int i = 0; (i < result_count) && (i < 20); i++) {
        char order_key[20];
        int order_key_len;
        sprintf(order_key, "%d", arr[i].order_key);
        order_key_len = my_len(order_key);

        char revenue[20];
        int revenue_len;
        sprintf(revenue, "%f", arr[i].sum_adrevenue);
        revenue_len = my_len(revenue);

        char order_date[20];
        int order_date_len;
        sprintf(order_date, "%d", arr[i].order_date);
        order_date_len = my_len(order_date);

        char ship_priority[20];
        int ship_priority_len;
        sprintf(ship_priority, "%f", arr[i].ship_priority);
        ship_priority_len = my_len(ship_priority);

        strncpy(char_current, order_key, order_key_len);
        char_current += order_key_len;
        char_current[0] = ' ';
        char_current += 1;
        strncpy(char_current, revenue, revenue_len);
        char_current += revenue_len;
        char_current[0] = ' ';
        char_current += 1;
        strncpy(char_current, order_date, order_date_len);
        char_current += order_date_len;
        char_current[0] = ' ';
        char_current += 1;
        strncpy(char_current, ship_priority, ship_priority_len);
        char_current += ship_priority_len;
        char_current[0] = ' ';
        char_current += 1;
        char_current[0] = '\n';
        char_current += 1;
    }
    char_current[0] = '\0';

    free(rows);
}
