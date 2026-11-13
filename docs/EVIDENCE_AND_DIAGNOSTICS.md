# Evidence and diagnostics — v1.6.0

The evidence engine keeps observed identity evidence separate from compatibility claims. It scores evidence categories for triage and identifies conflicting values. The score is not a probability, certification, or compatibility rating.

Evidence types include PnP hardware IDs, revision, manufacturer/product/serial strings, Container ID, parent ID, HID descriptor observations, parsed usages, trusted catalog records, vendor documentation and physical test records. A trusted catalog record only counts as corroborated evidence when it is independently verified and has a source.

Conflicts force `MANUAL_REVIEW_REQUIRED`. Without an exact corroborated record, results remain partial or detected-only. No evidence score enables a hardware write.

The diagnostic engine emits deterministic findings for missing HID interfaces, unreadable/invalid descriptors, unclassified usages, absent report fields, identity conflicts and missing/unverified adapters. Recommendations are read-only and do not send commands.
