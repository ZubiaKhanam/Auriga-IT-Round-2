# Engineering Reasoning

## Scope and Shape

This is intentionally a small C++17 project. `CinemaCounter` owns the configured ticket tiers and inventory. Plain structs carry ticket tiers, booking items, discount inputs, ticket lines, and receipt values. A single `book` method performs validation and pricing, while the separate `formatBill` function turns a completed receipt into customer-facing text.

This keeps the pricing API reusable for different counters and shows without adding a hierarchy of discount or tax classes that the current requirements do not need.

## Pricing Order

The implementation calculates in this order:

1. Ticket line subtotals and the base ticket total.
2. One flat festival discount, limited to the base total.
3. Member percentage discount on the post-festival amount.
4. Member discount cap and a final clamp to the remaining ticket amount.
5. Convenience fee based on the total number of tickets.
6. Taxable amount as discounted tickets plus convenience fee.
7. GST on the taxable amount.
8. Final payable amount as taxable amount plus GST.

This order makes the taxable base explicit and follows the assumption that both discounts reduce the ticket amount before the fee and GST are applied. Non-members skip the member discount entirely, even if a rate and cap are supplied.

## Integer Paise

Floating-point currency was avoided because binary floating-point values cannot represent every decimal amount exactly. `Paise` is an alias for `std::int64_t`, so `Rs.250.75` is stored as `25075` throughout the calculation.

Rates use basis points with a denominator of 10,000. The shared calculation pattern is:

```text
rounded amount = (amount in paise * basis points + 5000) / 10000
```

Adding half the denominator implements round-half-up for the non-negative values accepted by the engine. `__int128` is used for intermediate products and overflow checks; conversion to `Paise` happens only after the result has been checked.

## Discount Cap

The percentage member discount is calculated from the amount remaining after the festival discount. It is then limited by `memberDiscountCapPaise` and by the remaining ticket amount. This prevents the discount from exceeding its configured business limit or producing a negative ticket total.

The festival discount is also limited to the base ticket total. Negative discount, fee, and cap inputs are rejected rather than silently changing the calculation.

## GST and Fees

The convenience fee is a fixed paise amount per ticket, so the total is:

```text
total ticket quantity * convenience fee per ticket
```

The taxable amount is stored explicitly in the receipt:

```text
discounted ticket amount + convenience fee
```

GST is calculated from that taxable amount using the same basis-point round-half-up approach as the member percentage discount. The final payable amount is the taxable amount plus the rounded GST.

## Inventory Validation and Atomicity

The constructor rejects empty or duplicate tier names, negative prices, and negative seat counts. A booking rejects unknown tiers, non-positive quantities, sold-out tiers, and requests that exceed available seats.

Repeated lines for one tier are checked against their combined requested quantity. This prevents two individually valid lines from overbooking the same tier.

The booking method first validates all lines and completes all pricing and overflow checks. It decrements inventory only immediately before returning a successful receipt. Therefore, a rejected booking, including one that fails a later pricing overflow check, does not partially consume seats.

## Receipt and Bill Separation

The receipt contains computed ticket lines with tier name, quantity, unit price, and subtotal, along with every aggregate pricing component. `formatBill` consumes this data and performs only presentation work, including converting paise to `Rs.major.minor` text. Pricing does not depend on the output format.

## Testing Approach

The project uses a small assert-based executable registered with CTest. The tests cover:

- Valid single-tier and multi-tier bookings.
- Sold-out and insufficient inventory.
- Zero and negative quantities.
- Unknown tiers and duplicate tier-line quantities.
- Festival and member discounts.
- Member cap reached and exceeded.
- Non-member behavior.
- Member and GST rounding.
- Convenience fees and GST on the combined taxable amount.
- Exact line-by-line bill output.
- Inventory preservation when later pricing fails due to overflow.

The suite runs with:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```