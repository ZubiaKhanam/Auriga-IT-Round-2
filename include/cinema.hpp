#pragma once

#include <cstdint>
#include <string>
#include <vector>

using Paise = std::int64_t;

struct TicketTier {
    std::string name;
    Paise pricePaise;
    int availableSeats;
};

struct BookingItem {
    std::string tierName;
    int quantity;
};

struct TicketLine {
    std::string tierName;
    int quantity;
    Paise unitPricePaise;
    Paise subtotalPaise;
};

struct DiscountOptions {
    Paise festivalDiscountPaise = 0;
    bool isMember = false;
    int memberDiscountBasisPoints = 0;
    Paise memberDiscountCapPaise = 0;
    Paise convenienceFeePerTicketPaise = 0;
    int gstBasisPoints = 0;
};

struct BookingReceipt {
    std::vector<BookingItem> items;
    std::vector<TicketLine> ticketLines;
    Paise baseTicketTotalPaise;
    Paise festivalDiscountPaise;
    Paise memberDiscountPaise;
    Paise ticketTotalPaise;
    Paise convenienceFeePaise;
    Paise taxableAmountPaise;
    Paise gstPaise;
    Paise finalPayablePaise;
};

class CinemaCounter {
public:
    explicit CinemaCounter(std::vector<TicketTier> tiers);

    BookingReceipt book(
        const std::vector<BookingItem>& items,
        const DiscountOptions& discounts = {});

    const std::vector<TicketTier>& tiers() const;

private:
    std::vector<TicketTier> tiers_;
};

std::string formatBill(const BookingReceipt& receipt);