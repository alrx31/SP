# File Manager Application - Unlimited Version

A high-performance graphical file manager with table-like interface using SFML. 
Optimized for fast scanning like `ls -lR` with **NO DEPTH LIMITS** - shows ALL files recursively.

## Key Features
- **Unlimited recursive scanning** - displays all files from any directory tree
- **High-performance optimization** - can scan 400k+ files in seconds
- **Non-blocking UI** - handles window events during scanning to prevent "not responding" dialogs
- **Real-time progress** - shows scan progress every 100 directories
- **Interruptible scanning** - press ESC to stop long scans
- Table-like display with customizable columns (name, size, date, permissions)
- Interactive configuration menu with runtime adjustments
- Comprehensive error logging for permission issues

## Performance
- Tested on `/usr` directory: 410k+ files scanned in ~2.4 seconds
- Optimized with `lstat`, memory pre-allocation, and efficient string operations
- Progress feedback prevents system "not responding" notifications

## Usage
```bash
./table_app <directory> [m rows] [n cols] [frame size] [bgcolor hex] [linecolor hex] [line size] [font index] [header font] [border hex] [text hex] [font size]
```

### Examples
```bash
# Basic usage - scan current directory
./table_app .

# Scan /usr with custom display settings
./table_app /usr 25 4 3 "#000000" "#ffffff" 1 1 2 "#ff0000" "#00ff00" 1.2

# Scan entire filesystem (use with caution!)
./table_app /
```

## Controls
- **Arrow keys / Page Up/Down**: Navigate between pages
- **Mouse wheel**: Scroll through pages
- **M**: Open interactive configuration menu
- **R**: Refresh/rescan directory (full rescan)
- **L**: Show log file information
- **ESC**: Interrupt long scans (during scanning phase)

## Configuration Menu (Press M)
Runtime adjustable settings:
- Display rows and columns
- Frame and line sizes
- Font selection and sizes
- All color schemes (background, text, borders, directories)

## Technical Details
- **No depth limits**: Unlike other versions, scans complete directory trees
- **Error handling**: Logs inaccessible files to `unreadable_files.log`
- **Memory efficient**: Pre-allocates vectors and uses move semantics
- **String optimization**: Cached date formatting and reserved string sizes
- **Window responsiveness**: Processes SFML events every 10 directories during scan

## Requirements
- SFML graphics library (`libsfml-dev`)
- C++17 compatible compiler
- Font files in `assets/` directory (TTF format)

## Compilation
```bash
g++ -O3 -march=native -std=c++17 -o table_app main.cpp -lsfml-graphics -lsfml-window -lsfml-system
```

## System Compatibility
- Optimized for Linux systems
- Uses POSIX system calls (`lstat`, `access`, `opendir`)
- Handles permission denied gracefully
- Works with any filesystem (ext4, btrfs, xfs, etc.)

## Performance Tips
- For very large directories (100k+ files), consider using smaller display grids
- Monitor system resources when scanning network mounted filesystems
- Use ESC to interrupt if scan takes too long
- Check `unreadable_files.log` for permission issues

## File Size Display
The program shows allocated file sizes (space actually used on disk), similar to `ls -l` behavior:
- Small files are rounded up to filesystem block size (usually 4096 bytes)
- Large files show their actual size
- Directories show their directory entry size, not recursive content size (for performance)