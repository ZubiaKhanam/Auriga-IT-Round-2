# Cinema Pricing Engine

A small reusable C++17 cinema pricing engine. It accepts configurable ticket tiers, books multiple tiers in one request, validates inventory, calculates discounts, convenience fees, GST, and produces a customer-facing line-by-line bill.

## Features

- Configurable ticket tiers with prices stored in paise and available seats.
- Multiple ticket tiers in a single booking.
- Flat festival discount applied once per booking.
- Optional percentage member discount with a paise cap.
- Configurable convenience fee per ticket.
- Configurable GST rate.
- Integer-only monetary calculations with consistent rounding.
- Receipt data containing every pricing component.
- Separate `formatBill` function for customer-facing output.

## Project Structure

```text
.
├── CMakeLists.txt
├── include/
│   └── cinema.hpp          # Public data types and booking API
├── src/
│   └── cinema.cpp          # Booking, pricing, validation, and bill formatting
├── tests/
│   └── cinema_tests.cpp    # CTest executable with assert-based tests
├── README.md
└── REASONING.md
```

## Prerequisites

- A C++17 compiler. GCC 13 works in the development container.
- CMake 3.16 or newer.
- A shell with `cmake` and `ctest` on `PATH`.

No third-party libraries are required.

## Web UI

The project includes a lightweight dependency-free HTTP server that serves the browser UI and calls the existing C++ pricing engine for every booking calculation. The browser does not reimplement discounts, fees, GST, rounding, or inventory validation.

Build and start it from the repository root:

```bash
cmake -S . -B build
cmake --build build
./build/cinema_server 8080 web
```

Then open `http://localhost:8080`. The optional first argument is the port and the optional second argument is the web directory, for example `./build/cinema_server 8090 web`.

In GitHub Codespaces, open the **PORTS** panel in VS Code, add or inspect port `8080`, and click the globe/open-in-browser icon for that forwarded port. The server must remain running in the terminal. If port 8080 is unavailable, start it on another port and forward that port instead.

The UI shows configured tiers and live available seats, accepts quantities and all current pricing inputs, and refreshes inventory after a successful booking. Validation failures are shown above the bill.

## Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build
```

The build creates the `cinema_booking` library, the `cinema_booking_tests` executable, and the `cinema_server` UI server.

## Run Tests

Run the complete registered test suite:

```bash
ctest --test-dir build --output-on-failure
```

The test executable can also be run directly:

```bash
./build/cinema_booking_tests
```

## Sample Booking

There is no separate sample executable in the current CMake project. The test executable runs a representative multi-tier booking, including bill formatting. The equivalent API usage is:

```cpp
#include "cinema.hpp"
#include <iostream>

int main() {
	CinemaCounter counter({
		{"Silver", 15000, 5},
		{"Gold", 25000, 2},
	});

	const auto receipt = counter.book(
		{{"Silver", 2}, {"Gold", 1}},
		{5000, true, 1000, 10000, 1000, 1800});

	std::cout << formatBill(receipt);
}
```

The positional `DiscountOptions` values are:

```text
festival discount paise, member flag, member rate in basis points,
member cap paise, fee per ticket paise, GST rate in basis points
```

To run the repository's existing representative booking, build first and run:

```bash
./build/cinema_booking_tests
```

## Pricing Order

For a booking with one or more ticket lines:

1. Calculate each line subtotal: `quantity * unit price`.
2. Sum line subtotals into the base ticket total.
3. Apply the flat festival discount once, limited to the base total.
4. If the customer is a member, calculate the percentage discount on the post-festival amount, round it, apply the cap, and prevent it from exceeding the remaining ticket amount.
5. Calculate the convenience fee as `total ticket count * fee per ticket`.
6. Set the taxable amount to `discounted ticket amount + convenience fee`.
7. Calculate and round GST on the taxable amount.
8. Add GST to the taxable amount for the final payable amount.

GST is currently configurable and is applied to the discounted ticket amount plus the convenience fee.

## Money and Rounding

All monetary values use `std::int64_t` paise through the `Paise` alias. For example, `Rs.250.75` is stored as `25075`.

Percentage rates use basis points: `1000` means 10% and `1800` means 18%. Percentage calculations use round-half-up to the nearest paisa:

```cpp
(amountPaise * basisPoints + 5000) / 10000
```

The multiplication uses `__int128` intermediates to reduce overflow risk. Money is formatted as `Rs.major.minor` only when `formatBill` creates output.

## Validation and Inventory

The counter rejects empty tier names, duplicate tier names, negative prices, negative seat counts, unknown ticket tiers, sold-out tiers, insufficient seats, and zero or negative quantities. Discount, fee, and tax inputs must also be non-negative, with percentage rates between 0% and 100%.

Repeated lines for the same tier are validated against their combined quantity. A failed booking does not consume seats: all ticket, discount, fee, tax, and overflow checks complete before inventory is decremented.

## Example Bill

For two Silver tickets and one Gold ticket with a Rs.50 festival discount, a 10% member discount, a Rs.10 fee per ticket, and 18% GST:

```text
Cinema Bill
Silver x 2 @ Rs.150.00 = Rs.300.00
Gold x 1 @ Rs.250.00 = Rs.250.00
Base ticket total: Rs.550.00
Festival discount: -Rs.50.00
Member discount: -Rs.50.00
Convenience fee: Rs.30.00
Taxable amount: Rs.480.00
GST: Rs.86.40
Final payable amount: Rs.566.40
```

## Troubleshooting

- **CMake cannot find a compiler:** install a C++17 compiler and verify `g++ --version` or `c++ --version`.
- **CMake is missing:** install CMake 3.16 or newer and verify `cmake --version`.
- **Tests appear stale:** remove the build directory and configure again with `rm -rf build && cmake -S . -B build`.
- **A test fails:** rebuild with `cmake --build build`, then rerun `ctest --test-dir build --output-on-failure` to see the failing test output.
- **A booking is rejected:** check the tier name, requested quantity, available seats, and the non-negative pricing configuration values.
- **A total looks wrong:** inspect paise inputs and basis points. For example, `15000` is Rs.150.00 and `1800` is 18%, not 1800%.

See [REASONING.md](REASONING.md) for the engineering decisions behind the implementation.