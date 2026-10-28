#include "api_server.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <regex>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using socket_t = SOCKET;
static void closeSocket(socket_t s) {
    if (s != INVALID_SOCKET) {
        closesocket(s);
    }
}
static bool socketIsInvalid(socket_t s) {
    return s == INVALID_SOCKET;
}
static int lastSocketError() {
    return WSAGetLastError();
}
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cerrno>
using socket_t = int;
static void closeSocket(socket_t s) {
    if (s >= 0) {
        close(s);
    }
}
static bool socketIsInvalid(socket_t s) {
    return s < 0;
}
static int lastSocketError() {
    return errno;
}
#endif

namespace {

std::string jsonEscape(const std::string& input) {
    std::string output;
    output.reserve(input.size() + 8);

    for (unsigned char c : input) {
        switch (c) {
            case '"':  output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b";  break;
            case '\f': output += "\\f";  break;
            case '\n': output += "\\n";  break;
            case '\r': output += "\\r";  break;
            case '\t': output += "\\t";  break;
            default:
                if (c < 0x20) {
                    const char hex[] = "0123456789abcdef";
                    output += "\\u00";
                    output += hex[(c >> 4) & 0x0f];
                    output += hex[c & 0x0f];
                } else {
                    output += static_cast<char>(c);
                }
        }
    }
    return output;
}

std::string getString(const std::string& body,
                      const std::string& key,
                      const std::string& defaultValue = "") {
    const std::regex pattern(
        "\"" + key + "\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\""
    );
    std::smatch match;

    if (!std::regex_search(body, match, pattern)) {
        return defaultValue;
    }

    // Decode the common JSON escapes used by the dashboard.
    std::string value = match[1].str();
    std::string decoded;
    decoded.reserve(value.size());

    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            const char next = value[++i];
            switch (next) {
                case '"':  decoded += '"'; break;
                case '\\': decoded += '\\'; break;
                case '/':  decoded += '/'; break;
                case 'n':  decoded += '\n'; break;
                case 'r':  decoded += '\r'; break;
                case 't':  decoded += '\t'; break;
                case 'b':  decoded += '\b'; break;
                case 'f':  decoded += '\f'; break;
                default:
                    // Preserve unknown escape sequences instead of crashing.
                    decoded += next;
                    break;
            }
        } else {
            decoded += value[i];
        }
    }
    return decoded;
}

bool getBool(const std::string& body,
             const std::string& key,
             bool defaultValue = false) {
    const std::regex pattern(
        "\"" + key + "\"\\s*:\\s*(true|false|1|0)",
        std::regex::icase
    );
    std::smatch match;

    if (!std::regex_search(body, match, pattern)) {
        return defaultValue;
    }

    std::string value = match[1].str();
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return value == "true" || value == "1";
}

double getNumber(const std::string& body,
                 const std::string& key,
                 double defaultValue = 0.0) {
    const std::regex pattern(
        "\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)"
    );
    std::smatch match;

    if (!std::regex_search(body, match, pattern)) {
        return defaultValue;
    }

    try {
        return std::stod(match[1].str());
    } catch (...) {
        return defaultValue;
    }
}

std::string makeResponse(const std::string& body,
                         int code = 200,
                         const std::string& status = "OK") {
    std::ostringstream output;
    output << "HTTP/1.1 " << code << ' ' << status << "\r\n"
           << "Content-Type: application/json; charset=utf-8\r\n"
           << "Access-Control-Allow-Origin: *\r\n"
           << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
           << "Access-Control-Allow-Headers: Content-Type\r\n"
           << "Cache-Control: no-store\r\n"
           << "Connection: close\r\n"
           << "Content-Length: " << body.size() << "\r\n"
           << "\r\n"
           << body;
    return output.str();
}

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool parseContentLength(const std::string& headers, std::size_t& length) {
    std::istringstream stream(headers);
    std::string line;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        const std::string name = lowerCopy(line.substr(0, colon));
        if (name == "content-length") {
            try {
                const std::string value = line.substr(colon + 1);
                length = static_cast<std::size_t>(std::stoull(value));
                return true;
            } catch (...) {
                return false;
            }
        }
    }

    length = 0;
    return true;
}

bool sendAll(socket_t client, const std::string& response) {
    std::size_t sent = 0;

    while (sent < response.size()) {
#ifdef _WIN32
        const int result = send(
            client,
            response.data() + sent,
            static_cast<int>(std::min<std::size_t>(
                response.size() - sent,
                static_cast<std::size_t>(0x7fffffff))),
            0
        );
#else
        const ssize_t result = send(
            client,
            response.data() + sent,
            response.size() - sent,
            0
        );
#endif
        if (result <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(result);
    }

    return true;
}

} // namespace

ApiServer::ApiServer(Simulator& simulator, int port)
    : simulator_(simulator), port_(port) {}

int ApiServer::run() {
#ifdef _WIN32
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[API ERROR] Winsock initialization failed.\n";
        return 1;
    }
#endif

    socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (socketIsInvalid(server)) {
        std::cerr << "[API ERROR] socket() failed. Error: "
                  << lastSocketError() << '\n';
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    int reuseAddress = 1;
#ifdef _WIN32
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuseAddress),
               sizeof(reuseAddress));
#else
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
               &reuseAddress, sizeof(reuseAddress));
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<unsigned short>(port_));

    if (bind(server, reinterpret_cast<sockaddr*>(&address),
             static_cast<int>(sizeof(address))) < 0) {
        std::cerr << "[API ERROR] Cannot bind to port " << port_
                  << ". Check whether another process is using it. Error: "
                  << lastSocketError() << '\n';
        closeSocket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (listen(server, 16) < 0) {
        std::cerr << "[API ERROR] listen() failed. Error: "
                  << lastSocketError() << '\n';
        closeSocket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    std::cout << "ResiliX API listening on http://127.0.0.1:"
              << port_ << '\n'
              << "Press Ctrl+C to stop.\n";

    while (true) {
        sockaddr_in clientAddress{};
#ifdef _WIN32
        int addressLength = sizeof(clientAddress);
        socket_t client = accept(
            server,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &addressLength
        );
#else
        socklen_t addressLength = sizeof(clientAddress);
        socket_t client = accept(
            server,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &addressLength
        );
#endif

        if (socketIsInvalid(client)) {
            std::cerr << "[API WARNING] accept() failed. Error: "
                      << lastSocketError() << '\n';
            continue;
        }

        // Prevent one incomplete client request from blocking the whole API.
#ifdef _WIN32
        const DWORD timeoutMs = 3000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeoutMs),
                   sizeof(timeoutMs));
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&timeoutMs),
                   sizeof(timeoutMs));
#else
        timeval timeout{};
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                   &timeout, sizeof(timeout));
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO,
                   &timeout, sizeof(timeout));
#endif

        std::string rawRequest;
        std::size_t headerEnd = std::string::npos;
        std::size_t contentLength = 0;
        bool malformed = false;
        bool timedOutOrClosed = false;
        char buffer[4096];

        std::cout << "[DEBUG] Client connected.\n";

        // Read until the complete headers and declared body are present.
        while (rawRequest.size() < 1024 * 1024) {
#ifdef _WIN32
            const int bytesReceived = recv(
                client, buffer, static_cast<int>(sizeof(buffer)), 0
            );
#else
            const ssize_t bytesReceived = recv(
                client, buffer, sizeof(buffer), 0
            );
#endif
            if (bytesReceived <= 0) {
                timedOutOrClosed = true;
                break;
            }

            rawRequest.append(buffer, static_cast<std::size_t>(bytesReceived));
            headerEnd = rawRequest.find("\r\n\r\n");

            if (headerEnd != std::string::npos) {
                const std::string headers = rawRequest.substr(0, headerEnd);

                if (!parseContentLength(headers, contentLength)) {
                    malformed = true;
                    break;
                }

                if (contentLength > 1024 * 1024) {
                    malformed = true;
                    break;
                }

                const std::size_t bodyStart = headerEnd + 4;
                if (rawRequest.size() >= bodyStart + contentLength) {
                    break;
                }
            }
        }

        std::string responseText;

        if (malformed || rawRequest.size() >= 1024 * 1024) {
            responseText = makeResponse(
                "{\"error\":\"Invalid or oversized request body\"}",
                400, "Bad Request"
            );
        } else if (headerEnd == std::string::npos) {
            responseText = makeResponse(
                "{\"error\":\"Malformed or incomplete HTTP request\"}",
                400, "Bad Request"
            );
        } else if (timedOutOrClosed) {
            // A client that disconnects before sending a complete request
            // cannot receive a useful response; record the event and continue.
            std::cerr << "[API WARNING] Client disconnected or timed out "
                         "before the request was complete.\n";
            closeSocket(client);
            continue;
        } else {
            std::istringstream requestLine(rawRequest.substr(0, headerEnd));
            std::string method;
            std::string path;
            std::string version;
            requestLine >> method >> path >> version;

            const std::size_t queryPosition = path.find('?');
            if (queryPosition != std::string::npos) {
                path.resize(queryPosition);
            }

            const std::size_t bodyStart = headerEnd + 4;
            std::string body = rawRequest.substr(bodyStart, contentLength);

            std::cout << "[DEBUG] Request: " << method << ' '
                      << path << '\n';

            try {
                if (method == "OPTIONS") {
                    responseText = makeResponse("{}", 204, "No Content");
                } else if (method == "GET" && path == "/api/health") {
                    std::cout << "[DEBUG] Health route matched.\n";
                    const std::string json = simulator_.healthJson();
                    responseText = makeResponse(json);
                    std::cout << "[DEBUG] Health response created: "
                              << responseText.size() << " bytes.\n";
                } else if (method == "GET" && path == "/api/services") {
                    responseText = makeResponse(simulator_.servicesJson());
                } else if (method == "GET" && path == "/api/dependencies") {
                    responseText = makeResponse(simulator_.dependenciesJson());
                } else if (method == "GET" && path == "/api/incidents") {
                    responseText = makeResponse(simulator_.incidentsJson());
                } else if (method == "GET" && path == "/api/metrics") {
                    responseText = makeResponse(simulator_.metricsJson());
                } else if (method == "POST" && path == "/api/failure") {
                    const std::string service = getString(body, "service");
                    const std::string description = getString(
                        body, "description", "API failure injection"
                    );
                    responseText = makeResponse(
                        simulator_.injectFailure(service, description)
                    );
                } else if (method == "POST" && path == "/api/recovery") {
                    const std::string service = getString(body, "service");
                    const std::string strategy = getString(
                        body, "strategy", "restart"
                    );
                    responseText = makeResponse(
                        simulator_.recoverService(service, strategy)
                    );
                } else if (method == "POST" && path == "/api/metrics") {
                    Metrics metrics;
                    metrics.latencyMs = getNumber(body, "latencyMs");
                    metrics.errorRatePct = getNumber(body, "errorRatePct");
                    metrics.cpuPct = getNumber(body, "cpuPct");
                    metrics.memoryPct = getNumber(body, "memoryPct");

                    responseText = makeResponse(
                        simulator_.updateMetrics(
                            getString(body, "service"), metrics
                        )
                    );
                } else if (method == "POST" && path == "/api/evaluate") {
                    responseText = makeResponse(simulator_.evaluateAll());
                } else if (method == "POST" && path == "/api/evaluation") {
                    responseText = makeResponse(
                        simulator_.recordEvaluation(
                            getBool(body, "expectedFailure"),
                            getBool(body, "detectedFailure")
                        )
                    );
                } else {
                    responseText = makeResponse(
                        "{\"error\":\"Route not found\","
                        "\"routes\":["
                        "\"GET /api/health\","
                        "\"GET /api/services\","
                        "\"GET /api/dependencies\","
                        "\"GET /api/incidents\","
                        "\"GET /api/metrics\","
                        "\"POST /api/failure\","
                        "\"POST /api/recovery\","
                        "\"POST /api/metrics\","
                        "\"POST /api/evaluate\","
                        "\"POST /api/evaluation\"]}",
                        404, "Not Found"
                    );
                }
            } catch (const std::exception& exception) {
                std::cerr << "[API ERROR] Handler exception: "
                          << exception.what() << '\n';
                responseText = makeResponse(
                    "{\"error\":\"Internal server error\"}",
                    500, "Internal Server Error"
                );
            } catch (...) {
                std::cerr << "[API ERROR] Unknown handler exception.\n";
                responseText = makeResponse(
                    "{\"error\":\"Internal server error\"}",
                    500, "Internal Server Error"
                );
            }
        }

        if (!responseText.empty()) {
            const bool sent = sendAll(client, responseText);
            if (sent) {
                std::cout << "[DEBUG] Response sent successfully ("
                          << responseText.size() << " bytes).\n";
            } else {
                std::cerr << "[API WARNING] Failed to send full response. Error: "
                          << lastSocketError() << '\n';
            }
        }

        closeSocket(client);
    }

    closeSocket(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
