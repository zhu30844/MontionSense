// Webserver for MontionSense
// Copyright (C) 2025 ZIXUAN ZHU
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// For the terms of this license, see <http://www.gnu.org/licenses/>.

#include <web_server.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "web_server.c"

static struct mg_mgr mgr;
static int web_server_run_ = 1;
static const char *w_server_root = "/mnt/sdcard/MotionSense/www/html";          // web server root
static const char *w_DCIM_entrance = "/mnt/sdcard/MotionSense/www/html/hls";    // DCIM entrance
static const char *w_general_api = "/api/*";                                    // general api, apientry point
static const char *w_streamer_api = "/api/stream";                              // streamer api, jpg stream
static const char *w_get_motion_count_api = "/api/motion_counts";               // get motion count api, to draw heatmap
static const char *w_get_video_segments_api = "/api/video_segments*";           // get video segments api, get list for one day's video segments
static const char *w_get_motion_points_api = "/api/motion_points";              // get motion points api, get list of motion points
static const char *w_db_forbidden = "/hls/*.db*";                               // db forbidden api
static const char *DCIM_dir = "/mnt/sdcard/DCIM";                               // DCIM dir
static const char *w_ifname = "eth0";                                           // network interface name
static const char *w_ip_addr = "192.168.1.1";                                   // IP address
static const char *w_netmask = "255.255.255.0";                                 // netmask
pthread_t web_server_thr;
static image_addr_t *image_addr;

static int hls_checker()
{
    struct stat st;
    char link_target[256];
    ssize_t len;

    // check hls symlink
    if (lstat(w_DCIM_entrance, &st) == 0) {
        if (S_ISLNK(st.st_mode)) {
            // if it is a symlink, check if it points to the correct DCIM directory
            len = readlink(w_DCIM_entrance, link_target, sizeof(link_target) - 1);
            if (len == -1) {
                LOG_ERROR("Failed to read symlink: %s\n", strerror(errno));
                return HLS_SYMLINK_NOT_VALID;
            }
            link_target[len] = '\0';

            if (strcmp(link_target, DCIM_dir) == 0) {
                LOG_INFO("HLS symlink is valid, points to: %s\n", link_target);
                return HLS_SYMLINK_VALID;
            }

            LOG_INFO("HLS symlink is incorrect, points to: %s, expected: %s\n", link_target, DCIM_dir);
            if (unlink(w_DCIM_entrance) != 0) {
                LOG_ERROR("Failed to remove invalid symlink: %s\n", strerror(errno));
                return HLS_SYMLINK_NOT_VALID;
            }
            LOG_INFO("Removed invalid symlink, creating new one...\n");
        } else {
            // if it exists but is not a symlink, remove it
            LOG_INFO("HLS path exists but is not a symlink, removing...\n");
            if (unlink(w_DCIM_entrance) != 0) {
                LOG_ERROR("Failed to remove non-symlink file: %s\n", strerror(errno));
                return HLS_SYMLINK_NOT_VALID;
            }
        }
    } else {
        // lstat failed, check if the directory exists
        LOG_INFO("HLS symlink does not exist, creating...\n");
    }

    // create a new symlink
    if (symlink(DCIM_dir, w_DCIM_entrance) == 0) {
        LOG_INFO("HLS symlink created successfully\n");
        return HLS_SYMLINK_VALID;
    } else {
        LOG_ERROR("Failed to create symlink: %s\n", strerror(errno));
        return HLS_SYMLINK_NOT_VALID;
    }
}

int set_ip_address(const char *ifname, const char *ip_addr, const char *netmask)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct ifreq ifr;
    struct sockaddr_in *addr;

    if (sockfd < 0) return -1;

    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);


    addr = (struct sockaddr_in *)&ifr.ifr_addr;
    addr->sin_family = AF_INET;
    inet_pton(AF_INET, ip_addr, &addr->sin_addr);
    if (ioctl(sockfd, SIOCSIFADDR, &ifr) < 0) return -1;


    inet_pton(AF_INET, netmask, &addr->sin_addr);
    if (ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0) return -1;

    close(sockfd);
    return 0;
}

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

// broadcast mjpeg frame
static void broadcast_mjpeg_frame(struct mg_mgr *mgr, void *data, RK_U32 len)
{
    struct mg_connection *c;
    for (c = mgr->conns; c != NULL; c = c->next)
    {
        if (c->data[0] != 'S')
            continue; // Skip non-stream connections
        mg_printf(c,
                  "--foo\r\nContent-Type: image/jpeg\r\n"
                  "Content-Length: %lu\r\n\r\n",
                  len);
        mg_send(c, data, len);
        mg_send(c, "\r\n", 2);
    }
}

// timer callback,  to broadcast mjpeg frame
static void timer_callback(void *arg)
{
    pthread_mutex_lock(&image_addr->lock);
    if(image_addr->p != NULL)
        broadcast_mjpeg_frame(&mgr, image_addr->p, image_addr->size);
    pthread_mutex_unlock(&image_addr->lock);
}

// web server
static void web_server_handler(struct mg_connection *c, int ev, void *ev_data)
{
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    if (mg_match(hm->uri, mg_str(w_db_forbidden), NULL))
    {
        mg_http_reply(c, 404, "", "Not Found");
    }
    else
    {
        struct mg_http_serve_opts opts = {.root_dir = w_server_root};
        mg_http_serve_dir(c, ev_data, &opts);
    }
}

// motion count handler, get all motion counts in JSON format
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

// get all video segments in JSON format by date
static void video_segments_handler(struct mg_connection *c, int ev, void *ev_data)
{

    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    char date[16] = {0};
    struct mg_str;
    mg_http_get_var(&hm->query, "date", date, sizeof(date));
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

// TODO get motion points in JSON format
static void motion_points_handler(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        struct mg_str query = hm->query;
        
        // Parse date parameter from query string
        char date[32] = {0};
        char date_value[32] = {0};
        
        if (query.len > 0)
        {
            if (mg_http_get_var(&query, "date", date_value, sizeof(date_value)) > 0)
            {
                strncpy(date, date_value, sizeof(date) - 1);
                date[sizeof(date) - 1] = '\0';
            }
        }
        
        // If no date provided, use today's date
        if (strlen(date) == 0)
        {
            time_t now;
            struct tm *timeinfo;
            time(&now);
            timeinfo = localtime(&now);
            strftime(date, sizeof(date), "%Y-%m-%d", timeinfo);
        }
        
        // Get motion points data
        char *json_array_str = get_all_motion_points_json(date);
        if (json_array_str == NULL)
        {
            mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                          "{\"status\":\"error\",\"message\":\"Failed to retrieve motion points data\"}");
            return;
        }
        
        // Construct response
        const char *response_format = "{ \"motion_points\": %s }";
        size_t response_size = strlen(response_format) + strlen(json_array_str) + 1;
        char *response = malloc(response_size);
        
        if (response == NULL)
        {
            mg_http_reply(c, 500, "Content-Type: application/json\r\n",
                          "{\"status\":\"error\",\"message\":\"Memory allocation failed\"}");
            free(json_array_str);
            return;
        }
        
        snprintf(response, response_size, response_format, json_array_str);
        
        mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", response);
        
        free(response);
        free(json_array_str);
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

// web page & video recourse handler
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

// web server thread
static void *web_server_run(void *arg)
{
    while (web_server_run_ == 1)
    {
        mg_mgr_poll(&mgr, 30);
    }
    LOG_INFO("web_server_run exit\n");
    mg_mgr_free(&mgr);
    return 0;
}

// web server init
int web_server_init()
{
    PRINT_LINE();
    LOG_INFO("web_server_init start\n");
    set_ip_address(w_ifname, w_ip_addr, w_netmask);
    if (hls_checker() != HLS_SYMLINK_VALID)
    {
        LOG_ERROR("HLS symlink check failed\n");
        return RK_FAILURE;
    }

    mg_mgr_init(&mgr);
    LOG_INFO("mg_mgr_init done\n");
    mg_http_listen(&mgr, "http://0.0.0.0:80", web_handler, NULL); // web_handler
    LOG_INFO("mg_http_listen done\n");
    mg_timer_add(&mgr, 50, MG_TIMER_REPEAT, timer_callback, &mgr);
    if (pthread_create(&web_server_thr, NULL, web_server_run, NULL) != 0)
    {
        LOG_ERROR("web_server_init failed\n");
        return RK_FAILURE;
    }
    LOG_INFO("web_server_init done\n");
    return RK_SUCCESS;
}

// web server deinit
void web_server_deinit()
{
    web_server_run_ = 0;
    pthread_join(web_server_thr, NULL);
    mg_mgr_free(&mgr);
}

// get image address and lock from video.c
void web_send_image_init(image_addr_t *pimage_addr)
{
    image_addr = pimage_addr;
}