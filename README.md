# Metadata_Journaling

In this project, we create a virtual filesystem (VSFS) image file and implement a **metadata journaling** system to ensure filesystem consistency. The project demonstrates how to detect corruption in disk metadata and recover from it using journaling techniques.

## Project Overview

The system consists of four main components that work together:

1. **mkfs.c** - Filesystem initialization and creation
2. **corrupt.c** - Simulates filesystem corruption for testing
3. **validator.c** - Checks filesystem consistency and validates metadata
4. **journal.c** - Implements metadata journaling for crash recovery


### 1. **mkfs.c** - Filesystem Creation:

 Creates a virtual filesystem image with proper structure and metadata initialization.

#The following key mechanisms have been implemented in mkfs.c file:# 

 **Superblock Creation**: Writes a 128-byte superblock containing:
  1. Magic number (FS_MAGIC = 0x56534653)
  2. Block size (4096 bytes)
  3. Total blocks, inode count, and layout information
  4. Pointers to bitmap and inode table locations

 **Block Layout**:
  1. Block 0: Superblock
  2. Blocks 1-16: Journal area (for crash recovery)
  3. Block 17: Inode bitmap (tracks which inodes are in use)
  4. Block 18: Data bitmap (tracks which data blocks are in use)
  5. Blocks 19-20: Inode table (stores inode metadata)
  6. Blocks 21-84: Data area (file and directory content)

 **Inode Structure** (128 bytes each):
  1. Type: 0=free, 1=file, 2=directory
  2. Links: Link count for reference tracking
  3. Size: File/directory size in bytes
  4. Direct[8]: 8 direct block pointers for file data
  5. Timestamps: Creation and modification times

 **Directory Entries** (32 bytes each):
  1. Inode number
  2. Filename (up to 27 characters)

 **Root Directory Initialization**:
  1. Creates root inode (inode 0) as a directory
  2. Marks inode 0 and data block 0 as reserved
  3. Adds "." and ".." entries in root directory

### 2. **corrupt.c** - Corruption Simulator:

Simulates filesystem corruption for testing validation and recovery mechanisms.

**The following key mechanisms have been implemented in mkfs.c file:**

  1. Opens the vsfs.img file in read+write mode
  2. Reads the inode bitmap from block 17
  3. Sets bit 1 in the bitmap to mark inode 1 as "IN USE"
  4. Writes the corrupted bitmap back to disk
  5. Does NOT update the actual inode table or directory entries

**Corruption Pattern Created**:
  1. Inode bitmap says: "inode 1 is ALLOCATED"
  2. Inode table says: "inode 1 is FREE" (type = 0)
  3. No directory entry points to inode 1
  4. Creates an orphaned inode allocation which is a classic filesystem inconsistency

 **Simulated Crash Scenario**:
  1. Mimics a system crash after updating the bitmap
  2. Before changes could be fully propagated to the inode table
  3. Tests the validator's ability to detect such inconsistencies

### 3. **validator.c** - Filesystem Consistency Checker

**Purpose:** Validates filesystem metadata to detect and report inconsistencies.

**Key Mechanisms:**

- **Superblock Validation**:
  - Verifies magic number matches FS_MAGIC
  - Checks block size, total blocks, and inode count
  - Validates bitmap and inode table locations

- **Bitmap Consistency Checks**:
  - Compares inode bitmap against actual inode states (type != 0 means allocated)
  - Reports mismatches: "inode X allocation mismatch (inode vs bitmap)"
  - Verifies data bitmap matches inode data block references
  - Checks for stray bits set beyond valid ranges

- **Inode Validation**:
  - Verifies inode type is valid (0, 1, or 2)
  - Checks size consistency with allocated data blocks
  - Validates block pointers are within data region
  - Detects multiple inodes claiming same data block

- **Directory Structure Validation**:
  - Verifies directory size is aligned to directory entry size
  - Checks all directory entries point to valid inodes
  - Ensures directories have "." and ".." entries
  - Validates entry names are null-terminated
  - Builds link reference counts from directory entries

- **Link Count Verification**:
  - Tracks how many directory entries reference each inode
  - Compares against inode->links field
  - Reports discrepancies: "link count X disagrees with directory refs Y"

- **Error Reporting**:
  - Uses a variable-argument error function to report all issues
  - Counts total inconsistencies found
  - Returns 0 if filesystem is consistent, 1 if errors exist

**Example Detection**:
If corrupt.c sets inode bitmap bit 1 but inode 1 has type=0:
- Validator detects: "inode 1 allocation mismatch (inode vs bitmap)"

### 4. **journal.c** - Metadata Journaling System

**Purpose:** Implements a write-ahead journal to ensure crash recovery and maintain consistency.

**Key Mechanisms:**

#### **Journal Header** (at block 1):
```c
struct journal_header {
    uint32_t magic;       // JOURNAL_MAGIC = 0x4A524E4C ("JRNL")
    uint32_t nbytes_used; // Current journal size in bytes
};
```
Tracks what's currently in the journal.

#### **Record Format**:
- **Record Header**: Type (DATA=1 or COMMIT=2) and size
- **Data Record**: Contains block_no + full 4096-byte block content
- **Commit Record**: Signals end of transaction

#### **Transaction Workflow** (`create` command):

1. **Phase 1: Read Current State**
   - Read journal header to see what's already logged
   - Find a free inode from the inode bitmap
   - Find a free directory entry in the root directory

2. **Phase 2: Prepare Changes in Memory**
   - Mark the inode as used in bitmap
   - Create new inode with type=1 (file), size=0
   - Add directory entry linking filename to inode number
   - Update root directory size if needed
   - All changes kept in memory (not written yet)

3. **Phase 3: Write Records to Journal** (Crash-safe)
   - Record 1: Write updated inode bitmap
   - Record 2: Write updated inode table
   - Record 3: Write updated directory block
   - Record 4: Write COMMIT marker
   - Update journal header with new size

4. **Key Safety Feature**:
   - Changes are ONLY in the journal until COMMIT is written
   - If crash occurs before COMMIT, changes are ignored during recovery
   - If crash occurs after COMMIT, changes are recovered

#### **Recovery Workflow** (`install` command):

1. **Read Journal Header**
   - Check if journal contains valid data (magic + size > header)

2. **Scan Records Until COMMIT**
   - Read each record sequentially
   - Store DATA records in memory (up to 10 pending updates)
   - Stop when COMMIT record is found

3. **Write Changes to "Home" Locations**
   - Once COMMIT found, copy all pending DATA records from journal to actual blocks
   - Write inode bitmap to block 17
   - Write inode table to block 19
   - Write directory to block 21

4. **Clear the Journal**
   - Reset journal header nbytes_used to header size
   - Makes journal empty for next transaction

**Safety Guarantees**:
- **Atomicity**: Either all changes apply or none (no partial updates)
- **Durability**: Once COMMIT is written, data survives crashes
- **Consistency**: Journal ensures metadata stays consistent across crashes
- **Recovery**: On next mount, `install` command replays pending transactions

---

## Filesystem Disk Layout

```
Block  0: Superblock (filesystem metadata)
Block  1-16: Journal (16 blocks for write-ahead log)
Block  17: Inode Bitmap (256 inodes, 1 bit each)
Block  18: Data Bitmap (256 data blocks, 1 bit each)
Block  19-20: Inode Table (256 inodes × 128 bytes)
Block  21-84: Data Area (64 blocks for files/directories)
Total: 85 blocks (≈348 KB)
```

---

## How Journaling Ensures Consistency

**Without Journaling:**
- Crash during write → Inconsistent metadata
- Bitmap says inode is used, but inode table says it's free
- Validator detects errors but cannot fix them

**With Journaling:**
- All changes written to journal first
- COMMIT marker indicates transaction is complete
- On recovery, validator can replay uncommitted transactions
- Ensures metadata never enters half-written state

---

## Usage Example

```bash
# 1. Create the filesystem
./mkfs vsfs.img

# 2. Create a file (logs to journal)
./journal create myfile.txt

# 3. In production, this would survive crashes:
# If crash happens here, changes are in journal but not installed

# 4. Recover and apply changes from journal
./journal install

# 5. Validate filesystem consistency
./validator vsfs.img

# 6. Optionally, simulate corruption for testing
./corrupt
./validator vsfs.img  # Now reports inconsistencies
```

---

## Key Concepts

- **Metadata**: Information about files (inodes, bitmaps, directories)
- **Journaling**: Write-ahead log technique for crash recovery
- **Atomicity**: Operations either fully succeed or fully fail
- **Consistency**: Metadata relationships always remain valid
- **Crash Recovery**: System can recover from power loss/crashes
- **Bitmap**: Compact representation of which inodes/blocks are in use

