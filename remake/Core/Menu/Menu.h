#pragma once

struct ImFont;

namespace Menu
{
    void SetFonts(ImFont* regular, ImFont* semiBold, ImFont* icons);
    void Render();
}
