#include "price_list.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}

std::string canonicalName(const std::string& name) {
    std::string canonical = trim(name);
    std::transform(canonical.begin(), canonical.end(), canonical.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return canonical;
}

std::vector<std::string> splitRow(const std::string& row) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (start <= row.size()) {
        const auto comma = row.find(',', start);
        fields.push_back(trim(row.substr(start, comma == std::string::npos ? comma : comma - start)));
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return fields;
}

bool isHeader(const std::vector<std::string>& fields) {
    return fields.size() == 3
        && canonicalName(fields[0]) == "name"
        && canonicalName(fields[1]) == "price"
        && canonicalName(fields[2]) == "seats";
}

bool parsePrice(const std::string& raw, Paise& result, std::string& reason) {
    std::string value = trim(raw);
    const std::string rupeeSymbol = "\xE2\x82\xB9";
    if (value.compare(0, rupeeSymbol.size(), rupeeSymbol) == 0) {
        value = trim(value.substr(rupeeSymbol.size()));
    }
    if (value.empty()) {
        reason = "price is blank";
        return false;
    }
    if (value.front() == '-') {
        reason = "price cannot be negative";
        return false;
    }

    const auto decimal = value.find('.');
    if (decimal != std::string::npos && value.find('.', decimal + 1) != std::string::npos) {
        reason = "price has multiple decimal points";
        return false;
    }
    const std::string whole = decimal == std::string::npos ? value : value.substr(0, decimal);
    const std::string fraction = decimal == std::string::npos ? "" : value.substr(decimal + 1);
    if (whole.empty() || whole.find_first_not_of("0123456789") != std::string::npos
        || fraction.size() > 2
        || fraction.find_first_not_of("0123456789") != std::string::npos) {
        reason = "price must be rupees with up to two decimal places";
        return false;
    }

    try {
        const auto wholePaise = static_cast<__int128>(std::stoll(whole)) * 100;
        const auto fractionPaise = fraction.empty()
            ? 0
            : (fraction.size() == 1 ? (fraction[0] - '0') * 10 : std::stoll(fraction));
        const auto total = wholePaise + fractionPaise;
        if (total > std::numeric_limits<Paise>::max()) {
            reason = "price is too large";
            return false;
        }
        result = static_cast<Paise>(total);
        return true;
    } catch (const std::exception&) {
        reason = "price is not a valid number";
        return false;
    }
}

bool parseSeats(const std::string& raw, int& result, std::string& reason) {
    const std::string value = trim(raw);
    if (value.empty()) {
        reason = "seat count is blank";
        return false;
    }
    if (value.front() == '-') {
        reason = "seat count cannot be negative";
        return false;
    }
    if (value.find_first_not_of("0123456789") != std::string::npos) {
        reason = "seat count must be a non-negative integer";
        return false;
    }
    try {
        result = std::stoi(value);
        return true;
    } catch (const std::exception&) {
        reason = "seat count is too large";
        return false;
    }
}

void addIssue(
    ImportResult& result,
    std::size_t rowNumber,
    ImportStatus status,
    const std::string& rawRow,
    const std::string& name,
    const std::string& reason) {
    result.report.push_back({rowNumber, status, rawRow, name, reason});
    switch (status) {
    case ImportStatus::Imported:
        ++result.importedCount;
        break;
    case ImportStatus::Deduplicated:
        ++result.deduplicatedCount;
        break;
    case ImportStatus::Rejected:
        ++result.rejectedCount;
        break;
    case ImportStatus::Ignored:
        ++result.ignoredCount;
        break;
    }
}

} // namespace

ImportResult importPriceList(const std::string& csv) {
    ImportResult result;
    std::unordered_set<std::string> acceptedNames;
    std::istringstream input(csv);
    std::string row;
    std::size_t rowNumber = 0;

    while (std::getline(input, row)) {
        ++rowNumber;
        const auto fields = splitRow(row);
        if (trim(row).empty()) {
            addIssue(result, rowNumber, ImportStatus::Ignored, row, "", "blank row");
            continue;
        }
        if (isHeader(fields)) {
            addIssue(result, rowNumber, ImportStatus::Ignored, row, "", "header row");
            continue;
        }
        if (fields.size() != 3) {
            addIssue(result, rowNumber, ImportStatus::Rejected, row, "", "expected name, price, seats");
            continue;
        }

        const std::string name = trim(fields[0]);
        const std::string key = canonicalName(name);
        if (name.empty()) {
            addIssue(result, rowNumber, ImportStatus::Rejected, row, name, "tier name is blank");
            continue;
        }

        Paise price = 0;
        std::string reason;
        if (!parsePrice(fields[1], price, reason)) {
            addIssue(result, rowNumber, ImportStatus::Rejected, row, name, reason);
            continue;
        }
        int seats = 0;
        if (!parseSeats(fields[2], seats, reason)) {
            addIssue(result, rowNumber, ImportStatus::Rejected, row, name, reason);
            continue;
        }
        if (acceptedNames.find(key) != acceptedNames.end()) {
            addIssue(result, rowNumber, ImportStatus::Deduplicated, row, name, "duplicate of an earlier valid tier");
            continue;
        }

        acceptedNames.insert(key);
        result.tiers.push_back({name, price, seats});
        addIssue(result, rowNumber, ImportStatus::Imported, row, name, "accepted");
    }

    return result;
}

std::string importStatusName(ImportStatus status) {
    switch (status) {
    case ImportStatus::Imported:
        return "imported";
    case ImportStatus::Deduplicated:
        return "deduplicated";
    case ImportStatus::Rejected:
        return "rejected";
    case ImportStatus::Ignored:
        return "ignored";
    }
    return "unknown";
}