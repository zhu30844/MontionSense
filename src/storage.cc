#include "storage_comm.h"


FILE *fp_ts = fopen("/mnt/sdcard/output.ts", "w+");
FILE *fp_raw = fopen("/mnt/sdcard/output.ts", "w+");
int64_t timestamp = get_timestamp();
int64_t timestamp_increaser = 33;


ts_param_t param =
{
		.video_stream_id = 0xe0,
		.pmt_pid = 0x1000,
		.video_pid = 0x100,
		.program_count = 1,
		.program_list =
			{
				{0x01, 0x1000},
			},
		.output = packet_output
};

static int packet_output(uint8_t *buf, uint32_t len)
{
	if (!buf)
		return -1;
	fwrite(buf, 1, len, fp_ts);
	fflush(fp_ts);
	return 0;
}

int storage_init()
{
    fp_raw = fopen("/mnt/sdcard/output.raw", "w+");
    if(fp_raw == NULL)
    {
        printf("open file output.raw failed\n");
        return -1;
    }

    ts_stream_t *ts;
	frame_info_t frame;
	frame.frame = NULL;
	frame.len = 0;
	frame.timestamp = timestamp;
	frame.frame_type = FRAME_TYPE_VIDEO;
	ts = new_ts_stream(&param);
	//ts_write_pat(ts);
	//ts_write_pmt(ts);

    // check if the storage is mounted
    // check capacity
    // check if the storage is writable
    // check free space
    // check previous records via database
    printf("storage init success\n");
    return 0;
}

int storage_deinit()
{
    // check if the storage is mounted
    // check capacity
    // check if the storage is writable
    // check free space
    // check previous records via database
    fclose(fp_raw);
    printf("storage deinit success\n");
    return 0;
}

int write_ts_2_SD(RK_U8 * data, RK_U32 len, RK_BOOL is_key_frame)
{
    if(is_key_frame == RK_TRUE)
    {
        // can split
    }
    else
    {
        // cannot split
    }
    char * date = get_date_string();

}

int write_raw(RK_U8 * data, RK_U32 len)
{
    fwrite(data, 1, len, fp_raw);
	fflush(fp_raw);
    return 0;
}
