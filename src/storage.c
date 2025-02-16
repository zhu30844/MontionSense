/*****************************************************************************
* | File        :   storage.c
* | Author      :   ZIXUAN ZHU
* | Function    :   Storage operations
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
# furished to do so, subject to the following conditions:
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

#include "storage_comm.h"

FILE *fp_ts = NULL;
const char *video_folder = "/mnt/sdcard/DCIM/";
static char *date = NULL;
static char date_video_path[DATE_PATH_LENGTH] = {0};     // Example: "/mnt/sdcard/DCIM/2021-07-01/
static char ts_file_path[TS_FILE_PATH_LENGTH] = {0};     // Example: "/mnt/sdcard/DCIM/2021-07-01/0000/00001.ts"
static char m3u8_file_path[M3U8_FILE_PATH_LENGTH] = {0}; // Example: "/mnt/sdcard/DCIM/2021-07-01/0000/index.m3u8"
hls_m3u8_t *m3u = NULL;
hls_media_t *hls = NULL;
static char m3u8_list_buffer[2 * 1024 * 1024] = {0}; // 2MB buffer for m3u8 list
static pthread_t space_cleanup_tid;
static pthread_mutex_t s_mkdir_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_write_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_folder_switch_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_free_space_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_free_space_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t s_db_mutex = PTHREAD_MUTEX_INITIALIZER;

RK_BOOL SD_card_exist = RK_FALSE;

static int interrupt_times = 0;                  // interrupt times of the day
static RK_BOOL new_stream = RK_TRUE;             // new stream flag
static int ydy = 0;                              // date of the year
static int frame_number = 0, motion_counter = 0; // frame number and motion counter
static int g_storage_run_ = 1;                   // storage thread run flag
static int ts_file_count = 0;                    // ts file count

static int hls_handler(void *m3u8, const void *data, size_t bytes, int64_t pts, int64_t dts, int64_t duration)
{
    pthread_mutex_lock(&s_write_mutex);
    // ts cache done, write to file
    char ts_file_name[10] = {0};
    static int64_t s_dts = -1;
    static int i = 0;
    if (new_stream == RK_TRUE)
    {
        i = 0;
        s_dts = -1;
        new_stream = RK_FALSE;
    }

    int discontinue = -1 != s_dts ? 0 : (dts > s_dts + HLS_DURATION / 2 ? 1 : 0);
    s_dts = dts;
    // get the ts file path
    snprintf(ts_file_path, sizeof(ts_file_path) - 1, "%s%05d/%05d.ts", date_video_path, interrupt_times, i);
    snprintf(ts_file_name, sizeof(ts_file_name) - 1, "%05d.ts", i++);
    // get the m3u8 file path
    snprintf(m3u8_file_path, sizeof(m3u8_file_path) - 1, "%s%05d/index.m3u8", date_video_path, interrupt_times);
    // add ts file name to m3u8 list
    hls_m3u8_add((hls_m3u8_t *)m3u8, ts_file_name, pts, duration, discontinue);
    // get the m3u8 list
    hls_m3u8_playlist((hls_m3u8_t *)m3u8, 1, m3u8_list_buffer, sizeof(m3u8_list_buffer));
    // write the m3u8 file
    FILE *fp_m3u8 = fopen(m3u8_file_path, "wb");
    if (fp_m3u8)
    {
        fwrite(m3u8_list_buffer, 1, strlen(m3u8_list_buffer), fp_m3u8);
        fclose(fp_m3u8);
    }
    // write the ts file
    FILE *fp_ts = fopen(ts_file_path, "wb");
    if (fp_ts)
    {
        fwrite(data, 1, bytes, fp_ts);
        fclose(fp_ts);
    }

    // disk space check trigger, every 10 ts files
    pthread_mutex_lock(&s_free_space_mutex);
    ts_file_count++;
    if (ts_file_count >= CLEANUP_INTERVAL)
    {
        pthread_cond_signal(&s_free_space_cond);
    }
    pthread_mutex_unlock(&s_free_space_mutex);

    pthread_mutex_unlock(&s_write_mutex);
    return 0;
}

static int get_disk_size(char *path, uint32_t *total_size, uint32_t *free_size, uint64_t *used_size)
{
    struct statfs diskInfo;
    // get the disk info
    if (statfs(path, &diskInfo))
    {
        printf("statfs[%s] failed", path);
        return -1;
    }
    uint64_t used_disk = (uint64_t)(diskInfo.f_blocks - diskInfo.f_bfree) * diskInfo.f_bsize;
    uint64_t total_disk = (uint64_t)diskInfo.f_blocks * diskInfo.f_bsize;
    uint64_t available_disk = (uint64_t)diskInfo.f_bavail * diskInfo.f_bsize;
    if (total_size != NULL)
        *total_size = (uint32_t)((total_disk) >> 20);
    if (free_size != NULL)
        *free_size = (uint32_t)((available_disk) >> 20);
    if (used_size != NULL)
        *used_size = ((used_disk) >> 20);
    return 0;
}

int storage_init()
{
    // init mutex and cond
    pthread_mutex_t mutexes[] = {s_free_space_mutex, s_db_mutex, s_mkdir_mutex, s_write_mutex};
    pthread_cond_t conds[] = {s_free_space_cond};
    for (int i = 0; i < sizeof(mutexes) / sizeof(mutexes[0]); ++i)
    {
        if (pthread_mutex_init(&mutexes[i], NULL) != 0)
        {
            perror("pthread_mutex_init");
            return -1;
        }
    }
    for (int i = 0; i < sizeof(conds) / sizeof(conds[0]); ++i)
    {
        if (pthread_cond_init(&conds[i], NULL) != 0)
        {
            perror("pthread_cond_init");
            return -1;
        }
    }

    // check SD card
    uint32_t disk_size = 0;
    uint32_t free_size = 0;
    uint64_t used_size = 0;
    PRINT_LINE();
    DIR *d = opendir("/mnt/sdcard");
    if (d == NULL)
    {
        printf("No SD card found.\n");
        SD_card_exist = RK_FALSE;
        return RK_FAILURE;
    }
    else
    {
        printf("SD card loaded.\n");
        SD_card_exist = RK_TRUE;
        closedir(d);
    }
    get_disk_size("/mnt/sdcard", &disk_size, &free_size, &used_size);
    printf("Disk size is \t%.3f GB\nFree size is \t%.3f GB\nUsed disk is \t%llu MB\n", disk_size / 1024.0, free_size / 1024.0, used_size);
    PRINT_LINE();

    printf("create disk space cleanup thread\n");
    if (pthread_create(&space_cleanup_tid, NULL, space_cleanup_thread, NULL) != 0)
    {
        perror("pthread_create");
        return RK_FAILURE;
    }

    // check the video folder
    if (folder_create(video_folder) == FOLDER_CREATE_SUCCESS) // video folder DCIM exists
    {
        printf("Folder %s created. Nice to meet you~ \n", video_folder);
        // create a soft link for web server
        system("ln -s /mnt/sdcard/DCIM /mnt/sdcard/MotionSense/www/html/hls");
    }
    switch_folder();

    PRINT_LINE();

    // event_logs_db ---> VideoSegments
    addEventLog(interrupt_times, get_time_string(), -1);

    video_metadata_db_init();

    m3u = hls_m3u8_create(0, 3);
    hls = hls_media_create(HLS_DURATION * 1000, hls_handler, m3u);
    if (hls == NULL || m3u == NULL)
    {
        printf("storage init failed!\n");
        return RK_FAILURE;
    }
    printf("storage init success!\n");
    PRINT_LINE();
    return RK_SUCCESS;
}

int storage_deinit()
{
    printf("storage deinit...\n");
    // write the last ts file
    hls_media_input(hls, PSI_STREAM_H264, NULL, 0, 0, 0, 0);
    g_storage_run_ = 0;
    // destroy the hls and m3u8
    if (hls)
        hls_media_destroy(hls);
    if (m3u)
        hls_m3u8_destroy(m3u);

    // terminate the space cleanup thread
    pthread_mutex_lock(&s_free_space_mutex);
    ts_file_count = CLEANUP_INTERVAL + 2;
    pthread_cond_signal(&s_free_space_cond);
    pthread_mutex_unlock(&s_free_space_mutex);
    pthread_join(space_cleanup_tid, NULL);

    // deinit mutex and cond
    pthread_mutex_t mutexes[] = {s_free_space_mutex, s_db_mutex, s_mkdir_mutex, s_write_mutex};
    pthread_cond_t conds[] = {s_free_space_cond};
    for (int i = 0; i < sizeof(mutexes) / sizeof(mutexes[0]); ++i)
    {
        pthread_mutex_destroy(&mutexes[i]);
    }
    for (int i = 0; i < sizeof(conds) / sizeof(conds[0]); ++i)
    {
        pthread_cond_destroy(&conds[i]);
    }

    printf("storage deinit success.\n");
    return RK_SUCCESS;
}

int write_frame_2_SD(RK_U8 *data, RK_U32 len, RK_BOOL is_key_frame, int frame_cycle_time_ms)
{
    if (SD_card_exist == RK_FALSE)
        return -1; // no SD card, do nothing
    // default frame time stamp
    static int frame_cycle_time = 0;
    static int64_t pts = 10086;
    static int64_t dts = 0;
    // check the date
    if (ydy != get_days_in_year())
        new_day(&pts, &dts);
    // write the frame to hls
    pts += 33;
    dts = pts;
    frame_number++;
    hls_media_input(hls, PSI_STREAM_H264, data, len, pts, dts, (is_key_frame == RK_TRUE) ? HLS_FLAGS_KEYFRAME : 0);

    // frame rate changed, record event
    if (frame_cycle_time != frame_cycle_time_ms)
    {
        // printf("frame rate changed\n");
        motion_counter++;
        // inform db event_logs_db ---> EventDetails
        addEventDetail(interrupt_times, frame_number);
        // video_metadata_db ---> VideoMetadata
        int event_count = getEventDetailsCount();
        update_motion_time_db(date, event_count);
        frame_cycle_time = frame_cycle_time_ms;
    }

    // update the total frame number to db every 10 frames
    if (frame_number % 10 == 0)
    {
        // event_logs_db ---> VideoSegments
        update_video_Length_db(interrupt_times + 1, frame_number);
    }
    return 0;
}

int folder_create(const char *folder)
{
    pthread_mutex_lock(&s_mkdir_mutex);
    int ret = 0;
    DIR *d = opendir(folder);
    if (d == NULL)
    {
        if (mkdir(folder, 0777) == -1)
        {
            printf("Create %s fail...\n", folder);
            ret = -1;
        }
        else
        {
            printf("Create %s success!\n", folder);
            ret = 0;
        }
    }
    else
    {
        printf("Folder %s exists.\n", folder);
        closedir(d);
        ret = 1;
    }
    pthread_mutex_unlock(&s_mkdir_mutex);
    return ret;
}

int config_hls()
{
    new_stream = RK_TRUE;
    m3u = hls_m3u8_create(0, 3);
    printf("create m3u8 done\n");
    hls = hls_media_create(HLS_DURATION * 1000, hls_handler, m3u);
    printf("create hls done\n");
    if (hls == NULL || m3u == NULL)
    {
        printf("config_hls failed!\n");
        return RK_FAILURE;
    }
    else
    {
        printf("config_hls success!\n");
        return RK_SUCCESS;
    }
}

void *space_cleanup_thread(void *arg)
{
    char target_date[sizeof("1970-01-01")] = {0};
    char delete_cmd[64] = {0};
    int ret_flag = 0;
    sigset_t set, oldset;
    // block the signals
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);

    while (g_storage_run_ == 1 && SD_card_exist == RK_TRUE)
    {
        // printf("space cleanup thread running...\n");
        // wait for the signal
        pthread_mutex_lock(&s_free_space_mutex);
        while (ts_file_count < CLEANUP_INTERVAL)
        {
            pthread_cond_wait(&s_free_space_cond, &s_free_space_mutex);
        }
        ts_file_count = 0;
        pthread_mutex_unlock(&s_free_space_mutex);
        // check the free disk space
        uint32_t free_size = 0;
        get_disk_size("/mnt/sdcard", NULL, &free_size, NULL);
        if (free_size > DISK_SPACE_THRESHOLD)
        {
            //printf("Free disk space is enough, no need to clean up.\n");
            continue;
        }
        // start to free disk space
        printf("Free disk space...\n");
        // get the earliest date
        ret_flag = db_get_earliest_date(target_date);
        if (ret_flag == RK_FAILURE)
        {
            printf("Failed to fetch the earliest date.\n");
            continue;
        }
        // do nothing if only one day in record
        if (strcmp(date, target_date) == 0)
        {
            printf("Only one day, do nothing.\n");
            continue;
        }

        // delete the earliest date folder
        pthread_sigmask(SIG_BLOCK, &set, &oldset);
        snprintf(delete_cmd, sizeof(delete_cmd), "rm -rf %s%s", video_folder, target_date);
        for (int i = 0; i < 3; i++)
        {
            ret_flag = system(delete_cmd);
            if (ret_flag == -1)
            {
                perror("system call failed");
            }
            else if (WIFEXITED(ret_flag) && WEXITSTATUS(ret_flag) == 0)
            {
                printf("%s deleted! \n", target_date);
                break;
            }
            else
            {
                printf("fatal: %d\n", WEXITSTATUS(ret_flag));
            }
        }
        pthread_sigmask(SIG_SETMASK, &oldset, NULL);
        db_delete_record_date(target_date);
    }
    return 0;
}

int switch_folder()
{
    pthread_mutex_lock(&s_folder_switch_mutex);
    // update the date
    ydy = get_days_in_year();
    date = get_date_string();
    // create the date video path and update the interrupt times
    snprintf(date_video_path, sizeof(date_video_path), "%s%s/", video_folder, date);
    if (folder_create(date_video_path) == FOLDER_CREATE_EXIST) // update the interrupt times
    {
        interrupt_times = count_subdirectories(date_video_path);
    }
    else
    {

        interrupt_times = 0;
    }
    // close the old db and create a new one
    event_logs_db_deinit();
    event_logs_db_init(date_video_path);
    // create the ts folder
    char ts_folder[TS_FILE_PATH_LENGTH] = {0};
    snprintf(ts_folder, sizeof(ts_folder) - 1, "%s%05d/", date_video_path, interrupt_times);
    folder_create(ts_folder);
    pthread_mutex_unlock(&s_folder_switch_mutex);
    return 0;
}

int count_subdirectories(const char *dir_path)
{
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    if ((dir = opendir(dir_path)) == NULL)
    {
        perror("cannot open directory");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            if (strcmp(entry->d_name, ".") != 0 &&
                strcmp(entry->d_name, "..") != 0)
                count++;
        }
    }
    closedir(dir);
    return count;
}

int new_day(int64_t *p_pts, int64_t *p_dts)
{
    hls_media_input(hls, PSI_STREAM_H264, NULL, 0, 0, 0, 0);
    // write today's video metadata to db
    switch_folder();

    addEventLog(interrupt_times, get_time_string(), -1);

    // reset total frame number
    frame_number = 0;
    motion_counter = 0;

    // reset time stamp
    *p_pts = (int64_t)10086;
    *p_dts = (int64_t)0;

    // update the interrupt times to db
    new_stream = RK_TRUE;

    // destroy the old hls and m3u8
    hls_media_destroy(hls);
    hls_m3u8_destroy(m3u);
    printf("Destroy the old hls and m3u8 done\n");
    // create a new hls and m3u8
    if (config_hls() == RK_FAILURE)
        return RK_FAILURE;
    else
        return RK_SUCCESS;
}
