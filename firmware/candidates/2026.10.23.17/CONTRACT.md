# 2026.10.20.16 — startup synchronization candidate; NOT INSTALLED

Before any user request or telemetry query, listen for complete stock command/response boundaries. Initial partial parser copies time out without suppressing stock bytes or creating a permanent ownership fault. There cannot be an OWN transaction in this phase.

After both observed boundaries and the existing UART idle/drained predicate, send one existing START/END `printh` marker bundle without clicks. No mode, page, bkcmd, GPIO or actuator assignment. No bootstrap retry. Exact nonce/sequence START followed by END establishes the serial boundary. Pre-START responses remain stock; new stock requests queued behind the bundle retain their ledger entries until END.

Only then allow normal read queries. Boot synchronization does not arm controls. Explicit arming and existing mode/page/marker/TTL/ownership gates remain required. Missing, duplicated, malformed or failed bootstrap markers fail closed. Runtime errors and late OWN tombstones keep the existing fail-closed rules. No quiet-time reset of an established ledger.

First ownership reason/time/detail/injected-byte count persists throughout the ESP boot, including a later real panel startup. boot_sync: 0 listening,1 probe,2 synchronized,3 ownership fault. sync_discards counts initial parser-copy timeout/overflow; raw bytes are not discarded. sync_stock_errors counts pre-probe stock error frames.

Raw UART driver, setup, task scheduling, watchdog, supervisor, RX/TX ring and forwarding functions are byte-for-byte unchanged vs .14. Only event calls gain reason tags. This changes the transaction-layer startup behavior; hardware behavior remains unverified until installation under observation.
