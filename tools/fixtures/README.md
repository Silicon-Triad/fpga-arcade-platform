# `tools/fixtures/`

Test inputs for the repo check scripts. Not built, not linted.

- `includes_bad/`: violates QA-01 on purpose. `check_includes.py` must report exactly the violations listed in its `EXPECTED_BAD`; it checks this on every run.
- `includes_good/`: allowed includes only. `check_includes.py` must report nothing.
