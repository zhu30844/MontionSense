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

// initialize a database
int db_init(Database *database, const char *db_path)
{
    int rc;

    pthread_rwlock_init(&database->rwlock, NULL);
    pthread_rwlock_wrlock(&database->rwlock);

    rc = sqlite3_open(db_path, &database->db);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Connecting %s failed: %s\n", db_path, sqlite3_errmsg(database->db));
        sqlite3_close(database->db);
        pthread_rwlock_unlock(&database->rwlock);
        return 1;
    }

    pthread_rwlock_unlock(&database->rwlock);
    return 0;
}

// close a database
void db_close(Database *database)
{
    if (&database->rwlock == NULL)
        return;
    pthread_rwlock_wrlock(&database->rwlock);
    if (database->db != NULL)
        sqlite3_close(database->db);
    pthread_rwlock_unlock(&database->rwlock);
    pthread_rwlock_destroy(&database->rwlock);
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

    pthread_rwlock_rdlock(&database->rwlock);
    rc = sqlite3_exec(database->db, sql, table_exists_callback, &table_exists, &errMsg);
    pthread_rwlock_unlock(&database->rwlock);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return -1;
    }

    return table_exists;
}

// initialize the database EventLogs.db
void event_logs_db_init(const char *dated_video_path)
{
    printf("event_logs_db_init\n");
    pthread_mutex_lock(&db_init_mutex);
    char db_path[64];
    snprintf(db_path, sizeof(db_path), "%sEventLogs.db", dated_video_path);
    if (db_init(&event_logs_db, db_path) == 0)
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
    if (db_init(&video_metadata_db, "/mnt/sdcard/DCIM/VideoMetadata.db") == 0)
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
// 增加 VideoMetadata 数据
void addVideoMetadata(const char *date, int motion_count)
{
    printf("addVideoMetadata\n");
    printf("date: %s\n", date);
    printf("motion_count: %d\n", motion_count);
    pthread_rwlock_wrlock(&video_metadata_db.rwlock);
    const char *sql = "INSERT INTO VideoMetadata (date, motion_count) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(video_metadata_db.db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, date, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, motion_count);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    pthread_rwlock_unlock(&video_metadata_db.rwlock);
}

// VideoMetadata motion
void update_motion_time_db(const char *date, int new_motion_count)
{
    pthread_rwlock_wrlock(&video_metadata_db.rwlock);
    const char *sql_check = "SELECT COUNT(*) FROM VideoMetadata WHERE date = ?;";
    sqlite3_stmt *stmt_check;

    if (sqlite3_prepare_v2(video_metadata_db.db, sql_check, -1, &stmt_check, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt_check, 1, date, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt_check) == SQLITE_ROW && sqlite3_column_int(stmt_check, 0) == 0)
        {
            const char *sql_insert = "INSERT INTO VideoMetadata (date, motion_count) VALUES (?, ?);";
            sqlite3_stmt *stmt_insert;

            if (sqlite3_prepare_v2(video_metadata_db.db, sql_insert, -1, &stmt_insert, NULL) == SQLITE_OK)
            {
                sqlite3_bind_text(stmt_insert, 1, date, -1, SQLITE_STATIC);
                sqlite3_bind_int(stmt_insert, 2, new_motion_count);
                sqlite3_step(stmt_insert);
                sqlite3_finalize(stmt_insert);
            }
        }
        else
        {
            const char *sql_update = "UPDATE VideoMetadata SET motion_count = ? WHERE date = ?;";
            sqlite3_stmt *stmt_update;

            if (sqlite3_prepare_v2(video_metadata_db.db, sql_update, -1, &stmt_update, NULL) == SQLITE_OK)
            {
                sqlite3_bind_int(stmt_update, 1, new_motion_count);
                sqlite3_bind_text(stmt_update, 2, date, -1, SQLITE_STATIC);
                sqlite3_step(stmt_update);
                sqlite3_finalize(stmt_update);
            }
        }

        sqlite3_finalize(stmt_check);
    }

    pthread_rwlock_unlock(&video_metadata_db.rwlock);
}

// add power outage event
int addEventLog(int folder, const char *start_time, int length)
{
    int id = -1; // Initialize the ID to an invalid value
    pthread_rwlock_wrlock(&event_logs_db.rwlock);
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
    pthread_rwlock_unlock(&event_logs_db.rwlock);
    return id; // Return the ID
}

// update video length in frames arg:(interupt times, new frame number)
int update_video_Length_db(int id, int new_length)
{
    int result = -1; // Initialize result to an invalid value
    pthread_rwlock_wrlock(&event_logs_db.rwlock);
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
    pthread_rwlock_unlock(&event_logs_db.rwlock);
    return result; // Return result
}

// add motion time to EventDetails
void addEventDetail(int video_id, int motion_time)
{
    pthread_rwlock_wrlock(&event_logs_db.rwlock);
    const char *sql = "INSERT INTO EventDetails (video_id, motion_time) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(event_logs_db.db, sql, -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, video_id);
        sqlite3_bind_int(stmt, 2, motion_time);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    pthread_rwlock_unlock(&event_logs_db.rwlock);
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

    pthread_rwlock_wrlock(&database->rwlock);
    rc = sqlite3_exec(database->db, create_sql, 0, 0, &errMsg);
    pthread_rwlock_unlock(&database->rwlock);

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
    pthread_rwlock_rdlock(&event_logs_db.rwlock);
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
    pthread_rwlock_unlock(&event_logs_db.rwlock);
    return count;
}

// Get the motion count by date from VideoMetadata.db ---> VideoMetadata
int db_get_earliest_date(char *earliest_date)
{
    int result = RK_SUCCESS;
    pthread_rwlock_rdlock(&video_metadata_db.rwlock);
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
    pthread_rwlock_unlock(&video_metadata_db.rwlock);
    return result;
}

// Delete record by date from VideoMetadata.db ---> VideoMetadata
int db_delete_record_date(const char *date)
{
    int result = RK_SUCCESS;
    pthread_rwlock_wrlock(&video_metadata_db.rwlock);
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
    pthread_rwlock_unlock(&video_metadata_db.rwlock);
    return result;
}

// Get all motion counts in JSON format
char *get_all_motion_counts_json()
{
    const char *sql = "SELECT json_group_array(json_object('date', date, 'motion_count', motion_count)) FROM VideoMetadata;";
    char *json_result = NULL;
    sqlite3_stmt *stmt;

    pthread_rwlock_rdlock(&video_metadata_db.rwlock);
    int rc = sqlite3_prepare_v2(video_metadata_db.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(video_metadata_db.db));
        pthread_rwlock_unlock(&video_metadata_db.rwlock);
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
    pthread_rwlock_unlock(&video_metadata_db.rwlock);

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
    pthread_rwlock_rdlock(&pEventLogs.rwlock);
    int rc = sqlite3_prepare_v2(pEventLogs.db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(pEventLogs.db));
        pthread_rwlock_unlock(&pEventLogs.rwlock);
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
    pthread_rwlock_unlock(&pEventLogs.rwlock);
    return json_result;
}