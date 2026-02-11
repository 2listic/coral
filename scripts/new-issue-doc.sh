#!/bin/bash
# Creates a new issue planning document from the current git branch
#
# Usage: ./scripts/new-issue-doc.sh ISSUE_NUMBER
#
# Example:
#   git checkout -b 5-reference-handling
#   ./scripts/new-issue-doc.sh 5
#   # Creates: docs/issues/005-reference-handling.md

set -e

# Check if issue number provided
if [ -z "$1" ]; then
  echo "Error: Issue number required"
  echo "Usage: $0 ISSUE_NUMBER"
  echo ""
  echo "Example:"
  echo "  git checkout -b 5-reference-handling"
  echo "  $0 5"
  exit 1
fi

ISSUE_NUM=$1

# Validate issue number is numeric
if ! [[ "$ISSUE_NUM" =~ ^[0-9]+$ ]]; then
  echo "Error: Issue number must be numeric"
  exit 1
fi

# Get current branch name
BRANCH=$(git branch --show-current)

if [ -z "$BRANCH" ]; then
  echo "Error: Could not determine current git branch"
  exit 1
fi

# Extract issue name from branch
# Assumes format: N-issue-name or NN-issue-name or NNN-issue-name
if [[ "$BRANCH" =~ ^[0-9]+-(.+)$ ]]; then
  ISSUE_NAME="${BASH_REMATCH[1]}"
else
  echo "Error: Branch name doesn't follow expected format 'N-issue-name'"
  echo "Current branch: $BRANCH"
  echo "Expected format: 5-reference-handling"
  exit 1
fi

# Pad issue number (e.g., 5 → 005)
PADDED=$(printf "%03d" "$ISSUE_NUM")

# Construct filename
FILENAME="${PADDED}-${ISSUE_NAME}.md"
FILEPATH="issues/${FILENAME}"

# Check if file already exists
if [ -f "$FILEPATH" ]; then
  echo "Error: File already exists: $FILEPATH"
  exit 1
fi

# Get current date
DATE=$(date +%Y-%m-%d)

# Create issue title (convert kebab-case to Title Case)
ISSUE_TITLE=$(echo "$ISSUE_NAME" | sed 's/-/ /g' | sed 's/\b\(.\)/\u\1/g')

# Create the file with template
cat > "$FILEPATH" <<EOT
# Issue #${ISSUE_NUM}: ${ISSUE_TITLE}

**Status:** In Progress
**Branch:** \`${BRANCH}\`
**Issue:** #${ISSUE_NUM}

---

## Description

[Write problem statement and motivation]

---

## Implementation Plan

- [ ] Step 1: ...
  - [ ] Task 1.1: ...
  - [ ] **CHECKPOINT:** Build + run tests (verify nothing broke)

- [ ] Step 2: ...
  - [ ] Task 2.1: ...
  - [ ] **CHECKPOINT:** Build + run tests

---

## Implementation Log

### ${DATE}
- Started planning

**Design Decision:** [Title]
- Question: [What needed deciding?]
- Decision: [What was chosen?]
- Rationale: [Why?]
- Location: [Where in code?]

---

## Outcome

[To be filled when completed]
EOT

echo "Created: $FILEPATH"
echo ""
echo "Next steps:"
echo "  1. Edit $FILEPATH and fill in Description and Implementation Plan"
echo "  2. Commit: git add $FILEPATH && git commit -m 'docs: Add issue #${ISSUE_NUM} planning doc'"
echo "  3. Update issue #${ISSUE_NUM} to link this file"
