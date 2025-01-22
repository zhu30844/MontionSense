#include "sqlite_comm.h"

static sqlite3 *db_video_folders = NULL;
sqlite3 *db_motion_records = NULL;

int testSQLite()
{
    sqlite3 *db;
    char *errMsg = 0;
    int rc;
    const char *sql;
    // Open database connection
    rc = sqlite3_open(":memory:", &db);
    if (rc)
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    // Create SQL table
    sql = "CREATE TABLE TEST (ID INT PRIMARY KEY NOT NULL, NAME TEXT NOT NULL);";
    rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return 0;
    }
    // Insert data into table
    sql = "INSERT INTO TEST (ID, NAME) VALUES (1, 'Alice');";
    rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return 0;
    }
    // Read data from table
    sql = "SELECT ID, NAME FROM TEST;";
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *name = sqlite3_column_text(stmt, 1);
        printf("ID: %d, Name: %s\n", id, name);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 1;
}

int file_management_create_table(sqlite3 *db)
{
    char *err_msg = 0;
    const char *sql = "CREATE TABLE IF NOT EXISTS video_folders ("
                      "date TEXT NOT NULL, "
                      "file_status INTEGER NOT NULL, "
                      "recording_status INTEGER NOT NULL, "
                      "motion_count INTEGER DEFAULT -1);";
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL failed: %s\n", err_msg);
        sqlite3_free(err_msg);
        return rc;
    }
    return SQLITE_OK;
}

int video_folders_db_init()
{
    
    int rc = sqlite3_open("/mnt/sdcard/video_folders.db", &db_video_folders);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Connecting video_files.db failed: %s\n", sqlite3_errmsg(db_video_folders));
        sqlite3_close(db_video_folders);
        return 1;
    }
}