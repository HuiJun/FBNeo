// OpenGL via SDL2
#include "burner.h"
#include "vid_support.h"
#include "vid_softfx.h"

#include <SDL.h>
#include <SDL_opengl.h>

extern char videofiltering[3];

static SDL_GLContext glContext = NULL;

static int nInitedSubsytems = 0;

static int nGamesWidth = 0, nGamesHeight = 0; // screen size

static GLint texture_type = GL_UNSIGNED_BYTE;
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
    sdlWindow = NULL;

    SDL_GL_DeleteContext(glContext);
    glContext = NULL;

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

void init_gl()
{
#ifdef FBNEO_DEBUG
    printf("opengl config\n");
#endif
    if ((BurnDrvGetFlags() & BDF_16BIT_ONLY) || (nVidImageBPP != 3)) {
        texture_type = GL_UNSIGNED_SHORT_5_6_5;
    } else {
        texture_type = GL_UNSIGNED_BYTE;
    }

    glShadeModel(GL_FLAT);
    glDisable(GL_POLYGON_SMOOTH);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_POINT_SMOOTH);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    if (strcmp(videofiltering, "0") == 0) {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    } else {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, nTextureWidth, nTextureHeight, 0, GL_RGB, texture_type, texture);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

#ifdef FBNEO_DEBUG
    printf("opengl config done . . . \n");
#endif
}

static void GLAdjustViewport()
{
    int w;
    int h;
    SDL_GetWindowSize(sdlWindow, &w, &h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    if (!nRotateGame) {
        printf("!nRotateGame: %d", nRotateGame);
        glRotatef((bFlipped ? 180.0 : 0.0), 0.0, 0.0, 1.0);
        glOrtho(0, nGamesWidth, nGamesHeight, 0, -1, 1);
        int constrainedHeight = 1.0 / ((w * 1.0 * nGamesHeight) / nGamesWidth);
        glViewport( 0, h > constrainedHeight ? ((h - constrainedHeight) / 2.0) : 0, w, h > constrainedHeight ? constrainedHeight : h);
    } else {
        glRotatef((bFlipped ? 270.0 : 90.0), 0.0, 0.0, 1.0);
        glOrtho(0, nGamesHeight, nGamesWidth, 0, -1, 1);
        int constrainedWidth = (1.0 * h * nGamesWidth) / nGamesHeight;
        glViewport( w > constrainedWidth ? ((w - constrainedWidth) / 2.0) : 0, 0, w > constrainedWidth ? constrainedWidth : w, h);
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

int SDLScaleWindow()
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
            SDL_SetWindowSize(sdlWindow, (display_h * (display_w / display_h)) * 2, display_h * 2);
            SDL_SetWindowFullscreen(sdlWindow, SDL_TRUE);
        } else {
            SDL_RestoreWindow(sdlWindow);
            SDL_SetWindowSize(sdlWindow, display_w * 2, display_h * 2);
            SDL_SetWindowFullscreen(sdlWindow, SDL_FALSE);
        }
    } else {
        if (screenFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) {
            int w;
            int h;
            SDL_GetWindowSize(sdlWindow, &w, &h);
            printf("Unrotated Width/Height: %d, %d\n", w, h);
            //SDL_SetWindowSize(sdlWindow, (display_w * w / h) * 2, display_h * 2);
            SDL_SetWindowFullscreen(sdlWindow, SDL_TRUE);
        } else {
            SDL_RestoreWindow(sdlWindow);
            SDL_SetWindowSize(sdlWindow, display_w * 2, display_h * 2);
            SDL_SetWindowFullscreen(sdlWindow, SDL_FALSE);
        }
    }

    SDL_SetWindowPosition(sdlWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    GLAdjustViewport();

    return 0;
}


static int Init()
{
    nInitedSubsytems = SDL_WasInit(SDL_INIT_VIDEO);

    if (!(nInitedSubsytems & SDL_INIT_VIDEO)) {
        SDL_InitSubSystem(SDL_INIT_VIDEO);
    }

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

    Uint32 screenFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

    if (bAppFullscreen) {
        screenFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN;
    }

    sprintf(Windowtitle, "FBNeo - %s - %s", BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_FULLNAME));

    sdlWindow = SDL_CreateWindow(Windowtitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, nGamesWidth, nGamesHeight, screenFlags);

    if (sdlWindow == NULL) {
        printf("OPENGL failed : %s\n", SDL_GetError());
        return -1;
    }

    if (!nRotateGame) {
        nTextureWidth = GetTextureSize(nGamesWidth);
        nTextureHeight = GetTextureSize(nGamesHeight);
    } else {
        nTextureWidth = GetTextureSize(nGamesHeight);
        nTextureHeight = GetTextureSize(nGamesWidth);
    }

    glContext = SDL_GL_CreateContext(sdlWindow);

    if (glContext == NULL) {
        printf("OPENGL failed : %s\n", SDL_GetError());
        return -1;
    }

    // Initialize the buffer surfaces
    BlitFXInit();

    // Init opengl
    init_gl();

    SDLScaleWindow();

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

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, nTextureWidth, nTextureHeight, 0, GL_RGB, texture_type, texture);
}

static void TexToQuad()
{
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2i(0, 0);
    glTexCoord2f(0, 1);
    glVertex2i(0, nTextureHeight);
    glTexCoord2f(1, 1);
    glVertex2i(nTextureWidth, nTextureHeight);
    glTexCoord2f(1, 0);
    glVertex2i(nTextureWidth, 0);
    glEnd();
    glFinish();
}

// Paint the BlitFX surface onto the primary surface
static int Paint(int bValidate)
{

    SurfToTex();
    TexToQuad();

    if (bAppShowFPS && !bAppFullscreen) {
        sprintf(Windowtitle, "FBNeo - FPS: %s - %s - %s", fpsstring, BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_FULLNAME));
        SDL_SetWindowTitle(sdlWindow, Windowtitle);
    }
    SDL_GL_SwapWindow(sdlWindow);
    SDLScaleWindow();

    return 0;
}

static int vidScale(RECT*, int, int)
{
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
struct VidOut VidOutSDL2OpenGL = { Init, Exit, Frame, Paint, vidScale, GetSettings, _T("SDL OpenGL Video output") };
