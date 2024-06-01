// OpenGL via SDL2
#include "burner.h"
#include "vid_support.h"
#include "vid_softfx.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan_helper.h>

extern char videofiltering[3];

static int nInitedSubsytems = 0;

static int nGamesWidth = 0, nGamesHeight = 0; // screen size

extern SDL_Window* sdlWindow;

static unsigned char* texture = NULL;
static unsigned char* gamescreen = NULL;
static SDL_Rect dstrect;

static int nTextureWidth = 512;
static int nTextureHeight = 512;

static int nSize;
static int nUseBlitter;

static int nRotateGame = 0;
static bool bFlipped = false;

static char Windowtitle[512];


static int BlitFXExit()
{
    free(texture);
    free(gamescreen);

    SDL_DestroyWindow(sdlWindow);
    SDL_Vulkan_UnloadLibrary();

    nRotateGame = 0;

    return 0;
}

static int GetTextureSize(int Size)
{
    int nTextureSize = 128;

    while (nTextureSize < Size) {
        nTextureSize <<= 1;
    }

    return nTextureSize;
}

static int BlitFXInit()
{
    int nMemLen = 0;

    nVidImageDepth = bDrvOkay ? 16 : 32;
    nVidImageBPP = (nVidImageDepth + 7) >> 3;
    nBurnBpp = nVidImageBPP;

    SetBurnHighCol(nVidImageDepth);

    if (!nRotateGame) {
        nVidImageWidth = nGamesWidth;
        nVidImageHeight = nGamesHeight;
    } else {
        nVidImageWidth = nGamesHeight;
        nVidImageHeight = nGamesWidth;
    }

    nVidImagePitch = nVidImageWidth * nVidImageBPP;
    nBurnPitch = nVidImagePitch;

    nMemLen = nVidImageWidth * nVidImageHeight * nVidImageBPP;

#ifdef FBNEO_DEBUG
    printf("nVidImageWidth=%d nVidImageHeight=%d nVidImagePitch=%d\n", nVidImageWidth, nVidImageHeight, nVidImagePitch);
    printf("nTextureWidth=%d nTextureHeight=%d TexturePitch=%d\n", nTextureWidth, nTextureHeight, nTextureWidth * nVidImageBPP);
#endif

    texture = (unsigned char*)malloc(nTextureWidth * nTextureHeight * nVidImageBPP);

    gamescreen = (unsigned char*)malloc(nMemLen);
    if (gamescreen) {
        memset(gamescreen, 0, nMemLen);
        pVidImage = gamescreen;
        return 0;
    } else {
        pVidImage = NULL;
        return 1;
    }

    return 0;
}

static int Exit()
{
    BlitFXExit();

    if (!(nInitedSubsytems & SDL_INIT_VIDEO)) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    nInitedSubsytems = 0;

    return 0;
}

static int HandleVulkanEvents(void* data, SDL_Event* event) {
    if (event->type == SDL_WINDOWEVENT) {
        switch(event->window.event) {
            case SDL_WINDOWEVENT_RESIZED:
                SDL_Log("Resize event");
                break;
            case SDL_WINDOWEVENT_MINIMIZED:
                SDL_Log("Minimized window");
                break;
            case SDL_WINDOWEVENT_RESTORED:
                SDL_Log("Restored window");
                break;
            default:
                break;
        }
    }
    return 0;
}

static int SDLScaleWindow()
{
    Uint32 screenFlags = SDL_GetWindowFlags(sdlWindow);

    double display_w = 1.0 * nGamesWidth;
    double display_h = 1.0 * nGamesHeight;

    // Scale forced to "*2"
    // Screen center fix thanks to Woises
    if (nRotateGame) {
        if (screenFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) {
            int w;
            int h;
            SDL_GetWindowSize(sdlWindow, &w, &h);
            printf("Rotated Width/Height: %d, %d\n", w, h);
            //SDL_SetWindowSize(sdlWindow, (display_h * (display_w / display_h)) * 2, display_h * 2);
            //SDL_SetWindowFullscreen(sdlWindow, SDL_TRUE);
            dstrect.x = ((display_w * w / h) - display_w) / 2;
        } else {
            //SDL_RestoreWindow(sdlWindow);
            //SDL_SetWindowSize(sdlWindow, display_w * 2, display_h * 2);
            //SDL_SetWindowFullscreen(sdlWindow, SDL_FALSE);
            dstrect.x = (display_h - display_w) / 2;
        }
        dstrect.y = (display_w - display_h) / 2;
    } else {
        if (screenFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) {
            int w;
            int h;
            SDL_GetWindowSize(sdlWindow, &w, &h);
            printf("Unrotated Width/Height: %d, %d\n", w, h);
            //SDL_SetWindowSize(sdlWindow, (display_w * w / h) * 2, display_h * 2);
            //SDL_SetWindowFullscreen(sdlWindow, SDL_TRUE);
            if (display_w < (display_h * w / h)) {
                dstrect.x = ((display_h * w / h) - display_w) / 2;
                dstrect.y = 0;
            } else {
                dstrect.x = 0;
                dstrect.y = ((display_w * h / w) - display_h) / 2;
            }
        } else {
            //SDL_RestoreWindow(sdlWindow);
            //SDL_SetWindowSize(sdlWindow, display_w * 2, display_h * 2);
            //SDL_SetWindowFullscreen(sdlWindow, SDL_FALSE);
            dstrect.x = 0;
            dstrect.y = 0;
        }
    }

    SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    dstrect.w = display_w;
    dstrect.h = display_h;

    return 0;
}

static int Init()
{
    nInitedSubsytems = SDL_WasInit(SDL_INIT_VIDEO);

    if (!(nInitedSubsytems & SDL_INIT_VIDEO)) {
        SDL_Init(SDL_INIT_VIDEO);
    }

    SDL_Vulkan_LoadLibrary(nullptr);

    nGamesWidth = nVidImageWidth;
    nGamesHeight = nVidImageHeight;

    nRotateGame = 0;

    if (bDrvOkay) {
        // Get the game screen size
        BurnDrvGetVisibleSize(&nGamesWidth, &nGamesHeight);

        if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
#ifdef FBNEO_DEBUG
            printf("Vertical\n");
#endif
            nRotateGame = 1;
        }

        if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) {
#ifdef FBNEO_DEBUG
            printf("Flipped\n");
#endif
            bFlipped = true;
        }
    }

    Uint32 screenFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    if (bAppFullscreen) {
        screenFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN;
    }

    sprintf(Windowtitle, "FBNeo - %s - %s", BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_FULLNAME));

    sdlWindow = SDL_CreateWindow(Windowtitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, nGamesWidth, nGamesHeight, screenFlags);

    if (sdlWindow == NULL) {
        printf("SDL window creation failed : %s\n", SDL_GetError());
        return -1;
    }

    SDLScaleWindow();

    if (!nRotateGame) {
        nTextureWidth = GetTextureSize(nGamesWidth);
        nTextureHeight = GetTextureSize(nGamesHeight);
    } else {
        nTextureWidth = GetTextureSize(nGamesHeight);
        nTextureHeight = GetTextureSize(nGamesWidth);
    }


    uint32_t extensionCount;
    const char** extensionNames = 0;
    SDL_Vulkan_GetInstanceExtensions(sdlWindow, &extensionCount, nullptr);
    extensionNames = new const char *[extensionCount];
    SDL_Vulkan_GetInstanceExtensions(sdlWindow, &extensionCount, extensionNames);
    vkInst = createInstance(extensionCount, extensionNames);

    SDL_Vulkan_CreateSurface(sdlWindow, vkInst, &surface);

    if (SDL_GetError() != NULL) {
        SDL_Log("Initialized with errors: %s", SDL_GetError());
    }

    int w;
    int h;
    SDL_GetWindowSize(sdlWindow, &w, &h);

    VulkanInit(w, h);
    SDL_AddEventWatch(HandleVulkanEvents, sdlWindow);

    // Initialize the buffer surfaces
    BlitFXInit();

    return 0;
}

// Run one frame and render the screen
static int Frame(bool bRedraw) // bRedraw = 0
{
    if (pVidImage == NULL) {
        return 1;
    }

    VidFrameCallback(bRedraw);

    return 0;
}

static void SurfToTex()
{
    int nVidPitch = nTextureWidth * nVidImageBPP;

    unsigned char* ps = (unsigned char*)gamescreen;
    unsigned char* pd = (unsigned char*)texture;

    for (int y = nVidImageHeight; y--;) {
        memcpy(pd, ps, nVidImagePitch);
        pd += nVidPitch;
        ps += nVidImagePitch;
    }

}

// Paint the BlitFX surface onto the primary surface
static int Paint(int bValidate)
{


    if (bAppShowFPS && !bAppFullscreen) {
        sprintf(Windowtitle, "FBNeo - FPS: %s - %s - %s", fpsstring, BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_FULLNAME));
        SDL_SetWindowTitle(sdlWindow, Windowtitle);
    }
    //SDLScaleWindow();
    drawFrame();

    return 0;
}

static int Scale(RECT*, int, int)
{
    SDL_Log("Scale Called!!!");
    return 0;
}


static int GetSettings(InterfaceInfo* pInfo)
{
    TCHAR szString[MAX_PATH] = _T("");

    _sntprintf(szString, MAX_PATH, _T("Prescaling using %s (%i� zoom)"), VidSoftFXGetEffect(nUseBlitter), nSize);
    IntInfoAddStringModule(pInfo, szString);

    if (nRotateGame) {
        IntInfoAddStringModule(pInfo, _T("Using software rotation"));
    }

    return 0;
}

// The Video Output plugin:
struct VidOut VidOutSDL2Vulkan = { Init, Exit, Frame, Paint, Scale, GetSettings, _T("SDL Vulkan Video output") };
