# Agent Guidelines for shg-rcpp

## Git Workflow

### Pushes
- **ALL PUSHes MUST be proposed to the developer** - Never push directly to the repository without explicit approval
- Always propose pushes and wait for developer confirmation before executing

### Commits
- **Commits must be made one at a time** - Each commit should be proposed separately so the developer can review and approve before proceeding
- Do not batch multiple commits together
- Wait for approval after each commit before making the next one

### Code Quality
- **All changes must pass through the linter immediately** - Run the linter on any modified files before committing
- Fix any linter errors before proceeding with commits
- Do not commit code that fails linting checks

## Shared Files with shg-cli

The following `src/` files **MUST match shg-cli exactly**:
- `mersenne_class.cpp`, `mersenne_class.h`
- `rng_strategy.h`
- `RngStream.cpp`, `RngStream.h`
- `sim_exception.cpp`, `sim_exception.h`
- `smoking_sim.cpp`, `smoking_sim.h`
- `version.h`

**Rcpp-only files:** `wrapper.cpp`, `wrapper.h`, `RcppExports.cpp`

**DO NOT modify shared files in shg-rcpp** without first updating shg-cli. The CLI is the source of truth for shared simulation code.

## Sync Script

Use `tools/shg-sync.py` to manage synchronization:

```bash
python tools/shg-sync.py check           # Check if files match
python tools/shg-sync.py sync-to-rcpp    # Copy CLI → Rcpp (standard)
python tools/shg-sync.py sync-to-cli     # Copy Rcpp → CLI (dev only!)
python tools/shg-sync.py update-description  # Update DESCRIPTION sync fields
python tools/shg-sync.py validate        # Pre-release validation
```

## Version Management

Two separate version numbers:
- **R package version:** `DESCRIPTION` → `Version` (e.g., 0.0.3)
- **Core engine version:** `src/version.h` → `SHG_CORE_VERSION`

Also tracks CLI sync state in `DESCRIPTION`:
- `SHGMostRecentTag` - Which CLI tag core code matches
- `SHGCommitHash` - CLI commit hash
- `SHGsrcHash` - MD5 hash of shared files

### When to Bump Versions

| Change Type | DESCRIPTION Version | version.h |
|-------------|---------------------|-----------|
| Wrapper-only change | Bump | No change |
| CLI sync (shared code) | Bump | Update to match CLI |
| New R features | Bump | Depends |

## Release Checklist

1. Run `python tools/shg-sync.py validate` - ensure all checks pass
2. Update `src/version.h` to match CLI (if syncing)
3. Update `DESCRIPTION`:
   - Bump `Version` field
   - Run `python tools/shg-sync.py update-description`
4. Run `R CMD check .` - must pass
5. Create PR, wait for CI
6. Merge to master
7. Create git tag
8. Create GitHub release noting CLI version compatibility

