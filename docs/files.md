# Files beside the program

How the engine reads and writes the files a game keeps beside the program: settings, saves, MAIN.DAT's cooked copy, screenshots. Read this before editing `src/engine/platform/fs.c`, or before writing a file of a game's own.

## Names

- **Inside the root** (`ARENA2`): the original game's files, read only. `fs_open`, `fs_read_all`.
- **Beside the program**: names below the folder that holds the root, as deep as they go (`saves\Anna Smith\S0003.SAV`), with a backslash between folders on both halves. A name that could leave that folder is refused by every call: one that starts with a slash or a backslash, holds a colon (a drive), or has a `..` folder in it.
- **The movies** fall back to the CD-ROM drives when the install lacks them (`fs_open_media`, `docs/video.md`).

## Reading

Sizes and offsets are `df_u32`, so a file can be up to 4 GB - 1. That is FAT32's largest file; a FAT16 drive stops a file at 2 GB. A bigger file does not open.
- On Windows, `GetFileSize` is given its high word, so a size of 0xFFFFFFFF is told from a failure, and `SetFilePointer` is given a high word of 0, so the offset is unsigned. Before 2026-11-20 both took the low word alone. Windows then treats the offset as signed, and every read past 2 GB failed.
- On the host it is `lseek` and `pread`, to 4 GB - 1 (it stopped at 2 GB before).
- `fs_read_beside` reads a whole file into one buffer. A big file is read in pieces: `fs_open_beside`, then `fs_read_at` a chunk at a time.

## Writing

One file at a time. `fs_create` opens it, replacing one of that name. `fs_append` writes to it. `fs_finish` keeps it, and `fs_abort` deletes what was written.
- **`fs_append` writes it all or fails.** On Windows it is one `WriteFile`. Windows 98 answers a full disk with a short write and no error, so a short write is a full disk. On the host a short write goes round again, and the next write says why.
- **`fs_finish` answers 1 only when the file reached the disk and closed.** On Windows that is `FlushFileBuffers`, then `CloseHandle`; on the host, `fsync`, then `close`; both are checked. Windows 98 keeps what was written in its cache and can find the disk full only at the flush or the close, never at the write. On 0 the file is closed and left where it is, for the caller to `fs_delete`: the engine never deletes it.
- **A file that matters** is written under a name of its own and moved over the real one once it finished (`fs_move`, below). A save that fails then leaves the old save as it was.
- **The cost.** Every finish waits for the disk: a flush of the settings file, a screenshot, MAIN.DAT's first cooking, a save. On the 233 that is the disk's own speed, unmeasured; it is paid once per file, never per frame.

## Why it failed (`fs_last_error`)

Each `fs_create`, `fs_append`, `fs_finish` and `fs_move` sets it: `FS_ERR_NONE` when the call worked. So it is read right after the call that answered 0. Nothing else touches it: not `fs_abort`, `fs_delete` or a read.

| | Windows | host |
| --- | --- | --- |
| `FS_ERR_DISK_FULL` | `ERROR_DISK_FULL`, `ERROR_HANDLE_DISK_FULL`, a short write | `ENOSPC`, `EDQUOT`, `EFBIG` (a file past what the drive or the process may hold), a write of nothing |
| `FS_ERR_ACCESS` | `ERROR_ACCESS_DENIED`, `ERROR_SHARING_VIOLATION`, `ERROR_LOCK_VIOLATION`, `ERROR_WRITE_PROTECT` | `EACCES`, `EPERM`, `EROFS`, `ETXTBSY` |
| `FS_ERR_OTHER` | anything else | anything else |

Windows 98 does not always name a full disk: a flush or a close can fail with some other code. So a write that fails for a reason not in the table is checked against the drive. With less free than the write still needed, and 64 KB at the least (`FS_FULL_SLACK`, two of the largest clusters), it is `FS_ERR_DISK_FULL`. That spares guessing which other codes Windows 98 uses for it.

## Free space (`fs_free_bytes`)

Free bytes on the drive that holds a name beside the program, for this user (quotas counted), clamped to 0xFFFFFFFF. The name may be a file or a folder, there or not. It answers 0 when it cannot tell, and the caller then does not check ahead.
- **Windows:** the drive's root from the full path (`C:\`, or a share's `\\server\share\`). `GetDiskFreeSpaceExA` is Windows 95 OSR2's and later, so it is looked up with `GetProcAddress`, not imported, and the first Windows 95 still loads the program. Without it, `GetDiskFreeSpaceA`, clamped. Its 32-bit counts stop near 2 GB on a bigger FAT32 drive: it says less than there is, never more.
- **Host:** `statvfs` of the nearest folder that is there, free blocks for users times the block size.

## Replacing a file (`fs_move`)

Windows 98 has no replacing rename (`MoveFileEx` is NT's). So the old file is first renamed to its name with `.OLD` added, the new one takes the name, and the `.OLD` is deleted. If the new one cannot take the name, the old one is put back. Only a crash between the two renames leaves the name missing, with the old file waiting as `.OLD`, and a reader should take that. A failure says why in `fs_last_error`: the first step that failed, not the putting back. On a full disk even a rename can fail, when the folder needs room for the entry.

## Saves as big as they get

A save may be as big as the drive allows; a big file is better than a broken one.
- **Files:** 4 GB - 1 on FAT32, 2 GB on FAT16.
- **The engine's rows** (`keep.c` for the world, `automap.c` for the seen models) have no cap in code. They grow by doubling in heaps of their own that have no maximum (`HeapCreate(0, 64 KB, 0)`), and `mem_alloc` has no size ceiling. On the target a `Keep` row is 76 bytes and an `AutomapRow` 20.
  - 100,000 world rows sit in a 10 MB array, 15 MB while it doubles.
  - 500,000 world rows are 40 MB, near 60 MB while doubling: past the 64 MB machine's RAM, into the swap file.
  - 500,000 automap rows are 10 MB, 15 MB while doubling.
  - Windows 98's heap is tuned for blocks under 4 MB. That is a matter of speed, not a limit, and arrays this big are unmeasured on the target.
- **Load rows in key order** (`keep_at(i)`, the automap's i-th, as they were written). A put lands in key order by moving every row after it, so rows put in order land at the end with nothing moved. Hundreds of thousands out of order would be quadratic: minutes on the 233.

## Tests

hostcheck, in `tests/check_engine.c`:
- **`check_beside`, `check_beside_saves`:** names, folders, listing, deleting, moving with `.OLD`, partial reads, names that would leave the folder.
- **`check_fs_writes`:**
  - A good file finishes with 1 and is all there.
  - A finish or a write with no file open is refused.
  - `/dev/full` under the file's name is `FS_ERR_DISK_FULL` at the write, and its failed flush answers 0 and leaves the file.
  - A write cut short by the process's file size limit (`RLIMIT_FSIZE`, `SIGXFSZ` ignored) is `FS_ERR_DISK_FULL`.
  - A create, and a move, into a folder with no write permission are `FS_ERR_ACCESS`. This is not tried as root, which writes anywhere.
  - Free bytes are told, for a name not there yet too.
  - A sparse 2.5 GB file opens, says its size and reads at its end.
  - A host with no `/dev/full`, or no sparse files, prints that the case was not tried.

## Not yet

- **The Windows half has not run.** It builds and passes `pe_check`, but the codes Windows 98 gives at a full disk, the failing flush or close, and the free space on a big FAT32 drive are unverified on the target. A save onto a nearly full drive on the VM is the check.
