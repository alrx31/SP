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
};

// Функция для получения размера директории
std::uintmax_t getDirectorySize(const fs::path& path, FileAccessLogger* logger = nullptr) {
    std::uintmax_t size = 0;
    std::error_code ec;
    
    for (const auto& entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) {
            if (logger) {
                logger->logSystemError(path.string(), "directory_iteration", ec);
            }
            continue; // Skip if we can't access
        }
        
        if (entry.is_regular_file(ec) && !ec) {
            auto fileSize = entry.file_size(ec);
            if (!ec) {
                size += fileSize;
            } else {
                if (logger) {
                    logger->logSystemError(entry.path().string(), "file_size", ec);
                }
            }
        } else if (ec) {
            if (logger) {
                logger->logSystemError(entry.path().string(), "file_type_check", ec);
            }
        }
    }
    return size;
}

// Функция для получения прав доступа к файлу
std::string getFilePermissions(const fs::path& path, FileAccessLogger* logger = nullptr) {
    try {
        auto status = fs::status(path);
        auto perms = fs::status(path).permissions();
        std::string result;

        if (fs::is_directory(status)) {
            result += 'd';
        } else if (fs::is_symlink(status)) {
            result += 'l';
        } else if (fs::is_regular_file(status)) {
            result += '-';
        } else if (fs::is_block_file(status)) {
            result += 'b';
        } else if (fs::is_character_file(status)) {
            result += 'c';
        } else if (fs::is_fifo(status)) {
            result += 'p';
        } else if (fs::is_socket(status)) {
            result += 's';
        } else {
            result += '?';
        }

        // Владелец
        result += (perms & fs::perms::owner_read) != fs::perms::none? "r" : "-";
        result += (perms & fs::perms::owner_write) != fs::perms::none? "w" : "-";
        result += (perms & fs::perms::owner_exec) != fs::perms::none? "x" : "-";

        // Группа
        result += (perms & fs::perms::group_read) != fs::perms::none? "r" : "-";
        result += (perms & fs::perms::group_write) != fs::perms::none? "w" : "-";
        result += (perms & fs::perms::group_exec) != fs::perms::none? "x" : "-";

        // Остальные — с учётом sticky bit
        bool has_others_exec = (perms & fs::perms::others_exec) != fs::perms::none;
        bool has_sticky_bit = (perms & fs::perms::sticky_bit) != fs::perms::none;

        result += (perms & fs::perms::others_read) != fs::perms::none? "r" : "-";
        result += (perms & fs::perms::others_write) != fs::perms::none? "w" : "-";

        // Заменяем последний символ (others_exec) на t/T, если есть sticky bit
        if (has_sticky_bit) {
            result += has_others_exec ? "t" : "T";
        } else {
            result += has_others_exec ? "x" : "-";
        }

        return result;
    } catch (const std::filesystem::filesystem_error& e) {
        if (logger) {
            logger->logUnreadableFile(path.string(), "permissions_check", e.what());
        }
        return "----------";
    } catch (const std::exception& e) {
        if (logger) {
            logger->logUnreadableFile(path.string(), "permissions_check", e.what());
        }
        return "----------";
    }
}

// Функция для форматирования даты
std::string formatDate(const fs::file_time_type& ftime, FileAccessLogger* logger = nullptr, const std::string& filePath = "") {
    try {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
        std::tm* tm = std::localtime(&cftime);
        
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << tm->tm_mday << "."
            << std::setfill('0') << std::setw(2) << (tm->tm_mon + 1) << "."
            << (tm->tm_year + 1900);
        
        return oss.str();
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
    
public:
    FileManager(const std::string& path) : directoryPath(path) {
        // Initialize logger
        try {
            logger = std::make_unique<FileAccessLogger>();
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not initialize file access logger: " << e.what() << std::endl;
            logger = nullptr;
        }
        loadFiles();
    }
    
    void loadFiles() {
        files.clear();
        std::error_code ec;
        
        try {
            if (!fs::exists(directoryPath, ec) || ec) {
                if (logger) {
                    if (ec) {
                        logger->logSystemError(directoryPath, "directory_exists_check", ec);
                    } else {
                        logger->logFileNotFound(directoryPath, "directory_access");
                    }
                }
                return;
            }
            
            if (!fs::is_directory(directoryPath, ec) || ec) {
                if (logger && ec) {
                    logger->logSystemError(directoryPath, "directory_type_check", ec);
                }
                return;
            }
            
            // Try to create directory iterator - this will catch permission denied errors
            try {
                // First, try to access the directory with a simple directory_iterator (non-recursive)
                // to catch permission denied errors early
                fs::directory_iterator testIter(directoryPath, ec);
                if (ec) {
                    if (logger) {
                        logger->logSystemError(directoryPath, "directory_access_test", ec);
                    }
                    std::cerr << "Cannot access directory: " << directoryPath << " - " << ec.message() << std::endl;
                    return;
                }
                
                // If basic access works, proceed with recursive iteration
                // Use a queue-based approach to manually handle recursion and catch all permission errors
                std::queue<fs::path> dirsToProcess;
                dirsToProcess.push(directoryPath);
                
                while (!dirsToProcess.empty()) {
                    fs::path currentDir = dirsToProcess.front();
                    dirsToProcess.pop();
                    
                    std::error_code ec;
                    
                    try {
                        for (const auto& entry : fs::directory_iterator(currentDir, ec)) {
                            if (ec) {
                                if (logger) {
                                    logger->logSystemError(currentDir.string(), "directory_iteration", ec);
                                }
                                break; // Stop processing this directory
                            }
                            
                            FileInfo info;
                            info.name = entry.path().string();
                            info.isDirectory = entry.is_directory(ec);
                            
                            if (ec) {
                                if (logger) {
                                    logger->logSystemError(entry.path().string(), "directory_type_check", ec);
                                }
                                info.isDirectory = false;
                                ec.clear();
                            }
                            
                            // Get file size
                            if (info.isDirectory) {
                                try {
                                    auto dirSize = getDirectorySize(entry.path(), logger.get());
                                    info.size = std::to_string(dirSize);
                                } catch (const std::exception& e) {
                                    if (logger) {
                                        logger->logUnreadableFile(entry.path().string(), "directory_size_calculation", e.what());
                                    }
                                    info.size = "0";
                                }
                                
                                // Try to add directory to queue for recursive processing
                                // But first test if we can actually access it
                                std::error_code testEc;
                                try {
                                    fs::directory_iterator testIter(entry.path(), testEc);
                                    if (testEc) {
                                        if (logger) {
                                            logger->logSystemError(entry.path().string(), "subdirectory_access_test", testEc);
                                        }
                                    } else {
                                        dirsToProcess.push(entry.path());
                                    }
                                } catch (const std::filesystem::filesystem_error& fsErr) {
                                    if (logger) {
                                        logger->logUnreadableFile(entry.path().string(), "subdirectory_access", fsErr.what());
                                    }
                                } catch (const std::exception& e) {
                                    if (logger) {
                                        logger->logUnreadableFile(entry.path().string(), "subdirectory_test", e.what());
                                    }
                                }
                            } else {
                                auto fileSize = entry.file_size(ec);
                                if (ec) {
                                    if (logger) {
                                        logger->logSystemError(entry.path().string(), "file_size", ec);
                                    }
                                    info.size = "0";
                                    ec.clear();
                                } else {
                                    info.size = std::to_string(fileSize);
                                }
                            }
                            
                            // Get file modification time
                            auto ftime = entry.last_write_time(ec);
                            if (ec) {
                                if (logger) {
                                    logger->logSystemError(entry.path().string(), "last_write_time", ec);
                                }
                                info.date = "01.01.1970";
                                ec.clear();
                            } else {
                                info.date = formatDate(ftime, logger.get(), entry.path().string());
                            }
                            
                            // Get file permissions
                            info.permissions = getFilePermissions(entry.path(), logger.get());
                            
                            files.push_back(info);
                        }
                    } catch (const std::filesystem::filesystem_error& fsErr) {
                        if (logger) {
                            logger->logUnreadableFile(currentDir.string(), "directory_access", fsErr.what());
                        }
                    } catch (const std::exception& e) {
                        if (logger) {
                            logger->logUnreadableFile(currentDir.string(), "directory_processing", e.what());
                        }
                    }
                }
            } catch (const std::filesystem::filesystem_error& fsErr) {
                // This catches permission denied and other filesystem errors when creating the iterator
                if (logger) {
                    logger->logUnreadableFile(directoryPath, "directory_access", fsErr.what());
                }
                std::cerr << "Cannot access directory: " << fsErr.what() << std::endl;
                return;
            } catch (const std::exception& e) {
                if (logger) {
                    logger->logUnreadableFile(directoryPath, "directory_iterator_creation", e.what());
                }
                std::cerr << "Error creating directory iterator: " << e.what() << std::endl;
                return;
            }            // Сортировка: сначала каталоги, потом файлы
            std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) {
                if (a.isDirectory != b.isDirectory) {
                    return a.isDirectory > b.isDirectory;
                }
                return a.name < b.name;
            });
            
        } catch (const std::filesystem::filesystem_error& e) {
            if (logger) {
                logger->logUnreadableFile(directoryPath, "filesystem_operation", e.what());
            }
            std::cerr << "Filesystem error reading directory: " << e.what() << std::endl;
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
    int n = 4;                            // columns
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
        return 1;
    }
    
    std::string targetDirectory = argv[1];
    fs::path absPath = fs::absolute(targetDirectory);
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
    
    float cellNameWidth = 1400.0f;
    float cellSizeWidth = 216.0f; 
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

    // Загрузка файлов
    FileManager fileManager(targetDirectory);
    const auto& files = fileManager.getFiles();
    
    // Inform user about logging
    if (fileManager.isLoggingEnabled()) {
        std::cout << "Logging unreadable files to: " << fileManager.getLogFilePath() << std::endl;
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
        std::vector<std::string> headersNames = {absoluteDirectory, "Size (bytes)", "Date", "Permissions"};
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
        if (fileManager.isLoggingEnabled()) {
            oss << " | Log: " << fs::path(fileManager.getLogFilePath()).filename().string();
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

    // Initial setup
    refreshAll();

    while (window.isOpen()) {
        while (auto eventOpt = window.pollEvent()) {
            if (!eventOpt) break;
            const sf::Event& event = *eventOpt;
            
            if (event.is<sf::Event::Closed>()) {
                window.close();
            }
            
            if (event.is<sf::Event::KeyPressed>()) {
                if(const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()){
                    // Handle menu first if it's open
                    if (configMenu.getVisible()) {
                        configMenu.handleInput(keyPressed->scancode);
                        
                        // Refresh display if menu is still open (values might have changed)
                        if (configMenu.getVisible()) {
                            refreshAll();
                        }
                        continue; // Don't process other keys while menu is open
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
                        fileManager.loadFiles();
                        currentPage = 0;
                        refreshAll();
                    }
                    // Show log info
                    else if (keyPressed->scancode == sf::Keyboard::Scancode::L) {
                        if (fileManager.isLoggingEnabled()) {
                            std::cout << "Log file location: " << fileManager.getLogFilePath() << std::endl;
                            std::cout << "Use 'tail -f " << fileManager.getLogFilePath() << "' to monitor in real-time" << std::endl;
                            std::cout << "Note: If log file is not accessible, messages are displayed in console" << std::endl;
                        }
                    }
                }   
            }

            if (event.is<sf::Event::MouseWheelScrolled>()) {
                if (const auto* mouseWheelScrolled = event.getIf<sf::Event::MouseWheelScrolled>())
                {
                    // Don't handle mouse wheel if menu is open
                    if (configMenu.getVisible()) continue;
                    
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
        
        // Рисуем информацию о странице
        window.draw(pageInfo);

        // Draw configuration menu if visible
        configMenu.draw(window, width, height);

        window.display();
    }

    return 0;
}