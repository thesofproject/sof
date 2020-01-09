---
name: Bug report
about: Create a report to help us improve
title: "[BUG]"
labels: bug
assignees: ''

---

**Describe the bug**
A clear and concise description of what the bug is.
What have you tried to diagnose or workaround this issue?
Please also read https://thesofproject.github.io/latest/contribute/process/bug-tracking.html for further information on submitting bugs.

**To Reproduce**
Steps to reproduce the behavior: (e.g. list commands or actions used to reproduce the bug)

**Reproduction Rate**
How often does the issue happen ? i.e. 1/10 (once in ten attempts), 1/1000 or all the time.
Does the reproduction rate vary with any other configuration or user action, if so please describe and show the new reproduction rate.

**Expected behavior**
A clear and concise description of what you expected to happen.

**Impact**
What impact does this issue have on your progress (e.g., annoyance, showstopper)

**Environment**
1) Branch name and commit hash of the 2 repositories: sof (firmware/topology) and linux (kernel driver).
    * Kernel: {SHA}
    * SOF: {SHA}
2) Name of the topology file
    * Topology: {FILE}
3) Name of the platform(s) on which the bug is observed.
    * Platform: {PLATFORM}

**Screenshots or console output**
If applicable, add a screenshot (drag-and-drop an image), or console logs
(cut-and-paste text and put a code fence (\`\`\`) before and after, to help
explain the issue.

Please also include the relevant sections from the firmware log and kernel log in the report (and attach the full logs for complete reference). Kernel log is taken from *dmesg* and firmware log from *sof-logger*. See https://thesofproject.github.io/latest/developer_guides/debugability/logger/index.html
