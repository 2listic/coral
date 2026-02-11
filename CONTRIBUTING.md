# Contributing to Coral


## Development Workflow

### Working on Issues

1. **Create issue** on your issue tracker with title and short description
   - Issue tracker assigns issue number (e.g., #5)

2. **Create planning document** `issues/NNN-issue-name.md`
   - Use the issue number from step 1
   - Fill in Description section (detailed requirements)
   - Fill in Implementation Plan (steps with checkboxes)

3. **Update issue** to link the planning document
   - Add reference to `issues/005-issue-name.md` in issue description

4. **Create feature branch** following naming convention
   ```bash
   git checkout -b 5-issue-name
   # or: N-kebab-case-description
   ```

5. **Implement** following the plan
   - Update Implementation Log in the planning doc as you work
   - Document decisions, challenges, and solutions
   - Commit regularly with clear messages

6. **Open Pull Request**
   - Link to the issue (`Fixes #5`)
   - Reference the planning doc
   - Include summary of changes

---

## Planning Document Structure

Each issue has a corresponding markdown file in `issues/`:

**Filename:** `NNN-kebab-case-description.md` (e.g., `005-reference-handling.md`)

**Template:**
```markdown
# Issue #NNN: Title

**Status:** In Progress | Completed | Closed
**Branch:** `N-branch-name`
**Issue:** #NNN

---

## Description

[Detailed requirements, context, motivation - written by issue author]

---

## Implementation Plan

[Step-by-step approach with checkable tasks - written collaboratively]

- [ ] Step 1: Description
  - [ ] Subtask 1.1
  - [ ] Subtask 1.2
- [ ] Step 2: Description
- [ ] Step 3: Description

---

## Implementation Log

[Chronological log of decisions and progress - written during development]

### YYYY-MM-DD
- **Decision:** What was decided
  - **Rationale:** Why this choice
- **Implemented:** What was done
- **Challenge:** What problem arose
  - **Solution:** How it was solved

---

## Outcome

[Final summary when completed - what was achieved]
```

## Questions?

If you have questions about contributing, please open an issue for discussion.
