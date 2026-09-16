#include "price_list.hpp"

#include <cassert>
#include <string>

namespace {

void importsCaseInsensitiveFirstOccurrence() {
    const auto result = importPriceList(
        "name,price,seats\n"
        " Silver , 150.00 , 5\n"
        "SILVER,155,4\n"
        "Gold,250,2\n");

    assert(result.tiers.size() == 2);
    assert(result.tiers[0].name == "Silver");
    assert(result.tiers[0].pricePaise == 15000);
    assert(result.deduplicatedCount == 1);
    assert(result.ignoredCount == 1);
}

void parsesRupeeSymbolAndDecimals() {
    const auto result = importPriceList(
        "Silver,\xE2\x82\xB9" "150.5,1\nGold,250.75,1\n");

    assert(result.tiers[0].pricePaise == 15050);
    assert(result.tiers[1].pricePaise == 25075);
}

void rejectsInvalidPriceAndSeatRows() {
    const auto result = importPriceList(
        "BlankPrice,,2\n"
        "Negative,-10,2\n"
        "Malformed,12.345,2\n"
        "BadSeats,10,-1\n"
        "DecimalSeats,10,2.5\n");

    assert(result.tiers.empty());
    assert(result.rejectedCount == 5);
    assert(result.report[0].reason == "price is blank");
    assert(result.report[1].reason == "price cannot be negative");
}

void acceptsPartialImportAndReportsRows() {
    const auto result = importPriceList(
        "Silver,150,5\n"
        "\n"
        "Broken,not-money,2\n"
        "silver,155,4\n");

    assert(result.tiers.size() == 1);
    assert(result.importedCount == 1);
    assert(result.deduplicatedCount == 1);
    assert(result.rejectedCount == 1);
    assert(result.ignoredCount == 1);
    assert(result.report.size() == 4);
}

void allInvalidRowsProduceNoTiers() {
    const auto result = importPriceList("Silver,-1,5\nGold,,2\n");

    assert(result.tiers.empty());
    assert(result.rejectedCount == 2);
}

void reportsStatusNames() {
    assert(importStatusName(ImportStatus::Imported) == "imported");
    assert(importStatusName(ImportStatus::Deduplicated) == "deduplicated");
    assert(importStatusName(ImportStatus::Rejected) == "rejected");
    assert(importStatusName(ImportStatus::Ignored) == "ignored");
}

} // namespace

int main() {
    importsCaseInsensitiveFirstOccurrence();
    parsesRupeeSymbolAndDecimals();
    rejectsInvalidPriceAndSeatRows();
    acceptsPartialImportAndReportsRows();
    allInvalidRowsProduceNoTiers();
    reportsStatusNames();
}