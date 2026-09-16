#include "cinema.hpp"

#include <cassert>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

CinemaCounter makeCounter() {
    return CinemaCounter({
        {"Silver", 15000, 5},
        {"Gold", 25000, 2},
        {"Recliner", 40000, 0},
    });
}

void expectInvalidBooking(const std::function<void()>& action) {
    bool threw = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void validBookingCalculatesPaiseAndUpdatesSeats() {
    auto counter = makeCounter();

    const auto receipt = counter.book({{"Silver", 2}});

    assert(receipt.ticketTotalPaise == 30000);
    assert(receipt.items.size() == 1);
    expectInvalidBooking([&] { counter.book({{"Silver", 4}}); });
}

void multipleTiersAreIncludedInOneTotal() {
    auto counter = makeCounter();

    const auto receipt = counter.book({{"Silver", 2}, {"Gold", 1}});

    assert(receipt.ticketTotalPaise == 55000);
}

void soldOutTierIsRejected() {
    auto counter = makeCounter();

    expectInvalidBooking([&] { counter.book({{"Recliner", 1}}); });
}

void insufficientSeatsDoNotPartiallyBook() {
    auto counter = makeCounter();

    expectInvalidBooking([&] { counter.book({{"Silver", 2}, {"Gold", 3}}); });
    expectInvalidBooking([&] { counter.book({{"Silver", 3}, {"Silver", 3}}); });
    const auto receipt = counter.book({{"Silver", 5}});
    assert(receipt.ticketTotalPaise == 75000);
}

void invalidQuantitiesAndUnknownTiersAreRejected() {
    auto counter = makeCounter();

    expectInvalidBooking([&] { counter.book({{"Silver", 0}}); });
    expectInvalidBooking([&] { counter.book({{"Silver", -1}}); });
    expectInvalidBooking([&] { counter.book({{"Unknown", 1}}); });
}

void memberReceivesFestivalAndPercentageDiscounts() {
    auto counter = makeCounter();

    const auto receipt = counter.book(
        {{"Silver", 2}},
        {5000, true, 1000, 10000});

    assert(receipt.baseTicketTotalPaise == 30000);
    assert(receipt.festivalDiscountPaise == 5000);
    assert(receipt.memberDiscountPaise == 2500);
    assert(receipt.ticketTotalPaise == 22500);
}

void memberDiscountReachingCapUsesTheCap() {
    auto counter = makeCounter();

    const auto receipt = counter.book(
        {{"Silver", 2}},
        {0, true, 1000, 3000});

    assert(receipt.memberDiscountPaise == 3000);
    assert(receipt.ticketTotalPaise == 27000);
}

void memberDiscountAboveCapIsLimited() {
    auto counter = makeCounter();

    const auto receipt = counter.book(
        {{"Silver", 2}},
        {0, true, 2000, 4000});

    assert(receipt.memberDiscountPaise == 4000);
    assert(receipt.ticketTotalPaise == 26000);
}

void nonMemberReceivesNoMemberDiscount() {
    auto counter = makeCounter();

    const auto receipt = counter.book(
        {{"Silver", 2}},
        {5000, false, 2000, 4000});

    assert(receipt.memberDiscountPaise == 0);
    assert(receipt.ticketTotalPaise == 25000);
}

void memberPercentageRoundsToNearestPaise() {
    auto counter = makeCounter();

    const auto receipt = counter.book(
        {{"Gold", 1}},
        {0, true, 3333, 10000});

    assert(receipt.memberDiscountPaise == 8333);
}

void normalBookingIncludesFeeAndGst() {
    auto counter = makeCounter();

    const auto receipt = counter.book(
        {{"Silver", 2}},
        {0, false, 0, 0, 2000, 1800});

    assert(receipt.ticketTotalPaise == 30000);
    assert(receipt.convenienceFeePaise == 4000);
    assert(receipt.taxableAmountPaise == 34000);
    assert(receipt.gstPaise == 6120);
    assert(receipt.finalPayablePaise == 40120);
}

void multipleTicketsUseTheFeePerTicket() {
    auto counter = makeCounter();

    const auto receipt = counter.book(
        {{"Silver", 2}, {"Gold", 1}},
        {0, false, 0, 0, 1000, 1800});

    assert(receipt.convenienceFeePaise == 3000);
    assert(receipt.taxableAmountPaise == 58000);
    assert(receipt.finalPayablePaise == 68440);
}

void discountsFeeAndGstAreCombinedInOrder() {
    auto counter = makeCounter();

    const auto receipt = counter.book(
        {{"Silver", 2}},
        {5000, true, 1000, 10000, 2000, 1800});

    assert(receipt.ticketTotalPaise == 22500);
    assert(receipt.convenienceFeePaise == 4000);
    assert(receipt.taxableAmountPaise == 26500);
    assert(receipt.gstPaise == 4770);
    assert(receipt.finalPayablePaise == 31270);
}

void gstRoundsToNearestPaise() {
    auto counter = makeCounter();

    const auto receipt = counter.book(
        {{"Gold", 1}},
        {0, false, 0, 0, 0, 3333});

    assert(receipt.taxableAmountPaise == 25000);
    assert(receipt.gstPaise == 8333);
    assert(receipt.finalPayablePaise == 33333);
}

void billContainsTicketLinesAndPricingBreakdown() {
    auto counter = makeCounter();
    const auto receipt = counter.book(
        {{"Silver", 2}, {"Gold", 1}},
        {5000, true, 1000, 10000, 1000, 1800});

    assert(formatBill(receipt) ==
        "Cinema Bill\n"
        "Silver x 2 @ Rs.150.00 = Rs.300.00\n"
        "Gold x 1 @ Rs.250.00 = Rs.250.00\n"
        "Base ticket total: Rs.550.00\n"
        "Festival discount: -Rs.50.00\n"
        "Member discount: -Rs.50.00\n"
        "Convenience fee: Rs.30.00\n"
        "Taxable amount: Rs.480.00\n"
        "GST: Rs.86.40\n"
        "Final payable amount: Rs.566.40\n");
}

void pricingFailureDoesNotConsumeInventory() {
    CinemaCounter counter({
        {"Silver", std::numeric_limits<Paise>::max(), 1},
    });

    bool overflowed = false;
    try {
        counter.book({{"Silver", 1}}, {0, false, 0, 0, 1, 0});
    } catch (const std::overflow_error&) {
        overflowed = true;
    }
    assert(overflowed);

    const auto receipt = counter.book({{"Silver", 1}});
    assert(receipt.baseTicketTotalPaise == std::numeric_limits<Paise>::max());
}

} // namespace

int main() {
    validBookingCalculatesPaiseAndUpdatesSeats();
    multipleTiersAreIncludedInOneTotal();
    soldOutTierIsRejected();
    insufficientSeatsDoNotPartiallyBook();
    invalidQuantitiesAndUnknownTiersAreRejected();
    memberReceivesFestivalAndPercentageDiscounts();
    memberDiscountReachingCapUsesTheCap();
    memberDiscountAboveCapIsLimited();
    nonMemberReceivesNoMemberDiscount();
    memberPercentageRoundsToNearestPaise();
    normalBookingIncludesFeeAndGst();
    multipleTicketsUseTheFeePerTicket();
    discountsFeeAndGstAreCombinedInOrder();
    gstRoundsToNearestPaise();
    billContainsTicketLinesAndPricingBreakdown();
    pricingFailureDoesNotConsumeInventory();
    std::cout << "All cinema booking tests passed\n";
}