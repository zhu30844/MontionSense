#include "storage_comm.h"

FILE *fp_ts = NULL;
const char *video_folder = "/mnt/sdcard/DCIM/";
static char *date = NULL;
static char dated_video_path[DATED_PATH_LENGTH] = {0};   // Example: "/mnt/sdcard/DCIM/2021-07-01/
static char ts_file_path[TS_FILE_PATH_LENGTH] = {0};     // Example: "/mnt/sdcard/DCIM/2021-07-01/0000/00001.ts"
static char m3u8_file_path[M3U8_FILE_PATH_LENGTH] = {0}; // Example: "/mnt/sdcard/DCIM/2021-07-01/0000/index.m3u8"
hls_m3u8_t *m3u = NULL;
hls_media_t *hls = NULL;
static char m3u8_list_buffer[2 * 1024 * 1024] = {0}; // 2MB
static pthread_t space_cleanup_tid;
static pthread_mutex_t s_mkdir_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_write_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_free_space_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_free_space_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t s_db_mutex = PTHREAD_MUTEX_INITIALIZER;

RK_BOOL SD_card_exist = RK_FALSE;
RK_BOOL need_free_disk = RK_FALSE;

static int interrupt_times = 0;
static RK_BOOL new_stream = RK_TRUE;
static int ydy = 0; // date of the year
static int g_storage_run_ = 1;

static int hls_handler(void *m3u8, const void *data, size_t bytes, int64_t pts, int64_t dts, int64_t duration)
{
    pthread_mutex_lock(&s_write_mutex);
    // ts cache done, write to file
    printf("write ts to file\n");
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
    snprintf(ts_file_path, sizeof(ts_file_path) - 1, "%s%05d/%05d.ts", dated_video_path, interrupt_times, i++);
    printf("ts file path: %s\n", ts_file_path);
    // get the m3u8 file path
    snprintf(m3u8_file_path, sizeof(m3u8_file_path) - 1, "%s%05d/index.m3u8", dated_video_path, interrupt_times);
    // add ts file name to m3u8 list
    hls_m3u8_add((hls_m3u8_t *)m3u8, ts_file_path, pts, duration, discontinue);
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
    uint32_t free_size = 0;
    get_disk_size("/mnt/sdcard", NULL, &free_size, NULL);
    if (free_size < 2048)
    {
        free_up_disk_notify();
    }
    pthread_mutex_unlock(&s_write_mutex);
    return 0;
}

int storage_init()
{
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

    uint32_t disk_size = 0;
    uint32_t free_size = 0;
    uint64_t used_size = 0;
    PRINT_LINE();
    DIR *d = opendir("/mnt/sdcard");
    if (d == NULL)
    {
        printf("No SD card found.\n");
        SD_card_exist = RK_FALSE;
        return -1;
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
        return -1;
    }
    // update date info
    date = get_date_string();
    snprintf(dated_video_path, sizeof(dated_video_path), "%s%s/", video_folder, date);
    ydy = get_days_in_year();
    if (folder_create(video_folder) == 1) // video folder DCIM exists
    {
        if (folder_create(dated_video_path) == 1 && interrupt_times >= 0)
        {
            // uppdate the interrupt times from db
            printf("Folder %s exists, I WAS INTERRUPTED BY %d TIMES! :(\n", dated_video_path, ++interrupt_times);
            // update the interrupt times to db
        }
        else
        {
            // new date
            printf("Folder %s created.\n", dated_video_path);
        }
    }
    else
    {
        // first time to run
        printf("Folder %s created. Nice to meet you~ \n", video_folder);
        folder_create(dated_video_path);
    }
    snprintf(ts_file_path, sizeof(ts_file_path) - 1, "%s%05d/", dated_video_path, interrupt_times);
    folder_create(ts_file_path);
    PRINT_LINE();
    m3u = hls_m3u8_create(0, 3);
    hls = hls_media_create(HLS_DURATION * 1000, hls_handler, m3u);
    if (hls == NULL || m3u == NULL)
    {
        printf("storage init failed!\n");
        return -1;
    }
    printf("storage init success!\n");
    PRINT_LINE();
    return 0;
}

int storage_deinit()
{
    printf("storage deinit...\n");
    g_storage_run_ = 0;
    if (hls)
        hls_media_destroy(hls);
    if (m3u)
        hls_m3u8_destroy(m3u);
    pthread_mutex_lock(&s_free_space_mutex);
    pthread_cond_signal(&s_free_space_cond);
    pthread_mutex_unlock(&s_free_space_mutex);

    printf("join the space_cleanup_tid thread..\n");
    pthread_join(space_cleanup_tid, NULL);

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
    return 0;
}

int write_frame_2_SD(RK_U8 *data, RK_U32 len, RK_BOOL is_key_frame)
{
    if (SD_card_exist == RK_FALSE)
        return -1; // no SD card, do nothing
    if (ydy != get_days_in_year())
    {
        hls_media_input(hls, PSI_STREAM_H264, NULL, 0, 0, 0, 0);
        // new day
        printf("New day, forget the interrupt times.\n");
        ydy = get_days_in_year();
        date = get_date_string();
        snprintf(dated_video_path, sizeof(dated_video_path), "%s%s/", video_folder, date);
        folder_create(dated_video_path);
        // new day, forget the interrupt times
        interrupt_times = 0;
        snprintf(ts_file_path, sizeof(ts_file_path) - 1, "%s%05d/", dated_video_path, interrupt_times);
        folder_create(ts_file_path);
        // update the interrupt times to db
        new_stream = RK_TRUE;
        // destroy the old hls and m3u8
        hls_media_destroy(hls);
        hls_m3u8_destroy(m3u);
        printf("destroy the old hls and m3u8 done\n");
        // create a new hls and m3u8
        if (config_hls() == -1)
            return -1;
    }
    static int64_t pts = 10086;
    static int64_t dts = 0;
    pts += 33;
    dts = pts;
    hls_media_input(hls, PSI_STREAM_H264, data, len, pts, dts, (is_key_frame == RK_TRUE) ? HLS_FLAGS_KEYFRAME : 0);
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

static int get_disk_size(char *path, uint32_t *total_size, uint32_t *free_size, uint64_t *used_size)
{
    struct statfs diskInfo;
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
        return -1;
    }
    else
    {
        printf("config_hls success!\n");
        return 0;
    }
}

void free_up_disk_notify()
{
    pthread_mutex_lock(&s_free_space_mutex);
    need_free_disk = RK_TRUE;
    pthread_cond_signal(&s_free_space_cond);
    pthread_mutex_unlock(&s_free_space_mutex);
}

void *space_cleanup_thread(void *arg)
{
    while (g_storage_run_ == 1 && SD_card_exist == RK_TRUE) 
    {
        pthread_mutex_lock(&s_free_space_mutex);
        while (need_free_disk == RK_FALSE && g_storage_run_ == 1)
        {
            pthread_cond_wait(&s_free_space_cond, &s_free_space_mutex);
        }
        pthread_mutex_unlock(&s_free_space_mutex);
        if (g_storage_run_ != 1)
            break;    
        //pthread_mutex_unlock(&s_free_space_mutex);
        printf("Free disk space...\n");
        printf("inform db");

        printf("update flag");
        pthread_mutex_lock(&s_free_space_mutex);
        need_free_disk = RK_FALSE;
        pthread_mutex_unlock(&s_free_space_mutex);
    }
    return NULL;
}
