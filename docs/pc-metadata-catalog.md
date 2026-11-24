# PC metadata structural catalog

Version 0.820.0 extends a read-only catalog for native PC metadata records:

```text
rb4pc-builder <hdr-or-directory> --catalog-pc-metadata <catalog.json>
```

Recognized families are `.rbsong`, `.songdta_pc`, `.moggsong_dta_pc`, `.csv_pc`,
and `.dta`. For each family, schema version 19 records aggregate counts, bytes,
minimum and maximum sizes, empty and unreadable records, first-16-byte
signatures, first little-endian 32-bit values, and bounded ASCII-run statistics
from at most the first 256 bytes.

The report never stores internal record paths or the printable strings it finds.
ASCII data is represented only as counts, null-termination counts, maximum
length, and a length distribution. The operation does not extract, deserialize,
convert, or rewrite metadata payloads.
Schema version 19 profiles each complete aligned 32-bit word in the first 64
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

Version 0.600.0 makes bounded-offset region completeness a required invariant
for a complete combined corpus audit. It still does not dereference candidates
or assign semantic field names.

Schema version 11 measures whether each bounded candidate target falls inside
the 256-byte sample already read by the catalog and whether a complete 32-bit
target word is available. It also counts zero and printable first target bytes.

Schema version 12 adds strict candidate lists for fields whose target byte or
complete target word is present in every sampled observation.

Schema version 13 classifies fully sampled target first bytes as always zero,
always printable ASCII, or other/mixed. These are byte-shape observations only;
the catalog does not store the byte values or strings.

Schema version 14 publishes fully sampled and classified target-field totals
plus an explicit classification completeness invariant. The CLI, GUI, and
combined audit expose that status directly.

Version 0.700.0 rejects impossible sampled-target counters and requires sampled
target classification completeness for a complete combined corpus audit.

Schema version 15 measures fully sampled 32-bit target words: zero values,
strict in-record values, four-byte alignment, and prefix-versus-payload target
regions. Values themselves are never stored in the report.

Schema version 16 adds strict candidate lists where every sampled target word
is itself a nonzero in-record value, with a separate four-byte-aligned list.
These second-hop candidates are not followed or treated as confirmed pointers.

Schema version 17 classifies strict second-hop candidates as targeting the
profiled prefix, later payload, or both regions across observations. These lists
are mutually exclusive and remain structural evidence only.

Schema version 18 publishes bounded and region-classified second-hop field
totals plus an explicit completeness invariant in the catalog, CLI, GUI, and
combined audit.

Version 0.800.0 requires complete second-hop target-region classification
for a complete combined corpus audit. The catalog still does not dereference a
second target or claim that either value is a pointer.

Schema version 19 assigns every profiled field an objective evidence scope:
`singleRecord`, `singleHeader`, or `multiHeader`. Separate offset lists make
it possible to prioritize candidates backed by independent HDR cohorts without
claiming semantic confidence.

This milestone establishes the structural baseline needed before assigning
semantics to song declarations, object references, localization tables, or mix
metadata. The next real milestone should compare stable fields across multiple
user-owned records and then integrate this catalog into the combined PC corpus
audit and Builder GUI.