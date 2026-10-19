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

Milestones: M0 Foundation (Wed 21 Oct), M1 PC playable and M2 Video on board (Sun 1 Nov), M3 Hardware integration and M4 Sound (Sun 8 Nov), M5 Verification and release 1.0 (Sun 15 Nov 2026).

## 2. For a repository admin (once)

These settings put AGENTS.md section 7 into force. Set them under **Settings**:

**General → Pull Requests**
- Allow squash merging only; turn off merge commits and rebase merging.
- Turn on "Automatically delete head branches".

**Branches → add a branch protection rule (or ruleset) for `main`**
- Require a pull request before merging, with **1 approval**.
- Require review from **Code Owners**, so frozen-interface paths need all three (`.github/CODEOWNERS`).
- Dismiss stale approvals when new commits are pushed.
- Require conversation resolution before merging.
- Require status checks to pass: add the CI jobs once issue #2 has merged the workflow.
- Block force pushes and deletions.
- Do not allow bypassing the rules.
