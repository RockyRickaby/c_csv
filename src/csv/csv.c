#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "csv.h"

static int csv_parse(FILE* csvfile, char delim);
static int csv_read(FILE* csvfile, _row_t rows, struct _column_s* columns, size_t* row_l, size_t* col_l);
static char* readline(FILE* file, size_t* out_size);
static size_t str_split(const char* str, char delim, int trim, char*** out);
static void str_split_free(char** list, size_t len);
static char* ltrim(const char* str);
static char* rtrim(const char* str);

int csv_open(const char* filename, char delim, CsvFile* out) {
    if (!filename || !out) {
        return 1;
    }
    FILE* csvfile = fopen(filename, "r");
    if (!csvfile) {
        perror("csv_open");
        return 1;
    }
    int err = csv_parse(csvfile, delim);
    if (err != 0) {
        if (err == -1) {
            fprintf(stderr, "csv_parse: Malformed csv (empty file), delimiter %c\n", delim);
        } else if (err >= 1) {
            fprintf(stderr, "csv parse: Malformed csv (items in row %d don't match with number of columns), delimiter %c\n", err, delim);
        }
        fclose(csvfile);
        return 1;
    }
    size_t len = strlen(filename);
    char* fname = malloc((len + 1) * sizeof(char));
    strncpy(fname, filename, len);

    CsvFile file = {
        .file = csvfile,
        .filename = fname,
        .columns_len = 0,
        .columns = NULL,
        .rows = NULL,
        .rows_len = 0,
        .delim = delim,
    };
    *out = file; /* copy everything */
    return 0;
}

int csv_close(CsvFile* out);

char* csv_at(CsvFile* file, const char* column, int row);
int csv_row(CsvFile* file, int row, Row* out);
char* csv_row_at(Row* row, const char* column);
char* csv_row_at_idx(Row* row, int idx);


static int csv_parse(FILE* csvfile, char delim) {
    char delim_str[] = { delim, 0 };
    int line_count = 1;

    char* line = NULL;
    size_t line_len = 0;
    line = readline(csvfile, &line_len);
    if (line_len == 0) { /* don't accept empty files for now */
        return -1;
    }

    // columns
    char* token = strtok(line, delim_str);
    size_t col_count = token ? 1 : 0;
    while (token) {
        token = strtok(NULL, delim_str);
        col_count++;
    }
    free(line);

    // rows
    while ((line = readline(csvfile, &line_len))) {
        line_count++;
        char** out = NULL;
        size_t len = str_split(line, delim, 0, &out);
        if (len == 1) {
            if (*out == line) {
                free(out);
            } else {
                str_split_free(out, len);
            }
        }
        free(line);
        if (len != col_count) {
            return line_count;
        }
    }
    return 0;
}

static char* ltrim(const char* str) {
   while (isspace((unsigned int) *str)) str++;
   return str;  
}

static char* rtrim(const char* str) {
    char* stre = str + (strlen(str) - 1);
    while (isspace((unsigned int) *stre)) stre--;
    return stre + 1;
}

static size_t str_split(const char* str, char delim, int trim, char*** out) {
    size_t bufsize = 10;
    size_t count = 0;

    char** buffer = calloc(1, sizeof(char*)); /* in case the result is the string itself */
    if (!buffer) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    const char* strb = str;
    char* del = strchr(str, (unsigned int) delim);
    if (!del) {
        buffer[count++] = str;
        *out = buffer; 
        return count;
    }
    char** tmp = realloc(buffer, sizeof(char*) * bufsize);
    if (!tmp) {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    buffer = tmp;

    size_t split_len = del - strb;
    if (split_len != 0) {
        buffer[count] = malloc(sizeof(char) * (split_len + 1));
        strncpy(buffer[count], strb, split_len);
        buffer[count][split_len] = 0;
        count++;
    }
    strb = del + 1;
    while ((del = strchr(strb, (unsigned int) delim))) {
        split_len = del - strb;
        if (split_len == 0) {
            strb = del + 1;
            continue;
        }
        buffer[count] = malloc(sizeof(char) * (split_len + 1));
        strncpy(buffer[count], strb, split_len);
        buffer[count][split_len] = 0;
        strb = del + 1;
        count++;
        
        if (count >= bufsize) {
            bufsize *= 1.5f;
            tmp = realloc(buffer, sizeof(char*) * bufsize);
            if (!tmp) {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
            buffer = tmp;
        }
    }
    strb = strrchr(str, (unsigned int) delim) + 1;
    del = str + strlen(str);

    split_len = del - strb;
    if (split_len != 0) {
        buffer[count] = malloc(sizeof(char) * (split_len + 1));
        strncpy(buffer[count], strb, split_len);
        buffer[count][split_len] = 0;
        count++;
    }
    if (count == 0) {
        free(buffer);
        *out = NULL;
        return 0;
    }
    tmp = realloc(buffer, sizeof(char*) * count);
    if (!tmp) {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    *out = tmp;
    return count;
}

static void str_split_free(char** list, size_t len) {
    for (size_t i = 0; i < len; i++) {
        free(list[i]);
    }
    free(list);
}

static char* readline(FILE* file, size_t* out_size) {
    if (feof(file) || ferror(file)) {
        *out_size = 0;
        return NULL;   
    }

    size_t buf_size = 0;
    size_t buf_cap = 10;
    char* buf = calloc(buf_cap, sizeof(char));
    if (buf == NULL) {
        *out_size = 0;
        return NULL;
    }

    int ch = 0;
    int beg = 1; /* to skip over the leftover carriage returns and similar newline chars */
    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n' || ch == '\r') {
            if (beg && file != stdin) {
                continue;
            }
            break;
        }
        beg = 0;
        if (ch == '\b') {
            if (buf_size > 0) {
                buf_size -= 1;
            }
            continue;
        }
        if (buf_size >= buf_cap) {
            buf_cap *= 1.5f;
            char* new_buf = realloc(buf, sizeof(char) * buf_cap);
            if (new_buf == NULL) {
                free(buf);
                *out_size = 0;
                return NULL;
            }
            buf = new_buf;
        }
        buf[buf_size++] = ch;
    }

    char* new_buf = realloc(buf, sizeof(char) * (buf_size + 1)); /* +1 for the null character */
    if (new_buf == NULL) {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    buf = new_buf;
    buf[buf_size] = '\0';
    *out_size = buf_size;
    return buf;
}