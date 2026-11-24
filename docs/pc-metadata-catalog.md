# PC metadata structural catalog

Version 0.420.0 extends a read-only catalog for native PC metadata records:

```text
rb4pc-builder <hdr-or-directory> --catalog-pc-metadata <catalog.json>
```

Recognized families are `.rbsong`, `.songdta_pc`, `.moggsong_dta_pc`, `.csv_pc`,
and `.dta`. For each family, schema version 3 records aggregate counts, bytes,
minimum and maximum sizes, empty and unreadable records, first-16-byte
signatures, first little-endian 32-bit values, and bounded ASCII-run statistics
from at most the first 256 bytes.

The report never stores internal record paths or the printable strings it finds.
ASCII data is represented only as counts, null-termination counts, maximum
length, and a length distribution. The operation does not extract, deserialize,
convert, or rewrite metadata payloads.
Schema version 3 profiles each complete aligned 32-bit word in the first 64
bytes. It reports observations, distinct values, zeros, range, and correlations
with full or remaining record size. Candidate lists identify constant and
size-correlated offsets without assigning unverified semantic names.

Schema version 3 also records privacy-preserving HDR cohort coverage for each
family: the number of valid headers where it appears and the minimum and maximum
records contributed by one header. Header filenames and internal record paths
are never included.
This milestone establishes the structural baseline needed before assigning
semantics to song declarations, object references, localization tables, or mix
metadata. The next real milestone should compare stable fields across multiple
user-owned records and then integrate this catalog into the combined PC corpus
audit and Builder GUI.