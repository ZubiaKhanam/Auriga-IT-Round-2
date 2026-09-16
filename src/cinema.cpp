#include "cinema.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

CinemaCounter::CinemaCounter(std::vector<TicketTier> tiers)
    : tiers_(std::move(tiers)) {
    for (std::size_t index = 0; index < tiers_.size(); ++index) {
        const TicketTier& tier = tiers_[index];
        if (tier.name.empty()) {
            throw std::invalid_argument("ticket tier name cannot be empty");
        }
        if (tier.pricePaise < 0 || tier.availableSeats < 0) {
            throw std::invalid_argument("ticket tier price and seats cannot be negative");
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (tiers_[previous].name == tier.name) {
                throw std::invalid_argument("ticket tier names must be unique");
            }
        }
    }
}

const std::vector<TicketTier>& CinemaCounter::tiers() const {
    return tiers_;
}

BookingReceipt CinemaCounter::book(
    const std::vector<BookingItem>& items,
    const DiscountOptions& discounts) {
    if (discounts.festivalDiscountPaise < 0 || discounts.memberDiscountCapPaise < 0
        || discounts.convenienceFeePerTicketPaise < 0) {
        throw std::invalid_argument("discounts and fees cannot be negative");
    }
    if (discounts.memberDiscountBasisPoints < 0 || discounts.memberDiscountBasisPoints > 10000) {
        throw std::invalid_argument("member discount rate must be between 0% and 100%");
    }
    if (discounts.gstBasisPoints < 0 || discounts.gstBasisPoints > 10000) {
        throw std::invalid_argument("GST rate must be between 0% and 100%");
    }

    Paise baseTotal = 0;
    std::int64_t ticketCount = 0;
    std::vector<TicketLine> ticketLines;

    // Validate and calculate first so a failed booking cannot consume seats.
    for (const BookingItem& item : items) {
        if (item.quantity <= 0) {
            throw std::invalid_argument("ticket quantity must be positive");
        }

        auto tier = std::find_if(tiers_.begin(), tiers_.end(), [&](const TicketTier& candidate) {
            return candidate.name == item.tierName;
        });
        if (tier == tiers_.end()) {
            throw std::invalid_argument("ticket tier does not exist: " + item.tierName);
        }

        std::int64_t alreadyRequested = 0;
        for (const BookingItem& previousItem : items) {
            if (&previousItem == &item) {
                break;
            }
            if (previousItem.tierName == item.tierName) {
                alreadyRequested += previousItem.quantity;
            }
        }
        if (item.quantity > tier->availableSeats - alreadyRequested) {
            throw std::invalid_argument("not enough seats for tier: " + item.tierName);
        }

        const auto lineTotal = static_cast<__int128>(item.quantity) * tier->pricePaise;
        const auto newTotal = static_cast<__int128>(baseTotal) + lineTotal;
        if (newTotal > std::numeric_limits<Paise>::max()) {
            throw std::overflow_error("booking total exceeds supported amount");
        }
        baseTotal = static_cast<Paise>(newTotal);
        ticketCount += item.quantity;
        ticketLines.push_back({
            item.tierName,
            item.quantity,
            tier->pricePaise,
            static_cast<Paise>(lineTotal),
        });
    }

    const Paise festivalDiscount = std::min(discounts.festivalDiscountPaise, baseTotal);
    const Paise afterFestivalDiscount = baseTotal - festivalDiscount;

    Paise memberDiscount = 0;
    if (discounts.isMember) {
        const auto percentageAmount = static_cast<__int128>(afterFestivalDiscount)
            * discounts.memberDiscountBasisPoints;
        memberDiscount = static_cast<Paise>((percentageAmount + 5000) / 10000);
        memberDiscount = std::min(memberDiscount, discounts.memberDiscountCapPaise);
        memberDiscount = std::min(memberDiscount, afterFestivalDiscount);
    }

    const auto feeTotal = static_cast<__int128>(ticketCount)
        * discounts.convenienceFeePerTicketPaise;
    const auto taxableAmount = static_cast<__int128>(afterFestivalDiscount - memberDiscount)
        + feeTotal;
    if (feeTotal > std::numeric_limits<Paise>::max()
        || taxableAmount > std::numeric_limits<Paise>::max()) {
        throw std::overflow_error("taxable amount exceeds supported amount");
    }

    const Paise convenienceFee = static_cast<Paise>(feeTotal);
    const Paise discountedTicketTotal = afterFestivalDiscount - memberDiscount;
    const Paise taxableTotal = static_cast<Paise>(taxableAmount);
    const auto gstAmount = static_cast<__int128>(taxableTotal) * discounts.gstBasisPoints;
    const Paise gst = static_cast<Paise>((gstAmount + 5000) / 10000);
    const auto finalAmount = static_cast<__int128>(taxableTotal) + gst;
    if (finalAmount > std::numeric_limits<Paise>::max()) {
        throw std::overflow_error("final amount exceeds supported amount");
    }

    for (const BookingItem& item : items) {
        auto tier = std::find_if(tiers_.begin(), tiers_.end(), [&](const TicketTier& candidate) {
            return candidate.name == item.tierName;
        });
        tier->availableSeats -= item.quantity;
    }

    return BookingReceipt{
        items,
        std::move(ticketLines),
        baseTotal,
        festivalDiscount,
        memberDiscount,
        discountedTicketTotal,
        convenienceFee,
        taxableTotal,
        gst,
        static_cast<Paise>(finalAmount),
    };
}

namespace {

std::string formatMoney(Paise amountPaise) {
    std::ostringstream output;
    output << "Rs." << amountPaise / 100 << '.'
           << std::setw(2) << std::setfill('0') << amountPaise % 100;
    return output.str();
}

} // namespace

std::string formatBill(const BookingReceipt& receipt) {
    std::ostringstream bill;
    bill << "Cinema Bill\n";
    for (const TicketLine& line : receipt.ticketLines) {
        bill << line.tierName << " x " << line.quantity
             << " @ " << formatMoney(line.unitPricePaise)
             << " = " << formatMoney(line.subtotalPaise) << "\n";
    }
    bill << "Base ticket total: " << formatMoney(receipt.baseTicketTotalPaise) << "\n"
         << "Festival discount: -" << formatMoney(receipt.festivalDiscountPaise) << "\n"
         << "Member discount: -" << formatMoney(receipt.memberDiscountPaise) << "\n"
         << "Convenience fee: " << formatMoney(receipt.convenienceFeePaise) << "\n"
         << "Taxable amount: " << formatMoney(receipt.taxableAmountPaise) << "\n"
         << "GST: " << formatMoney(receipt.gstPaise) << "\n"
         << "Final payable amount: " << formatMoney(receipt.finalPayablePaise) << "\n";
    return bill.str();
}