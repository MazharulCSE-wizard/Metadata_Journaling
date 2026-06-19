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

**The following key mechanisms have been implemented in mkfs.c file:** 

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

**The following key mechanisms have been implemented in corrupt.c file:**

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

 Validates filesystem metadata to detect and report inconsistencies.

**The following key mechanisms have been implemented in validator.c file:**

 **Superblock Validation**:
  1. Verifies magic number matches FS_MAGIC
  2. Checks block size, total blocks, and inode count
  3. Validates bitmap and inode table locations

 **Bitmap Consistency Checks**:
  1. Compares inode bitmap against actual inode states (type != 0 means allocated)
  2. Reports mismatches: "inode X allocation mismatch (inode vs bitmap)"
  3. Verifies data bitmap matches inode data block references
  4. Checks for stray bits set beyond valid ranges

 **Inode Validation**:
  1. Verifies inode type is valid (0, 1, or 2)
  2. Checks size consistency with allocated data blocks
  3. Validates block pointers are within data region
  4. Detects multiple inodes claiming same data block

 **Directory Structure Validation**:
  1. Verifies directory size is aligned to directory entry size
  2. Checks all directory entries point to valid inodes
  3. Ensures directories have "." and ".." entries
  4. Validates entry names are null-terminated
  5. Builds link reference counts from directory entries

 **Link Count Verification**:
  1. Tracks how many directory entries reference each inode
  2. Compares against inode->links field
  3. Reports discrepancies: "link count X disagrees with directory refs Y"

 **Error Reporting**:
  - Uses a variable-argument error function to report all issues
  - Counts total inconsistencies found
  - Returns 0 if filesystem is consistent, 1 if errors exist

**Example Detection**:
If corrupt.c sets inode bitmap bit 1 but inode 1 has type=0:
- Validator detects: "inode 1 allocation mismatch (inode vs bitmap)"

### 4. **journal.c** - Metadata Journaling System

 Implements a write-ahead journal to ensure crash recovery and maintain consistency.

**The following key mechanisms have been implemented in journal.c file:**

#### **Journal Header** (at block 1):
```c
struct journal_header {
    uint32_t magic;       // JOURNAL_MAGIC = 0x4A524E4C ("JRNL")
    uint32_t nbytes_used; // Current journal size in bytes
};
```
Tracks what's currently in the journal.

#### **Record Format**:
1. **Record Header**: Type (DATA=1 or COMMIT=2) and size
2. **Data Record**: Contains block_no + full 4096-byte block content
3. **Commit Record**: Signals end of transaction

#### **Transaction Workflow** (`create` command):

1. **Phase 1: Read Current State**
   i. Read journal header to see what's already logged
   ii. Find a free inode from the inode bitmap
   iii. Find a free directory entry in the root directory

2. **Phase 2: Prepare Changes in Memory**
   i. Mark the inode as used in bitmap
   ii. Create new inode with type=1 (file), size=0
   iii. Add directory entry linking filename to inode number
   iv. Update root directory size if needed
   v. All changes kept in memory (not written yet)

3. **Phase 3: Write Records to Journal** (Crash-safe)
   i. Record 1: Write updated inode bitmap
   ii. Record 2: Write updated inode table
   iii. Record 3: Write updated directory block
   iv. Record 4: Write COMMIT marker
   v. Update journal header with new size

4. **Key Safety Feature**:
   i. Changes are ONLY in the journal until COMMIT is written
   ii. If crash occurs before COMMIT, changes are ignored during recovery
   iii. If crash occurs after COMMIT, changes are recovered

#### **Recovery Workflow** (`install` command):

1. **Read Journal Header:**
    Check if journal contains valid data (magic + size > header)

2. **Scan Records Until COMMIT**
   i. Read each record sequentially
   ii. Store DATA records in memory (up to 10 pending updates)
   iii. Stop when COMMIT record is found

3. **Write Changes to "Home" Locations**
   i. Once COMMIT found, copy all pending DATA records from journal to actual blocks
   ii. Write inode bitmap to block 17
   iii. Write inode table to block 19
   iv. Write directory to block 21

4. **Clear the Journal**
   i. Reset journal header nbytes_used to header size
   ii. Makes journal empty for next transaction

**Safety Guarantees**:
1. **Atomicity**: Either all changes apply or none (no partial updates)
2. **Durability**: Once COMMIT is written, data survives crashes
3. **Consistency**: Journal ensures metadata stays consistent across crashes
4. **Recovery**: On next mount, `install` command replays pending transactions

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
File system will crash because of the inconsistent metadata. Morever, validator will return errors and will show the inconsistency of the metadata.

**With Journaling:**
Firstly, all changes written to journal first. Then COMMIT marker indicates transaction is complete. On recovery, validator can replay uncommitted transactions ensuring metadata never enters half-written state.
---
## Usage Example

```bash
# 1. Create the filesystem
./mkfs vsfs.img

# 2. Validate filesystem consistency
./validator vsfs.img

# 3. Create a file (logs to journal)
./journal create dummy.txt

# In production, this would survive crashes:
# If crash happens here, changes are in journal but not installed

# 4. simulate corruption for testing
./corrupt
./validator vsfs.img  # Now reports inconsistencies

# 4. Recover and apply changes from journal
./journal install

# 5. Validate filesystem consistency
./validator vsfs.img    #now it will return filesystem is consistent

```
---


