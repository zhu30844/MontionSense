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

    typedef struct
    {
        sqlite3 *db;
        pthread_rwlock_t rwlock;
    } Database;

    int db_init(Database *database, const char *db_path);
    void db_close(Database *database);
    int check_table_exists(Database *database, const char *table_name);
    void event_logs_db_init(const char *dated_video_path);
    void video_metadata_db_init();
    void databases_deinit();
    void event_logs_db_deinit();
    void video_metadata_db_deinit();
    void addVideoMetadata(const char *date, int motion_count);
    int addEventLog(int folder, const char *start_time, int length);
    void addEventDetail(int video_id, int motion_time);
    Database *get_database(const char *db_name);
    int create_table(Database *database, const char *create_sql);
    int update_video_Length_db(int id, int new_length);
    void update_motion_time_db(const char *date, int new_motion_count);
    int getEventDetailsCount();
    int db_delete_record_date(const char *date);
    int db_get_earliest_date(char *earliest_date);
    char *get_all_video_segments_json(char *date);
    char *get_all_video_segments_json_(Database pEventLogs);
    char *get_all_motion_counts_json();

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif