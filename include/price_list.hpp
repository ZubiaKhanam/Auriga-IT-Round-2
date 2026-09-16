#pragma once

#include "cinema.hpp"

#include <cstddef>
#include <string>
#include <vector>

enum class ImportStatus {
    Imported,
    Deduplicated,
    Rejected,
    Ignored,
};

struct ImportIssue {
    std::size_t rowNumber;
    ImportStatus status;
    std::string rawRow;
    std::string name;
    std::string reason;
};

struct ImportResult {
    std::vector<TicketTier> tiers;
    std::vector<ImportIssue> report;
    std::size_t importedCount = 0;
    std::size_t deduplicatedCount = 0;
    std::size_t rejectedCount = 0;
    std::size_t ignoredCount = 0;
};

ImportResult importPriceList(const std::string& csv);

std::string importStatusName(ImportStatus status);