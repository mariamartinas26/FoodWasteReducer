# FoodWasteReducer

A TCP-based, concurrent client–server application that reduces food waste by connecting stores/restaurants with charities or individuals in real time, facilitating donations of unsold food.

✨ Features

Listings & Matching: Donors post surplus food; seekers search/filter by category, location, expiry time.

Reservation/Claim Flow: Prevents double-booking using server-side synchronization.

User Roles: Donor (store/restaurant) and Seeker (charity/individual).

Basic Auth: Account creation/login.

Data Store: SQLite

Server: Accepts TCP connections, spawns a worker (thread/async task) per client, manages shared state (listings, reservations).

Clients: Lightweight apps for donors/seekers.

Concurrency: Thread pool + synchronized sections for critical operations.

🧰 Tech Stack

Languages: C

Networking: TCP sockets 

DB: SQLite

🔐 Security (baseline)

Passwords: Hashed before storage.
