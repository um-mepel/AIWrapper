#include "httplib.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <curl/curl.h>
#include "json.hpp"

using json = nlohmann::json;

// =========================================================
// Read a whole file
// =========================================================
std::string load_file(const std::string &path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    return { std::istreambuf_iterator<char>(f),
             std::istreambuf_iterator<char>() };
}

// =========================================================
// libcurl write callback
// =========================================================
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
}

// =========================================================
// Call OpenAI API using libcurl
// =========================================================
std::string call_openai(const std::string &prompt) {
    const char *key = std::getenv("OPENAI_API_KEY");
    if (!key) {
        return R"({"error":"OPENAI_API_KEY missing"})";
    }

    CURL *curl = curl_easy_init();
    std::string response;

    if (curl) {
        json payload = {
            {"model", "gpt-4o-mini"},
            {"messages", {
                { {"role","user"}, {"content", prompt} }
            }}
        };

        std::string json_str = payload.dump();

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + std::string(key)).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }

    return response;
}

// =========================================================
// Main server using cpp-httplib
// =========================================================
int main() {
    httplib::Server svr;

    // ---------- STATIC FILES ----------
    svr.Get("/", [](const httplib::Request&, httplib::Response &res) {
        std::string html = load_file("index.html");
        if (html.empty()) {
            res.status = 404;
            res.set_content("index.html missing", "text/plain");
        } else {
            res.set_content(html, "text/html");
        }
    });

    // ---------- CORS ----------
    svr.Options(R"((.*))", [](const httplib::Request&, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 200;
    });

    // ---------- POST /api/chat ----------
    svr.Post("/api/chat", [](const httplib::Request &req, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");

        if (!req.body.size()) {
            res.status = 400;
            res.set_content(R"({"error":"Empty request body"})", "application/json");
            return;
        }

        json received = json::parse(req.body, nullptr, false);
        if (received.is_discarded() || !received.contains("message")) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid JSON or missing 'message'"})", "application/json");
            return;
        }

        std::string user_msg = received["message"];
        std::string api_response = call_openai(user_msg);

        res.status = 200;
        res.set_content(api_response, "application/json");
    });

    std::cout << "Server running on port 8080\n";
    svr.listen("0.0.0.0", 8080);
}
