/*****************************************************************************
* | File        :   db.c
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

#include "db_comm.h"

pthread_mutex_t db_init_mutex;

Database video_metadata_db;
Database event_logs_db;

// Helper function to convert SQLite error codes to our error codes
db_result_t db_convert_sqlite_error(int sqlite_rc)
{
    switch (sqlite_rc) {
        case SQLITE_OK:
            return DB_SUCCESS;
        case SQLITE_ERROR:
        case SQLITE_INTERNAL:
        case SQLITE_PERM:
        case SQLITE_ABORT:
        case SQLITE_BUSY:
        case SQLITE_LOCKED:
        case SQLITE_NOMEM:
        case SQLITE_READONLY:
        case SQLITE_INTERRUPT:
        case SQLITE_IOERR:
        case SQLITE_CORRUPT:
        case SQLITE_NOTFOUND:
        case SQLITE_FULL:
        case SQLITE_CANTOPEN:
        case SQLITE_PROTOCOL:
        case SQLITE_EMPTY:
        case SQLITE_SCHEMA:
        case SQLITE_TOOBIG:
        case SQLITE_CONSTRAINT:
        case SQLITE_MISMATCH:
        case SQLITE_MISUSE:
        case SQLITE_NOLFS:
        case SQLITE_AUTH:
        case SQLITE_FORMAT:
        case SQLITE_RANGE:
        case SQLITE_NOTADB:
        case SQLITE_NOTICE:
        case SQLITE_WARNING:
        case SQLITE_ROW:
        case SQLITE_DONE:
        default:
            return DB_ERROR_SQL_EXECUTE;
    }
}

// Helper function to get error message
const char* db_get_error_message(db_result_t result)
{
    switch (result) {
        case DB_SUCCESS:
            return "Operation completed successfully";
        case DB_ERROR_NULL_PARAM:
            return "NULL parameter provided";
        case DB_ERROR_INVALID_PARAM:
            return "Invalid parameter value";
        case DB_ERROR_NOT_INITIALIZED:
            return "Database not initialized";
        case DB_ERROR_MUTEX_INIT:
            return "Failed to initialize mutex";
        case DB_ERROR_DB_OPEN:
            return "Failed to open database";
        case DB_ERROR_SQL_PREPARE:
            return "Failed to prepare SQL statement";
        case DB_ERROR_SQL_EXECUTE:
            return "Failed to execute SQL statement";
        case DB_ERROR_SQL_BIND:
            return "Failed to bind SQL parameters";
        case DB_ERROR_SQL_STEP:
            return "Failed to step SQL statement";
        case DB_ERROR_TABLE_NOT_EXISTS:
            return "Table does not exist";
        case DB_ERROR_RECORD_NOT_FOUND:
            return "Record not found";
        case DB_ERROR_DUPLICATE_RECORD:
            return "Duplicate record";
        case DB_ERROR_MEMORY:
            return "Memory allocation failed";
        case DB_ERROR_UNKNOWN:
        default:
            return "Unknown error";
    }
}

// initialize a database, create a new connection to the database
db_result_t db_init(Database *database, const char *db_path)
{
    int rc;
    
    // Check input parameters
    DB_CHECK_NULL(database);
    DB_CHECK_NULL(db_path);
    
    // Initialize mutex first
    rc = pthread_mutex_init(&database->mutex, NULL);
    if (rc != 0) {
        fprintf(stderr, "db_init: Failed to initialize mutex: %s\n", strerror(rc));
        return DB_ERROR_MUTEX_INIT;
    }
    
    pthread_mutex_lock(&database->mutex);
    
    // Open database connection
    rc = sqlite3_open(db_path, &database->db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Connecting %s failed: %s\n", db_path, sqlite3_errmsg(database->db));
        sqlite3_close(database->db);
        database->db = NULL;
        pthread_mutex_unlock(&database->mutex);
        pthread_mutex_destroy(&database->mutex);
        return DB_ERROR_DB_OPEN;
    }

    // Configure database for better performance
    char *errMsg = 0;
    if (sqlite3_exec(database->db, "PRAGMA journal_mode=WAL", 0, 0, &errMsg) != SQLITE_OK) {
        fprintf(stderr, "Failed to set WAL mode: %s\n", errMsg);
        sqlite3_free(errMsg);
    }
    
    if (sqlite3_exec(database->db, "PRAGMA synchronous=NORMAL", 0, 0, &errMsg) != SQLITE_OK) {
        fprintf(stderr, "Failed to set synchronous mode: %s\n", errMsg);
        sqlite3_free(errMsg);
    }
    
    if (sqlite3_exec(database->db, "PRAGMA cache_size=1000", 0, 0, &errMsg) != SQLITE_OK) {
        fprintf(stderr, "Failed to set cache size: %s\n", errMsg);
        sqlite3_free(errMsg);
    }

    pthread_mutex_unlock(&database->mutex);
    return DB_SUCCESS;
}

// close a database
void db_close(Database *database)
{
    if (database == NULL) {
        return;
    }
    
    pthread_mutex_lock(&database->mutex);
    if (database->db != NULL) {
        sqlite3_close(database->db);
        database->db = NULL;
    }
    pthread_mutex_unlock(&database->mutex);
    pthread_mutex_destroy(&database->mutex);
}

// callback function for checking if a table exists
int table_exists_callback(void *data, int argc, char **argv, char **col_name)
{
    int *exists = (int *)data;
    *exists = (argc > 0);
    return 0;
}

// check if a table exists
int check_table_exists(Database *database, const char *table_name)
{
    int rc;
    char *errMsg = 0;
    int table_exists = 0;
    char sql[256];

    snprintf(sql, sizeof(sql), "SELECT name FROM sqlite_master WHERE type='table' AND name='%s';", table_name);

    pthread_mutex_lock(&database->mutex);
    rc = sqlite3_exec(database->db, sql, table_exists_callback, &table_exists, &errMsg);
    pthread_mutex_unlock(&database->mutex);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return -1;
    }

    return table_exists;
}

// initialize the database EventLogs.db for each day 
void event_logs_db_init(const char *dated_video_path)
{
    printf("event_logs_db_init\n");
    pthread_mutex_lock(&db_init_mutex);
    char db_path[64];
    snprintf(db_path, sizeof(db_path), "%sEventLogs.db", dated_video_path);
    if (db_init(&event_logs_db, db_path) == DB_SUCCESS)
    {
        // check and create VideoSegments table
        if (!check_table_exists(&event_logs_db, "VideoSegments"))
        {
            const char *create_videosegments_sql = "CREATE TABLE VideoSegments ("
                                                   "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                                   "folder TEXT NOT NULL,"
                                                   "start_time TEXT NOT NULL,"
                                                   "length INTEGER NOT NULL);";
            create_table(&event_logs_db, create_videosegments_sql);
        }

        // check and create EventDetails table
        if (!check_table_exists(&event_logs_db, "EventDetails"))
        {
            const char *create_eventdetails_sql = "CREATE TABLE EventDetails ("
                                                  "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                                  "video_id INTEGER NOT NULL,"
                                                  "motion_time INTEGER NOT NULL,"
                                                  "FOREIGN KEY(video_id) REFERENCES VideoSegments(id));";
            create_table(&event_logs_db, create_eventdetails_sql);
        }
    }
    pthread_mutex_unlock(&db_init_mutex);
}

// initialize the database VideoMetadata.db
void video_metadata_db_init()
{
    pthread_mutex_lock(&db_init_mutex);
    if (db_init(&video_metadata_db, "/mnt/sdcard/DCIM/VideoMetadata.db") == DB_SUCCESS)
    {
        // check and create VideoMetadata table
        if (!check_table_exists(&video_metadata_db, "VideoMetadata"))
        {
            const char *create_videometadata_sql = "CREATE TABLE VideoMetadata ("
                                                   "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                                   "date TEXT NOT NULL,"
                                                   "motion_count INTEGER DEFAULT -1 NOT NULL);";
            create_table(&video_metadata_db, create_videometadata_sql);
        }
    }
    pthread_mutex_unlock(&db_init_mutex);
}

// deinitialize the databases
void databases_deinit()
{
    db_close(&video_metadata_db);
    db_close(&event_logs_db);
    printf("Databases deinitialized.\n");
}

// deinitialize the database EventLogs.db
void event_logs_db_deinit()
{
    db_close(&event_logs_db);
}

// deinitialize the database VideoMetadata.db
void video_metadata_db_deinit()
{
    db_close(&video_metadata_db);
}

// curd operations
db_result_t addVideoMetadata(const char *date, int motion_count)
{
    DB_CHECK_NULL(date);
    DB_CHECK_INIT(&video_metadata_db);
    
    printf("addVideoMetadata\n");
    printf("date: %s\n", date);
    printf("motion_count: %d\n", motion_count);
    
    pthread_mutex_lock(&video_metadata_db.mutex);
    
    const char *sql = "INSERT INTO VideoMetadata (date, motion_count) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    db_result_t result = DB_SUCCESS;
    
    if (sqlite3_prepare_v2(video_metadata_db.db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_bind_text(stmt, 1, date, -1, SQLITE_STATIC) != SQLITE_OK) {
            fprintf(stderr, "addVideoMetadata: Failed to bind date: %s\n", 
                    sqlite3_errmsg(video_metadata_db.db));
            result = DB_ERROR_SQL_BIND;
        } else if (sqlite3_bind_int(stmt, 2, motion_count) != SQLITE_OK) {
            fprintf(stderr, "addVideoMetadata: Failed to bind motion_count: %s\n", 
                    sqlite3_errmsg(video_metadata_db.db));
            result = DB_ERROR_SQL_BIND;
        } else if (sqlite3_step(stmt) != SQLITE_DONE) {
            fprintf(stderr, "addVideoMetadata: Failed to insert data: %s\n", 
                    sqlite3_errmsg(video_metadata_db.db));
            result = DB_ERROR_SQL_STEP;
        }
        
        sqlite3_finalize(stmt);
    } else {
        fprintf(stderr, "addVideoMetadata: Failed to prepare statement: %s\n", 
                sqlite3_errmsg(video_metadata_db.db));
        result = DB_ERROR_SQL_PREPARE;
    }
    
    pthread_mutex_unlock(&video_metadata_db.mutex);
    return result;
}

// VideoMetadata motion
db_result_t update_motion_time_db(const char *date, int new_motion_count)
{
    DB_CHECK_NULL(date);
    DB_CHECK_INIT(&video_metadata_db);
    
    pthread_mutex_lock(&video_metadata_db.mutex);
    
    const char *sql_check = "SELECT COUNT(*) FROM VideoMetadata WHERE date = ?;";
    sqlite3_stmt *stmt_check;
    db_result_t result = DB_SUCCESS;

    if (sqlite3_prepare_v2(video_metadata_db.db, sql_check, -1, &stmt_check, NULL) == SQLITE_OK) {
        if (sqlite3_bind_text(stmt_check, 1, date, -1, SQLITE_STATIC) != SQLITE_OK) {
            fprintf(stderr, "update_motion_time_db: Failed to bind date: %s\n", 
                    sqlite3_errmsg(video_metadata_db.db));
            result = DB_ERROR_SQL_BIND;
        } else if (sqlite3_step(stmt_check) == SQLITE_ROW && sqlite3_column_int(stmt_check, 0) == 0) {
            // Record doesn't exist, insert new one
            const char *sql_insert = "INSERT INTO VideoMetadata (date, motion_count) VALUES (?, ?);";
            sqlite3_stmt *stmt_insert;

            if (sqlite3_prepare_v2(video_metadata_db.db, sql_insert, -1, &stmt_insert, NULL) == SQLITE_OK) {
                if (sqlite3_bind_text(stmt_insert, 1, date, -1, SQLITE_STATIC) != SQLITE_OK) {
                    fprintf(stderr, "update_motion_time_db: Failed to bind date for insert: %s\n", 
                            sqlite3_errmsg(video_metadata_db.db));
                    result = DB_ERROR_SQL_BIND;
                } else if (sqlite3_bind_int(stmt_insert, 2, new_motion_count) != SQLITE_OK) {
                    fprintf(stderr, "update_motion_time_db: Failed to bind motion_count for insert: %s\n", 
                            sqlite3_errmsg(video_metadata_db.db));
                    result = DB_ERROR_SQL_BIND;
                } else if (sqlite3_step(stmt_insert) != SQLITE_DONE) {
                    fprintf(stderr, "update_motion_time_db: Failed to insert: %s\n", 
                            sqlite3_errmsg(video_metadata_db.db));
                    result = DB_ERROR_SQL_STEP;
                }
                
                sqlite3_finalize(stmt_insert);
            } else {
                fprintf(stderr, "update_motion_time_db: Failed to prepare insert: %s\n", 
                        sqlite3_errmsg(video_metadata_db.db));
                result = DB_ERROR_SQL_PREPARE;
            }
        } else {
            // Record exists, update it
            const char *sql_update = "UPDATE VideoMetadata SET motion_count = ? WHERE date = ?;";
            sqlite3_stmt *stmt_update;

            if (sqlite3_prepare_v2(video_metadata_db.db, sql_update, -1, &stmt_update, NULL) == SQLITE_OK) {
                if (sqlite3_bind_int(stmt_update, 1, new_motion_count) != SQLITE_OK) {
                    fprintf(stderr, "update_motion_time_db: Failed to bind motion_count for update: %s\n", 
                            sqlite3_errmsg(video_metadata_db.db));
                    result = DB_ERROR_SQL_BIND;
                } else if (sqlite3_bind_text(stmt_update, 2, date, -1, SQLITE_STATIC) != SQLITE_OK) {
                    fprintf(stderr, "update_motion_time_db: Failed to bind date for update: %s\n", 
                            sqlite3_errmsg(video_metadata_db.db));
                    result = DB_ERROR_SQL_BIND;
                } else if (sqlite3_step(stmt_update) != SQLITE_DONE) {
                    fprintf(stderr, "update_motion_time_db: Failed to update: %s\n", 
                            sqlite3_errmsg(video_metadata_db.db));
                    result = DB_ERROR_SQL_STEP;
                }
                
                sqlite3_finalize(stmt_update);
            } else {
                fprintf(stderr, "update_motion_time_db: Failed to prepare update: %s\n", 
                        sqlite3_errmsg(video_metadata_db.db));
                result = DB_ERROR_SQL_PREPARE;
            }
        }

        sqlite3_finalize(stmt_check);
    } else {
        fprintf(stderr, "update_motion_time_db: Failed to prepare check: %s\n", 
                sqlite3_errmsg(video_metadata_db.db));
        result = DB_ERROR_SQL_PREPARE;
    }

    pthread_mutex_unlock(&video_metadata_db.mutex);
    return result;
}

// add power outage event
int addEventLog(int folder, const char *start_time, int length)
{
    int id = -1; // Initialize the ID to an invalid value
    pthread_mutex_lock(&event_logs_db.mutex);
    char folder_s[7];
    snprintf(folder_s, sizeof(folder_s), "%05d", folder);
    const char *sql = "INSERT INTO VideoSegments (folder, start_time, length) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(event_logs_db.db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, folder_s, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, start_time, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, length);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            id = (int)sqlite3_last_insert_rowid(event_logs_db.db); // Get the ID of the inserted row
        }
        sqlite3_finalize(stmt);
    }
    pthread_mutex_unlock(&event_logs_db.mutex);
    return id; // Return the ID
}

// update video length in frames arg:(interupt times, new frame number)
int update_video_Length_db(int id, int new_length)
{
    int result = -1; // Initialize result to an invalid value
    pthread_mutex_lock(&event_logs_db.mutex);
    // printf("update_video_Length_db\n");
    const char *sql = "UPDATE VideoSegments SET length = ? WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(event_logs_db.db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, new_length);
        sqlite3_bind_int(stmt, 2, id);
        if (sqlite3_step(stmt) == SQLITE_DONE)
        {
            result = 0; // Update succeeded
        }
        sqlite3_finalize(stmt);
    }
    pthread_mutex_unlock(&event_logs_db.mutex);
    return result; // Return result
}

// add motion time to EventDetails
db_result_t addEventDetail(int video_id, int motion_time)
{
    if (video_id < 0) {
        fprintf(stderr, "addEventDetail: Invalid video_id: %d\n", video_id);
        return DB_ERROR_INVALID_PARAM;
    }
    
    DB_CHECK_INIT(&event_logs_db);
    
    pthread_mutex_lock(&event_logs_db.mutex);
    
    const char *sql = "INSERT INTO EventDetails (video_id, motion_time) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    db_result_t result = DB_SUCCESS;
    
    if (sqlite3_prepare_v2(event_logs_db.db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_bind_int(stmt, 1, video_id) != SQLITE_OK) {
            fprintf(stderr, "addEventDetail: Failed to bind video_id: %s\n", 
                    sqlite3_errmsg(event_logs_db.db));
            result = DB_ERROR_SQL_BIND;
        } else if (sqlite3_bind_int(stmt, 2, motion_time) != SQLITE_OK) {
            fprintf(stderr, "addEventDetail: Failed to bind motion_time: %s\n", 
                    sqlite3_errmsg(event_logs_db.db));
            result = DB_ERROR_SQL_BIND;
        } else if (sqlite3_step(stmt) != SQLITE_DONE) {
            fprintf(stderr, "addEventDetail: Failed to insert data: %s\n", 
                    sqlite3_errmsg(event_logs_db.db));
            result = DB_ERROR_SQL_STEP;
        }
        
        sqlite3_finalize(stmt);
    } else {
        fprintf(stderr, "addEventDetail: Failed to prepare statement: %s\n", 
                sqlite3_errmsg(event_logs_db.db));
        result = DB_ERROR_SQL_PREPARE;
    }
    
    pthread_mutex_unlock(&event_logs_db.mutex);
    return result;
}

// get a database pointer by name
Database *get_database(const char *db_name)
{
    if (strcmp(db_name, "VideoMetadata") == 0)
        return &video_metadata_db;
    else if (strcmp(db_name, "EventLogs") == 0)
        return &event_logs_db;
    else
        return NULL;
}

// Create table
int create_table(Database *database, const char *create_sql)
{
    int rc;
    char *errMsg = 0;

    pthread_mutex_lock(&database->mutex);
    rc = sqlite3_exec(database->db, create_sql, 0, 0, &errMsg);
    pthread_mutex_unlock(&database->mutex);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return 1;
    }

    return 0;
}

// Get the motion count by date from EventLogs.db.db ---> EventDetails
int getEventDetailsCount()
{
    int count = 0;
    pthread_mutex_lock(&event_logs_db.mutex);
    const char *sql = "SELECT COUNT(*) FROM EventDetails;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(event_logs_db.db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        fprintf(stderr, "Failed to fetch count: %s\n", sqlite3_errmsg(event_logs_db.db));
    }
    pthread_mutex_unlock(&event_logs_db.mutex);
    return count;
}

// Get the motion count by date from VideoMetadata.db ---> VideoMetadata
int db_get_earliest_date(char *earliest_date)
{
    int result = RK_SUCCESS;
    pthread_mutex_lock(&video_metadata_db.mutex);
    const char *sql = "SELECT MIN(date) FROM VideoMetadata;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(video_metadata_db.db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const unsigned char *date = sqlite3_column_text(stmt, 0);
            if (date)
            {
                strncpy(earliest_date, (const char *)date, sizeof("1970-01-01"));
                earliest_date[sizeof("1970-01-01") - 1] = '\0';
            }
            else
            {
                strncpy(earliest_date, "NULL", sizeof("1970-01-01"));
                earliest_date[sizeof("NULL") - 1] = '\0';
                result = RK_FAILURE;
            }
        }
        else
        {
            strncpy(earliest_date, "NULL", sizeof("1970-01-01"));
            earliest_date[sizeof("NULL") - 1] = '\0';
            result = RK_FAILURE;
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(video_metadata_db.db));
        strncpy(earliest_date, "NULL", sizeof("1970-01-01"));
        earliest_date[sizeof("NULL") - 1] = '\0';
        result = RK_FAILURE;
    }
    pthread_mutex_unlock(&video_metadata_db.mutex);
    return result;
}

// Delete record by date from VideoMetadata.db ---> VideoMetadata
int db_delete_record_date(const char *date)
{
    int result = RK_SUCCESS;
    pthread_mutex_lock(&video_metadata_db.mutex);
    const char *sql = "DELETE FROM VideoMetadata WHERE date = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(video_metadata_db.db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, date, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            fprintf(stderr, "Failed to delete record: %s\n", sqlite3_errmsg(video_metadata_db.db));
            result = RK_FAILURE;
        }
        else
        {
            printf("Record with date %s deleted successfully.\n", date);
        }
        sqlite3_finalize(stmt);
    }
    else
    {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(video_metadata_db.db));
        result = RK_FAILURE;
    }
    pthread_mutex_unlock(&video_metadata_db.mutex);
    return result;
}

// Get all motion counts in JSON format
char *get_all_motion_counts_json()
{
    const char *sql = "SELECT json_group_array(json_object('date', date, 'motion_count', motion_count)) FROM VideoMetadata;";
    char *json_result = NULL;
    sqlite3_stmt *stmt;

    pthread_mutex_lock(&video_metadata_db.mutex);
    int rc = sqlite3_prepare_v2(video_metadata_db.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(video_metadata_db.db));
        pthread_mutex_unlock(&video_metadata_db.mutex);
        return NULL;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *json_text = sqlite3_column_text(stmt, 0);
        if (json_text)
        {
            json_result = strdup((const char *)json_text);
        }
        else
        {
            json_result = strdup("[]"); // Return empty array if no data
        }
    }
    else
    {
        fprintf(stderr, "Failed to retrieve data: %s\n", sqlite3_errmsg(video_metadata_db.db));
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&video_metadata_db.mutex);

    return json_result;
}

// Get all video segments in JSON format by date
char *get_all_video_segments_json(char *date)
{
    char *json_result = NULL;
    // check if the date is today
    time_t now;
    struct tm *timeinfo;
    char date_string[DATE_STRING_LENGTH];
    time(&now);
    timeinfo = localtime(&now);
    strftime(date_string, DATE_STRING_LENGTH, "%Y-%m-%d", timeinfo);
    if (strcmp(date, date_string) != 0)
    {
        // create new database connection
        char previous_db_path[64] = {0}; // Example: "/mnt/sdcard/DCIM/2021-07-01/
        snprintf(previous_db_path, sizeof(previous_db_path), "/mnt/sdcard/DCIM/%s/EventLogs.db", date);
        Database previous_event_logs_db;
        db_init(&previous_event_logs_db, previous_db_path);
        json_result = get_all_video_segments_json_(previous_event_logs_db);
        db_close(&previous_event_logs_db);
    }
    else
    {
        // just use the existing database connection
        json_result = get_all_video_segments_json_(event_logs_db);
    }

    return json_result;
}

// Get all video segments in JSON format by database pointer
char *get_all_video_segments_json_(Database pEventLogs)
{
    const char *sql = "SELECT json_group_array(json_object('folder', folder, 'start_time', start_time, 'length', length)) FROM VideoSegments;";
    char *json_result = NULL;
    sqlite3_stmt *stmt;
    pthread_mutex_lock(&pEventLogs.mutex);
    int rc = sqlite3_prepare_v2(pEventLogs.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(pEventLogs.db));
        pthread_mutex_unlock(&pEventLogs.mutex);
        return NULL;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char *json_text = sqlite3_column_text(stmt, 0);
        if (json_text)
        {
            json_result = strdup((const char *)json_text);
        }
        else
        {
            json_result = strdup("[]"); // Return empty array if no data
        }
    }
    else
    {
        fprintf(stderr, "Failed to retrieve data: %s\n", sqlite3_errmsg(pEventLogs.db));
    }
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&pEventLogs.mutex);
    return json_result;
}