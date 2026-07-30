# Backlog

This directory turns SDS-08 into machine-readable project planning artifacts.

- `backlog.yml` is the source list of planned issues.
- `issues/` can be generated from `backlog.yml` as one Markdown file per issue.
- `.github/ISSUE_TEMPLATE/` contains the user-facing issue forms.
- `.github/labels.yml` defines the project label taxonomy.
- `.github/milestones.yml` defines the suggested milestone set.
- `tools/export-backlog-issues.ps1` exports `backlog.yml` into reviewable
  GitHub issue Markdown files.

The backlog follows the SDS-08 priority model:

- `Must-have` targets v1.0.
- `Should-have` targets v1.x.
- `Nice-to-have` targets v2.0+.

Suggested GitHub milestones:

- `v1.0`
- `v1.x`
- `v2.0+`

To regenerate issue files locally:

```powershell
pwsh -File tools/export-backlog-issues.ps1
```

The generated files are useful for reviewing scope, manually creating GitHub
issues, or feeding a separate GitHub automation step.

The current generated set contains 59 issue files, matching the SDS-08 task
count.
