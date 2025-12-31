#!/usr/bin/env python3
"""
SHG Sync Tool - Synchronize shared files between shg-cli and shg-rcpp

Usage:
    python shg-sync.py check              # Compare shared files, report differences
    python shg-sync.py sync-to-rcpp       # Copy shared files: CLI → Rcpp
    python shg-sync.py sync-to-cli        # Copy shared files: Rcpp → CLI (dev only)
    python shg-sync.py update-description # Update DESCRIPTION sync fields
    python shg-sync.py validate           # Pre-release validation

Assumes shg-cli and shg-rcpp are sibling directories.
"""

import subprocess
import shutil
import hashlib
import argparse
import sys
from pathlib import Path

# Shared files that must be identical between CLI and Rcpp
SHARED_FILES = [
    "mersenne_class.cpp",
    "mersenne_class.h",
    "rng_strategy.h",
    "RngStream.cpp",
    "RngStream.h",
    "sim_exception.cpp",
    "sim_exception.h",
    "smoking_sim.cpp",
    "smoking_sim.h",
    "version.h",
]

# Determine paths relative to this script
SCRIPT_DIR = Path(__file__).parent.resolve()
RCPP_ROOT = SCRIPT_DIR.parent
CLI_ROOT = RCPP_ROOT.parent / "shg-cli"

CLI_SRC = CLI_ROOT / "src"
RCPP_SRC = RCPP_ROOT / "src"


def md5_file(filepath):
    """Calculate MD5 hash of a file."""
    hash_md5 = hashlib.md5()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()


def md5_shared_files(src_dir):
    """Calculate MD5 hash of all shared files in a directory."""
    hash_md5 = hashlib.md5()
    for filename in sorted(SHARED_FILES):
        filepath = src_dir / filename
        if filepath.exists():
            hash_md5.update(filename.encode())
            with open(filepath, "rb") as f:
                for chunk in iter(lambda: f.read(4096), b""):
                    hash_md5.update(chunk)
    return hash_md5.hexdigest()


def files_match(file1, file2):
    """Check if two files have identical content."""
    if not file1.exists() or not file2.exists():
        return False
    return md5_file(file1) == md5_file(file2)


def get_version_from_header(src_dir):
    """Extract SHG_CORE_VERSION from version.h"""
    version_h = src_dir / "version.h"
    if not version_h.exists():
        return None
    with open(version_h, "r") as f:
        for line in f:
            if "SHG_CORE_VERSION" in line and "#define" in line:
                # Extract version string from: #define SHG_CORE_VERSION "6.4.3"
                parts = line.split('"')
                if len(parts) >= 2:
                    return parts[1]
    return None


def cmd_check():
    """Compare shared files between repos, report differences."""
    print("Checking shared files between CLI and Rcpp...\n")
    
    if not CLI_SRC.exists():
        print(f"ERROR: CLI src directory not found: {CLI_SRC}")
        return 1
    if not RCPP_SRC.exists():
        print(f"ERROR: Rcpp src directory not found: {RCPP_SRC}")
        return 1
    
    mismatches = []
    missing_cli = []
    missing_rcpp = []
    
    for filename in SHARED_FILES:
        cli_file = CLI_SRC / filename
        rcpp_file = RCPP_SRC / filename
        
        if not cli_file.exists():
            missing_cli.append(filename)
        elif not rcpp_file.exists():
            missing_rcpp.append(filename)
        elif not files_match(cli_file, rcpp_file):
            mismatches.append(filename)
    
    if missing_cli:
        print("Missing in CLI:")
        for f in missing_cli:
            print(f"  - {f}")
        print()
    
    if missing_rcpp:
        print("Missing in Rcpp:")
        for f in missing_rcpp:
            print(f"  - {f}")
        print()
    
    if mismatches:
        print("Files with differences:")
        for f in mismatches:
            print(f"  - {f}")
        print()
    
    if not mismatches and not missing_cli and not missing_rcpp:
        print("✓ All shared files are in sync!")
        
        # Show versions
        cli_ver = get_version_from_header(CLI_SRC)
        rcpp_ver = get_version_from_header(RCPP_SRC)
        print(f"\nCLI version:  {cli_ver or 'not found'}")
        print(f"Rcpp version: {rcpp_ver or 'not found'}")
        
        if cli_ver and rcpp_ver and cli_ver == rcpp_ver:
            print("✓ Versions match!")
        elif cli_ver != rcpp_ver:
            print("⚠ Version mismatch!")
        
        return 0
    
    return 1


def cmd_sync_to_rcpp():
    """Copy shared files from CLI to Rcpp."""
    print("Syncing shared files: CLI → Rcpp\n")
    
    if not CLI_SRC.exists():
        print(f"ERROR: CLI src directory not found: {CLI_SRC}")
        return 1
    
    for filename in SHARED_FILES:
        cli_file = CLI_SRC / filename
        rcpp_file = RCPP_SRC / filename
        
        if cli_file.exists():
            shutil.copy2(cli_file, rcpp_file)
            print(f"  Copied: {filename}")
        else:
            print(f"  SKIP (not in CLI): {filename}")
    
    print("\n✓ Sync complete. Run 'R CMD check' to verify build.")
    return 0


def cmd_sync_to_cli():
    """Copy shared files from Rcpp to CLI (reverse sync for development)."""
    print("=" * 60)
    print("WARNING: This syncs Rcpp → CLI (reverse direction)")
    print("Only use during development when changes originate in Rcpp")
    print("=" * 60)
    
    response = input("\nContinue? [y/N] ").strip().lower()
    if response != 'y':
        print("Aborted.")
        return 0
    
    print("\nSyncing shared files: Rcpp → CLI\n")
    
    for filename in SHARED_FILES:
        cli_file = CLI_SRC / filename
        rcpp_file = RCPP_SRC / filename
        
        if rcpp_file.exists():
            shutil.copy2(rcpp_file, cli_file)
            print(f"  Copied: {filename}")
        else:
            print(f"  SKIP (not in Rcpp): {filename}")
    
    print("\n✓ Sync complete. Build CLI to verify: make clean && make")
    return 0


def cmd_update_description():
    """Update DESCRIPTION file with CLI sync information."""
    print("Updating DESCRIPTION with CLI sync info...\n")
    
    # Get CLI commit hash
    try:
        commit_hash = subprocess.check_output(
            ['git', 'rev-parse', '--short', 'HEAD'],
            cwd=CLI_ROOT
        ).strip().decode('utf-8')
    except subprocess.CalledProcessError:
        commit_hash = "unknown"
    
    # Get CLI tag
    try:
        result = subprocess.run(
            ['git', 'describe', '--tags', '--abbrev=0'],
            cwd=CLI_ROOT,
            capture_output=True,
            text=True
        )
        recent_tag = result.stdout.strip() if result.returncode == 0 else "unknown"
    except Exception:
        recent_tag = "unknown"
    
    # Calculate hash of shared files from CLI
    src_hash = md5_shared_files(CLI_SRC)
    
    # Update DESCRIPTION
    description_path = RCPP_ROOT / "DESCRIPTION"
    with open(description_path, 'r') as f:
        lines = f.readlines()
    
    with open(description_path, 'w') as f:
        for line in lines:
            if line.startswith('SHGMostRecentTag:'):
                f.write(f'SHGMostRecentTag: {recent_tag}\n')
            elif line.startswith('SHGCommitHash:'):
                f.write(f'SHGCommitHash: {commit_hash}\n')
            elif line.startswith('SHGsrcHash:'):
                f.write(f'SHGsrcHash: {src_hash}\n')
            else:
                f.write(line)
    
    print(f"  SHGMostRecentTag: {recent_tag}")
    print(f"  SHGCommitHash: {commit_hash}")
    print(f"  SHGsrcHash: {src_hash}")
    print("\n✓ DESCRIPTION updated.")
    return 0


def cmd_validate():
    """Pre-release validation checks."""
    print("Running pre-release validation...\n")
    
    errors = []
    warnings = []
    
    # 1. Check shared files are in sync
    print("1. Checking shared files...")
    for filename in SHARED_FILES:
        cli_file = CLI_SRC / filename
        rcpp_file = RCPP_SRC / filename
        
        if not cli_file.exists():
            errors.append(f"Missing in CLI: {filename}")
        elif not rcpp_file.exists():
            errors.append(f"Missing in Rcpp: {filename}")
        elif not files_match(cli_file, rcpp_file):
            errors.append(f"Out of sync: {filename}")
    
    if not any("Out of sync" in e or "Missing" in e for e in errors):
        print("   ✓ All shared files in sync")
    
    # 2. Check versions match
    print("2. Checking versions...")
    cli_ver = get_version_from_header(CLI_SRC)
    rcpp_ver = get_version_from_header(RCPP_SRC)
    
    if cli_ver and rcpp_ver and cli_ver == rcpp_ver:
        print(f"   ✓ Versions match: {cli_ver}")
    else:
        errors.append(f"Version mismatch: CLI={cli_ver}, Rcpp={rcpp_ver}")
    
    # 3. Check for uncommitted changes
    print("3. Checking for uncommitted changes...")
    for name, root in [("CLI", CLI_ROOT), ("Rcpp", RCPP_ROOT)]:
        try:
            result = subprocess.run(
                ['git', 'status', '--porcelain'],
                cwd=root,
                capture_output=True,
                text=True
            )
            if result.stdout.strip():
                warnings.append(f"{name} has uncommitted changes")
            else:
                print(f"   ✓ {name} working directory clean")
        except Exception:
            warnings.append(f"Could not check {name} git status")
    
    # Summary
    print("\n" + "=" * 40)
    if errors:
        print("ERRORS (must fix before release):")
        for e in errors:
            print(f"  ✗ {e}")
    if warnings:
        print("WARNINGS:")
        for w in warnings:
            print(f"  ⚠ {w}")
    if not errors and not warnings:
        print("✓ All validation checks passed!")
    print("=" * 40)
    
    return 1 if errors else 0


def main():
    parser = argparse.ArgumentParser(
        description="SHG Sync Tool - Synchronize shared files between shg-cli and shg-rcpp"
    )
    parser.add_argument(
        "command",
        choices=["check", "sync-to-rcpp", "sync-to-cli", "update-description", "validate"],
        help="Command to run"
    )
    
    args = parser.parse_args()
    
    commands = {
        "check": cmd_check,
        "sync-to-rcpp": cmd_sync_to_rcpp,
        "sync-to-cli": cmd_sync_to_cli,
        "update-description": cmd_update_description,
        "validate": cmd_validate,
    }
    
    return commands[args.command]()


if __name__ == "__main__":
    sys.exit(main())

