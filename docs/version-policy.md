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