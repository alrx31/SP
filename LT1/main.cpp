#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <optional>
#include <cstdint>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <functional>
#include <queue>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <pwd.h>
#include <grp.h>
#include <sys/statvfs.h>

namespace fs = std::filesystem;

// Logger class for tracking files that cannot be read
class FileAccessLogger {
private:
    std::string logFilePath;
    std::ofstream logFile;
    bool loggingEnabled;

public:
    FileAccessLogger() 
        : logFilePath("unreadable_files.log"), loggingEnabled(true) {
        try {
            logFile.open(logFilePath, std::ios::app);
            if (logFile.is_open()) {
                // Write session header
                auto now = std::chrono::system_clock::now();
                auto time_t = std::chrono::system_clock::to_time_t(now);
                logFile << "\n=== Session started: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") 
                        << " ===" << std::endl;
                logFile.flush();
            } else {
                std::cout << "[LOG] Warning: Could not open log file: " << logFilePath << " - using console output" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "[LOG] Warning: Failed to initialize file logger: " << e.what() << " - using console output" << std::endl;
        }
        
        // Always log session start, either to file or console
        if (!logFile.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::cout << "[LOG] === Session started: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") 
                      << " ===" << std::endl;
        }
    }

    ~FileAccessLogger() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        if (logFile.is_open()) {
            logFile << "=== Session ended: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") 
                    << " ===\n" << std::endl;
            logFile.close();
        } else {
            std::cout << "[LOG] === Session ended: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") 
                      << " ===" << std::endl;
        }
    }

    void logUnreadableFile(const std::string& filePath, const std::string& operation, const std::string& errorMsg) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::ostringstream logMessage;
        logMessage << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S") << "] "
                   << "FAILED " << operation << ": " << filePath;
        
        if (!errorMsg.empty()) {
            logMessage << " - Error: " << errorMsg;
        }
        
        // Try to write to log file if available
        if (loggingEnabled && logFile.is_open()) {
            try {
                logFile << logMessage.str() << std::endl;
                logFile.flush();
            } catch (const std::exception& e) {
                // If file logging fails, output to console
                std::cout << "[LOG] " << logMessage.str() << std::endl;
            }
        } else {
            // If no log file available, output to console
            std::cout << "[LOG] " << logMessage.str() << std::endl;
        }
    }

    void logAccessDenied(const std::string& filePath, const std::string& operation = "access") {
        logUnreadableFile(filePath, operation, "Permission denied");
    }

    void logFileNotFound(const std::string& filePath, const std::string& operation = "access") {
        logUnreadableFile(filePath, operation, "File not found");
    }

    void logSystemError(const std::string& filePath, const std::string& operation, const std::error_code& ec) {
        logUnreadableFile(filePath, operation, ec.message());
    }

    void logFileModification(const std::string& filePath, const std::string& operation, const std::string& details = "") {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::ostringstream logMessage;
        logMessage << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S") << "] "
                   << "MODIFIED " << operation << ": " << filePath;
        
        if (!details.empty()) {
            logMessage << " - " << details;
        }
        
        // Try to write to log file if available
        if (loggingEnabled && logFile.is_open()) {
            try {
                logFile << logMessage.str() << std::endl;
                logFile.flush();
            } catch (const std::exception& e) {
                // If file logging fails, output to console
                std::cout << "[LOG] " << logMessage.str() << std::endl;
            }
        } else {
            // If no log file available, output to console
            std::cout << "[LOG] " << logMessage.str() << std::endl;
        }
    }

    std::string getLogFilePath() const {
        return logFilePath;
    }

    bool isLoggingEnabled() const {
        return loggingEnabled;
    }
};

struct ColorParse {
    static bool hexToColor(const std::string& hex, sf::Color& out) {
        if (hex.size() != 7 || hex[0] != '#') return false;
        unsigned int r, g, b;
        std::istringstream(hex.substr(1, 2)) >> std::hex >> r;
        std::istringstream(hex.substr(3, 2)) >> std::hex >> g;
        std::istringstream(hex.substr(5, 2)) >> std::hex >> b;
        out = sf::Color(
            static_cast<std::uint8_t>(r),
            static_cast<std::uint8_t>(g),
            static_cast<std::uint8_t>(b)
        );
        return true;
    }
};

struct FileInfo {
    std::string name;
    std::string size;
    std::string date;
    std::string permissions;
    bool isDirectory;
    std::uintmax_t actualSize;    // размер данных
    std::uintmax_t allocatedSize; // фактически занимаемое место
};

// Structure to track cell editing state
struct CellEditState {
    bool isEditing = false;
    int row = -1;
    int column = -1;
    std::string originalValue;
    std::string currentValue;
    sf::RectangleShape cursor;
    sf::Clock cursorBlink;
    
    void reset() {
        isEditing = false;
        row = -1;
        column = -1;
        originalValue.clear();
        currentValue.clear();
    }
};

// Helper function to get filesystem block size
std::uintmax_t getFilesystemBlockSize(const std::string& path) {
    struct statvfs vfs;
    if (statvfs(path.c_str(), &vfs) == 0) {
        return vfs.f_bsize;
    }
    return 4096; // default block size if statvfs fails
}

// Helper function to calculate allocated size (rounded up to block size)
std::uintmax_t calculateAllocatedSize(std::uintmax_t actualSize, std::uintmax_t blockSize) {
    if (actualSize == 0) return 0;
    return ((actualSize + blockSize - 1) / blockSize) * blockSize;
}

// Helper function to format size with both actual and allocated sizes
std::string formatSizeInfo(std::uintmax_t actualSize, std::uintmax_t allocatedSize) {
    // For performance, just return the actual size as string
    // Pre-allocate string to avoid reallocation
    std::string result;
    result.reserve(16);  // Reserve space for typical size strings
    return std::to_string(actualSize) + "/" + std::to_string(allocatedSize);
}

// REMOVED: Recursive directory size calculation - too slow for large directories
// For performance, we now just show the directory entry size, not the total content size
// This is similar to how 'ls -l' works by default

// Fast directory size - just returns the directory entry size (like ls -l)
std::uintmax_t getDirectorySize(const fs::path& path, FileAccessLogger* logger = nullptr) {
    struct stat statBuf;
    if (stat(path.c_str(), &statBuf) == -1) {
        if (logger) {
            logger->logUnreadableFile(path.string(), "stat_for_dir_size", std::string("stat failed: ") + strerror(errno));
        }
        return 0;
    }
    return statBuf.st_size;  // Just return directory entry size
}

// Optimized version that reuses existing stat data
std::string getFilePermissionsFromStat(const struct stat& statBuf) {
    std::string result;
    result.reserve(10);  // Pre-allocate for efficiency
    
    // File type
    if (S_ISDIR(statBuf.st_mode)) {
        result += 'd';
    } else if (S_ISLNK(statBuf.st_mode)) {
        result += 'l';
    } else if (S_ISREG(statBuf.st_mode)) {
        result += '-';
    } else if (S_ISBLK(statBuf.st_mode)) {
        result += 'b';
    } else if (S_ISCHR(statBuf.st_mode)) {
        result += 'c';
    } else if (S_ISFIFO(statBuf.st_mode)) {
        result += 'p';
    } else if (S_ISSOCK(statBuf.st_mode)) {
        result += 's';
    } else {
        result += '?';
    }
    
    // Owner permissions
    result += (statBuf.st_mode & S_IRUSR) ? "r" : "-";
    result += (statBuf.st_mode & S_IWUSR) ? "w" : "-";
    result += (statBuf.st_mode & S_IXUSR) ? "x" : "-";
    
    // Group permissions
    result += (statBuf.st_mode & S_IRGRP) ? "r" : "-";
    result += (statBuf.st_mode & S_IWGRP) ? "w" : "-";
    result += (statBuf.st_mode & S_IXGRP) ? "x" : "-";
    
    // Others permissions with sticky bit consideration
    bool has_others_exec = (statBuf.st_mode & S_IXOTH) != 0;
    bool has_sticky_bit = (statBuf.st_mode & S_ISVTX) != 0;
    
    result += (statBuf.st_mode & S_IROTH) ? "r" : "-";
    result += (statBuf.st_mode & S_IWOTH) ? "w" : "-";
    
    // Replace last character (others_exec) with t/T if sticky bit is set
    if (has_sticky_bit) {
        result += has_others_exec ? "t" : "T";
    } else {
        result += has_others_exec ? "x" : "-";
    }
    
    return result;
}

// Функция для получения прав доступа к файлу (старая версия, оставлена для совместимости)
std::string getFilePermissions(const fs::path& path, FileAccessLogger* logger = nullptr) {
    struct stat statBuf;
    
    if (stat(path.c_str(), &statBuf) == -1) {
        if (logger) {
            logger->logUnreadableFile(path.string(), "permissions_check", std::string("stat failed: ") + strerror(errno));
        }
        return "----------";
    }
    
    return getFilePermissionsFromStat(statBuf);
}

// Static cache for current year to avoid repeated time() calls
static time_t cached_now = 0;
static int cached_current_year = 0;

// Optimized date formatting function
std::string formatDate(time_t mtime, FileAccessLogger* logger = nullptr, const std::string& filePath = "") {
    try {
        std::tm* tm = std::localtime(&mtime);
        if (!tm) {
            if (logger && !filePath.empty()) {
                logger->logUnreadableFile(filePath, "date_format", "Failed to convert time_t to tm");
            }
            return "Jan  1  1970";
        }
        
        // Cache current year to avoid repeated time() calls
        time_t now = time(nullptr);
        if (now - cached_now > 3600) {  // Update cache every hour
            cached_now = now;
            std::tm* now_tm = std::localtime(&now);
            cached_current_year = now_tm ? now_tm->tm_year : 0;
        }
        
        std::string result;
        result.reserve(12);  // Pre-allocate for efficiency
        
        // Month names like ls -l
        const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        
        result += months[tm->tm_mon];
        result += " ";
        
        // Day with appropriate spacing (ls -l style)
        if (tm->tm_mday < 10) {
            result += " ";
            result += std::to_string(tm->tm_mday);
        } else {
            result += std::to_string(tm->tm_mday);
        }
        result += " ";
        
        // If file is from this year, show time; otherwise show year
        if (cached_current_year > 0 && tm->tm_year == cached_current_year) {
            char timeBuf[6];
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", tm->tm_hour, tm->tm_min);
            result += timeBuf;
        } else {
            result += " ";
            result += std::to_string(tm->tm_year + 1900);
        }
        
        return result;
    } catch (const std::exception& e) {
        if (logger && !filePath.empty()) {
            logger->logUnreadableFile(filePath, "date_format", e.what());
        }
        return "Jan  1  1970";
    }
}

// Overload for filesystem compatibility (if needed)
std::string formatDate(const fs::file_time_type& ftime, FileAccessLogger* logger = nullptr, const std::string& filePath = "") {
    try {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
        return formatDate(cftime, logger, filePath);
    } catch (const std::exception& e) {
        if (logger && !filePath.empty()) {
            logger->logUnreadableFile(filePath, "date_format", e.what());
        }
        return "01.01.1970";
    }
}

class FileManager {
private:
    std::vector<FileInfo> files;
    std::string directoryPath;
    std::unique_ptr<FileAccessLogger> logger;
    bool scanInterrupted = false;
    sf::RenderWindow* window = nullptr;  // Reference to window for event handling
    
public:
    FileManager(const std::string& path, sf::RenderWindow* win = nullptr) 
        : directoryPath(path), window(win) {
        // Initialize logger
        try {
            logger = std::make_unique<FileAccessLogger>();
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not initialize file access logger: " << e.what() << std::endl;
            logger = nullptr;
        }
        loadFiles();
    }
    
    // Method to reload a single file's information
    bool reloadSingleFile(const std::string& filePath) {
        // Find the file in the files vector
        auto it = std::find_if(files.begin(), files.end(), 
            [&filePath](const FileInfo& info) { return info.name == filePath; });
        
        if (it == files.end()) {
            if (logger) {
                logger->logUnreadableFile(filePath, "reload_single_file", "File not found in list");
            }
            return false;
        }
        
        // Reload file information
        struct stat statBuf;
        if (lstat(filePath.c_str(), &statBuf) == -1) {
            if (logger) {
                logger->logUnreadableFile(filePath, "reload_single_file_lstat", std::string("lstat failed: ") + strerror(errno));
            }
            return false;
        }
        
        // Get filesystem block size
        std::uintmax_t blockSize = getFilesystemBlockSize(fs::path(filePath).parent_path().string());
        
        // Update file info
        it->isDirectory = S_ISDIR(statBuf.st_mode);
        it->actualSize = statBuf.st_size;
        
        if (it->isDirectory) {
            it->allocatedSize = calculateAllocatedSize(statBuf.st_size, blockSize);
            it->size = formatSizeInfo(it->actualSize, it->allocatedSize);
        } else {
            it->allocatedSize = calculateAllocatedSize(statBuf.st_size, blockSize);
            it->size = formatSizeInfo(it->actualSize, it->allocatedSize);
        }
        
        it->date = formatDate(statBuf.st_mtime, logger.get(), filePath);
        it->permissions = getFilePermissionsFromStat(statBuf);
        
        if (logger) {
            logger->logFileModification(filePath, "file_info_reloaded", "Successfully updated file information");
        }
        
        return true;
    }
    
    // Method to update file metadata (name, permissions, etc.)
    bool updateFileMetadata(const std::string& filePath, int columnIndex, const std::string& newValue) {
        try {
            switch (columnIndex) {
                case 0: // Name - rename file
                {
                    fs::path oldPath(filePath);
                    fs::path newPath = oldPath.parent_path() / newValue;
                    
                    if (fs::exists(newPath)) {
                        if (logger) {
                            logger->logUnreadableFile(filePath, "rename", "Target file already exists: " + newPath.string());
                        }
                        return false;
                    }
                    
                    fs::rename(oldPath, newPath);
                    
                    // Update in files vector
                    auto it = std::find_if(files.begin(), files.end(), 
                        [&filePath](const FileInfo& info) { return info.name == filePath; });
                    if (it != files.end()) {
                        it->name = newPath.string();
                    }
                    
                    if (logger) {
                        logger->logFileModification(filePath, "rename", "Renamed to: " + newPath.string());
                    }
                    
                    return reloadSingleFile(newPath.string());
                }
                
                case 3: // Permissions
                {
                    // Parse permissions string (e.g., "drwxr-xr-x")
                    if (newValue.length() != 10) {
                        if (logger) {
                            logger->logUnreadableFile(filePath, "chmod", "Invalid permissions format: " + newValue);
                        }
                        return false;
                    }
                    
                    mode_t mode = 0;
                    
                    // Owner permissions
                    if (newValue[1] == 'r') mode |= S_IRUSR;
                    if (newValue[2] == 'w') mode |= S_IWUSR;
                    if (newValue[3] == 'x') mode |= S_IXUSR;
                    
                    // Group permissions
                    if (newValue[4] == 'r') mode |= S_IRGRP;
                    if (newValue[5] == 'w') mode |= S_IWGRP;
                    if (newValue[6] == 'x') mode |= S_IXGRP;
                    
                    // Others permissions
                    if (newValue[7] == 'r') mode |= S_IROTH;
                    if (newValue[8] == 'w') mode |= S_IWOTH;
                    if (newValue[9] == 'x' || newValue[9] == 't') mode |= S_IXOTH;
                    
                    // Sticky bit
                    if (newValue[9] == 't' || newValue[9] == 'T') mode |= S_ISVTX;
                    
                    if (chmod(filePath.c_str(), mode) == -1) {
                        if (logger) {
                            logger->logUnreadableFile(filePath, "chmod", std::string("chmod failed: ") + strerror(errno));
                        }
                        return false;
                    }
                    
                    if (logger) {
                        logger->logFileModification(filePath, "chmod", "Changed permissions to: " + newValue);
                    }
                    
                    return reloadSingleFile(filePath);
                }
                
                default:
                    if (logger) {
                        logger->logUnreadableFile(filePath, "update_metadata", 
                            "Cannot modify column " + std::to_string(columnIndex) + " (read-only)");
                    }
                    return false;
            }
        } catch (const std::exception& e) {
            if (logger) {
                logger->logUnreadableFile(filePath, "update_metadata", e.what());
            }
            return false;
        }
    }
    
    void interruptScan() { scanInterrupted = true; }
    
    void loadFilesRecursive(const std::string& path, std::queue<std::pair<std::string, int>>& dirsToProcess, int currentDepth = 0) {
        // Check for interruption
        if (scanInterrupted) {
            return;
        }
        
        DIR* dir = opendir(path.c_str());
        
        if (!dir) {
            if (logger) {
                logger->logUnreadableFile(path, "opendir", std::string("Failed to open directory: ") + strerror(errno));
            }
            return;
        }
        
        // Get filesystem block size for this directory
        std::uintmax_t blockSize = getFilesystemBlockSize(path);
        
        // Pre-allocate space to avoid frequent reallocations
        std::vector<FileInfo> localFiles;
        localFiles.reserve(1000);  // Reserve space for efficiency
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr && !scanInterrupted) {
            // Skip . and ..
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            std::string fullPath = path + "/" + entry->d_name;
            struct stat statBuf;
            
            // Use lstat instead of stat for better performance (doesn't follow symlinks)
            if (lstat(fullPath.c_str(), &statBuf) == -1) {
                if (logger) {
                    logger->logUnreadableFile(fullPath, "lstat", std::string("lstat failed: ") + strerror(errno));
                }
                continue;
            }
            
            FileInfo info;
            info.name = fullPath;
            info.isDirectory = S_ISDIR(statBuf.st_mode);
            
            // Get actual and allocated file sizes
            info.actualSize = statBuf.st_size;
            if (info.isDirectory) {
                // For directories, just use the directory entry size (don't calculate recursive size)
                info.allocatedSize = calculateAllocatedSize(statBuf.st_size, blockSize);
                info.size = formatSizeInfo(info.actualSize, info.allocatedSize);
                
                // Add directory to queue for recursive processing if accessible
                if (access(fullPath.c_str(), R_OK | X_OK) == 0) {
                    dirsToProcess.push({fullPath, currentDepth + 1});
                } else if (logger) {
                    logger->logUnreadableFile(fullPath, "subdirectory_access_test", std::string("access denied: ") + strerror(errno));
                }
            } else {
                // Regular file or other file type
                info.allocatedSize = calculateAllocatedSize(statBuf.st_size, blockSize);
                info.size = formatSizeInfo(info.actualSize, info.allocatedSize);
            }
            
            // Get file modification time
            info.date = formatDate(statBuf.st_mtime, logger.get(), fullPath);
            
            // Simplified permissions (reuse stat data)
            info.permissions = getFilePermissionsFromStat(statBuf);
            
            localFiles.push_back(std::move(info));
        }
        
        closedir(dir);
        
        // Batch append to main files vector
        files.insert(files.end(), std::make_move_iterator(localFiles.begin()), 
                     std::make_move_iterator(localFiles.end()));
    }
    
    void loadFiles() {
        files.clear();
        
        // Check if directory exists and is accessible
        struct stat statBuf;
        if (stat(directoryPath.c_str(), &statBuf) == -1) {
            if (logger) {
                logger->logUnreadableFile(directoryPath, "directory_exists_check", std::string("stat failed: ") + strerror(errno));
            }
            return;
        }
        
        if (!S_ISDIR(statBuf.st_mode)) {
            if (logger) {
                logger->logUnreadableFile(directoryPath, "directory_type_check", "Not a directory");
            }
            return;
        }
        
        // Test basic directory access
        if (access(directoryPath.c_str(), R_OK | X_OK) != 0) {
            if (logger) {
                logger->logUnreadableFile(directoryPath, "directory_access_test", std::string("access denied: ") + strerror(errno));
            }
            std::cerr << "Cannot access directory: " << directoryPath << " - " << strerror(errno) << std::endl;
            return;
        }
        
        try {
            std::cout << "Scanning directory tree: " << directoryPath << std::endl;
            
            // Pre-allocate memory for better performance
            files.reserve(10000);  // Reserve space for 10k files initially
            
            // Use queue-based approach to handle recursion and catch all permission errors
            std::queue<std::pair<std::string, int>> dirsToProcess;
            dirsToProcess.push({directoryPath, 0});
            
            int processedDirs = 0;
            auto startTime = std::chrono::steady_clock::now();
            
            while (!dirsToProcess.empty() && !scanInterrupted) {
                auto [currentDir, depth] = dirsToProcess.front();
                dirsToProcess.pop();
                processedDirs++;
                
                // Handle window events every 10 directories to prevent "not responding" dialog
                if (window && processedDirs % 10 == 0) {
                    while (auto eventOpt = window->pollEvent()) {
                        if (!eventOpt) break;
                        const sf::Event& event = *eventOpt;
                        
                        if (event.is<sf::Event::Closed>()) {
                            scanInterrupted = true;
                            break;
                        }
                        
                        if (event.is<sf::Event::KeyPressed>()) {
                            if(const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()){
                                if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
                                    scanInterrupted = true;
                                    break;
                                }
                            }
                        }
                    }
                    
                    // Update window display to show we're still alive
                    if (window->isOpen()) {
                        window->clear(sf::Color::Black);
                        
                        // Show progress text
                        sf::Font font;
                        if (font.openFromFile("assets/Sansation-Regular.ttf")) {
                            sf::Text progressText(font, "Scanning: " + std::to_string(processedDirs) + 
                                                " dirs, " + std::to_string(files.size()) + " files\nPress ESC to stop", 24);
                            progressText.setFillColor(sf::Color::White);
                            progressText.setPosition(sf::Vector2f(50, 50));
                            window->draw(progressText);
                        }
                        
                        window->display();
                    }
                }
                
                // Progress feedback every 100 directories for console output
                if (processedDirs % 100 == 0) {
                    auto currentTime = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
                    std::cout << "\rProcessed " << processedDirs << " directories, found " << files.size() 
                              << " files, depth " << depth << " (" << elapsed << "ms) [Press ESC to stop]" << std::flush;
                }
                
                try {
                    loadFilesRecursive(currentDir, dirsToProcess, depth);
                } catch (const std::exception& e) {
                    if (logger) {
                        logger->logUnreadableFile(currentDir, "directory_processing", e.what());
                    }
                }
            }
            
            auto endTime = std::chrono::steady_clock::now();
            auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            
            if (scanInterrupted) {
                std::cout << "\rScan interrupted: " << processedDirs << " directories, " << files.size() 
                          << " files in " << totalTime << "ms (partial results)" << std::endl;
            } else {
                std::cout << "\rScan complete: " << processedDirs << " directories, " << files.size() 
                          << " files in " << totalTime << "ms" << std::endl;
            }
            
            // Сортировка: сначала каталоги, потом файлы
            std::cout << "Sorting files..." << std::flush;
            std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) {
                if (a.isDirectory != b.isDirectory) {
                    return a.isDirectory > b.isDirectory;
                }
                return a.name < b.name;
            });
            std::cout << " done!" << std::endl;
            
        } catch (const std::exception& e) {
            if (logger) {
                logger->logUnreadableFile(directoryPath, "general_error", e.what());
            }
            std::cerr << "Error reading directory: " << e.what() << std::endl;
        }
    }
    
    const std::vector<FileInfo>& getFiles() const {
        return files;
    }
    
    size_t getFileCount() const {
        return files.size();
    }
    
    std::string getLogFilePath() const {
        return logger ? logger->getLogFilePath() : "";
    }
    
    bool isLoggingEnabled() const {
        return logger ? logger->isLoggingEnabled() : false;
    }
};

// >>> Helper: Truncate string with ellipsis
std::string truncate(const std::string& str, size_t maxLen = 35) {
    if (str.length() <= maxLen) return str;
    return str.substr(0, maxLen - 3) + "...";
}

enum class HAlign { Left, Center, Right };
enum class VAlign { Top, Center, Bottom };

// Configuration structure for runtime menu
struct AppConfig {
    int m = 20;                           // rows
    int n = 4;                            // columns (for ls -l format)
    float frameSize = 5.0f;               // frame size (border size)
    float lineSize = 2.0f;                // line size
    size_t currentFontIndex = 1;          // font index
    size_t currentFontHeaderIndex = 2;    // header font index
    float fontSize = 1.5f;                // font size multiplier
    
    sf::Color borderColor = sf::Color::Red;
    sf::Color textColor = sf::Color::Magenta;
    sf::Color bgColor = sf::Color::Black;
    sf::Color lineColor = sf::Color::White;
    sf::Color dirColor = sf::Color::Cyan;
    sf::Color pageInfoColor = sf::Color::Green;
};

// Menu system for runtime configuration
class ConfigMenu {
private:
    AppConfig& config;
    std::vector<sf::Font>& fonts;
    bool isVisible = false;
    int selectedIndex = 0;
    std::vector<std::string> menuItems;
    sf::Font menuFont;
    
public:
    ConfigMenu(AppConfig& cfg, std::vector<sf::Font>& fontsRef) : config(cfg), fonts(fontsRef) {
        menuItems = {
            "Rows (m): ",
            "Columns (n): ",
            "Frame Size: ",
            "Line Size: ",
            "Font Index: ",
            "Header Font Index: ",
            "Font Size: ",
            "Background Color",
            "Text Color",
            "Border Color", 
            "Line Color",
            "Directory Color",
            "Page Info Color",
            "Close Menu"
        };
        
        // Load menu font (use default if available)
        if (!fonts.empty()) {
            menuFont = fonts[0];
        }
    }
    
    void toggle() { isVisible = !isVisible; }
    bool getVisible() const { return isVisible; }
    
    void handleInput(sf::Keyboard::Scancode key) {
        if (!isVisible) return;
        
        switch(key) {
            case sf::Keyboard::Scancode::Up:
                selectedIndex = (selectedIndex - 1 + menuItems.size()) % menuItems.size();
                break;
            case sf::Keyboard::Scancode::Down:
                selectedIndex = (selectedIndex + 1) % menuItems.size();
                break;
            case sf::Keyboard::Scancode::Left:
                adjustValue(-1);
                break;
            case sf::Keyboard::Scancode::Right:
                adjustValue(1);
                break;
            case sf::Keyboard::Scancode::Enter:
                if (selectedIndex == menuItems.size() - 1) { // Close Menu
                    isVisible = false;
                }
                break;
            case sf::Keyboard::Scancode::Escape:
                isVisible = false;
                break;
            default:
                break;
        }
    }
    
private:
    void adjustValue(int delta) {
        switch(selectedIndex) {
            case 0: // Rows
                config.m = std::max(5, std::min(50, config.m + delta));
                break;
            case 1: // Columns
                config.n = std::max(2, std::min(10, config.n + delta));
                break;
            case 2: // Frame Size
                config.frameSize = std::max(0.0f, std::min(20.0f, config.frameSize + delta));
                break;
            case 3: // Line Size
                config.lineSize = std::max(0.5f, std::min(10.0f, config.lineSize + delta * 0.5f));
                break;
            case 4: // Font Index
                if (!fonts.empty()) {
                    config.currentFontIndex = (config.currentFontIndex + delta + fonts.size()) % fonts.size();
                }
                break;
            case 5: // Header Font Index
                if (!fonts.empty()) {
                    config.currentFontHeaderIndex = (config.currentFontHeaderIndex + delta + fonts.size()) % fonts.size();
                }
                break;
            case 6: // Font Size
                config.fontSize = std::max(0.5f, std::min(5.0f, config.fontSize + delta * 0.1f));
                break;
            case 7: // Background Color
                cycleColor(config.bgColor, delta);
                break;
            case 8: // Text Color
                cycleColor(config.textColor, delta);
                break;
            case 9: // Border Color
                cycleColor(config.borderColor, delta);
                break;
            case 10: // Line Color
                cycleColor(config.lineColor, delta);
                break;
            case 11: // Directory Color
                cycleColor(config.dirColor, delta);
                break;
            case 12: // Page Info Color
                cycleColor(config.pageInfoColor, delta);
                break;
        }
    }
    
    void cycleColor(sf::Color& color, int delta) {
        std::vector<sf::Color> colors = {
            sf::Color::Black, sf::Color::White, sf::Color::Red, sf::Color::Green,
            sf::Color::Blue, sf::Color::Yellow, sf::Color::Magenta, sf::Color::Cyan,
            sf::Color(128, 128, 128), sf::Color(64, 64, 64), sf::Color(192, 192, 192)
        };
        
        // Find current color or use first as default
        int currentIndex = 0;
        for (size_t i = 0; i < colors.size(); i++) {
            if (colors[i].toInteger() == color.toInteger()) {
                currentIndex = i;
                break;
            }
        }
        
        currentIndex = (currentIndex + delta + colors.size()) % colors.size();
        color = colors[currentIndex];
    }
    
public:
    void draw(sf::RenderWindow& window, unsigned int windowWidth, unsigned int windowHeight) {
        if (!isVisible) return;
        
        // Semi-transparent overlay
        sf::RectangleShape overlay(sf::Vector2f(windowWidth, windowHeight));
        overlay.setFillColor(sf::Color(0, 0, 0, 128));
        window.draw(overlay);
        
        // Menu background
        float menuWidth = 400;
        float menuHeight = menuItems.size() * 30 + 40;
        float menuX = (windowWidth - menuWidth) / 2;
        float menuY = (windowHeight - menuHeight) / 2;
        
        sf::RectangleShape menuBg(sf::Vector2f(menuWidth, menuHeight));
        menuBg.setPosition(sf::Vector2f(menuX, menuY));
        menuBg.setFillColor(sf::Color(40, 40, 40));
        menuBg.setOutlineThickness(2);
        menuBg.setOutlineColor(sf::Color::White);
        window.draw(menuBg);
        
        // Title
        sf::Text title(menuFont, "Configuration Menu", 20);
        title.setFillColor(sf::Color::White);
        title.setPosition(sf::Vector2f(menuX + 10, menuY + 10));
        window.draw(title);
        
        // Menu items
        for (size_t i = 0; i < menuItems.size(); i++) {
            sf::Text item(menuFont, getMenuItemText(i), 16);
            item.setFillColor(i == selectedIndex ? sf::Color::Yellow : sf::Color::White);
            item.setPosition(sf::Vector2f(menuX + 10, menuY + 40 + i * 30));
            window.draw(item);
        }
    }
    
private:
    std::string getMenuItemText(int index) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1);
        
        switch(index) {
            case 0: oss << menuItems[index] << config.m; break;
            case 1: oss << menuItems[index] << config.n; break;
            case 2: oss << menuItems[index] << config.frameSize; break;
            case 3: oss << menuItems[index] << config.lineSize; break;
            case 4: oss << menuItems[index] << config.currentFontIndex; break;
            case 5: oss << menuItems[index] << config.currentFontHeaderIndex; break;
            case 6: oss << menuItems[index] << config.fontSize; break;
            case 7: oss << menuItems[index] << " (" << getColorName(config.bgColor) << ")"; break;
            case 8: oss << menuItems[index] << " (" << getColorName(config.textColor) << ")"; break;
            case 9: oss << menuItems[index] << " (" << getColorName(config.borderColor) << ")"; break;
            case 10: oss << menuItems[index] << " (" << getColorName(config.lineColor) << ")"; break;
            case 11: oss << menuItems[index] << " (" << getColorName(config.dirColor) << ")"; break;
            case 12: oss << menuItems[index] << " (" << getColorName(config.pageInfoColor) << ")"; break;
            default: oss << menuItems[index]; break;
        }
        
        return oss.str();
    }
    
    std::string getColorName(const sf::Color& color) {
        if (color == sf::Color::Black) return "Black";
        if (color == sf::Color::White) return "White";
        if (color == sf::Color::Red) return "Red";
        if (color == sf::Color::Green) return "Green";
        if (color == sf::Color::Blue) return "Blue";
        if (color == sf::Color::Yellow) return "Yellow";
        if (color == sf::Color::Magenta) return "Magenta";
        if (color == sf::Color::Cyan) return "Cyan";
        return "Custom";
    }
};

void setTextPosition(sf::Text& text, const sf::FloatRect& bounds, HAlign hAlign = HAlign::Center, VAlign vAlign = VAlign::Center, float hPadding = 10.0f, float vPadding = 10.0f) {
    auto localBounds = text.getLocalBounds();

    float x = 0.0f;
    float y = 0.0f;

    // Horizontal alignment
    switch (hAlign) {
        case HAlign::Left:
            x = bounds.position.x + hPadding - localBounds.position.x;
            break;
        case HAlign::Center:
            x = bounds.position.x + bounds.size.x / 2.0f - localBounds.position.x - localBounds.size.x / 2.0f;
            break;
        case HAlign::Right:
            x = bounds.position.x + bounds.size.x - hPadding - localBounds.position.x - localBounds.size.x;
            break;
    }

    // Vertical alignment
    switch (vAlign) {
        case VAlign::Top:
            y = bounds.position.y + vPadding - localBounds.position.y;
            break;
        case VAlign::Center:
            y = bounds.position.y + bounds.size.y / 2.0f - localBounds.position.y - localBounds.size.y / 2.0f;
            break;
        case VAlign::Bottom:
            y = bounds.position.y + bounds.size.y - vPadding - localBounds.size.y - localBounds.position.y;
            break;
    }

    text.setPosition(sf::Vector2f(x, y));
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <dir> [m rows] [n cols] [frame size] [bgcolor hex] [linecolor hex] [line size] [font index] [border hex] [text hex] [font size]\n";
        std::cerr << "Optimized for fast scanning like 'ls -lR'. Shows ALL files recursively with no depth limits.\n";
        std::cerr << "Controls: Arrow keys/PgUp/PgDn = navigate, R = rescan, M = menu, L = show log info, ESC = interrupt scan\n";
        return 1;
    }
    
    std::string targetDirectory = argv[1];
    fs::path absPath = fs::canonical(targetDirectory);
    std::string absoluteDirectory = absPath.string();
    
    // Initialize configuration with command line arguments or defaults
    AppConfig config;
    config.m = (argc >= 3) ? std::stoi(argv[2]) : 20;                        // m (rows)
    config.n = (argc >= 4) ? std::stoi(argv[3]) : 4;                         // n        
    config.frameSize = (argc >= 5) ? std::stoi(argv[4]) : 5.0f;              // frame size (border size)
    
    if (argc >= 6) ColorParse::hexToColor(argv[5], config.bgColor);
    if (argc >= 7) ColorParse::hexToColor(argv[6], config.lineColor);
    
    config.lineSize = argc >= 8 ? std::stof(argv[7]) : 2.f;                  // line size
    config.currentFontIndex = argc >= 9 ? std::stoi(argv[8]) : 1;            // font index
    config.currentFontHeaderIndex = argc >= 10 ? std::stoi(argv[9]) : 2;     // font index
    
    if (argc >= 11) ColorParse::hexToColor(argv[10], config.borderColor);
    if (argc >= 12) ColorParse::hexToColor(argv[11], config.textColor);
    
    config.fontSize = argc >= 13 ? std::stof(argv[12]) : 1.5f;               // font size multiplier

    auto desktop = sf::VideoMode::getDesktopMode();
    unsigned int width = desktop.size.x;
    unsigned int height = desktop.size.y - 37;

    sf::RenderWindow window(desktop, "", sf::State::Fullscreen);

    // Загрузка шрифтов
    std::vector<sf::Font> fonts;
    std::vector<std::string> fontNames;
    if (fs::exists("assets") && fs::is_directory("assets")) {
        for (const auto& entry : fs::directory_iterator("assets")) {
            if (entry.path().extension() == ".ttf") {
                sf::Font f;
                if (f.openFromFile(entry.path().string())) {
                    fonts.push_back(std::move(f));
                    fontNames.push_back(entry.path().filename().string());
                }
            }
        }
    }

    sf::Font font;
    sf::Font Headerfont;

    // Function to update fonts based on current indices
    auto updateFonts = [&]() {
        if (!fonts.empty() && config.currentFontIndex < fonts.size()) {
            font = fonts[config.currentFontIndex];
        } else {
            // Попытка загрузить системный шрифт
            fs::path defaultFont = fs::path("assets") / "Sansation-Regular.ttf";
            if (!font.openFromFile(defaultFont.string())) {
                std::cerr << "Warning: Could not load font\n";
                return false;
            }
        }
        
        if (!fonts.empty() && config.currentFontHeaderIndex < fonts.size()) {
            Headerfont = fonts[config.currentFontHeaderIndex];
        } else {
            // Попытка загрузить системный шрифт
            fs::path defaultFont = fs::path("assets") / "Sansation-Regular.ttf";
            if (!Headerfont.openFromFile(defaultFont.string())) {
                std::cerr << "Warning: Could not load font\n";
                return false;
            }
        }
        return true;
    };

    if (!updateFonts()) {
        return 1;
    }

    // Initialize configuration menu
    ConfigMenu configMenu(config, fonts);

    // >>> Pre-cache common glyphs to reduce runtime pressure
    std::string commonChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 _-./:()[]{}<>";
    auto preloadGlyphs = [&]() {
        unsigned int charSize = static_cast<unsigned int>(16 * config.fontSize);
        for (char c : commonChars) {
            (void)font.getGlyph(c, charSize, false, 0);
        }
    };
    preloadGlyphs();

    // Function to recalculate layout based on current config
    auto recalculateLayout = [&]() {
        return std::make_tuple(
            (width - config.frameSize * 2) / static_cast<float>(config.n),  // cellWidth
            (height - config.frameSize * 2) / static_cast<float>(config.m)  // cellHeight
        );
    };

    auto [cellWidth, cellHeight] = recalculateLayout();
    
    //2048
    
    float cellNameWidth = 1250.0f;
    float cellSizeWidth = 366.0f; 
    float cellDateWidth = 216.0f; 
    float cellPermWidth = 216.0f; 

    auto calcCellWidthByNumber = [&](int j){
        switch (j)
        {
            case -1: return 0.0f;
            case 0: return cellNameWidth;
            case 1: return cellNameWidth + cellSizeWidth;
            case 2: return cellNameWidth + cellSizeWidth + cellDateWidth;
            case 3: return cellNameWidth + cellSizeWidth + cellDateWidth + cellPermWidth;
        }
        return cellNameWidth + cellSizeWidth + cellDateWidth + cellPermWidth;
    };

    // Загрузка файлов (no depth limits - show all files)
    std::cout << "Scanning all files recursively (no depth limit)..." << std::endl;
    
    std::unique_ptr<FileManager> fileManagerPtr = std::make_unique<FileManager>(absoluteDirectory, &window);
    auto files = fileManagerPtr->getFiles();
    
    // Inform user about logging
    if (fileManagerPtr->isLoggingEnabled()) {
        std::cout << "Logging unreadable files to: " << fileManagerPtr->getLogFilePath() << std::endl;
    }
    
    // Пагинация
    int currentPage = 0;
    auto calculatePagination = [&]() {
        int itemsPerPage = (config.m - 1) * config.n; // Первая строка для заголовков
        int totalPages = (files.size() + itemsPerPage - 1) / itemsPerPage;
        return std::make_tuple(itemsPerPage, totalPages);
    };
    
    auto [itemsPerPage, totalPages] = calculatePagination();

    std::vector<sf::Text> headers;
    std::vector<sf::Text> cells;

    // Function to update headers
    auto updateHeaders = [&]() {
        headers.clear();
        std::vector<std::string> headersNames = {absoluteDirectory, "Size (data/allocated)", "Date", "Permissions"};
        unsigned int charSize = static_cast<unsigned int>(16 * config.fontSize);
        
        for (int j = 0; j < std::min(config.n, 4); j++) {
            sf::Text t(Headerfont, headersNames[j], config.fontSize);
            t.setFillColor(config.textColor);
            t.setCharacterSize(charSize + 4);
            
            float cellWidtht = calcCellWidthByNumber(j-1);
            float x = j < 4 ? config.frameSize + cellWidtht : config.frameSize + cellWidtht + j * cellWidth;
            sf::FloatRect cellBounds(
                sf::Vector2f(x, config.frameSize),
                sf::Vector2f(cellWidtht, cellHeight)
            );

            setTextPosition(t, cellBounds, HAlign::Left, VAlign::Center);
            headers.push_back(t);
        }
    };

    // Function to initialize cells
    auto initializeCells = [&]() {
        cells.clear();
        cells.reserve(itemsPerPage);
        unsigned int charSize = static_cast<unsigned int>(16 * config.fontSize);

        for (int i = 0; i < itemsPerPage; ++i) {
            sf::Text t(font, "", charSize);
            t.setFillColor(config.textColor);
            cells.push_back(std::move(t));
        }
    };

    auto updateCells = [&](int page) {
        int startIndex = page * itemsPerPage;
        unsigned int charSize = static_cast<unsigned int>(16 * config.fontSize);
        
        // Update cell character size
        for (auto& cell : cells) {
            cell.setCharacterSize(charSize);
        }

        for (int i = 0; i < config.m - 1; i++) {
            for (int j = 0; j < config.n; j++) {
                int idx = i * config.n + j;
                if (idx >= (int)cells.size()) {
                    continue;
                }
                int fileIndex = startIndex + i;
                if (fileIndex >= (int)files.size()) {
                    cells[idx].setString("");
                    continue;
                }
                const FileInfo& fileInfo = files[fileIndex];
                std::string text;
                
                switch (j) {
                    case 0: text = truncate(fs::path(fileInfo.name).filename().string()); break;
                    case 1: text = fileInfo.size; break;
                    case 2: text = fileInfo.date; break;
                    case 3: text = fileInfo.permissions; break;
                    default: text = "";
                }
                
                auto& t = cells[idx];
                t.setString(text);
                t.setFillColor(fileInfo.isDirectory ? config.dirColor : config.textColor);
                
                float cellWidtht = calcCellWidthByNumber(j-1);
                float x = j < 4 ? config.frameSize + cellWidtht : config.frameSize + cellWidtht + j * cellWidth;
                sf::FloatRect cellBounds(
                    sf::Vector2f(x, config.frameSize + (i + 1) * cellHeight),
                    sf::Vector2f(cellWidtht, cellHeight)
                );

                setTextPosition(t, cellBounds, HAlign::Left, VAlign::Center);
            }
        }
    };

    sf::Text pageInfo(font, "", config.fontSize);

    auto updatePageInfo = [&]() {
        std::ostringstream oss;
        oss << "Page " << (currentPage + 1) << "/" << totalPages
            << " | Files: " << files.size();
        
        // Add logging information if available
        if (fileManagerPtr->isLoggingEnabled()) {
            oss << " | Log: " << fs::path(fileManagerPtr->getLogFilePath()).filename().string();
        }
            //<< " | Dir: " << truncate(targetDirectory, 50);
        
        unsigned int charSize = static_cast<unsigned int>(16 * config.fontSize);
        sf::Text t(font, oss.str(), charSize - 2);
        t.setFillColor(config.pageInfoColor);
        
        sf::FloatRect cellBounds(
            sf::Vector2f(0, height - 40),
            sf::Vector2f(width, 40)
        );

        setTextPosition(t, cellBounds, HAlign::Center, VAlign::Center);
        pageInfo = t;
    };

    // Function to refresh everything when config changes
    auto refreshAll = [&]() {
        updateFonts();
        preloadGlyphs();
        auto [newCellWidth, newCellHeight] = recalculateLayout();
        cellWidth = newCellWidth;
        cellHeight = newCellHeight;
        auto [newItemsPerPage, newTotalPages] = calculatePagination();
        itemsPerPage = newItemsPerPage;
        totalPages = newTotalPages;
        
        // Adjust current page if necessary
        if (currentPage >= totalPages && totalPages > 0) {
            currentPage = totalPages - 1;
        }
        
        updateHeaders();
        initializeCells();
        updateCells(currentPage);
        updatePageInfo();
    };
    
    // Function to rescan directory
    auto rescanDirectory = [&]() {
        std::cout << "Rescanning directory (no depth limit)..." << std::endl;
        
        // Create new FileManager instance
        fileManagerPtr = std::make_unique<FileManager>(absoluteDirectory, &window);
        files = fileManagerPtr->getFiles();
        currentPage = 0;
        refreshAll();
    };

    // Initial setup
    refreshAll();

    // Cell editing state
    CellEditState editState;

    while (window.isOpen()) {
        while (auto eventOpt = window.pollEvent()) {
            if (!eventOpt) break;
            const sf::Event& event = *eventOpt;
            
            if (event.is<sf::Event::Closed>()) {
                window.close();
            }
            
            // Handle text input during editing
            if (editState.isEditing && event.is<sf::Event::TextEntered>()) {
                if (const auto* textEntered = event.getIf<sf::Event::TextEntered>()) {
                    if (textEntered->unicode < 128) {
                        char c = static_cast<char>(textEntered->unicode);
                        
                        // Handle backspace
                        if (c == '\b') {
                            if (!editState.currentValue.empty()) {
                                editState.currentValue.pop_back();
                            }
                        }
                        // Handle enter - save changes
                        else if (c == '\r' || c == '\n') {
                            // Apply changes
                            int fileIndex = currentPage * itemsPerPage + editState.row;
                            if (fileIndex < (int)files.size() && editState.currentValue != editState.originalValue) {
                                const FileInfo& fileInfo = files[fileIndex];
                                
                                // Only allow editing name (column 0) and permissions (column 3)
                                if (editState.column == 0 || editState.column == 3) {
                                    if (fileManagerPtr->updateFileMetadata(fileInfo.name, editState.column, editState.currentValue)) {
                                        std::cout << "Successfully updated " << fileInfo.name << std::endl;
                                        // Refresh the files list
                                        files = fileManagerPtr->getFiles();
                                        updateCells(currentPage);
                                    } else {
                                        std::cout << "Failed to update " << fileInfo.name << std::endl;
                                    }
                                }
                            }
                            editState.reset();
                        }
                        // Handle escape - cancel editing
                        else if (c == 27) {
                            editState.reset();
                        }
                        // Add printable characters
                        else if (c >= 32 && c < 127) {
                            editState.currentValue += c;
                        }
                    }
                }
            }
            
            if (event.is<sf::Event::KeyPressed>()) {
                if(const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()){
                    // Handle escape during editing
                    if (editState.isEditing && keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
                        editState.reset();
                        continue;
                    }
                    
                    // Handle menu first if it's open
                    if (configMenu.getVisible()) {
                        configMenu.handleInput(keyPressed->scancode);
                        
                        // Refresh display if menu is still open (values might have changed)
                        if (configMenu.getVisible()) {
                            refreshAll();
                        }
                        continue; // Don't process other keys while menu is open
                    }
                    
                    // Don't process other keys during editing
                    if (editState.isEditing) {
                        continue;
                    }
                    
                    // Toggle menu with M key
                    if (keyPressed->scancode == sf::Keyboard::Scancode::M) {
                        configMenu.toggle();
                        continue;
                    }
                    
                    // Regular navigation
                    if (keyPressed->scancode == sf::Keyboard::Scancode::Right || keyPressed->scancode == sf::Keyboard::Scancode::Down) {
                        if (currentPage < totalPages - 1) {
                            currentPage++;
                            updateCells(currentPage);
                            updatePageInfo();
                        }
                    }
                    else if (keyPressed->scancode == sf::Keyboard::Scancode::Left || keyPressed->scancode == sf::Keyboard::Scancode::Up) {
                        if (currentPage > 0) {
                            currentPage--;
                            updateCells(currentPage);
                            updatePageInfo();
                        }
                    }
                    // Reset
                    else if (keyPressed->scancode == sf::Keyboard::Scancode::R) {
                        // Перезагрузка файлов
                        rescanDirectory();
                    }
                    // Show log info
                    else if (keyPressed->scancode == sf::Keyboard::Scancode::L) {
                        if (fileManagerPtr->isLoggingEnabled()) {
                            std::cout << "Log file location: " << fileManagerPtr->getLogFilePath() << std::endl;
                            std::cout << "Use 'tail -f " << fileManagerPtr->getLogFilePath() << "' to monitor in real-time" << std::endl;
                            std::cout << "Note: If log file is not accessible, messages are displayed in console" << std::endl;
                        }
                    }
                }   
            }

            // Handle mouse clicks for cell editing
            if (event.is<sf::Event::MouseButtonPressed>() && !configMenu.getVisible() && !editState.isEditing) {
                if (const auto* mouseButtonPressed = event.getIf<sf::Event::MouseButtonPressed>()) {
                    if (mouseButtonPressed->button == sf::Mouse::Button::Left) {
                        sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                        
                        // Check if click is within table bounds
                        if (mousePos.x >= config.frameSize && mousePos.x <= width - config.frameSize &&
                            mousePos.y >= config.frameSize + cellHeight && mousePos.y <= height - config.frameSize - 40) {
                            
                            // Calculate which cell was clicked
                            float relativeY = mousePos.y - config.frameSize - cellHeight;
                            int row = static_cast<int>(relativeY / cellHeight);
                            
                            // Determine column based on x position
                            int column = -1;
                            float relativeX = mousePos.x - config.frameSize;
                            
                            if (relativeX < cellNameWidth) {
                                column = 0;
                            } else if (relativeX < cellNameWidth + cellSizeWidth) {
                                column = 1;
                            } else if (relativeX < cellNameWidth + cellSizeWidth + cellDateWidth) {
                                column = 2;
                            } else if (relativeX < cellNameWidth + cellSizeWidth + cellDateWidth + cellPermWidth) {
                                column = 3;
                            }
                            
                            if (row >= 0 && row < config.m - 1 && column >= 0) {
                                int fileIndex = currentPage * itemsPerPage + row;
                                
                                if (fileIndex < (int)files.size()) {
                                    const FileInfo& fileInfo = files[fileIndex];
                                    
                                    // Only allow editing name (column 0) and permissions (column 3)
                                    if (column == 0 || column == 3) {
                                        editState.isEditing = true;
                                        editState.row = row;
                                        editState.column = column;
                                        
                                        // Get original value
                                        if (column == 0) {
                                            editState.originalValue = fs::path(fileInfo.name).filename().string();
                                        } else if (column == 3) {
                                            editState.originalValue = fileInfo.permissions;
                                        }
                                        
                                        editState.currentValue = editState.originalValue;
                                        
                                        std::cout << "Editing cell [" << row << ", " << column << "]: " 
                                                  << editState.originalValue << std::endl;
                                        std::cout << "Press Enter to save, Escape to cancel" << std::endl;
                                    } else {
                                        std::cout << "Column " << column << " is read-only (only Name and Permissions can be edited)" << std::endl;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (event.is<sf::Event::MouseWheelScrolled>()) {
                if (const auto* mouseWheelScrolled = event.getIf<sf::Event::MouseWheelScrolled>())
                {
                    // Don't handle mouse wheel if menu is open or editing
                    if (configMenu.getVisible() || editState.isEditing) continue;
                    
                    if (mouseWheelScrolled->wheel == sf::Mouse::Wheel::Vertical) {
                        if (mouseWheelScrolled->delta > 0) {
                            // Прокрутка вверх
                            if (currentPage > 0) {
                                currentPage--;
                                updateCells(currentPage);
                                updatePageInfo();
                            }
                        } else {
                            // Прокрутка вниз
                            if (currentPage < totalPages - 1) {
                                currentPage++;
                                updateCells(currentPage);
                                updatePageInfo();
                            }
                        }
                    }
                }
            }
        }

        window.clear(config.bgColor);

        sf::RectangleShape topBorder(sf::Vector2f(width, (float)config.frameSize));
        topBorder.setPosition(sf::Vector2f(0, 0));
        topBorder.setFillColor(config.borderColor);
        window.draw(topBorder);

        sf::RectangleShape bottomBorder(sf::Vector2f(width, (float)config.frameSize));
        bottomBorder.setPosition(sf::Vector2f(0, (float)height - config.frameSize));
        bottomBorder.setFillColor(config.borderColor);
        window.draw(bottomBorder);
        
        sf::RectangleShape leftBorder(sf::Vector2f((float)config.frameSize, height));
        leftBorder.setPosition(sf::Vector2f(0, (float)config.frameSize));
        leftBorder.setFillColor(config.borderColor);
        window.draw(leftBorder);
        
        sf::RectangleShape rightBorder(sf::Vector2f((float)config.frameSize, height));
        rightBorder.setPosition(sf::Vector2f(width - config.frameSize, (float)config.frameSize));
        rightBorder.setFillColor(config.borderColor);
        window.draw(rightBorder);

        for (int i = 1; i < config.m; i++) {
            sf::RectangleShape line({(float)width - config.frameSize * 2, (float)config.lineSize});
            line.setFillColor(config.lineColor);
            float y = config.frameSize + i * cellHeight;
            line.setPosition(sf::Vector2f(config.frameSize, y));
            window.draw(line);
        }

        for (int j = 1; j <= config.n; j++) {
            sf::RectangleShape vline({(float)config.lineSize, (float)height - config.frameSize * 2});
            vline.setFillColor(config.lineColor);
            float cellWidtht = calcCellWidthByNumber(j-1);
            float cellCalcWidth = calcCellWidthByNumber(j-1);
            float x = j <= 4 ? config.frameSize + cellCalcWidth : config.frameSize + cellCalcWidth + j * cellWidth;
            vline.setPosition(sf::Vector2f(x, config.frameSize));
            window.draw(vline);
        }

        for (auto& t : headers) {
            window.draw(t);
        }
        
        for (auto& t : cells) {
            window.draw(t);
        }
        
        // Draw editing overlay if editing
        if (editState.isEditing && editState.row >= 0 && editState.column >= 0) {
            // Calculate cell bounds
            float cellWidtht = 0;
            switch (editState.column) {
                case 0: cellWidtht = cellNameWidth; break;
                case 1: cellWidtht = cellSizeWidth; break;
                case 2: cellWidtht = cellDateWidth; break;
                case 3: cellWidtht = cellPermWidth; break;
            }
            
            float cellX = config.frameSize + calcCellWidthByNumber(editState.column - 1);
            float cellY = config.frameSize + (editState.row + 1) * cellHeight;
            
            // Draw highlight background
            sf::RectangleShape editHighlight(sf::Vector2f(cellWidtht, cellHeight));
            editHighlight.setPosition(sf::Vector2f(cellX, cellY));
            editHighlight.setFillColor(sf::Color(100, 100, 200, 128));
            editHighlight.setOutlineColor(sf::Color::Yellow);
            editHighlight.setOutlineThickness(2);
            window.draw(editHighlight);
            
            // Draw edited text
            unsigned int charSize = static_cast<unsigned int>(16 * config.fontSize);
            sf::Text editText(font, editState.currentValue, charSize);
            editText.setFillColor(sf::Color::White);
            
            sf::FloatRect cellBounds(
                sf::Vector2f(cellX, cellY),
                sf::Vector2f(cellWidtht, cellHeight)
            );
            setTextPosition(editText, cellBounds, HAlign::Left, VAlign::Center);
            window.draw(editText);
            
            // Draw blinking cursor
            if (editState.cursorBlink.getElapsedTime().asMilliseconds() % 1000 < 500) {
                auto textBounds = editText.getGlobalBounds();
                sf::RectangleShape cursor(sf::Vector2f(2, cellHeight - 10));
                cursor.setPosition(sf::Vector2f(textBounds.position.x + textBounds.size.x + 2, cellY + 5));
                cursor.setFillColor(sf::Color::White);
                window.draw(cursor);
            }
            
            // Draw instruction text at bottom
            sf::Text instructions(font, "Editing: Enter to save, Escape to cancel", static_cast<unsigned int>(14 * config.fontSize));
            instructions.setFillColor(sf::Color::Yellow);
            instructions.setPosition(sf::Vector2f(config.frameSize + 10, height - 60));
            window.draw(instructions);
        }
        
        // Рисуем информацию о странице
        window.draw(pageInfo);

        // Draw configuration menu if visible
        configMenu.draw(window, width, height);

        window.display();
    }

    return 0;
}