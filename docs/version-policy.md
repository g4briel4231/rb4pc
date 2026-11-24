# Version milestone policy

RB4PC versions advance only when a concrete, testable capability is complete.
A target version is a planning boundary, not permission to bump numbers without
finishing the associated work.

When an objective is larger than expected, it is divided into real intermediate
milestones. Each milestone must have:

- a specific technical outcome;
- automated or reproducible validation proportional to its risk;
- honest documentation of supported and unsupported behavior;
- no proprietary game content in source, tests, packages, or reports.

The final target is reached only after the intermediate capabilities integrate
and pass Debug and Release validation. Local packages may be generated for
verification, but GitHub Releases are not published unless the user explicitly
requests one.

For the v0.200.0 to v0.300.0 cycle, the real milestones were:

- v0.220.0: functional PC asset catalog in the GUI;
- v0.240.0: functional PC audio audit in the GUI;
- v0.270.0: atomic combined PC corpus audit in the core and CLI;
- v0.300.0: combined audit workflow in the GUI and release packaging.

For the v0.320.0 to v0.400.0 cycle, the real milestones were:

- v0.320.0: dedicated native PC metadata structure catalog;
- v0.340.0: metadata integrated into the transactional corpus audit;
- v0.360.0: functional PC metadata catalog in the GUI;
- v0.380.0: aligned metadata field-candidate profiles;
- v0.400.0: cross-catalog HDR consistency contract, unified reporting, and release validation.

For the v0.420.0 to v0.500.0 cycle, the real milestones were:

- v0.420.0: privacy-preserving metadata family coverage per HDR cohort;
- v0.440.0: aligned-word stability measured within and across HDR sets;
- v0.460.0: mutually exclusive structural evidence classes;
- v0.480.0: evidence completeness integrated into the catalog, audit, CLI, and GUI;
- v0.500.0: evidence completeness enforced by the corpus-audit completion contract.

For the v0.520.0 to v0.600.0 cycle, the real milestones were:

- v0.520.0: strict in-record and aligned bounded-offset candidates;
- v0.540.0: prefix-versus-payload target-region measurements;
- v0.560.0: mutually exclusive bounded target-region classes;
- v0.580.0: relation totals and integrity status in the catalog, audit, CLI, and GUI;
- v0.600.0: relation integrity enforced by the corpus-audit completion contract.
