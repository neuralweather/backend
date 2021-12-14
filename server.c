#include <ctype.h>
#include <http.h>
#include <json-c/json.h>
#include <sqlite3.h>
#include <time.h>

// error and exit
#define ERROR(s, ...)                                                                    \
    fprintf(stderr, "\033[31mERROR\033[0m " s "\n", ##__VA_ARGS__), exit(EXIT_FAILURE);

// macro to check for errors when calling sqlite functions
// if there is an error, print the error message and return internal server error
#define SQLITE_TRY(x)                                                                    \
    if (x != SQLITE_OK) {                                                                \
        printf("\033[31mERROR\033[0m %s\n", sqlite3_errmsg(db));                         \
        return HTTP_RESPONSE("Internal Server Error",                                    \
                             HTTP_STATUS_INTERNAL_SERVER_ERROR);                         \
    }

// global db handle
// initialized in main()
sqlite3* db;

// helper function to check if a string is a valid integer
// used before calling atoi()
int str_is_number(char* str) {
    for (int i = 0; str[i]; i++) {
        if (!isdigit(str[i])) {
            return 0;
        }
    }
    return 1;
}

// handle POST requests to /data
// inserts a new row into the database and
http_response_t* handle_data_post(http_request_t* request) {
    // parse the request body as json
    struct json_object* body = json_tokener_parse(request->body);
    if (body == NULL) {
        return HTTP_RESPONSE("Invalid JSON body", HTTP_STATUS_BAD_REQUEST);
    }

    // extract data from json body
    struct json_object* temperature = json_object_object_get(body, "temperature");
    struct json_object* humidity = json_object_object_get(body, "humidity");
    struct json_object* windspeed = json_object_object_get(body, "windspeed");
    struct json_object* pressure = json_object_object_get(body, "pressure");
    struct json_object* rain = json_object_object_get(body, "rain");

    // check if all data is present
    if (temperature == NULL || humidity == NULL || windspeed == NULL ||
        pressure == NULL || rain == NULL) {
        return HTTP_RESPONSE("Missing data", HTTP_STATUS_BAD_REQUEST);
    }

    // get current unix timestamp
    time_t ts = time(NULL);

    // insert data into database

    char* sql = "INSERT INTO data (temperature, humidity, windspeed, pressure, rain, "
                "timestamp) VALUES "
                "(?, ?, ?, ?, ?, ?)";

    sqlite3_stmt* stmt;
    SQLITE_TRY(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL));
    SQLITE_TRY(sqlite3_bind_double(stmt, 1, json_object_get_double(temperature)));
    SQLITE_TRY(sqlite3_bind_double(stmt, 2, json_object_get_double(humidity)));
    SQLITE_TRY(sqlite3_bind_double(stmt, 3, json_object_get_double(windspeed)));
    SQLITE_TRY(sqlite3_bind_double(stmt, 4, json_object_get_double(pressure)));
    SQLITE_TRY(sqlite3_bind_double(stmt, 5, json_object_get_double(rain)));
    SQLITE_TRY(sqlite3_bind_int(stmt, 6, ts));
    // sqlite_step returns SQLITE_DONE on success instead of SQLITE_OK
    // so we need to check manually (not using SQLITE_TRY)
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        return HTTP_RESPONSE("Internal Server Error", HTTP_STATUS_INTERNAL_SERVER_ERROR);
    }

    // free resources
    SQLITE_TRY(sqlite3_finalize(stmt));
    json_object_put(body);

    // return success
    return HTTP_RESPONSE("OK", HTTP_STATUS_OK);
}

// handle GET requests to /data
// returns a json array of all data points
http_response_t* handle_data_get(http_request_t* request) {

    http_query_param_t* from_param = http_query_params_get(request->query_params, "from");
    http_query_param_t* to_param = http_query_params_get(request->query_params, "to");

    // check if from and to parameters are valid
    if (from_param == NULL || to_param == NULL) {
        return HTTP_RESPONSE("Invalid query parameters", HTTP_STATUS_BAD_REQUEST);
    }

    // check if from and to parameters are valid integers
    if (!str_is_number(from_param->value) || !str_is_number(to_param->value)) {
        return HTTP_RESPONSE("Invalid query parameters", HTTP_STATUS_BAD_REQUEST);
    }

    // get from and to parameters as integers/time_t
    time_t from_ts = atoi(from_param->value);
    time_t to_ts = atoi(to_param->value);

    // get data from database
    char* sql = "SELECT * FROM data WHERE timestamp >= ? AND timestamp <= ?";
    sqlite3_stmt* stmt;
    SQLITE_TRY(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL));
    SQLITE_TRY(sqlite3_bind_int(stmt, 1, from_ts));
    SQLITE_TRY(sqlite3_bind_int(stmt, 2, to_ts));

    // create json array
    struct json_object* array = json_object_new_array();

    // iterate over results
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        // create json object
        struct json_object* obj = json_object_new_object();

        // add data to json object
        json_object_object_add(obj, "temperature",
                               json_object_new_double(sqlite3_column_double(stmt, 0)));
        json_object_object_add(obj, "humidity",
                               json_object_new_double(sqlite3_column_double(stmt, 1)));
        json_object_object_add(obj, "windspeed",
                               json_object_new_double(sqlite3_column_double(stmt, 2)));
        json_object_object_add(obj, "pressure",
                               json_object_new_double(sqlite3_column_double(stmt, 3)));
        json_object_object_add(obj, "rain",
                               json_object_new_double(sqlite3_column_double(stmt, 4)));
        json_object_object_add(obj, "timestamp",
                               json_object_new_int(sqlite3_column_int(stmt, 5)));

        // add json object to array
        json_object_array_add(array, obj);
    }

    const char* json_str = json_object_to_json_string(array);
    // printf("%s\n", json_str);

    // free resources
    SQLITE_TRY(sqlite3_finalize(stmt));
    // TODO: memory leak - free json object, can't atm because we need to return the
    // string representation, this is a design problem with the http server library
    // json_object_put(array);

    return HTTP_RESPONSE((char*)json_str, HTTP_STATUS_OK,
                         HTTP_HEADERS(("Access-Control-Allow-Origin", "*"), // allow cors
                                      ("Content-Type", "application/json")));
}

// route handler for the '/data' endpoint
// forwards the request to the appropriate functions depending on the method
// or returns an error if the method is not supported
http_response_t* handle_data(http_request_t* request) {
    if (request->method == HTTP_METHOD_POST) {
        return handle_data_post(request);
    } else if (request->method == HTTP_METHOD_GET) {
        return handle_data_get(request);
    } else {
        return HTTP_RESPONSE("Method not allowed", HTTP_STATUS_METHOD_NOT_ALLOWED);
    }
}

http_response_t* handle_index(http_request_t* request) {
    return HTTP_RESPONSE(
        "Not too much to see here, you should take a look at our "
        "<a href=\"https://github.com/neuralweather\">Github organization</a> for "
        "more information on the project.<br>The purpose of this webserver is to provide "
        "the /data route which is used by the app and the microcontroller.",
        HTTP_STATUS_OK, HTTP_HEADERS(("Content-Type", "text/html")));
}

int main(int argc, char** argv) {

    // second argv: host
    // third argv: port
    // fourth argv: db file
    if (argc != 4) {
        ERROR("Usage: %s <host> <port> <db file>", argv[0]);
    }

    // check if port is valid
    if (!str_is_number(argv[2])) {
        ERROR("Invalid port: %s", argv[2]);
    }

    // open the database file
    // don't use SQLITE_TRY() here, because we want to terminate the program instead of
    // returning an HTTP error
    if (sqlite3_open(argv[3], &db) != SQLITE_OK) {
        ERROR("Could not open database file: %s", sqlite3_errmsg(db));
    }

    // create the table if it doesn't exist
    char* sql = "CREATE TABLE IF NOT EXISTS data ("
                "temperature REAL, "
                "humidity REAL, "
                "windspeed REAL, "
                "pressure REAL, "
                "rain REAL, "
                "timestamp INTEGER"
                ")";

    if (sqlite3_exec(db, sql, NULL, NULL, NULL)) {
        ERROR("Could not create table: %s", sqlite3_errmsg(db));
    }

    // create the server
    http_server_t* server = http_server_new();
    http_server_add_handler(server, "/", handle_index);
    http_server_add_handler(server, "/data", handle_data);
    http_server_run(server, argv[1], atoi(argv[2]));

    // http_server_run is not supposed to return and process termination
    // will free all resources anyway... but just do it for good measure
    http_server_free(server);
    sqlite3_close(db);

    return 0;
}
