#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <optional>
#include <cstdint>
#include <algorithm>

namespace fs = std::filesystem;

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

// Функция для получения прав доступа к файлу
std::string getFilePermissions(const fs::path& path) {
    try {
        auto perms = fs::status(path).permissions();
        std::string result;
        
        // Владелец
        result += (perms & fs::perms::owner_read) != fs::perms::none ? "r" : "-";
        result += (perms & fs::perms::owner_write) != fs::perms::none ? "w" : "-";
        result += (perms & fs::perms::owner_exec) != fs::perms::none ? "x" : "-";
        
        // Группа
        result += (perms & fs::perms::group_read) != fs::perms::none ? "r" : "-";
        result += (perms & fs::perms::group_write) != fs::perms::none ? "w" : "-";
        result += (perms & fs::perms::group_exec) != fs::perms::none ? "x" : "-";
        
        // Остальные
        result += (perms & fs::perms::others_read) != fs::perms::none ? "r" : "-";
        result += (perms & fs::perms::others_write) != fs::perms::none ? "w" : "-";
        result += (perms & fs::perms::others_exec) != fs::perms::none ? "x" : "-";
        
        return result;
    } catch (...) {
        return "----------";
    }
}

// Функция для форматирования даты
std::string formatDate(const fs::file_time_type& ftime) {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
    std::tm* tm = std::localtime(&cftime);
    
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << tm->tm_mday << "."
        << std::setfill('0') << std::setw(2) << (tm->tm_mon + 1) << "."
        << (tm->tm_year + 1900);
    
    return oss.str();
}

class FileManager {
private:
    std::vector<FileInfo> files;
    std::string directoryPath;
    
public:
    FileManager(const std::string& path) : directoryPath(path) {
        loadFiles();
    }
    
    void loadFiles() {
        files.clear();
        try {
            if (fs::exists(directoryPath) && fs::is_directory(directoryPath)) {
                for (const auto& entry : fs::recursive_directory_iterator(directoryPath)) {
                    FileInfo info;
                    info.name = entry.path().string();
                    info.isDirectory = entry.is_directory();
                    
                    if (info.isDirectory) {
                        info.size = "<DIR>";
                    } else {
                        try {
                            auto fileSize = fs::file_size(entry.path());
                            info.size = std::to_string(fileSize);
                        } catch (...) {
                            info.size = "0";
                        }
                    }
                    
                    try {
                        auto ftime = fs::last_write_time(entry.path());
                        info.date = formatDate(ftime);
                    } catch (...) {
                        info.date = "01.01.1970";
                    }
                    
                    info.permissions = getFilePermissions(entry.path());
                    
                    files.push_back(info);
                }
                
                // Сортировка: сначала каталоги, потом файлы
                std::sort(files.begin(), files.end(), [](const FileInfo& a, const FileInfo& b) {
                    if (a.isDirectory != b.isDirectory) {
                        return a.isDirectory > b.isDirectory;
                    }
                    return a.name < b.name;
                });
            }
        } catch (const std::exception& e) {
            std::cerr << "Error reading directory: " << e.what() << std::endl;
        }
    }
    
    const std::vector<FileInfo>& getFiles() const {
        return files;
    }
    
    size_t getFileCount() const {
        return files.size();
    }
};
// >>> Helper: Truncate string with ellipsis
std::string truncate(const std::string& str, size_t maxLen = 35) {
    if (str.length() <= maxLen) return str;
    return str.substr(0, maxLen - 3) + "...";
}

enum class HAlign { Left, Center, Right };
enum class VAlign { Top, Center, Bottom };

void setTextPosition(sf::Text& text, const sf::FloatRect& bounds, HAlign hAlign = HAlign::Center, VAlign vAlign = VAlign::Center, float hPadding = 0.0f, float vPadding = 0.0f) {
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
    int m = (argc >= 3) ? std::stoi(argv[2]) : 20;                        // m (rows)
    int n = (argc >= 4) ? std::stoi(argv[3]) : 4;                                            // n        
    float frameSize = (argc >= 5) ? std::stoi(argv[4]) : 10.0f;             // frame size (border size)
    
    sf::Color borderColor = sf::Color::Red;                               // border color
    sf::Color textColor = sf::Color::Red;                                 // text color
    sf::Color bgColor = sf::Color::Black;                                 // bg color
    sf::Color lineColor = sf::Color::White;                               // line color
    sf::Color dirColor = sf::Color::Cyan;                               // line color
    sf::Color pageInfoColor = sf::Color::Green;                               // line color
    
    if (argc >= 6) ColorParse::hexToColor(argv[5], bgColor);
    if (argc >= 7) ColorParse::hexToColor(argv[6], lineColor);
    
    float lineSize = argc >= 8 ? std::stof(argv[7]) : 2.f;               // line size
    size_t currentFontIndex = argc >= 9 ? std::stoi(argv[8]) : 1;         // font index
    
    if (argc >= 10) ColorParse::hexToColor(argv[9], borderColor);
    if (argc >= 11) ColorParse::hexToColor(argv[10], textColor);
    
    float fontSize = argc >= 12 ? std::stof(argv[11]) : 1.5f ;               // font size multiplier
    unsigned int charSize = static_cast<unsigned int>(16 * fontSize);      // Base size 16

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
    if (!fonts.empty() && currentFontIndex < fonts.size()) {
        font = fonts[currentFontIndex];
    } else {
        // Попытка загрузить системный шрифт
        fs::path defaultFont = fs::path("assets") / "Sansation-Regular.ttf";
        if (!font.openFromFile(defaultFont.string())) {
            std::cerr << "Warning: Could not load font\n";
            return 1;
        }
    }

    // >>> Pre-cache common glyphs to reduce runtime pressure
    std::string commonChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 _-./:()[]{}<>";
    for (char c : commonChars) {
        (void)font.getGlyph(c, charSize, false, 0);
    }

    float cellWidth = (width - frameSize * 2) / static_cast<float>(n);
    float cellHeight = (height - frameSize * 2) / static_cast<float>(m);

    // Загрузка файлов
    FileManager fileManager(targetDirectory);
    const auto& files = fileManager.getFiles();
    
    // Пагинация
    int currentPage = 0;
    int itemsPerPage = (m - 1) * n; // Первая строка для заголовков
    int totalPages = (files.size() + itemsPerPage - 1) / itemsPerPage;

    std::vector<sf::Text> headers;

    {
        std::vector<std::string> headersNames = {"Name", "Size (bytes)", "Date", "Permissions"};
        
        for (int j = 0; j < std::min(n, 4); j++) {
            sf::Text t(font, headersNames[j], fontSize);
            t.setFillColor(textColor);
            t.setCharacterSize(charSize + 4);
            
            //t.setPosition(sf::Vector2f(cx, cy));
            
            sf::FloatRect cellBounds(
                sf::Vector2f(frameSize + j * cellWidth,frameSize),
                sf::Vector2f(cellWidth,cellHeight)
            );

            setTextPosition(t,cellBounds,HAlign::Center,VAlign::Center);

            headers.push_back(t);
        }
    };

    std::vector<sf::Text> cells;

    cells.clear();
    cells.reserve(itemsPerPage); // optional optimization

    for (int i = 0; i < itemsPerPage; ++i) {
        sf::Text t(font,"", charSize);
        t.setFillColor(textColor);
        cells.push_back(std::move(t));
    }

    auto updateCells = [&](int page) {
        int startIndex = page * itemsPerPage;
        cells.clear();
        cells.reserve(itemsPerPage);
        
        for (int i = 0; i < itemsPerPage; ++i) {
            sf::Text t(font,"", charSize);
            t.setFillColor(textColor);
            cells.push_back(std::move(t));
        }

        for (int i = 0; i < m - 1; i++) {
            for (int j = 0; j < n; j++) {
                int idx = i * n + j;
                int fileIndex = startIndex + idx;

                if (fileIndex >= (int)files.size()) {
                    cells[idx].setString(""); // hide unused
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
                t.setFillColor(fileInfo.isDirectory ? dirColor : textColor);
                
                //t.setOrigin(sf::Vector2f(bounds.position.x + bounds.size.x / 2.f,

                sf::FloatRect cellBounds(
                    sf::Vector2f(frameSize + j * cellWidth,frameSize + (i + 1) * cellHeight),
                    sf::Vector2f(cellWidth,cellHeight)
                );

                setTextPosition(t,cellBounds,HAlign::Center,VAlign::Center);

                cells.push_back(t);
            }
        }
    };

    sf::Text pageInfo(font, "", fontSize);

    auto updatePageInfo = [&]() {
        std::ostringstream oss;
        oss << "Page " << (currentPage + 1) << "/" << totalPages
            << " | Files: " << files.size()
            << " | Dir: " << truncate(targetDirectory, 50);
        
        sf::Text t(font, oss.str(), charSize - 2);
        t.setFillColor(pageInfoColor);
        
        auto bounds = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(bounds.position.x / 2.f, bounds.position.y / 2.f));
        t.setPosition(sf::Vector2f(width / 2.f, height - 20));

        sf::FloatRect cellBounds(
            sf::Vector2f(0, height - 40),
            sf::Vector2f(width, 40)
        );

        setTextPosition(t,cellBounds,HAlign::Center,VAlign::Center);
    };

    // Initial render
    updateCells(currentPage);
    updatePageInfo();

    while (window.isOpen()) {
        while (auto eventOpt = window.pollEvent()) {
            if (!eventOpt) break;
            const sf::Event& event = *eventOpt;
            
            if (event.is<sf::Event::Closed>()) {
                window.close();
            }
            
            if (event.is<sf::Event::KeyPressed>()) {
                if(const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()){
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
                        totalPages = (fileManager.getFileCount() + itemsPerPage - 1) / itemsPerPage;
                        updateCells(currentPage);
                        updatePageInfo();
                    }
                }   
            }

            if (event.is<sf::Event::MouseWheelScrolled>()) {
                if (const auto* mouseWheelScrolled = event.getIf<sf::Event::MouseWheelScrolled>())
                {
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

        window.clear(bgColor);

        sf::RectangleShape topBorder(sf::Vector2f(width, (float)frameSize));
        topBorder.setPosition(sf::Vector2f(0, 0));
        topBorder.setFillColor(borderColor);
        window.draw(topBorder);

        sf::RectangleShape bottomBorder(sf::Vector2f(width, (float)frameSize));
        bottomBorder.setPosition(sf::Vector2f(0, (float)height - frameSize));
        bottomBorder.setFillColor(borderColor);
        window.draw(bottomBorder);
        
        sf::RectangleShape leftBorder(sf::Vector2f((float)frameSize, height));
        leftBorder.setPosition(sf::Vector2f(0, (float)frameSize));
        leftBorder.setFillColor(borderColor);
        window.draw(leftBorder);
        
        sf::RectangleShape rightBorder(sf::Vector2f((float)frameSize, height));
        rightBorder.setPosition(sf::Vector2f(width - frameSize, (float)frameSize));
        rightBorder.setFillColor(borderColor);
        window.draw(rightBorder);

        for (int i = 1; i < m; i++) {
            sf::RectangleShape line({(float)width - frameSize * 2, (float)lineSize});
            line.setFillColor(lineColor);
            float y = frameSize + i * cellHeight;
            line.setPosition(sf::Vector2f(frameSize,y));
            window.draw(line);
        }

        for (int j = 1; j < n; j++) {
            sf::RectangleShape vline({(float)lineSize, (float)height - frameSize * 2});
            vline.setFillColor(lineColor);
            float x = frameSize + j * cellWidth;
            vline.setPosition(sf::Vector2f(x, frameSize));
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

        window.display();
    }

    return 0;
}