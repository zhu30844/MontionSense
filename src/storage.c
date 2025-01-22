#include "storage_comm.h"

FILE *fp_ts = NULL;
int64_t timestamp = 0;
int64_t timestamp_increaser = 33;
const char *video_folder = "/mnt/sdcard/DCIM/";
static char *date = NULL;
static void *ts = NULL;
static struct mpeg_ts_func_t h;
static int pts = 0;
static int dts = 0;
static int stream_id = 0;
static int16_t ts_file_counter = 1;

double frame_duration = 1.0 / 30.0;
static char dated_video_path[DATED_PATH_LENGTH] = {'\0'}; // /mnt/sdcard/DCIM/2021-07-01
static char ts_file_path[TS_FILE_PATH_LENGTH] = {'\0'};
static pthread_mutex_t g_mkdir_mutex = PTHREAD_MUTEX_INITIALIZER;
RK_BOOL SD_card_exist = RK_FALSE;
static uint32_t disk_size = 0;
static uint32_t free_size = 0;
static uint64_t used_size = 0;
static int ydy = 0;

// assisant function for ts muxer alloc
static void *ts_alloc(void *param, size_t bytes)
{
    static char s_buffer[188];
    assert(bytes <= sizeof(s_buffer));
    return s_buffer;
}

// assisant function for ts muxer free
static void ts_free(void *param, void *packet)
{
    if (fp_ts != NULL)
    {
        fflush(fp_ts);
    }
    return;
}
// assisant function for ts muxer write
static int ts_write(void *param, const void *packet, size_t bytes)
{
    return 1 == fwrite(packet, bytes, 1, (FILE *)param) ? 0 : ferror((FILE *)param);
}

// get the stream id
static int ts_stream(void *ts, int codecid)
{
    return mpeg_ts_add_stream(ts, codecid, NULL, 0);
}

int storage_init()
{
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

    ydy = get_days_in_year();
    if (folder_create(video_folder) == 1)
    {
        snprintf(dated_video_path, sizeof(dated_video_path), "%s%s", video_folder, get_date_string());
        if (folder_create(dated_video_path) == 1)
        {
            // recover the recording
            printf("Folder %s exists, try recovering recording...\n", video_folder);
            // recover tsfile counter
            // recover frame number
            // check m3u8 file and recover the last ts file
        }
    }

    // storage init done
    ts_muxer_config(); // fist time configure the ts muxer
    // check previous records via database
    printf("storage init success\n");
    PRINT_LINE();
    return 0;
}

int storage_deinit()
{
    printf("storage deinit success\n");
    return 0;
}

int write_ts_2_SD(RK_U8 *data, RK_U32 len, RK_BOOL is_key_frame)
{
    if (SD_card_exist == RK_FALSE)
        return -1; // no SD card, do nothing
    static int frame_number_d = 0;
    static int frame_number_ts = 0;
    if (ydy != get_days_in_year())
    {
        // new day
        // reset the frame number
        frame_number_d = 0;
        frame_number_ts = 0;
        ydy = get_days_in_year();
        date = get_date_string();
        snprintf(dated_video_path, sizeof(dated_video_path), "%s%s", video_folder, date);
        folder_create(dated_video_path);
        ts_file_counter = 1;
    }
    frame_number_d++;
    frame_number_ts++;
    printf("frame_number is %d\n", frame_number_d);
    if (frame_number_ts > 540 || (frame_number_ts >= 500 && is_key_frame == RK_TRUE))
    {
        if (is_key_frame == RK_TRUE)
            printf("Find a key frame and the duration is enough, clipt!\n");
        else
            printf("The duration is enough, foced clipt!\n");
        // write M3U8 file
        // reset the frame number of per ts package
        frame_number_ts = 0;
        mpeg_ts_destroy(ts);
        ts_file_counter++;
        ts_muxer_config();
    }
    pts = frame_number_d * frame_duration * 90000;
    dts = pts;
    uint8_t nalu_type = data[0] & 0x1F;
    mpeg_ts_write(ts, stream_id, nalu_type == 0x09 ? 0x8000 : 0x0001, pts, dts, data, len);
}

int folder_create(const char *folder)
{
    pthread_mutex_lock(&g_mkdir_mutex);
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
    pthread_mutex_unlock(&g_mkdir_mutex);
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
    *total_size = (uint32_t)((total_disk) >> 20);
    *free_size = (uint32_t)((available_disk) >> 20);
    *used_size = ((used_disk) >> 20);
    return 0;
}

// this function will configure the ts muxser USING THE TIME when IT IS CALLED
int ts_muxer_config()
{
    date = get_date_string();
    snprintf(dated_video_path, sizeof(dated_video_path), "%s%s", video_folder, date);
    snprintf(ts_file_path, sizeof(ts_file_path), "%s/%05d.ts", dated_video_path, ts_file_counter);
    fp_ts = fopen(ts_file_path, "wb");
    h.alloc = ts_alloc;
    h.write = ts_write;
    h.free = ts_free;
    ts = mpeg_ts_create(&h, fp_ts);
    stream_id = mpeg_ts_add_stream(ts, PSI_STREAM_H264, NULL, 0);
}

int record_recovery()
{
    // read file Last.index to get the file name 
    // append the file name and duration to the m3u8 file
    return 0;
}