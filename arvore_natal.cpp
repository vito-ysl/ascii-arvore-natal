// Arvore de Natal animada em ASCII puro -- noite estrelada, neve e brilho discreto ao fundo.
// Ceu estatico (sem piscar), neve suave e 30 quadros por segundo.
// Tamanho detectado automaticamente: centraliza e preenche o terminal inteiro.
//
// Compilar:  g++ -std=c++17 -O2 arvore_natal.cpp -o arvore_natal
// Rodar:     ./arvore_natal              (animacao; Ctrl+C para sair)
//            ./arvore_natal --still      (um unico quadro)
//            ./arvore_natal --still --plain  (sem cores ANSI)
//
//Funciona em qualquer monitor: usa o tamanho do terminal onde o programa roda.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace {

enum class Ink {
    Clear, Night, StarDim, StarBright, Glow, Moon,
    TreeDark, TreeMid, TreeLight, Trunk,
    Gold, Red, Blue, Snow, SnowFront, White
};

bool g_color = true;

// ---------------------------------------------------------------------------
// Tamanho do terminal / resolucao
// ---------------------------------------------------------------------------

struct Size { int cols, rows; };

Size fallbackSize() {
    const char* c = std::getenv("COLUMNS");
    const char* r = std::getenv("LINES");
    int cols = (c && std::atoi(c) > 0) ? std::atoi(c) : 100;
    int rows = (r && std::atoi(r) > 0) ? std::atoi(r) : 42;
    return {cols, rows};
}

// Funcao que determina o tamanho da "resolucao" do terminal.
// Em POSIX usa TIOCGWINSZ; no Windows usa a janela do console.
// Como consulta a saida padrao, respeita automaticamente o monitor onde
// o terminal esta aberto e acompanha redimensionamentos.
Size terminalSize() {
#if defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO info{};
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(out, &info)) {
        int cols = info.srWindow.Right - info.srWindow.Left + 1;
        int rows = info.srWindow.Bottom - info.srWindow.Top + 1;
        if (cols > 0 && rows > 0) return {cols, rows};
    }
#else
    struct winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0)
        return {static_cast<int>(ws.ws_col), static_cast<int>(ws.ws_row)};
#endif
    return fallbackSize();
}

void enableWindowsAnsi() {
#if defined(_WIN32)
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(out, &mode))
        SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

// ---------------------------------------------------------------------------
// Layout dinamico, sempre centralizado
// ---------------------------------------------------------------------------

struct Layout {
    int w, h, cx;
    int starY, treeTop, treeBottom, tier1End, tier2End;
    int maxHalf;
    int h1End, h2Start, h2End, h3Start;
    int garland1, garland2, garland3;
    int trunkTop, trunkBottom, rootsY, snowmanY, groundY;
};

Layout L;

int clampi(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

int lerpInt(int a, int b, int y, int y0, int y1) {
    if (y1 <= y0) return b;
    double t = static_cast<double>(y - y0) / (y1 - y0);
    return static_cast<int>(std::lround(a + (b - a) * t));
}

// Recalcula todas as posicoes a partir do tamanho atual do terminal.
void updateLayout(int cols, int rows) {
    L.w = std::max(20, cols);
    L.h = std::max(10, rows);
    L.cx = L.w / 2;

    L.groundY    = L.h - 1;
    L.snowmanY   = L.h - 2;
    L.rootsY     = L.h - 3;
    L.trunkBottom= L.h - 4;

    L.starY   = clampi(static_cast<int>(std::lround((L.h - 8) * 0.16)), 0, L.h / 4);
    L.treeTop = std::min(L.starY + 2, L.h - 6);
    L.trunkTop= clampi(std::max(L.treeTop + 2, L.h - 6), L.treeTop + 2, L.trunkBottom);
    L.treeBottom = L.trunkTop - 1;
    if (L.treeBottom < L.treeTop) {
        L.treeTop = L.treeBottom;
        L.starY = std::max(0, L.treeTop - 1);
    }

    int th = L.treeBottom - L.treeTop + 1;
    L.tier1End = L.treeTop + std::max(1, static_cast<int>(std::lround(th * 0.30))) - 1;
    L.tier2End = std::min(L.treeBottom,
                          L.tier1End + std::max(1, static_cast<int>(std::lround(th * 0.32))));

    int byHeight = std::max(2, static_cast<int>(std::lround(th * 0.85)));
    int byWidth  = std::max(2, L.w / 2 - 3);
    L.maxHalf = std::min(60, std::min(byHeight, byWidth));

    L.h1End   = std::max(2, static_cast<int>(std::lround(L.maxHalf * 0.42)));
    L.h2Start = std::max(2, static_cast<int>(std::lround(L.maxHalf * 0.21)));
    L.h2End   = std::max(L.h2Start + 1, static_cast<int>(std::lround(L.maxHalf * 0.71)));
    L.h3Start = std::max(3, static_cast<int>(std::lround(L.maxHalf * 0.33)));

    L.garland1 = L.treeTop + (L.tier1End - L.treeTop) / 2;
    L.garland2 = L.tier1End + (L.tier2End - L.tier1End) / 2;
    L.garland3 = L.tier2End + (L.treeBottom - L.tier2End) / 2;
}

// Meia-largura da arvore em cada linha; -1 fora da copa.
int treeHalfAt(int y) {
    if (y >= L.treeTop && y <= L.tier1End)
        return lerpInt(1, L.h1End, y, L.treeTop, L.tier1End);
    if (y > L.tier1End && y <= L.tier2End)
        return lerpInt(L.h2Start, L.h2End, y, L.tier1End + 1, L.tier2End);
    if (y > L.tier2End && y <= L.treeBottom)
        return lerpInt(L.h3Start, L.maxHalf, y, L.tier2End + 1, L.treeBottom);
    return -1;
}

// ---------------------------------------------------------------------------
// Cores e canvas
// ---------------------------------------------------------------------------

const char* ansi(Ink ink) {
    if (!g_color) return "";
    switch (ink) {
        case Ink::Night:     return "\033[38;2;95;125;165m";
        case Ink::StarDim:   return "\033[38;2;150;175;215m";
        case Ink::StarBright:return "\033[38;2;255;246;205m";
        case Ink::Glow:      return "\033[38;2;140;110;70m";
        case Ink::Moon:      return "\033[38;2;255;232;180m";
        case Ink::TreeDark:  return "\033[38;2;18;88;55m";
        case Ink::TreeMid:   return "\033[38;2;30;140;80m";
        case Ink::TreeLight: return "\033[38;2;70;185;105m";
        case Ink::Trunk:     return "\033[38;2;120;80;52m";
        case Ink::Gold:      return "\033[38;2;255;193;74m";
        case Ink::Red:       return "\033[38;2;255;96;104m";
        case Ink::Blue:      return "\033[38;2;126;186;255m";
        case Ink::Snow:      return "\033[38;2;176;205;235m";
        case Ink::SnowFront: return "\033[38;2;236;248;255m";
        default:             return "";
    }
}

// Degrade noturno fixo em toda a largura e altura da tela.
std::string background(int y) {
    if (!g_color) return "";
    double t = static_cast<double>(y) / std::max(1, L.h - 1);
    int r = 5  + static_cast<int>(11 * t * t);
    int g = 9  + static_cast<int>(17 * t);
    int b = 20 + static_cast<int>(24 * t);
    return "\033[48;2;" + std::to_string(r) + ";" + std::to_string(g) + ";"
         + std::to_string(b) + "m";
}

struct Cell { std::string text = " "; Ink ink = Ink::Clear; };
using Canvas = std::vector<Cell>;

struct Star { int x, y; };
struct Flake { double x, y, speed, phase; int glyph; bool front; };

const char* kFlakes[] = {"*", "+", ".", ",", "o", "*", "'"};

unsigned hash2(int x, int y) {
    unsigned h = static_cast<unsigned>(x) * 374761393u + static_cast<unsigned>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

void put(Canvas& g, int x, int y, const std::string& s, Ink ink) {
    if (x < 0 || x >= L.w || y < 0 || y >= L.h) return;
    g[y * L.w + x] = Cell{s, ink};
}

// ---------------------------------------------------------------------------
// Cena
// ---------------------------------------------------------------------------

void buildScene(std::vector<Star>& stars, std::vector<Flake>& flakes) {
    std::mt19937 rng(20261224);
    std::uniform_real_distribution<double> uni(0.0, 1.0);

    int starCount = clampi(L.w * L.h / 40, 24, 260);
    for (int i = 0; i < starCount; ++i) {
        Star s{static_cast<int>(rng() % L.w), 1 + static_cast<int>(rng() % std::max(2, L.h - 10))};
        if (std::abs(s.x - L.cx) < L.maxHalf + 3 && s.y >= L.treeTop && s.y <= L.treeBottom) {
            --i;
            continue;
        }
        stars.push_back(s);
    }

    int flakeCount = clampi(L.w * 13 / 10, 40, 380);
    double speedScale = std::max(0.45, L.h / 42.0);
    for (int i = 0; i < flakeCount; ++i) {
        bool front = (i % 4 == 0);
        flakes.push_back(Flake{
            uni(rng) * L.w,
            uni(rng) * L.h,
            (front ? 0.16 + uni(rng) * 0.14 : 0.05 + uni(rng) * 0.06) * speedScale,
            uni(rng) * 6.2831853,
            static_cast<int>(rng() % 7),
            front
        });
    }
}

void drawGlow(Canvas& g) {
    int th = L.treeBottom - L.treeTop + 1;
    double gy = L.treeTop + th * 0.60;
    double rx = std::max(4.0, L.maxHalf * 0.90 + 6.0);
    double ry = std::max(3.0, th * 0.42);

    // Halo quente e discreto atras da arvore.
    for (int y = 0; y < L.h; ++y) {
        for (int x = 0; x < L.w; ++x) {
            double d = std::hypot((x - L.cx) / rx, (y - gy) / ry);
            if (d >= 1.0) continue;
            unsigned chance = static_cast<unsigned>((1.0 - d) * 34.0);
            if (hash2(x, y) % 100 < chance)
                put(g, x, y, (hash2(y, x) % 7 == 0) ? ":" : ".", Ink::Glow);
        }
    }

    // Lua crescente no canto superior direito, com brilho pequeno.
    int mx = L.w - std::min(12, std::max(5, L.w / 8));
    int my = clampi(L.h / 6, 2, 8);
    int hrx = std::min(7, std::max(3, L.w / 14));
    int hry = std::min(6, std::max(3, L.h / 7));
    put(g, mx, my, "(", Ink::Moon);
    for (int dy = -hry; dy <= hry; ++dy) {
        for (int dx = -hrx; dx <= hrx; ++dx) {
            if (dx == 0 && dy == 0) continue;
            double d = std::hypot(dx / static_cast<double>(hrx),
                                  dy / static_cast<double>(hry));
            if (d >= 1.0) continue;
            unsigned h = hash2(mx + dx, my + dy);
            if (h % 100 < static_cast<unsigned>((1.0 - d) * 26.0))
                put(g, mx + dx, my + dy, (h % 11 == 0) ? "+" : ".",
                    (h % 11 == 0) ? Ink::Moon : Ink::Glow);
        }
    }
    put(g, mx - 5, my - 3, "*", Ink::StarDim);
    put(g, mx + 6, my + 4, ".", Ink::StarDim);
}

void drawStars(Canvas& g, const std::vector<Star>& stars) {
    // Brilho fixo por estrela: nada de cintilar ou piscar.
    for (const Star& s : stars) {
        unsigned h = hash2(s.x, s.y);
        if (h % 9 == 0)      put(g, s.x, s.y, "*", Ink::StarBright);
        else if (h % 4 == 0) put(g, s.x, s.y, "+", Ink::StarDim);
        else if (h % 2 == 0) put(g, s.x, s.y, ".", Ink::StarDim);
        else                 put(g, s.x, s.y, "'", Ink::Night);
    }
    // Estrelas maiores, com raios em cruz.
    for (int i = 0; i < 5; ++i) {
        int x = clampi(8 + i * (L.w - 16) / 5, 3, L.w - 4);
        int y = clampi(3 + (i * 5) % std::max(3, L.h / 3), 2, L.h - 5);
        put(g, x - 2, y, "-", Ink::StarDim);
        put(g, x + 2, y, "-", Ink::StarDim);
        put(g, x, y - 2, "|", Ink::StarDim);
        put(g, x, y + 2, "|", Ink::StarDim);
        put(g, x - 1, y, ".", Ink::StarDim);
        put(g, x + 1, y, ".", Ink::StarDim);
        put(g, x, y, "+", Ink::StarBright);
    }
}

void drawGift(Canvas& g, int x, int y, int w, int h, Ink body) {
    x = clampi(x, 1, L.w - w - 2);
    if (h < 2 || w < 3) return;
    int mid = x + w / 2;
    for (int i = 0; i < w; ++i) {
        put(g, x + i, y, "=", Ink::Gold);
        for (int j = y + 1; j < y + h - 1; ++j)
            put(g, x + i, j, "#", body);
        put(g, x + i, y + h - 1, "=", Ink::Gold);
    }
    put(g, x, y, "+", Ink::Gold);
    put(g, x + w - 1, y, "+", Ink::Gold);
    put(g, x, y + h - 1, "+", Ink::Gold);
    put(g, x + w - 1, y + h - 1, "+", Ink::Gold);
    for (int j = y + 1; j < y + h - 1; ++j) {
        put(g, x, j, "|", body);
        put(g, x + w - 1, j, "|", body);
        put(g, mid, j, "|", Ink::Gold);
    }
    put(g, mid, y, "*", Ink::StarBright);
}

void drawTree(Canvas& g) {
    // Tronco e raizes.
    for (int y = L.trunkTop; y <= L.trunkBottom; ++y) {
        put(g, L.cx - 1, y, "|", Ink::Trunk);
        put(g, L.cx,     y, "|", Ink::Trunk);
        put(g, L.cx + 1, y, "|", Ink::Trunk);
    }
    put(g, L.cx - 3, L.rootsY, "\\", Ink::Trunk);
    put(g, L.cx - 2, L.rootsY, "_", Ink::Trunk);
    put(g, L.cx - 1, L.rootsY, "_", Ink::Trunk);
    put(g, L.cx,     L.rootsY, "|", Ink::Trunk);
    put(g, L.cx + 1, L.rootsY, "_", Ink::Trunk);
    put(g, L.cx + 2, L.rootsY, "_", Ink::Trunk);
    put(g, L.cx + 3, L.rootsY, "/", Ink::Trunk);

    static const char* needles[] = {"#", "#", "%", "@", "*", "#", "^", "&"};

    for (int y = L.treeTop; y <= L.treeBottom; ++y) {
        int half = treeHalfAt(y);
        if (half < 0) continue;
        for (int dx = -half; dx <= half; ++dx) {
            int x = L.cx + dx;
            if (dx == -half) { put(g, x, y, "/", Ink::TreeLight); continue; }
            if (dx ==  half) { put(g, x, y, "\\", Ink::TreeLight); continue; }
            if (dx == 0)     { put(g, x, y, "|", Ink::TreeMid); continue; }

            // Guirlandas em tres alturas proporcionais.
            if ((y == L.garland1 || y == L.garland2 || y == L.garland3)
                && (dx + half) % 3 != 0) {
                bool orb = ((dx + half) % 6 == 1);
                put(g, x, y, orb ? "o" : "~",
                    orb ? (((dx + y) % 2) ? Ink::Red : Ink::Blue) : Ink::Gold);
                continue;
            }

            unsigned h = hash2(x, y);
            const char* glyph = needles[h % 8];
            Ink ink = (h % 5 == 0) ? Ink::TreeLight
                    : (h % 3 == 0) ? Ink::TreeDark : Ink::TreeMid;
            put(g, x, y, glyph, ink);
        }
    }

    // Enfeites: posicao vertical proporcional a altura da arvore.
    struct Bauble { double t, frac; const char* glyph; Ink ink; };
    const Bauble baubles[] = {
        {0.11, -0.62, "@", Ink::Red},  {0.14,  0.58, "O", Ink::Blue},
        {0.21, -0.78, "0", Ink::Gold}, {0.25,  0.70, "&", Ink::Red},
        {0.36, -0.50, "%", Ink::Blue}, {0.39,  0.82, "@", Ink::Gold},
        {0.54, -0.84, "8", Ink::Red},  {0.57,  0.46, "0", Ink::Blue},
        {0.68, -0.32, "$", Ink::Gold}, {0.71,  0.72, "%", Ink::Red},
        {0.86, -0.70, "O", Ink::Blue}, {0.89,  0.86, "@", Ink::Gold},
        {0.96, -0.44, "0", Ink::Red},  {0.96,  0.30, "8", Ink::Blue},
    };
    int span = std::max(1, L.treeBottom - L.treeTop);
    int lastY = -100;
    for (const Bauble& b : baubles) {
        int y = L.treeTop + static_cast<int>(std::lround(b.t * span));
        if (y == lastY) continue; // telas pequenas: evita empilhar enfeites
        lastY = y;
        int half = treeHalfAt(y);
        if (half < 2) continue;
        int x = L.cx + static_cast<int>(std::lround(b.frac * (half - 1)));
        put(g, x, y, b.glyph, b.ink);
        put(g, x, y - 1, ".", Ink::Gold);
    }

    // Estrela do topo, com aura fixa.
    put(g, L.cx, L.starY, "*", Ink::StarBright);
    put(g, L.cx - 1, L.starY, "+", Ink::Gold);
    put(g, L.cx + 1, L.starY, "+", Ink::Gold);
    put(g, L.cx, L.starY - 1, ".", Ink::Gold);
    put(g, L.cx, L.starY + 1, "+", Ink::Gold);
    put(g, L.cx - 2, L.starY - 1, ".", Ink::Glow);
    put(g, L.cx + 2, L.starY - 1, ".", Ink::Glow);
    put(g, L.cx - 2, L.starY + 1, ".", Ink::Glow);
    put(g, L.cx + 2, L.starY + 1, ".", Ink::Glow);
    put(g, L.cx - 3, L.starY, "+", Ink::Glow);
    put(g, L.cx + 3, L.starY, "+", Ink::Glow);
    put(g, L.cx, L.starY - 3, "|", Ink::Glow);
    put(g, L.cx, L.starY - 2, ".", Ink::Glow);

    // Presentes aos pes da arvore.
    int giftW = clampi(L.maxHalf / 3, 4, 10);
    int giftH = (L.h >= 16) ? 3 : 2;
    drawGift(g, L.cx - L.maxHalf - 2, L.rootsY - (giftH - 1), giftW, giftH, Ink::Red);
    drawGift(g, L.cx + L.maxHalf - giftW + 2, L.rootsY - (giftH - 1), giftW + 1, giftH, Ink::Blue);
    int smallW = std::max(4, giftW - 2);
    drawGift(g, L.cx - smallW / 2, L.rootsY - (giftH - 2), smallW, std::max(2, giftH - 1), Ink::Gold);

    // Banco de neve em toda a largura e um boneco pequeno a direita.
    for (int x = 0; x < L.w; ++x) {
        unsigned h = hash2(x, 99);
        if (h % 100 < 72) {
            const char* s = (h % 4 == 0) ? "," : ((h % 3 == 0) ? ":" : ".");
            put(g, x, L.groundY, s, Ink::Snow);
        }
    }
    int sx = clampi(L.cx + L.maxHalf + 14, 2, L.w - 2);
    put(g, sx, L.snowmanY - 2, "*", Ink::Snow);
    put(g, sx, L.snowmanY - 1, "O", Ink::SnowFront);
    put(g, sx, L.snowmanY,     "O", Ink::SnowFront);
    put(g, sx + 1, L.snowmanY, "+", Ink::Snow);
}

void drawSnow(Canvas& g, const std::vector<Flake>& flakes, bool front) {
    for (const Flake& f : flakes) {
        if (f.front != front) continue;
        put(g, static_cast<int>(std::lround(f.x)),
               static_cast<int>(std::lround(f.y)),
               kFlakes[f.glyph], front ? Ink::SnowFront : Ink::Snow);
    }
}

void advanceSnow(std::vector<Flake>& flakes, int frame) {
    for (Flake& f : flakes) {
        f.y += f.speed;
        f.x += 0.014 * std::sin(frame * 0.012 + f.phase);
        if (f.y > L.h - 2) {
            f.y = -1.0;
            if (f.x < 0 || f.x >= L.w) f.x = 1.0;
        }
        if (f.x < 0) f.x += L.w;
        if (f.x >= L.w) f.x -= L.w;
    }
}

std::string render(const Canvas& g) {
    std::string out;
    out.reserve(g.size() * 12);
    Ink current = Ink::Clear;
    for (int y = 0; y < L.h; ++y) {
        // Posiciona cada linha explicitamente: evita quebra de linha acidental
        // e rolagem quando a largura ocupa exatamente a coluna final.
        if (g_color) out += "\033[" + std::to_string(y + 1) + ";1H";
        out += background(y);
        for (int x = 0; x < L.w; ++x) {
            const Cell& c = g[y * L.w + x];
            if (c.ink != current) {
                out += ansi(c.ink);
                current = c.ink;
            }
            out += c.text;
        }
        if (g_color) out += "\033[0m";
        current = Ink::Clear;
        if (!g_color) out += '\n';
    }
    return out;
}

void cleanup(int) {
    std::cout << "\033[0m\033[?25h\033[2J\033[H" << std::flush;
    std::exit(0);
}

} // namespace

int main(int argc, char** argv) {
    bool still = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--still") still = true;
        if (arg == "--plain") g_color = false;
    }

    if (g_color) enableWindowsAnsi();

    std::vector<Star> stars;
    std::vector<Flake> flakes;

    if (g_color && !still) {
        std::signal(SIGINT, cleanup);
        std::signal(SIGTERM, cleanup);
        // Limpa uma vez; em cada quadro apenas volta o cursor ao inicio (sem piscar).
        std::cout << "\033[2J\033[H\033[?25l";
    }

    const auto frameTime = std::chrono::microseconds(33333); // 30 fps
    auto next = std::chrono::steady_clock::now();
    Size last{0, 0};
    int frame = 0;

    while (true) {
        Size now = terminalSize();
        if (now.cols != last.cols || now.rows != last.rows) {
            last = now;
            updateLayout(now.cols, now.rows);
            stars.clear();
            flakes.clear();
            buildScene(stars, flakes);
            if (g_color && !still && frame > 0) std::cout << "\033[2J";
        }

        Canvas g(static_cast<size_t>(L.w) * L.h);
        drawGlow(g);
        drawStars(g, stars);
        drawSnow(g, flakes, false); // neve atras da arvore
        drawTree(g);
        drawSnow(g, flakes, true);  // neve na frente da arvore

        std::cout << (still || !g_color ? "" : "\033[H") << render(g) << std::flush;
        if (still) break;

        ++frame;
        advanceSnow(flakes, frame);

        next += frameTime;
        if (next < std::chrono::steady_clock::now())
            next = std::chrono::steady_clock::now() + frameTime;
        std::this_thread::sleep_until(next);
    }

    if (g_color && !still) std::cout << "\033[0m\033[?25h" << std::flush;
    return 0;
}
