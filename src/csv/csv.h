#ifndef CSV_H
#define CSV_H

#include <stdio.h>

#define CSV_DEFAULT_DELIMITER ','

struct _column_s {
    struct csvf_s* csvfile;
    char* name;
    int idx;
};
typedef struct _column_s _column_t; /* for consistency */

struct _row_s {
    struct csvf_s* csvfile;
    char** values;
    size_t len;
};

#ifdef CSV_USE_ROW_STRUCT
typedef struct _row_s _row_t; /* list of rows */
#else
typedef char* _row_t;
#endif /* ROW_STRUCT */

struct csvf_s {
    FILE* file;
    char* filename;
    
    _column_t* columns; /* list of columns */
    size_t columns_len;
    _row_t* rows;
    size_t rows_len;

    char delim;
};

typedef struct _row_s Row;
typedef struct csvf_s CsvFile;

int csv_open(const char* filename, char delim, CsvFile* out);
int csv_close(CsvFile* out);

char* csv_at(CsvFile* file, const char* column, int row);
int csv_row(CsvFile* file, int row, Row* out);
char* csv_row_at(Row* row, const char* column);
char* csv_row_at_idx(Row* row, int idx);

#endif /* CSV_H */