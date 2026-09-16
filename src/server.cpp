#include "cinema.hpp"
#include "price_list.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

constexpr int defaultPort = 8080;

const std::vector<TicketTier> defaultTiers = {
    {"Silver", 15000, 5},
    {"Gold", 25000, 2},
    {"Recliner", 40000, 0},
};

std::string urlDecode(const std::string& value) {
    std::string decoded;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '+') {
            decoded += ' ';
        } else if (value[index] == '%' && index + 2 < value.size()) {
            const auto hex = value.substr(index + 1, 2);
            decoded += static_cast<char>(std::stoi(hex, nullptr, 16));
            index += 2;
        } else {
            decoded += value[index];
        }
    }
    return decoded;
}

std::vector<std::pair<std::string, std::string>> parseForm(const std::string& body) {
    std::vector<std::pair<std::string, std::string>> fields;
    std::size_t start = 0;
    while (start <= body.size()) {
        const std::size_t end = body.find('&', start);
        const std::string field = body.substr(start, end == std::string::npos ? end : end - start);
        const std::size_t separator = field.find('=');
        if (separator != std::string::npos) {
            fields.emplace_back(
                urlDecode(field.substr(0, separator)),
                urlDecode(field.substr(separator + 1)));
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return fields;
}

std::string fieldValue(
    const std::vector<std::pair<std::string, std::string>>& fields,
    const std::string& name,
    const std::string& fallback = "") {
    for (const auto& field : fields) {
        if (field.first == name) {
            return field.second;
        }
    }
    return fallback;
}

std::string jsonEscape(const std::string& value) {
    std::string escaped;
    for (const char character : value) {
        if (character == '\"' || character == '\\') {
            escaped += '\\';
        } else if (character == '\n') {
            escaped += "\\n";
        } else if (character == '\r') {
            escaped += "\\r";
        } else if (character == '\t') {
            escaped += "\\t";
        } else {
            escaped += character;
        }
    }
    return escaped;
}

std::string tiersJson(const CinemaCounter& counter) {
    std::ostringstream output;
    output << "[";
    for (std::size_t index = 0; index < counter.tiers().size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const auto& tier = counter.tiers()[index];
        output << "{\"name\":\"" << jsonEscape(tier.name)
               << "\",\"pricePaise\":" << tier.pricePaise
               << ",\"availableSeats\":" << tier.availableSeats << "}";
    }
    output << "]";
    return output.str();
}

std::string receiptJson(const BookingReceipt& receipt) {
    std::ostringstream output;
    output << "{\"ticketLines\":[";
    for (std::size_t index = 0; index < receipt.ticketLines.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const auto& line = receipt.ticketLines[index];
        output << "{\"tierName\":\"" << jsonEscape(line.tierName)
               << "\",\"quantity\":" << line.quantity
               << ",\"unitPricePaise\":" << line.unitPricePaise
               << ",\"subtotalPaise\":" << line.subtotalPaise << "}";
    }
    output << "],\"baseTicketTotalPaise\":" << receipt.baseTicketTotalPaise
           << ",\"festivalDiscountPaise\":" << receipt.festivalDiscountPaise
           << ",\"memberDiscountPaise\":" << receipt.memberDiscountPaise
           << ",\"ticketTotalPaise\":" << receipt.ticketTotalPaise
           << ",\"convenienceFeePaise\":" << receipt.convenienceFeePaise
           << ",\"taxableAmountPaise\":" << receipt.taxableAmountPaise
           << ",\"gstPaise\":" << receipt.gstPaise
           << ",\"finalPayablePaise\":" << receipt.finalPayablePaise
           << ",\"bill\":\"" << jsonEscape(formatBill(receipt)) << "\"}";
    return output.str();
}

std::string errorJson(const std::string& message) {
    return "{\"error\":\"" + jsonEscape(message) + "\"}";
}

std::string importResultJson(const ImportResult& result) {
    std::ostringstream output;
    output << "{\"tiers\":";
    output << "[";
    for (std::size_t index = 0; index < result.tiers.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const auto& tier = result.tiers[index];
        output << "{\"name\":\"" << jsonEscape(tier.name)
               << "\",\"pricePaise\":" << tier.pricePaise
               << ",\"availableSeats\":" << tier.availableSeats << "}";
    }
    output << "],\"counts\":{";
    output << "\"imported\":" << result.importedCount
           << ",\"deduplicated\":" << result.deduplicatedCount
           << ",\"rejected\":" << result.rejectedCount
           << ",\"ignored\":" << result.ignoredCount << "},\"report\":[";
    for (std::size_t index = 0; index < result.report.size(); ++index) {
        if (index > 0) {
            output << ",";
        }
        const auto& issue = result.report[index];
        output << "{\"rowNumber\":" << issue.rowNumber
               << ",\"status\":\"" << importStatusName(issue.status)
               << "\",\"rawRow\":\"" << jsonEscape(issue.rawRow)
               << "\",\"name\":\"" << jsonEscape(issue.name)
               << "\",\"reason\":\"" << jsonEscape(issue.reason) << "\"}";
    }
    output << "]}";
    return output.str();
}

std::string httpResponse(
    const std::string& status,
    const std::string& contentType,
    const std::string& body) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status << "\r\n"
             << "Content-Type: " << contentType << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
    return response.str();
}

std::string readPage(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open UI file: " + path);
    }
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

BookingReceipt createBooking(
    CinemaCounter& counter,
    const std::vector<std::pair<std::string, std::string>>& fields) {
    std::vector<BookingItem> items;
    const std::string tickets = fieldValue(fields, "tickets");
    std::size_t start = 0;
    while (start <= tickets.size()) {
        const std::size_t end = tickets.find(',', start);
        const std::string entry = tickets.substr(start, end == std::string::npos ? end : end - start);
        const std::size_t separator = entry.find(':');
        if (separator != std::string::npos && !entry.substr(separator + 1).empty()) {
            items.push_back({
                entry.substr(0, separator),
                std::stoi(entry.substr(separator + 1)),
            });
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    DiscountOptions options;
    options.festivalDiscountPaise = std::stoll(fieldValue(fields, "festival", "0"));
    options.isMember = fieldValue(fields, "member", "0") == "1";
    options.memberDiscountBasisPoints = std::stoi(fieldValue(fields, "memberRate", "0"));
    options.memberDiscountCapPaise = std::stoll(fieldValue(fields, "memberCap", "0"));
    options.convenienceFeePerTicketPaise = std::stoll(fieldValue(fields, "fee", "0"));
    options.gstBasisPoints = std::stoi(fieldValue(fields, "gst", "0"));
    return counter.book(items, options);
}

void handleConnection(
    int client,
    std::unique_ptr<CinemaCounter>& counter,
    const std::string& webRoot) {
    char buffer[8192] = {};
    const ssize_t received = recv(client, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        close(client);
        return;
    }

    const std::string request(buffer, static_cast<std::size_t>(received));
    const std::size_t firstLineEnd = request.find("\r\n");
    const std::string requestLine = request.substr(0, firstLineEnd);
    std::istringstream requestLineStream(requestLine);
    std::string method;
    std::string path;
    requestLineStream >> method >> path;

    try {
        std::string response;
        if (method == "GET" && path == "/") {
            response = httpResponse("200 OK", "text/html; charset=utf-8", readPage(webRoot + "/index.html"));
        } else if (method == "GET" && path == "/api/tiers") {
            response = httpResponse("200 OK", "application/json", tiersJson(*counter));
        } else if (method == "POST" && path == "/api/import") {
            const std::size_t headerEnd = request.find("\r\n\r\n");
            const std::string body = headerEnd == std::string::npos ? "" : request.substr(headerEnd + 4);
            const ImportResult result = importPriceList(body);
            response = httpResponse(
                result.tiers.empty() ? "400 Bad Request" : "200 OK",
                "application/json",
                importResultJson(result));
            if (!result.tiers.empty()) {
                counter = std::make_unique<CinemaCounter>(result.tiers);
            }
        } else if (method == "POST" && path == "/api/book") {
            const std::size_t headerEnd = request.find("\r\n\r\n");
            const std::string body = headerEnd == std::string::npos ? "" : request.substr(headerEnd + 4);
            response = httpResponse("200 OK", "application/json", receiptJson(createBooking(*counter, parseForm(body))));
        } else {
            response = httpResponse("404 Not Found", "application/json", errorJson("route not found"));
        }
        send(client, response.data(), response.size(), 0);
    } catch (const std::exception& error) {
        const std::string response = httpResponse("400 Bad Request", "application/json", errorJson(error.what()));
        send(client, response.data(), response.size(), 0);
    }
    close(client);
}

} // namespace

int main(int argc, char* argv[]) {
    const int port = argc > 1 ? std::stoi(argv[1]) : defaultPort;
    const std::string webRoot = argc > 2 ? argv[2] : "web";

    auto counter = std::make_unique<CinemaCounter>(defaultTiers);
    const int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        std::cerr << "Unable to create server socket\n";
        return 1;
    }

    int reuseAddress = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0
        || listen(server, 8) < 0) {
        std::cerr << "Unable to listen on port " << port << ": " << std::strerror(errno) << "\n";
        close(server);
        return 1;
    }

    std::cout << "Cinema UI running at http://localhost:" << port << "\n";
    while (true) {
        const int client = accept(server, nullptr, nullptr);
        if (client >= 0) {
            handleConnection(client, counter, webRoot);
        }
    }
}