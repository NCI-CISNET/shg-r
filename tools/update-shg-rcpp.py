# This script is used to copy src files from shg-cli to shg-rcpp and update its DESCRIPTION file
# with the most recent tag, commit hash and src hash.
# Assumes that the shg-cli directory is present in the parent directory

import subprocess
import shutil
import hashlib
from pathlib import Path

source_dir = Path('../smoking-history-generator/src')
target_dir = Path('src')

def md5_update_from_dir(directory, hash):
    assert Path(directory).is_dir()
    for path in sorted(Path(directory).iterdir(), key=lambda p: str(p).lower()):
        hash.update(path.name.encode())
        if path.is_file():
            with open(path, "rb") as f:
                for chunk in iter(lambda: f.read(4096), b""):
                    hash.update(chunk)
        elif path.is_dir():
            hash = md5_update_from_dir(path, hash)
    return hash


def md5_dir(directory):
    return md5_update_from_dir(directory, hashlib.md5()).hexdigest()

# Step 1: copy the src files from shg-cli to shg-rcpp
shutil.copytree(source_dir, target_dir, dirs_exist_ok=True)


# Step 2: update the DESCRIPTION file as necessary

# Get the current HEAD commit value
commit_hash = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD'], cwd=source_dir).strip().decode('utf-8')

# Run the git command to get the current tag (if exists)
result = subprocess.run(['git', 'describe', '--tags', '--abbrev=0'], cwd=source_dir, capture_output=True, text=True)
if result.returncode == 0:
    recent_tag = result.stdout.strip()
else:
    recent_tag = "No recent tag found"

src_hash = md5_dir(target_dir)

# Read DESCRIPTION file in rcpp-shg
with open('DESCRIPTION', 'r') as file:
    lines = file.readlines()

# Replace appropriate values in DESCRIPTION
with open('DESCRIPTION', 'w') as file:
    for line in lines:
        if line.startswith('SHGMostRecentTag: '):
            file.write(f'SHGMostRecentTag: {recent_tag}\n')
        elif line.startswith('SHGCommitHash: '):
            file.write(f'SHGCommitHash: {commit_hash}\n')
        elif line.startswith('SHGsrcHash: '):
            file.write(f'SHGsrcHash: {src_hash}\n')
        else:
            file.write(line)

