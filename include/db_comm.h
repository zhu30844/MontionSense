/*****************************************************************************
* | File        :   db_comm.h
* | Author      :   ZIXUAN ZHU
* | Function    :   Database operations
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2025-02-16
* | Info        :   Basic version
*
# The MIT License (MIT)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/


#ifndef __DB_COMM_H__
#define __DB_COMM_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif

#include "sqlite3.h"
#include <stdio.h>
#include <pthread.h>
#include "util_comm.h"
#include "storage_comm.h"

// Database operation result codes
typedef enum {
    DB_SUCCESS = 0,                    // Operation completed successfully
    DB_ERROR_NULL_PARAM,              // NULL parameter provided
    DB_ERROR_INVALID_PARAM,           // Invalid parameter value
    DB_ERROR_NOT_INITIALIZED,         // Database not initialized
    DB_ERROR_MUTEX_INIT,              // Failed to initialize mutex
    DB_ERROR_DB_OPEN,                 // Failed to open database
    DB_ERROR_SQL_PREPARE,             // Failed to prepare SQL statement
    DB_ERROR_SQL_EXECUTE,             // Failed to execute SQL statement
    DB_ERROR_SQL_BIND,                // Failed to bind SQL parameters
    DB_ERROR_SQL_STEP,                // Failed to step SQL statement
    DB_ERROR_TABLE_NOT_EXISTS,        // Table does not exist
    DB_ERROR_RECORD_NOT_FOUND,        // Record not found
    DB_ERROR_DUPLICATE_RECORD,        // Duplicate record
    DB_ERROR_MEMORY,                  // Memory allocation failed
    DB_ERROR_UNKNOWN                  // Unknown error
} db_result_t;


typedef struct
{
    sqlite3 *db;
    pthread_mutex_t mutex;
} Database;

// Helper macro for error checking
#define DB_CHECK_NULL(ptr) \
    if ((ptr) == NULL) { \
        fprintf(stderr, "%s: NULL parameter\n", __func__); \
        return DB_ERROR_NULL_PARAM; \
    }

#define DB_CHECK_INIT(db_ptr) \
    if ((db_ptr) == NULL || (db_ptr)->db == NULL) { \
        fprintf(stderr, "%s: Database not initialized\n", __func__); \
        return DB_ERROR_NOT_INITIALIZED; \
    }

// Helper function to convert SQLite error codes to our error codes
db_result_t db_convert_sqlite_error(int sqlite_rc);

// Helper function to get error message
const char* db_get_error_message(db_result_t result);

db_result_t db_init(Database *database, const char *db_path);
void db_close(Database *database);
int check_table_exists(Database *database, const char *table_name);
void event_logs_db_init(const char *dated_video_path);
void video_metadata_db_init();
void databases_deinit();
void event_logs_db_deinit();
void video_metadata_db_deinit();
db_result_t addVideoMetadata(const char *date, int motion_count);
int addEventLog(int folder, const char *start_time, int length);
db_result_t addEventDetail(int video_id, int motion_time);
Database *get_database(const char *db_name);
int create_table(Database *database, const char *create_sql);
int update_video_Length_db(int id, int new_length);
db_result_t update_motion_time_db(const char *date, int new_motion_count);
int getEventDetailsCount();
int db_delete_record_date(const char *date);
int db_get_earliest_date(char *earliest_date);
char *get_all_video_segments_json(char *date);
char *get_all_video_segments_json_(Database pEventLogs);
char *get_all_motion_counts_json();
char *get_all_motion_points_json(char *date);
char *get_all_motion_points_json_(Database pEventLogs);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif