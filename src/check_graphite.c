#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <string.h>
#include <json.h>

#define EXIT_OK 0
#define EXIT_WARNING 1
#define EXIT_CRITICAL 2
#define EXIT_UNKNOWN 3

struct DataStruct {
    char *data;
    size_t size;
};

int help(bool failure);
size_t write_to_buffer(void *contents, size_t size, size_t nmemb, void *userp);

int main(int argc, char **argv) {
    char *name = "value", *url = NULL, *metric = NULL;
    int duration = 5;
    double warning = 0, critical = 0, scale = 1.0;
    struct option long_options[] = {
        {"name", required_argument, 0, 'n'},
        {"url", required_argument, 0, 'u'},
        {"duration", required_argument, 0, 'd'},
        {"metric", required_argument, 0, 'm'},
        {"warning", required_argument, 0, 'w'},
        {"critical", required_argument, 0, 'c'},
        {"scale", required_argument, 0, 's'},
        {"help", required_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    CURL *curl;
    CURLcode res;
    char full_url[2048];
    double total = 0;

    while(1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "n:u:d:m:w:c:s:h", long_options, &option_index);

        if(c == -1) {
            break;
        }

        switch(c) {
            case 'n':
                if(optarg) {
                    name = optarg;
                } else {
                    help(true);
                }
                break;
            case 'm':
                if(optarg) {
                    metric = optarg;
                } else {
                    help(true);
                }
                break;
            case 'd':
                if(optarg) {
                    duration = atoi(optarg);
                } else {
                    help(true);
                }
                break;
            case 'u':
                if(optarg) {
                    url = optarg;
                } else {
                    help(true);
                }
                break;
            case 'w':
                if(optarg) {
                    warning = atof(optarg);
                } else {
                    help(true);
                }
                break;
            case 'c':
                if(optarg) {
                    critical = atof(optarg);
                } else {
                    help(true);
                }
                break;
            case 's':
                if(optarg) {
                    scale = atof(optarg);
                } else {
                    help(true);
                }
                break;
            case 'h':
                help(false);
                break;
            default:
                help(true);
                break;
        }
    }

    if(url == NULL) {
        fprintf(stderr, "You must specify --url\n");
        help(true);
    }

    if(metric == NULL) {
        fprintf(stderr, "You must specify --metric\n");
        help(true);
    }

    if(warning == 0) {
        fprintf(stderr, "You must specify --warning\n");
        help(true);
    }

    if(critical == 0) {
        fprintf(stderr, "You must specify --critical\n");
        help(true);
    }

    if(scale == 1.0) {
        snprintf(full_url, sizeof(full_url), "%s/render/?target=%s&format=json&from=-%dmins", url, metric, duration);
    } else {
        snprintf(full_url, sizeof(full_url), "%s/render/?target=scale(%s,%.2f)&format=json&from=-%dmins", url, metric, scale, duration);
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(curl) {
        struct DataStruct graphite_data;
        graphite_data.data = malloc(1);
        graphite_data.size = 0;

        curl_easy_setopt(curl, CURLOPT_URL, full_url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "check_graphite/libcurl-agent/1.0");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_buffer);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&graphite_data);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            fprintf(stderr, "%s UNKNOWN: CURL error - %s\n", name, curl_easy_strerror(res));
            exit(EXIT_UNKNOWN);
        }

        json_object *json_targets = json_tokener_parse_verbose(graphite_data.data);
        if(json_targets == NULL) {
            fprintf(stderr, "%s UNKNOWN: JSON parse error\n", name);
            exit(EXIT_UNKNOWN);
        }

        int num_targets = json_object_array_length(json_targets);
        int i;
        for(i = 0; i < num_targets; i++) {
            double subtotal = 0;
            json_object *target_data = json_object_array_get_idx(json_targets, i);
            json_object *target_datapoints = json_object_object_get(target_data, "datapoints");
            int num_datapoints = json_object_array_length(target_datapoints);
            int j;
            for(j = 0; j < num_datapoints; j++) {
                json_object *datapoint = json_object_array_get_idx(target_datapoints, j);
                json_object *datapoint_value = json_object_array_get_idx(datapoint, 0);
                subtotal = subtotal + json_object_get_double(datapoint_value);
            }
            total = total + (subtotal / (num_datapoints));
        }
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();

    if(critical > warning) {
        if(total >= critical) {
            printf("%s CRITICAL: %.2f\n", name, total);
            exit(EXIT_CRITICAL);
        } else if(total >= warning) {
            printf("%s WARNING: %.2f\n", name, total);
            exit(EXIT_WARNING);
        } else {
            printf("%s OK: %.2f\n", name, total);
            exit(EXIT_OK);
        }
    } else {
        if(total <= critical) {
            printf("%s CRITICAL: %.2f\n", name, total);
            exit(EXIT_CRITICAL);
        } else if(total <= warning) {
            printf("%s WARNING: %.2f\n", name, total);
            exit(EXIT_WARNING);
        } else {
            printf("%s OK: %.2f\n", name, total);
            exit(EXIT_OK);
        }
    }
}

int help(bool failure) {
    printf("Usage: check_graphite [options]\n");
    printf("    -n, --name NAME         Descriptive name\n");
    printf("    -u, --url URL           Target URL\n");
    printf("    -m, --metric NAME       Metric path string\n");
    printf("    -d, --duration LENGTH   Length in minutes of data to parse (default: 5)\n");
    printf("    -w, --warning VALUE     Warning threshold\n");
    printf("    -c, --critical VALUE    Critical threshold\n");
    printf("    -s, --scale VALUE       Scale adjustment (default: 1)\n");
    printf("    -h, --help              Display this screen\n");

    exit(failure == true);
}

size_t write_to_buffer(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct DataStruct *mem = (struct DataStruct *)userp;

    mem->data = realloc(mem->data, mem->size + realsize + 1);
    if(mem->data == NULL) {
        fprintf(stderr, "UNKNOWN - not enough memory, realloc() returned NULL\n");
        exit(EXIT_UNKNOWN);
    }

    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    return realsize;
}
