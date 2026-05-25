#pragma once
#include <vector>
#include <algorithm>

struct RGB { unsigned char r, g, b; };

class Colormap {
public:
    static RGB get_viridis(float value) {
        // Clamp value between 0.0 and 1.0
        value = std::max(0.0f, std::min(1.0f, value));
        
        // Simple interpolation approximating Viridis
        const int num_stops = 4;
        float stops[num_stops] = {0.0f, 0.33f, 0.66f, 1.0f};
        RGB colors[num_stops] = {
            {68, 1, 84},      // Dark purple
            {49, 104, 142},   // Blue
            {53, 183, 121},   // Green
            {253, 231, 37}    // Yellow
        };
        
        for (int i = 0; i < num_stops - 1; ++i) {
            if (value >= stops[i] && value <= stops[i+1]) {
                float t = (value - stops[i]) / (stops[i+1] - stops[i]);
                RGB c;
                c.r = static_cast<unsigned char>(colors[i].r + t * (colors[i+1].r - colors[i].r));
                c.g = static_cast<unsigned char>(colors[i].g + t * (colors[i+1].g - colors[i].g));
                c.b = static_cast<unsigned char>(colors[i].b + t * (colors[i+1].b - colors[i].b));
                return c;
            }
        }
        return colors[num_stops - 1];
    }

    static RGB get_inferno(float value) {
        value = std::max(0.0f, std::min(1.0f, value));
        const int num_stops = 5;
        float stops[num_stops] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        RGB colors[num_stops] = {
            {0, 0, 4},        // Black/Dark Blue
            {66, 10, 104},    // Purple
            {165, 44, 96},    // Magenta/Red
            {248, 149, 64},   // Orange
            {252, 255, 164}   // Yellow/White
        };
        
        for (int i = 0; i < num_stops - 1; ++i) {
            if (value >= stops[i] && value <= stops[i+1]) {
                float t = (value - stops[i]) / (stops[i+1] - stops[i]);
                RGB c;
                c.r = static_cast<unsigned char>(colors[i].r + t * (colors[i+1].r - colors[i].r));
                c.g = static_cast<unsigned char>(colors[i].g + t * (colors[i+1].g - colors[i].g));
                c.b = static_cast<unsigned char>(colors[i].b + t * (colors[i+1].b - colors[i].b));
                return c;
            }
        }
        return colors[num_stops - 1];
    }

    static RGB get_plasma(float value) {
        value = std::max(0.0f, std::min(1.0f, value));
        const int num_stops = 5;
        float stops[num_stops] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        RGB colors[num_stops] = {
            {13, 8, 135},     // Dark blue
            {126, 3, 168},    // Purple
            {204, 71, 120},   // Pink/Red
            {248, 149, 64},   // Orange
            {240, 249, 33}    // Yellow
        };
        
        for (int i = 0; i < num_stops - 1; ++i) {
            if (value >= stops[i] && value <= stops[i+1]) {
                float t = (value - stops[i]) / (stops[i+1] - stops[i]);
                RGB c;
                c.r = static_cast<unsigned char>(colors[i].r + t * (colors[i+1].r - colors[i].r));
                c.g = static_cast<unsigned char>(colors[i].g + t * (colors[i+1].g - colors[i].g));
                c.b = static_cast<unsigned char>(colors[i].b + t * (colors[i+1].b - colors[i].b));
                return c;
            }
        }
        return colors[num_stops - 1];
    }

    static RGB get_magma(float value) {
        value = std::max(0.0f, std::min(1.0f, value));
        const int num_stops = 5;
        float stops[num_stops] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        RGB colors[num_stops] = {
            {0, 0, 4},        // Black
            {81, 18, 124},    // Dark purple
            {182, 54, 121},   // Magenta
            {251, 136, 97},   // Coral/Orange
            {252, 253, 191}   // Light yellow
        };
        
        for (int i = 0; i < num_stops - 1; ++i) {
            if (value >= stops[i] && value <= stops[i+1]) {
                float t = (value - stops[i]) / (stops[i+1] - stops[i]);
                RGB c;
                c.r = static_cast<unsigned char>(colors[i].r + t * (colors[i+1].r - colors[i].r));
                c.g = static_cast<unsigned char>(colors[i].g + t * (colors[i+1].g - colors[i].g));
                c.b = static_cast<unsigned char>(colors[i].b + t * (colors[i+1].b - colors[i].b));
                return c;
            }
        }
        return colors[num_stops - 1];
    }

    static RGB get_coolwarm(float value) {
        value = std::max(0.0f, std::min(1.0f, value));
        const int num_stops = 7;
        float stops[num_stops] = {0.0f, 0.1666666f, 0.3333333f, 0.5f, 0.6666666f, 0.8333333f, 1.0f};
        RGB colors[num_stops] = {
            {58, 76, 192},
            {111, 145, 242},
            {170, 198, 253},
            {221, 220, 219},
            {246, 183, 156},
            {230, 116, 90},
            {179, 3, 38}
        };
        
        for (int i = 0; i < num_stops - 1; ++i) {
            if (value >= stops[i] && value <= stops[i+1]) {
                float t = (value - stops[i]) / (stops[i+1] - stops[i]);
                RGB c;
                c.r = static_cast<unsigned char>(colors[i].r + t * (colors[i+1].r - colors[i].r));
                c.g = static_cast<unsigned char>(colors[i].g + t * (colors[i+1].g - colors[i].g));
                c.b = static_cast<unsigned char>(colors[i].b + t * (colors[i+1].b - colors[i].b));
                return c;
            }
        }
        return colors[num_stops - 1];
    }

    static RGB get_turbo(float value) {
        value = std::max(0.0f, std::min(1.0f, value));
        // Approximation of Google's Turbo colormap (perceptually uniform Jet)
        const int num_stops = 7;
        float stops[num_stops] = {0.0f, 0.166f, 0.333f, 0.5f, 0.666f, 0.833f, 1.0f};
        RGB colors[num_stops] = {
            {48, 18, 59},     // Dark Blue
            {66, 136, 235},   // Blue
            {40, 251, 149},   // Cyan-Green
            {164, 252, 60},   // Yellow-Green
            {243, 153, 19},   // Orange
            {209, 41, 7},     // Red
            {122, 4, 3}       // Dark Red
        };
        for (int i = 0; i < num_stops - 1; ++i) {
            if (value >= stops[i] && value <= stops[i+1]) {
                float t = (value - stops[i]) / (stops[i+1] - stops[i]);
                RGB c;
                c.r = static_cast<unsigned char>(colors[i].r + t * (colors[i+1].r - colors[i].r));
                c.g = static_cast<unsigned char>(colors[i].g + t * (colors[i+1].g - colors[i].g));
                c.b = static_cast<unsigned char>(colors[i].b + t * (colors[i+1].b - colors[i].b));
                return c;
            }
        }
        return colors[num_stops - 1];
    }
    
    static RGB get_jet(float value) {
        value = std::max(0.0f, std::min(1.0f, value));
        const int num_stops = 6;
        float stops[num_stops] = {0.0f, 0.125f, 0.375f, 0.625f, 0.875f, 1.0f}; 
        RGB colors[num_stops] = {
            {0, 0, 127},      // Dark Blue
            {0, 0, 255},      // Blue
            {0, 255, 255},    // Cyan
            {255, 255, 0},    // Yellow
            {255, 0, 0},      // Red
            {127, 0, 0}       // Dark Red
        };
        for (int i = 0; i < num_stops - 1; ++i) {
            if (value >= stops[i] && value <= stops[i+1]) {
                float t = (value - stops[i]) / (stops[i+1] - stops[i]);
                RGB c;
                c.r = static_cast<unsigned char>(colors[i].r + t * (colors[i+1].r - colors[i].r));
                c.g = static_cast<unsigned char>(colors[i].g + t * (colors[i+1].g - colors[i].g));
                c.b = static_cast<unsigned char>(colors[i].b + t * (colors[i+1].b - colors[i].b));
                return c;
            }
        }
        return colors[num_stops - 1];
    }
};
