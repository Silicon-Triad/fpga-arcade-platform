# Git and GitHub setup

## 1. For everyone

1. Accept the invitation to `Silicon-Triad/fpga-arcade-platform`.
2. Set your name and e-mail: `git config --global user.name "..."` and `git config --global user.email "..."`.
3. Read `AGENTS.md` section 7 (branches, PRs, reviews).
4. Watch the issues assigned to you. Each has an owner label, a milestone and its SRS IDs.

| Labels | Meaning |
|---|---|
| `area:*` | Part of the system |
| `type:*` | feature, test, bug, docs, chore |
| `prio:shall` / `prio:should` | Mandatory / desirable, from the SRS wording |
| `owner:*` | Owner (also the assignee) |
| `frozen-interface` | Needs all three approvals (AGENTS.md section 6) |
| `needs-board` | Cannot be finished without the DE1-SoC |
| `agent-ok` | Scoped tightly enough to hand to an AI agent |
| `blocked` | Waiting on something else |
| `hw-milestone` | PR closes M2 or M4; CI then requires Quartus reports in `hw/reports/` |

Milestones: M0 Foundation (Wed 21 Oct), M1 PC playable and M2 Video on board (Sun 1 Nov), M3 Hardware integration and M4 Sound (Sun 8 Nov), M5 Verification and release 1.0 (Sun 15 Nov 2026).

## 2. For a repository admin (once)

These settings put AGENTS.md section 7 into force.

**Settings → General → Pull Requests**
- Allow squash merging: on. Allow merge commits: off. Allow rebase merging: off.
- Default squash commit message: "Pull request title and description".
- Automatically delete head branches: on.
- Always suggest updating pull request branches: on.

**Settings → Rules → Rulesets → New branch ruleset**
- Name `main`, enforcement **Active**, target: the default branch.
- Bypass list: empty, or only an admin with "for pull requests only" as an emergency exit.
- Restrict deletions: on.
- Block force pushes: on.
- Require linear history: on (works with squash-only merging).
- Require a pull request before merging: on, with:
  - Required approvals: **1**
  - Dismiss stale pull request approvals when new commits are pushed: on
  - Require approval of the most recent reviewable push: on
  - Require conversation resolution before merging: on
  - Require review from Code Owners: **off**
- Require status checks to pass: on, with:
  - `sw (build, tests, coverage, cross build, lint)`
  - `hw (Verilator and Verible lint, Verilator testbenches)`
  - `checks (QuestaSim logs, Quartus reports)`
  - Require branches to be up to date before merging: on

**Why "Require review from Code Owners" is off.** Each area has one owner in `.github/CODEOWNERS`, and GitHub never lets authors approve their own PRs. With the setting on, an owner's PR in their own area could never merge. CODEOWNERS still requests the right reviewers automatically; R-7.3.1 (the area owner reviews, or another member if the author is the owner) is kept by the team.

**"All three approve" is not enforced by GitHub.** When a CODEOWNERS line lists several people, one approval from any of them is enough. The all-three rule for frozen interfaces (AGENTS.md R-6.3) is kept by the team: reviewers do not merge a `frozen-interface` PR until both non-authors have approved.
