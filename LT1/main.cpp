#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <optional>
#include <cstdint>

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

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <m rows> <n cols> [frame size] [bgcolor hex] [linecolor hex] [line size] [font index] [border hex] [text hex] [font size]\n";
        return 1;
    }
    
    int m = std::stoi(argv[1]);                                           // m
    int n = std::stoi(argv[2]);                                           // n        
    int frameSize = (argc >= 4) ? std::stoi(argv[3]) : 10.0f;             // frame size (border size)
    
    sf::Color borderColor = sf::Color::Red;                               // border color
    sf::Color textColor = sf::Color::Red;                                 // text color
    sf::Color bgColor = sf::Color::Black;                                 // bg color
    sf::Color lineColor = sf::Color::White;                               // line color
    
    if (argc >= 5) ColorParse::hexToColor(argv[4], bgColor);
    if (argc >= 6) ColorParse::hexToColor(argv[5], lineColor);
    
    float lineSize = argc >= 7 ? std::stof(argv[6]) : 10.f;               // line size
    size_t currentFontIndex = argc >= 8 ? std::stoi(argv[7]) : 1;         // font index
    
    if (argc >= 9) ColorParse::hexToColor(argv[8], borderColor);
    if (argc >= 10) ColorParse::hexToColor(argv[9], textColor);
    
    int fontSize = argc >= 11 ? std::stof(argv[10]) : 1.5f ;               // font size multiplier

    auto desktop = sf::VideoMode::getDesktopMode();
    unsigned int width = desktop.size.x;
    unsigned int height = desktop.size.y - 37;

    sf::RenderWindow window(desktop, "", sf::State::Fullscreen);

    std::vector<sf::Font> fonts;
    std::vector<std::string> fontNames;
    for (const auto& entry : fs::directory_iterator("assets")) {
        if (entry.path().extension() == ".ttf") {
            sf::Font f;
            if (f.openFromFile(entry.path().string())) {
                fonts.push_back(std::move(f));
                fontNames.push_back(entry.path().filename().string());
            }
        }
    }

    if (fonts.empty()) {
        std::cerr << "Cand load fonts from assets/\n";
        return 1;
    }

    sf::Font* activeFont = &fonts[currentFontIndex];
    sf::Font font;
    fs::path defaultFont = fs::path("assets") / "Sansation-Regular.ttf";

    bool fontLoaded = font.openFromFile(defaultFont.string());

    if (!fontLoaded) {
        std::cerr << "Failed to load default font: " << defaultFont << std::endl;
        return 1;
    }

    float cellWidth = (width - frameSize * 2) / static_cast<float>(n);
    float cellHeight = (height - frameSize * 2) / static_cast<float>(m);

    auto buildHeaders = [&](sf::Font* fontPtr) {
        std::vector<sf::Text> result;
        for (int j = 0; j < n; j++) {
            std::ostringstream oss;
            oss << "col" << j + 1;

            unsigned int size = 40;
            sf::Text t(*fontPtr, oss.str(), size);
            t.setFillColor(textColor);

            auto bounds = t.getLocalBounds();
            t.setCharacterSize(size * fontSize);

            bounds = t.getLocalBounds();

            float cx = frameSize + j * cellWidth + cellWidth / 2.f;
            float cy = frameSize + cellHeight / 2.f;

            t.setOrigin(sf::Vector2f(bounds.position.x + bounds.size.x / 2.f,
                                 bounds.position.y + bounds.size.y / 2.f));
            t.setPosition(sf::Vector2f(cx, cy));

            result.push_back(t);
        }
        return result;
    };

    std::vector<sf::Text> headers = buildHeaders(activeFont);
    
    while (window.isOpen()) {
        while (auto eventOpt = window.pollEvent()) {
            if (!eventOpt) break;
            const sf::Event& event = *eventOpt;

            if (event.is<sf::Event::Closed>()) {
                window.close();
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

        window.display();
    }

    return 0;
}