#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "creds.h"

#define SPOTIFY_API_BASE "https://api.spotify.com/v1/me/player"

#define TOKEN_FILE "/tmp/spotify_token"
#define MAX_RESPONSE_SIZE 8192
#define MAX_TOKEN_SIZE 256
#define MAX_VALUE_SIZE 64

#define STATUS "is_playing"
#define ALBUM 2
#define ARTIST 3
#define TRACK 0

static char response_buffer[MAX_RESPONSE_SIZE];
static size_t response_size = 0;

static size_t request_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    
    if (response_size + realsize >= MAX_RESPONSE_SIZE - 1) {
        printf("Response too large\n");
        return 0;
    }
    
    memcpy(&response_buffer[response_size], contents, realsize);
    response_size += realsize;
    response_buffer[response_size] = '\0';
    
    return realsize;
}

int json_parser(const char *json, const char *key, char *output, size_t output_size) {
    char search_pattern[64];

    if(strcmp(key, "access_token") == 0 || strcmp(key, "refresh_token") == 0) {
        snprintf(search_pattern, sizeof(search_pattern), "\"%s\":\"", key);
    }
    else if(strcmp(key, "id") == 0) {
        snprintf(search_pattern, sizeof(search_pattern), "\"%s\" : \"", key);
    } else {
        snprintf(search_pattern, sizeof(search_pattern), "\"%s\" : ", key);
    }
 
    char *start = strstr(json, search_pattern);
    if (!start) return -1;
    
    start += strlen(search_pattern);
    char *end = strchr(start, '"');
    if (!end) return -1;
    
    size_t len = end - start;
    if (len >= output_size) len = output_size - 1;
    
    strncpy(output, start, len);
    output[len] = '\0';

    return 0;
}

int get_track_info(const char *json, int n, char *output, size_t output_size) {
    const char *pattern = "\"name\" : \"";
    const char *pos = json;
    int count = 0;
    int found = 0;

    while ((pos = strstr(pos, pattern)) != NULL) {
        pos += strlen(pattern);
        count++;

        char *end = strchr(pos, '"');
        if (!end)
            return -1;

        /* If asking for the nth name */
        if (n > 0 && count == n) {
            size_t len = end - pos;
            if (len >= output_size) len = output_size - 1;

            strncpy(output, pos, len);
            output[len] = '\0';
            return 0;
        }

        /* If asking for the last name (n == 0), keep overwriting */
        if (n == 0) {
            size_t len = end - pos;
            if (len >= output_size) len = output_size - 1;

            strncpy(output, pos, len);
            output[len] = '\0';
            found = 1;
        }
    }

    /* For last-name mode */
    if (n == 0 && found)
        return 0;

    return -1;
}

int read_token(const char *token_type, char *output, size_t output_size) {
    FILE *fp = fopen(TOKEN_FILE, "r");
    if (!fp) {
        printf("Error: Token file not found. Run setup first.\n");
        return -1;
    }
    
    char line[MAX_TOKEN_SIZE];
    int found = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, token_type, strlen(token_type)) == 0) {
            char *equals = strchr(line, '=');
            if (equals) {
                equals++;
                // Remove newline
                char *newline = strchr(equals, '\n');
                if (newline) *newline = '\0';
                
                strncpy(output, equals, output_size - 1);
                output[output_size - 1] = '\0';
                found = 1;
                break;
            }
        }
    }
    
    fclose(fp);
    return found ? 0 : -1;
}

int write_tokens(const char *access_token, const char *refresh_token) {
    FILE *fp = fopen(TOKEN_FILE, "w");
    if (!fp) {
        printf("Error: Cannot write to token file\n");
        return -1;
    }
    
    fprintf(fp, "access_token=%s\n", access_token);
    fprintf(fp, "refresh_token=%s\n", refresh_token);
    fclose(fp);
    
    return 0;
}

int refresh_token() {
    char refresh_tok[MAX_TOKEN_SIZE];
    
    if (read_token("refresh_token", refresh_tok, sizeof(refresh_tok)) != 0) {
        return -1;
    }
    
    CURL *curl;
    CURLcode res;
    
    response_size = 0;
    memset(response_buffer, 0, sizeof(response_buffer));
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (curl) {
        char post_data[2048];
        snprintf(post_data, sizeof(post_data),
                "grant_type=refresh_token&refresh_token=%s&client_id=%s&client_secret=%s",
                refresh_tok, CLIENT_ID, CLIENT_SECRET);
        
        curl_easy_setopt(curl, CURLOPT_URL, "https://accounts.spotify.com/api/token");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, request_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            char access_token[MAX_TOKEN_SIZE];
            if (json_parser(response_buffer, "access_token", access_token, sizeof(access_token)) == 0) {
                write_tokens(access_token, refresh_tok);
                printf("Token refreshed successfully\n");
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return 0;
            }
        }
        
        curl_easy_cleanup(curl);
    }
    
    curl_global_cleanup();
    return -1;
}

int get_active_device_id(const char *access_token, char *device_id, size_t device_id_size) {

    response_size = 0;
    memset(response_buffer, 0, sizeof(response_buffer));

    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    struct curl_slist *headers = NULL;
    char auth_header[MAX_TOKEN_SIZE + 32];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", access_token);
    headers = curl_slist_append(headers, auth_header);

    char url[256];
    snprintf(url, sizeof(url), "%s/devices", SPOTIFY_API_BASE);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, request_cb);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "Failed to fetch devices: %s\n", curl_easy_strerror(res));
        return -1;
    }

    if (json_parser(response_buffer, "id", device_id, device_id_size) != 0) {
        fprintf(stderr, "No device ID found in response\n");
        return -1;
    }

    return 0;
}

int change_track(const char *command_or_uri, const char *device_id_param) {
    char access_token[MAX_TOKEN_SIZE];

    if (read_token("access_token", access_token, sizeof(access_token)) != 0) {
        return -1;
    }

    char device_id[64];
    if (device_id_param) {
        strncpy(device_id, device_id_param, sizeof(device_id)-1);
        device_id[sizeof(device_id)-1] = '\0';
    } else {
        if (get_active_device_id(access_token, device_id, sizeof(device_id)) != 0) {
            fprintf(stderr, "Error: No active Spotify device found.\n");
            return -1;
        }
        printf("Using device ID: %s\n", device_id);
    }

    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    struct curl_slist *headers = NULL;
    char auth_header[MAX_TOKEN_SIZE + 32];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", access_token);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");

    char url[512];

    if (strcmp(command_or_uri, "next") == 0) {
        snprintf(url, sizeof(url), "%s/next?device_id=%s", SPOTIFY_API_BASE, device_id);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    } else if (strcmp(command_or_uri, "prev") == 0 || strcmp(command_or_uri, "previous") == 0) {
        snprintf(url, sizeof(url), "%s/previous?device_id=%s", SPOTIFY_API_BASE, device_id);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    } else {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, request_cb);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Spotify API request failed: %s\n", curl_easy_strerror(res));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return -1;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return 0;
}

int get_current_track() {
    char access_token[MAX_TOKEN_SIZE];
    
    if (read_token("access_token", access_token, sizeof(access_token)) != 0) {
        return -1;
    }

    response_size = 0;
    memset(response_buffer, 0, sizeof(response_buffer));
    
    CURL *curl;
    CURLcode res;
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (curl) {
        struct curl_slist *headers = NULL;
        char auth_header[MAX_TOKEN_SIZE + 64];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", access_token);
        headers = curl_slist_append(headers, auth_header);
        
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.spotify.com/v1/me/player/currently-playing");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, request_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            // Check if token expired
            if (strstr(response_buffer, "token expired")) {
                printf("Token expired, refreshing...\n");
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                refresh_token();
                return get_current_track(); // Retry
            }
            
            // Check if nothing is playing
            if (response_size == 0 || strstr(response_buffer, "\"item\":null")) {
                printf("Nothing is currently playing\n");
                curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return 0;
            }
            
            // Extract track information
            char album[MAX_VALUE_SIZE];
            char artist[MAX_VALUE_SIZE];
            char track[MAX_VALUE_SIZE];
            char is_playing_str[16];
            char progress_ms_str[32];
            char duration_ms_str[32];
            
            get_track_info(response_buffer, ALBUM, album, sizeof(album));
            get_track_info(response_buffer, ARTIST, artist, sizeof(artist));
            get_track_info(response_buffer, TRACK, track, sizeof(track));

            json_parser(response_buffer, STATUS, is_playing_str, sizeof(is_playing_str));
            int is_playing = (memcmp(is_playing_str, "true", sizeof("true") - 1) == 0);
            
            printf("Now Playing:\n");
            printf("  Track:  %s\n", track);
            printf("  Artist: %s\n", artist);
            printf("  Album:  %s\n", album);
            printf("  Status: %s\n", is_playing ? "Playing" : "Paused");
            
            if (json_parser(response_buffer, "progress_ms", progress_ms_str, sizeof(progress_ms_str)) == 0 &&
                json_parser(response_buffer, "duration_ms", duration_ms_str, sizeof(duration_ms_str)) == 0) {
                int progress_ms = atoi(progress_ms_str);
                int duration_ms = atoi(duration_ms_str);
                int progress_sec = progress_ms / 1000;
                int duration_sec = duration_ms / 1000;
                printf("  Time:   %d:%02d / %d:%02d\n",
                       progress_sec / 60, progress_sec % 60,
                       duration_sec / 60, duration_sec % 60);
            }
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    
    curl_global_cleanup();
    return 0;
}

int setup() {
    char code[256];

    printf("=== Spotify API Setup ===\n\n");
    printf("Step 1: Open this URL in your browser:\n");
    printf("https://accounts.spotify.com/authorize?client_id=%s&response_type=code&redirect_uri=https://127.0.0.1:8888/callback&scope=user-read-currently-playing%%20user-read-playback-state\n\n", CLIENT_ID);
    printf("Step 2: After logging in, copy the 'code' parameter from the redirect URL\n\n");
    printf("Enter the code: ");
    
    if (fgets(code, sizeof(code), stdin) == NULL) {
        printf("Error reading code\n");
        return -1;
    }
    
    // Remove newline
    char *newline = strchr(code, '\n');
    if (newline) *newline = '\0';
    
    printf("Exchanging code for tokens...\n");
    
    CURL *curl;
    CURLcode res;
    
    response_size = 0;
    memset(response_buffer, 0, sizeof(response_buffer));
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (curl) {
        char post_data[384];
        snprintf(post_data, sizeof(post_data),
                "grant_type=authorization_code&code=%s&redirect_uri=https://127.0.0.1:8888/callback",
                code);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

        curl_easy_setopt(curl, CURLOPT_URL, "https://accounts.spotify.com/api/token");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_USERPWD, CLIENT_ID ":" CLIENT_SECRET);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, request_cb);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            char access_token[MAX_TOKEN_SIZE];
            char refresh_token[MAX_TOKEN_SIZE];
            
            if (json_parser(response_buffer, "access_token", access_token, sizeof(access_token)) == 0 &&
                json_parser(response_buffer, "refresh_token", refresh_token, sizeof(refresh_token)) == 0) {
                write_tokens(access_token, refresh_token);
                printf("Setup complete! Tokens saved to %s\n", TOKEN_FILE);
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return 0;
            } else {
                printf("Error: Failed to get tokens\n");
                printf("Response: %s\n", response_buffer);
            }
        } else {
            printf("Error: %s\n", curl_easy_strerror(res));
        }
        
        curl_easy_cleanup(curl);
    }
    
    curl_global_cleanup();
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s {setup|now|refresh}\n\n", argv[0]);
        printf("  setup   - Initial setup (run once)\n");
        printf("  change  - Manually change to next track\n");
        printf("  now     - Get currently playing track\n");
        printf("  refresh - Manually refresh access token\n");
        return 1;
    }
    
    if (strcmp(argv[1], "setup") == 0) {
        return setup();
    } else if (strcmp(argv[1], "change") == 0 && argc == 3) {
        return change_track(argv[2], NULL);
    } else if (strcmp(argv[1], "now") == 0) {
        return get_current_track();
    } else if (strcmp(argv[1], "refresh") == 0) {
        return refresh_token();
    } else {
        printf("Unknown command: %s\n", argv[1]);
        return 1;
    }
    
    return 0;
}
