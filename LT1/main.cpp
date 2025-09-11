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
        std::cerr << "Usage: " << argv[0] << " <m rows> <n cols> [frame size] [bgcolor hex] [linecolor hex] [line size]\n";
        return 1;
    }

    
    int m = std::stoi(argv[1]);
    int n = std::stoi(argv[2]);
    int frameSize = (argc >= 4) ? std::stoi(argv[3]) : 1;
    
    sf::Color bgColor = sf::Color::Black;
    sf::Color lineColor = sf::Color::White;
    
    if (argc >= 5) ColorParse::hexToColor(argv[4], bgColor);
    if (argc >= 6) ColorParse::hexToColor(argv[5], lineColor);

    float lineSize = argc >= 7 ? std::stof(argv[6]) : 1.f;

    auto desktop = sf::VideoMode::getDesktopMode();
    unsigned int width = desktop.size.x;
    unsigned int height = desktop.size.y - 40;

    sf::RenderWindow window(desktop, "", sf::State::Fullscreen);

    sf::Font font;
    
    fs::path defaultFont = fs::path("assets") / "Sansation-Regular.ttf";
    bool fontLoaded = font.openFromFile(defaultFont.string());

    if (!fontLoaded) {
        std::cerr << "Failed to load default font: " << defaultFont << std::endl;
        return 1;
    }

    float cellWidth = (width - frameSize * 2 - lineSize) / static_cast<float>(n);
    float cellHeight = (height - frameSize * 2 - lineSize) / static_cast<float>(m);

    std::vector<sf::Text> headers;
    for (int j = 0; j < n; j++) {
        std::ostringstream oss;
        oss << "col" << j + 1;

        sf::Text t(font, oss.str(), 40);
        t.setFillColor(lineColor);

        auto bounds = t.getLocalBounds();
        float targetW = cellWidth * 0.8f;
        unsigned int size = 40;

        while (size > 6 && bounds.size.x > targetW) {
            size -= 2;
            t.setCharacterSize(size);
            bounds = t.getLocalBounds();
        }

        float cx = frameSize + j * cellWidth + cellWidth / 2.f;
        float cy = frameSize + cellHeight / 2.f;

        
        t.setOrigin(sf::Vector2f(bounds.position.x + bounds.size.x / 2.f,
                                 bounds.position.y + bounds.size.y / 2.f));
        t.setPosition(sf::Vector2f(cx, cy));

        headers.push_back(t);
    }


    while (window.isOpen()) {
        while (auto eventOpt = window.pollEvent()) {
            if (!eventOpt) break;
            const sf::Event& event = *eventOpt;

            if (event.is<sf::Event::Closed>()) {
                window.close();
            }
            if (event.is<sf::Event::KeyPressed>()) {
                if (event.getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape) {
                    window.close();
                }
            }
        }

        window.clear(bgColor);

        for (int i = 0; i <= m; i++) {
            sf::RectangleShape line({(float)width - frameSize * 2, lineSize});
            line.setFillColor(lineColor);
            float y = frameSize + i * cellHeight;
            line.setPosition(sf::Vector2f(frameSize,y));
            window.draw(line);
        }

        for (int j = 0; j <= n; j++) {
            sf::RectangleShape vline({lineSize, (float)height - frameSize * 2});
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
