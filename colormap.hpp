#pragma once
#include <vector>
#include <algorithm>

struct RGB { unsigned char r, g, b; };

class Colormap {
public:
    static RGB get_frog(float value) {
        value = std::max(0.0f, std::min(1.0f, value));
        const int num_stops = 7;
        float stops[num_stops] = {0.0f, 0.1f, 0.3f, 0.5f, 0.7f, 0.85f, 1.0f};
        RGB colors[num_stops] = {
            {0, 0, 0},        // Black
            {0, 80, 0},       // Dark Green
            {0, 180, 0},      // Forest Green
            {0, 255, 0},      // Pure Bright Green
            {255, 255, 0},    // Max Yellow
            {255, 0, 0},      // Poison Dart Red
            {255, 255, 255}   // Pure White
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
    static RGB get_grape(float value) {
        value = std::max(0.0f, std::min(1.0f, value));
        const int num_stops = 7;
        float stops[num_stops] = {0.0f, 0.1f, 0.25f, 0.45f, 0.65f, 0.85f, 1.0f};
        RGB colors[num_stops] = {
            {0, 0, 0},        // Black
            {60, 0, 120},     // Brighter Purple
            {150, 0, 200},    // Bright Violet
            {220, 0, 255},    // Neon Purple
            {255, 100, 255},  // Lighter Hot Pink
            {0, 255, 255},    // Cyan Burst
            {255, 255, 255}   // Pure White
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


    static RGB get_electric(float value) {
        value = std::max(0.0f, std::min(1.0f, value));
        const int num_stops = 6;
        float stops[num_stops] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
        RGB colors[num_stops] = {
            {0, 0, 0},        // Black
            {0, 0, 100},      // Dark Blue
            {0, 0, 255},      // Blue
            {0, 255, 255},    // Cyan
            {255, 255, 0},    // Yellow
            {255, 255, 255}   // White
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

    static RGB get_gqrx(float value) {
        value = std::max(0.0f, std::min(1.0f, value));
        const int num_stops = 6;
        float stops[num_stops] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
        RGB colors[num_stops] = {
            {0, 0, 0},        // Black
            {0, 0, 150},      // Dark Blue
            {0, 150, 255},    // Light Blue
            {255, 255, 0},    // Yellow
            {255, 0, 0},      // Red
            {255, 255, 255}   // White
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

    static RGB get_pablo(float value) {
        value = std::max(0.0f, std::min(1.0f, value));
        const int num_stops = 6;
        float stops[num_stops] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
        RGB colors[num_stops] = {
            {0, 0, 0},        // Black
            {0, 0, 255},      // Blue
            {0, 255, 0},      // Green
            {255, 255, 0},    // Yellow
            {255, 0, 0},      // Red
            {255, 255, 255}   // White
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

    static RGB get_websdr(float value) {
        value = std::max(0.0f, std::min(1.0f, value));
        const int num_stops = 5;
        float stops[num_stops] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        RGB colors[num_stops] = {
            {0, 0, 0},        // Black
            {80, 0, 150},     // Purple
            {255, 0, 0},      // Red
            {255, 255, 0},    // Yellow
            {255, 255, 255}   // White
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
