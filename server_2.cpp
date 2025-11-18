#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <curl/curl.h>
#include "json.hpp"
using json = nlohmann::json;

// =========================================================
// UTILITY: Read a whole file
// =========================================================
std::string load_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    return { std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>() };
}

// =========================================================
// UTILITY: Write callback (for libcurl)
// =========================================================
size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// =========================================================
// SEND REQUEST TO OPENAI API
// =========================================================
std::string call_openai_api(const std::string& prompt) {
    const char* key = std::getenv("OPENAI_API_KEY");
    if (!key) return "{\"error\":\"API key missing\"}";

    CURL* curl = curl_easy_init();
    std::string response;

    if (curl) {
        json payload = {
            // Using the model specified in the original file
            {"model", "gpt-5.1"},
            {"messages", {
                // The entire persona and JSON request is injected as the user's content
                { {"role","user"}, {"content", prompt} }
            }}
        };

        std::string json_str = payload.dump();

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + std::string(key)).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            response = "{\"error\":\"curl request failed\"}";
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }

    return response;
}

// =========================================================
// VERY SIMPLE HTTP SERVER (NO EXTERNAL LIBS!)
// =========================================================
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

void send_http_response(int client, const std::string& content, const std::string& type="application/json") {
    std::string header =
        "HTTP/1.1 200 OK\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Type: " + type + "\r\n"
        "Content-Length: " + std::to_string(content.size()) + "\r\n"
        "Connection: close\r\n\r\n";

    std::string full = header + content;
    send(client, full.c_str(), full.size(), 0);
}

int main() {
    const char* key = std::getenv("OPENAI_API_KEY");
    if (!key) {
        std::cout << "ERROR: OPENAI_API_KEY not set\n";
        return 1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8081);

    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);

    std::cout << "Backend running at http://localhost:8081\n";

    while (true) {
        int client = accept(server_fd, nullptr, nullptr);

        char buffer[4096];
        int bytes = read(client, buffer, sizeof(buffer));
        if (bytes <= 0) {
            close(client);
            continue;
        }

        std::string request(buffer, bytes);

        // -----------------------------------------
        // Serve index.html
        // -----------------------------------------
        if (request.find("GET / ") == 0) {
            std::string html = load_file("Public/index_2.html");
            if (html.empty()) {
                send_http_response(client, "index.html not found", "text/plain");
            } else {
                send_http_response(client, html, "text/html");
            }
        }
        // -----------------------------------------
        // Serve /api/chat - Injecting the structured market analysis prompt
        // -----------------------------------------
        else if (request.find("POST /api/chat") == 0) {
            std::string body = request.substr(request.find("\r\n\r\n") + 4);
            json data = json::parse(body);
            std::string user_idea = data["message"];

            // --- MANDATORY SYSTEM PROMPT INJECTION TO FORCE JSON STRUCTURE ---
            std::string prompt_template = R"DELIM(
                Act as the "Synergistic Market Cap Hyper-Projection Nexus (Synapse-MX)", an unbiased AI financial analyst.
                Your task is to:
                1. Generate a highly buzzword-heavy, future-proof startup idea that aligns with the user's input: ')DELIM";

            std::string prompt_end = R"DELIM('. If the input is too vague, generate a cutting-edge, general tech startup (e.g., "Decentralized Quantum Edge Computing Platform").
                2. Analyze the market for this startup.
                3. Find 3 to 5 closely aligned public companies (competitors or analogous businesses) for comparison. Use realistic, illustrative, or current financial data.
                4. Provide the required financial metrics for the comparable companies: Revenue (in billions, e.g., $15.5B), Market Cap (in billions, e.g., $150B), 3-year Revenue Projection (YoY growth rate, e.g., +15%), and P/E Ratio (e.g., 35.2x).
                5. Provide a final, unbiased assessment (GOOD or BAD idea) and a rationale.

                You MUST respond with a single JSON object (AS PLAIN TEXT) following this exact schema. DO NOT include any introductory or concluding text, or markdown code blocks (e.g., ```json).

                {
                  "startupName": "...",
                  "startupIdea": "...",
                  "assessment": "GOOD" or "BAD",
                  "rationale": "...",
                  "comparables": [
                    {
                      "company": "...",
                      "industry": "...",
                      "revenueB": "...",
                      "marketCapB": "...",
                      "projectionYoY": "...",
                      "peRatio": "..."
                    }
                  ],
                  "metricsExplanation": "A brief, buzzword-heavy explanation of why these metrics matter for the startup's Total Addressable Market (TAM) and long-term Scalability Matrix."
                }
            )DELIM";

            std::string final_prompt = prompt_template + user_idea + prompt_end;
            std::string raw = call_openai_api(final_prompt);

// Parse OpenAI real JSON
json openai_json;
try {
    openai_json = json::parse(raw);
} catch (...) {
    send_http_response(client, "{\"error\":\"invalid json from openai\"}");
    close(client);
    continue;
}

// Extract assistant text
std::string text;
try {
    text = openai_json["choices"][0]["message"]["content"];
} catch (...) {
    send_http_response(client, "{\"error\":\"missing content\"}");
    close(client);
    continue;
}

// Wrap into frontend-expected schema
json wrapped = {
    {"candidates", {
        {
            {"content", {
                {"parts", {
                    { { "text", text } }
                }}
            }}
        }
    }}
};

send_http_response(client, wrapped.dump(), "application/json");

        }
        // -----------------------------------------
        // 404
        // -----------------------------------------
        else {
            send_http_response(client, "Not Found", "text/plain");
        }

        close(client);
    }

    close(server_fd);
    return 0;
}