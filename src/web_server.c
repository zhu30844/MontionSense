#include <web_server.h>

static struct mg_mgr mgr;
static struct mg_str image = {.buf = NULL, .len = 0};
static int web_server_run_ = 1;
static const char *w_server_root = "/mnt/sdcard/MyMD_demo/www/html"; // web server root
static const char *w_general_api = "/api/*";                         // general api, apientry point
static const char *w_streamer_api = "/api/stream";                   // streamer api, jpg stream
static const char *w_get_motion_count_api = "/api/motion_counts";    // get motion count api, to draw heatmap
static const char *w_get_video_segments_api = "/api/video_segments*"; // get video segments api, get list for one day's video segments
static const char *w_get_motion_points_api = "/api/motion_points";   // get motion points api, get list of motion points
static const char *DCIM_dir = "/mnt/sdcard/DCIM";                    // DCIM dir
pthread_t web_server_thr;

// streamer
static void streamer_handler(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev == MG_EV_HTTP_MSG)
    {
        c->data[0] = 'S';
        mg_printf(
            c, "%s",
            "HTTP/1.0 200 OK\r\n"
            "Cache-Control: no-cache\r\n"
            "Pragma: no-cache\r\nExpires: Thu, 01 Dec 1994 16:00:00 GMT\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=--foo\r\n\r\n");
    }
}

static void broadcast_mjpeg_frame(struct mg_mgr *mgr)
{
    struct mg_connection *c;
    for (c = mgr->conns; c != NULL; c = c->next)
    {
        if (c->data[0] != 'S')
            continue; // Skip non-stream connections
        mg_printf(c,
                  "--foo\r\nContent-Type: image/jpeg\r\n"
                  "Content-Length: %lu\r\n\r\n",
                  image.len);
        mg_send(c, image.buf, image.len);
        mg_send(c, "\r\n", 2);
    }
}

// web server
static void web_server_handler(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_serve_opts opts = {.root_dir = w_server_root};
        mg_http_serve_dir(c, ev_data, &opts);
    }
}

static void web_handler(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;

        if (mg_match(hm->uri, mg_str(w_general_api), NULL))
            api_handler(c, ev, ev_data);
        else
            web_server_handler(c, ev, ev_data);
    }
}

// api handler
static void api_handler(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        if (mg_match(hm->uri, mg_str(w_streamer_api), NULL))
        {
            streamer_handler(c, ev, ev_data);
        }
        else if (mg_match(hm->uri, mg_str(w_get_motion_count_api), NULL))
        {
            motion_count_handler(c, ev, ev_data);
        }
        else if (mg_match(hm->uri, mg_str(w_get_video_segments_api), NULL))
        {
            printf("video_segments_handler\n");
            video_segments_handler(c, ev, ev_data);
        }
        else if (mg_match(hm->uri, mg_str(w_get_motion_points_api), NULL))
        {
            motion_points_handler(c, ev, ev_data);
        }
        else
        {
            web_server_handler(c, ev, ev_data);
        }
    }
}
// db api list

static void motion_count_handler(struct mg_connection *c, int ev, void *ev_data)
{

    char *json_array_str = get_all_motion_counts_json();
    if (json_array_str == NULL)
    {
        mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                      "{\"status\":\"error\",\"message\":\"Failed to retrieve data\"}");
        return;
    }
    // Construct response
    const char *response_format = "{ \"motion_counts\": %s }";
    size_t response_size = strlen(response_format) + strlen(json_array_str) + 1;
    char *response = malloc(response_size);
    if (response == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        free(json_array_str);
        mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                      "{\"status\":\"error\",\"message\":\"Memory allocation failed\"}");
        return;
    }
    snprintf(response, response_size, response_format, json_array_str);
    mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", response);
    free(response);
    free(json_array_str);
}

static void video_segments_handler(struct mg_connection *c, int ev, void *ev_data)
{

    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    char date[16] = {0};
    struct mg_str;
    mg_http_get_var(&hm->query, "date", date, sizeof(date));
    char buf[100] = "";
    if (date[0] == '\0')
    {
        mg_http_reply(c, 400, "Content-Type: application/json", "{\"video_segments\": []}");
        return;
    }
    else
    {
        char *json_array_str = get_all_video_segments_json(date);
        if (json_array_str == NULL)
        {
            mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                          "{\"status\":\"error\",\"message\":\"Failed to retrieve data\"}");
            return;
        }
        const char *response_format = "{ \"video_segments\": %s }";
        size_t response_size = strlen(response_format) + strlen(json_array_str) + 1;
        char *response = malloc(response_size);
        if (response == NULL)
        {
            fprintf(stderr, "Memory allocation failed\n");
            free(json_array_str);
            mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                          "{\"status\":\"error\",\"message\":\"Memory allocation failed\"}");
            return;
        }
        snprintf(response, response_size, response_format, json_array_str);
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", response);
        free(response);
        free(json_array_str);
    }
}

static void motion_points_handler(struct mg_connection *c, int ev, void *ev_data)
{

    mg_http_reply(c, 200, "Content-Type: application/json", "{\"motion_points\": []}");
}

int web_server_init()
{
    printf("web_server_init\n");
    system("ifconfig eth0 192.168.1.1 netmask 255.255.255.0");
    system("route add default gw 192.168.1.1 eth0");
    mg_mgr_init(&mgr);
    printf("mg_mgr_init done\n");
    mg_http_listen(&mgr, "http://0.0.0.0:80", web_handler, NULL); // web_handler
    printf("mg_http_listen done\n");
    pthread_create(&web_server_thr, NULL, web_server_run, NULL);
    printf("web_server_init done\n");
}

void web_server_deinit()
{
    web_server_run_ = 0;
    sleep(1);
    pthread_join(web_server_thr, NULL);
    mg_mgr_free(&mgr);
}

static void *web_server_run(void *arg)
{
    while (web_server_run_ == 1)
        mg_mgr_poll(&mgr, 50);
}

void image_reciver(void *data, size_t len)
{
    if (data == NULL)
        return;
    image.buf = data;
    image.len = len;
    broadcast_mjpeg_frame(&mgr);
}
