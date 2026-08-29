// examples/App.h
//
// A minimal SDL application scaffold shared by DxvUI examples (NOT part of the
// public DxvUI API / not installed). It is intentionally DxvUI-agnostic: it only
// sets up a window + renderer, runs the main loop and clears the frame, exactly
// like a plain SDL host application would. Each example subclasses it and owns
// the DxvUI integration itself (creating its Scene/SDLRenderer in init(), feeding
// events from handleEvent(), drawing the UI in draw()).
//
// Why: examples should mirror real-world usage — an app embeds DxvUI into its own
// SDL loop, drawing its own content first and the UI on top. Keeping the scaffold
// free of DxvUI makes that contract explicit.

#pragma once

#include <SDL.h>

#include <chrono>
#include <cstdlib>

namespace DxvUIEx {

class SdlApp {
   public:
    SdlApp(const char* title, int width, int height, bool resizable = true, bool vsync = true)
        : title_(title), width_(width), height_(height), resizable_(resizable), vsync_(vsync) {}

    virtual ~SdlApp() {
        // Give a derived app a chance to release its own resources (e.g. reset its
        // Scene/SDLRenderer) while the window and renderer are still alive.
        shutdown();

        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    SdlApp(const SdlApp&) = delete;
    SdlApp& operator=(const SdlApp&) = delete;

    // Runs the application: initializes SDL, creates the window + renderer, calls
    // init(), then enters the frame loop (update -> clear -> draw -> present).
    int run() {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) return -1;

        Uint32 windowFlags = SDL_WINDOW_SHOWN | (resizable_ ? SDL_WINDOW_RESIZABLE : 0);
        window_ = SDL_CreateWindow(title_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width_,
                                   height_, windowFlags);
        if (!window_) return -1;

        renderer_ = SDL_CreateRenderer(
            window_, -1, SDL_RENDERER_ACCELERATED | (vsync_ ? SDL_RENDERER_PRESENTVSYNC : 0));
        if (!renderer_) return -1;

        if (!init()) return 0;  // e.g. a scripted headless run already finished

        bool quit = false;
        SDL_Event event;

        const char* framesEnv = std::getenv("DXVUI_FRAMES");
        const long frameCap = framesEnv ? std::atol(framesEnv) : 0;
        long frameCount = 0;

        auto prev = std::chrono::steady_clock::now();
        while (!quit) {
            while (SDL_PollEvent(&event) != 0) {
                if (handleEvent(event)) {
                    quit = true;
                    break;
                }
            }

            const auto now = std::chrono::steady_clock::now();
            const float dtMs = std::chrono::duration<float, std::milli>(now - prev).count();
            prev = now;

            update(dtMs);

            // Clear the frame to the configured background colour before draw().
            SDL_SetRenderDrawColor(renderer_, clearR_, clearG_, clearB_, clearA_);
            SDL_RenderClear(renderer_);
            draw();

            SDL_RenderPresent(renderer_);

            if (frameCap > 0 && ++frameCount >= frameCap) quit = true;
        }
        return 0;
    }

   protected:
    // Hook called right after the window and renderer exist, before the loop.
    // Return false to skip the frame loop entirely (e.g. a scripted headless run
    // already did its work in init()).
    virtual bool init() { return true; }

    // Called before the SDL window and renderer are destroyed (from the
    // destructor). Derived apps can release their own resources here.
    virtual void shutdown() {}

    // Configure the background colour used by SDL_RenderClear each frame.
    // Defaults to white (255, 255, 255, 255).
    void setClearColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) {
        clearR_ = r;
        clearG_ = g;
        clearB_ = b;
        clearA_ = a;
    }

    // Called every frame before draw(); dtMs is the elapsed time since the last
    // frame in milliseconds.
    virtual void update(float /*dtMs*/) {}

    // Draw the current frame. The frame is already cleared (SDL_RenderClear) by
    // the loop, so examples draw their own content here and, on top, the UI.
    virtual void draw() {}

    // Called for every SDL event. Return true to exit the main loop (e.g. on
    // DxvUI::EventType::Quit translated from SDL_QUIT).
    virtual bool handleEvent(const SDL_Event& /*event*/) { return false; }

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;

   private:
    const char* title_;
    int width_;
    int height_;
    bool resizable_;
    bool vsync_;
    Uint8 clearR_ = 255;
    Uint8 clearG_ = 255;
    Uint8 clearB_ = 255;
    Uint8 clearA_ = 255;
};

}  // namespace DxvUIEx
