---
name: Bug report
about: Create a report to help us improve
title: "[BUG]"
labels: bug
assignees: michalgrodzicki

---

**Describe the bug**
A clear and concise description of what the bug is.
What have you tried to diagnose or workaround this issue?
Please also read https://thesofproject.github.io/latest/howtos/process/bug-tracking.html for further information on submitting bugs.

**To Reproduce**
Steps to reproduce the behavior: (e.g. list commands or actions used to reproduce the bug) 

**Expected behavior**
A clear and concise description of what you expected to happen.

**Impact**
What impact does this issue have on your progress (e.g., annoyance, showstopper)

**Environment**
1) Branch name and commit hash of 3 repositories: sof (firmware), linux (kernel driver) and soft (tools & topology).
2) Name of the topology file
3) Name of the platform(s) on which the bug is observed.
4) Reproducibility Rate. If you can only reproduce it randomly, it’s useful to report how many times the bug has been reproduced vs. the number of attempts it’s taken to reproduce the bug.

**Screenshots or console output**
If applicable, add a screenshot (drag-and-drop an image), or console logs
(cut-and-paste text and put a code fence (\`\`\`) before and after, to help
explain the issue.

Please also include the relevant sections from the firmware log and kernel log in the report (and attach the full logs for complete reference). Kernel log is taken from *dmesg* and firmware log from *sof-logger*. See https://thesofproject.github.io/latest/developer_guides/debugability/logger/index.html
