# PC metadata structural catalog

Version 0.580.0 extends a read-only catalog for native PC metadata records:

```text
rb4pc-builder <hdr-or-directory> --catalog-pc-metadata <catalog.json>
```

Recognized families are `.rbsong`, `.songdta_pc`, `.moggsong_dta_pc`, `.csv_pc`,
and `.dta`. For each family, schema version 10 records aggregate counts, bytes,
minimum and maximum sizes, empty and unreadable records, first-16-byte
signatures, first little-endian 32-bit values, and bounded ASCII-run statistics
from at most the first 256 bytes.

The report never stores internal record paths or the printable strings it finds.
ASCII data is represented only as counts, null-termination counts, maximum
length, and a length distribution. The operation does not extract, deserialize,
convert, or rewrite metadata payloads.
Schema version 10 profiles each complete aligned 32-bit word in the first 64
bytes. It reports observations, distinct values, zeros, range, and correlations
with full or remaining record size. Candidate lists identify constant and
size-correlated offsets without assigning unverified semantic names.

Schema version 3 also records privacy-preserving HDR cohort coverage for each
family: the number of valid headers where it appears and the minimum and maximum
records contributed by one header. Header filenames and internal record paths
are never included.

Schema version 4 measures each aligned word across HDR cohorts. It reports how
often the word is present, how often it is constant within one HDR, and how many
different per-HDR constants were observed. `crossHeaderConstant` requires complete
family coverage, constancy inside every HDR, and one shared value across HDRs.
Schema version 5 classifies every profiled word as cross-HDR constant,
per-HDR constant with differing values, variable between records, or missing
from part of the HDR cohort. These mutually exclusive evidence classes make
research priorities reproducible while deliberately avoiding semantic labels.
Schema version 6 totals profiled and classified fields and publishes an explicit
`evidenceClassificationComplete` invariant. The CLI, Windows Builder, and
combined corpus audit surface that invariant directly.
Schema version 7 counts nonzero words, values strictly inside their own record,
and four-byte-aligned in-bounds values. `boundedOffset` and
`alignedBoundedOffset` list only offsets where every observation satisfies the
corresponding rule. They are structural research candidates, not decoded
pointers or object references.
Schema version 8 separates in-bounds observations below byte 64 from values
that point into the later payload region. This measures target regions without
dereferencing data or assigning pointer semantics.

Schema version 9 classifies every strict bounded-offset candidate as always
targeting the profiled prefix, always targeting the later payload region, or
varying between those regions. The three lists are mutually exclusive.

Schema version 10 publishes catalog-wide bounded-offset and region-classified
field totals plus an explicit completeness invariant. The CLI, Windows Builder,
and combined audit expose that status directly.

This milestone establishes the structural baseline needed before assigning
semantics to song declarations, object references, localization tables, or mix
metadata. The next real milestone should compare stable fields across multiple
user-owned records and then integrate this catalog into the combined PC corpus
audit and Builder GUI.