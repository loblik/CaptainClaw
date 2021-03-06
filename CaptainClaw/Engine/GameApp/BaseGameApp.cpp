#include "../Resource/ResourceCache.h"
#include "../Audio/Audio.h"
#include "../Events/EventMgr.h"
#include "../Events/EventMgrImpl.h"
#include "../Events/Events.h"
#include "BaseGameLogic.h"
#include "../UserInterface/HumanView.h"
#include "../Resource/ResourceMgr.h"
#include "../Graphics2D/Image.h"

// Resource loaders
#include "../Resource/Loaders/DefaultLoader.h"
#include "../Resource/Loaders/XmlLoader.h"
#include "../Resource/Loaders/WwdLoader.h"
#include "../Resource/Loaders/PalLoader.h"
#include "../Resource/Loaders/PidLoader.h"
#include "../Resource/Loaders/AniLoader.h"
#include "../Resource/Loaders/WavLoader.h"
#include "../Resource/Loaders/MidiLoader.h"
#include "../Resource/Loaders/PcxLoader.h"
#include "../Resource/Loaders/PngLoader.h"

#include "BaseGameApp.h"

TiXmlElement* CreateDefaultDisplayConfig();
TiXmlElement* CreateDefaultAudioConfig();
TiXmlElement* CreateDefaultFontConfig();
TiXmlElement* CreateDefaultAssetsConfig();
TiXmlDocument CreateDefaultConfig();

BaseGameApp* g_pApp = NULL;

BaseGameApp::BaseGameApp()
{
    g_pApp = this;

    m_pGame = NULL;
    m_pResourceCache = NULL;
    m_pEventMgr = NULL;
    m_pWindow = NULL;
    m_pRenderer = NULL;
    m_pPalette = NULL;
    m_pAudio = NULL;
    m_pConsoleFont = NULL;
    m_IsRunning = false;
    m_QuitRequested = false;
    m_IsQuitting = false;
}

bool BaseGameApp::Initialize(int argc, char** argv)
{
    RegisterEngineEvents();
    VRegisterGameEvents();

    // Initialization sequence
    if (!InitializeEventMgr()) return false;
    if (!InitializeDisplay(m_GameOptions)) return false;
    if (!InitializeAudio(m_GameOptions)) return false;
    if (!InitializeFont(m_GameOptions)) return false;
    if (!InitializeResources(m_GameOptions)) return false;
    if (!InitializeLocalization(m_GameOptions)) return false;

    RegisterAllDelegates();

    m_pGame = VCreateGameAndView();
    if (!m_pGame)
    {
        LOG_ERROR("Failed to initialize game logic.");
        return false;
    }

    m_pResourceCache->Preload("/CLAW/*", NULL);
    m_pResourceCache->Preload("/GAME/*", NULL);
    m_pResourceCache->Preload("/STATES/*", NULL);

    m_pResourceMgr->VPreload("*", NULL, CUSTOM_RESOURCE);

    m_IsRunning = true;

    return true;
}

void BaseGameApp::Terminate()
{
    LOG("Terminating...");

    RemoveAllDelegates();

    SAFE_DELETE(m_pGame);
    SDL_DestroyRenderer(m_pRenderer);
    SDL_DestroyWindow(m_pWindow);
    SAFE_DELETE(m_pAudio);
    // TODO - this causes crashes
    //SAFE_DELETE(m_pEventMgr);
    //SAFE_DELETE(m_pResourceCache);

    SaveGameOptions();
}

//=====================================================================================================================
// BaseGameApp::Run - Main game loop
//
//    Handle events -> update game -> render views
//=====================================================================================================================

int32 BaseGameApp::Run()
{
    static uint32 lastTime = SDL_GetTicks();
    SDL_Event event;
    int consecutiveLagSpikes = 0;

    while (m_IsRunning)
    {
        //PROFILE_CPU("MAINLOOP");

        uint32 now = SDL_GetTicks();
        uint32 elapsedTime = now - lastTime;
        lastTime = now;

        // This occurs when recovering program from background or after load
        // We want to ignore these situations
        if (elapsedTime > 1000)
        {
            consecutiveLagSpikes++;
            if (consecutiveLagSpikes > 10)
            {
                LOG_ERROR("Experiencing lag spikes, " + ToStr(consecutiveLagSpikes) + "high latency frames in a row");
            }
            continue;
        }
        consecutiveLagSpikes = 0;

        // Handle all input events
        while (SDL_PollEvent(&event))
        {
            OnEvent(event);
        }

        if (m_pGame)
        {
            // Update game
            {
                //PROFILE_CPU("ONLY GAME UPDATE");
                IEventMgr::Get()->VUpdate(20); // Allow event queue to process for up to 20 ms
                m_pGame->VOnUpdate(elapsedTime);
            }

            // Render game
            for (auto pGameView : m_pGame->m_GameViews)
            {
                //PROFILE_CPU("ONLY RENDER");
                pGameView->VOnRender(elapsedTime);
            }
            
            //m_pGame->VRenderDiagnostics();
        }

        // Artificially decrease fps. Configurable from console
        SDL_Delay(m_GlobalOptions.cpuDelayMs);
    }

    Terminate();

    return 0;
}

void BaseGameApp::OnEvent(SDL_Event& event)
{
    switch (event.type)
    {
        case SDL_QUIT:
        case SDL_APP_TERMINATING:
        {
            m_IsRunning = false;
            break;
        }

        case SDL_WINDOWEVENT:
        {
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED /*||
                event.window.event == SDL_WINDOWEVENT_RESIZED*/)
            {
                OnDisplayChange(event.window.data1, event.window.data2);
            }
            else if (event.window.event == SDL_WINDOWEVENT_RESTORED)
            {
                VOnRestore();
            }
            else if (event.window.event == SDL_WINDOWEVENT_MINIMIZED)
            {
                void VOnMinimized();
            }
            break;
        }

        case SDL_APP_LOWMEMORY:
        {
            LOG_WARNING("Running low on memory");
            break;
        }

        case SDL_APP_DIDENTERBACKGROUND:
        {
            LOG("Entered background");
            break;
        }

        case SDL_APP_DIDENTERFOREGROUND:
        {
            LOG("Entered foreground");
            break;
        }

        case SDL_KEYDOWN:
        case SDL_KEYUP:
        case SDL_TEXTEDITING:
        case SDL_TEXTINPUT:
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
        case SDL_FINGERUP:
        case SDL_FINGERDOWN:
        case SDL_FINGERMOTION:
        {
            if (m_pGame)
            {
                for (GameViewList::reverse_iterator iter = m_pGame->m_GameViews.rbegin();
                    iter != m_pGame->m_GameViews.rend(); ++iter)
                {
                    (*iter)->VOnEvent(event);
                }
            }
            break;
        }
    }
}

void BaseGameApp::OnDisplayChange(int newWidth, int newHeight)
{
    LOG("Display changed. New Width-Height: " + ToStr(newWidth) + "-" + ToStr(newHeight));
}

void BaseGameApp::VOnRestore()
{
    LOG("Window restored.");
}

void BaseGameApp::VOnMinimized()
{
    LOG("Window minimized");
}

bool BaseGameApp::LoadStrings(std::string language)
{
    return true;
}

std::string BaseGameApp::GetString(std::string stringId)
{
    return "";
}

HumanView* BaseGameApp::GetHumanView() const
{
    HumanView *pView = NULL;
    for (GameViewList::iterator i = m_pGame->m_GameViews.begin(); i != m_pGame->m_GameViews.end(); ++i)
    {
        if ((*i)->VGetType() == GameView_Human)
        {
            shared_ptr<IGameView> pIGameView(*i);
            pView = static_cast<HumanView *>(&*pIGameView);
            break;
        }
    }

    return pView;
}

bool BaseGameApp::LoadGameOptions(const char* inConfigFile)
{
    if (!m_XmlConfiguration.LoadFile(inConfigFile))
    {
        LOG_WARNING("Configuration file: " + std::string(inConfigFile)
            + " not found - creating default configuration");
        m_XmlConfiguration = CreateAndReturnDefaultConfig(inConfigFile);
    }

    TiXmlElement* configRoot = m_XmlConfiguration.RootElement();
    if (configRoot == NULL)
    {
        LOG_ERROR("Could not load root element for config file");
        return false;
    }

    //-------------------------------------------------------------------------
    // Display
    //-------------------------------------------------------------------------
    TiXmlElement* displayElem = configRoot->FirstChildElement("Display");
    if (displayElem)
    {
        TiXmlElement* windowSizeElem = displayElem->FirstChildElement("Size");
        if (windowSizeElem)
        {
            windowSizeElem->Attribute("width", &m_GameOptions.windowWidth);
            windowSizeElem->Attribute("height", &m_GameOptions.windowHeight);
        }

        ParseValueFromXmlElem(&m_GameOptions.scale,
            displayElem->FirstChildElement("Scale"));
        ParseValueFromXmlElem(&m_GameOptions.useVerticalSync,
            displayElem->FirstChildElement("UseVerticalSync"));
        ParseValueFromXmlElem(&m_GameOptions.isFullscreen,
            displayElem->FirstChildElement("IsFullscreen"));
        ParseValueFromXmlElem(&m_GameOptions.isFullscreenDesktop,
            displayElem->FirstChildElement("IsFullscreenDesktop"));
    }

    //-------------------------------------------------------------------------
    // Audio
    //-------------------------------------------------------------------------
    TiXmlElement* audioElem = configRoot->FirstChildElement("Audio");
    if (audioElem)
    {
        ParseValueFromXmlElem(&m_GameOptions.frequency,
            audioElem->FirstChildElement("Frequency"));
        ParseValueFromXmlElem(&m_GameOptions.soundChannels,
            audioElem->FirstChildElement("SoundChannels"));
        ParseValueFromXmlElem(&m_GameOptions.mixingChannels,
            audioElem->FirstChildElement("MixingChannels"));
        ParseValueFromXmlElem(&m_GameOptions.chunkSize,
            audioElem->FirstChildElement("ChunkSize"));
        ParseValueFromXmlElem(&m_GameOptions.midiRpcServerPath,
            audioElem->FirstChildElement("MusiscRpcServerPath"));
        ParseValueFromXmlElem(&m_GameOptions.soundVolume,
            audioElem->FirstChildElement("SoundVolume"));
        ParseValueFromXmlElem(&m_GameOptions.musicVolume,
            audioElem->FirstChildElement("MusicVolume"));
        ParseValueFromXmlElem(&m_GameOptions.soundOn,
            audioElem->FirstChildElement("SoundOn"));
        ParseValueFromXmlElem(&m_GameOptions.musicOn,
            audioElem->FirstChildElement("MusicOn"));
    }

    //-------------------------------------------------------------------------
    // Assets
    //-------------------------------------------------------------------------
    TiXmlElement* assetsElem = configRoot->FirstChildElement("Assets");
    if (assetsElem)
    {
        ParseValueFromXmlElem(&m_GameOptions.rezArchivePath,
            assetsElem->FirstChildElement("RezArchive"));
        ParseValueFromXmlElem(&m_GameOptions.customArchivePath,
            assetsElem->FirstChildElement("CustomArchive"));
        ParseValueFromXmlElem(&m_GameOptions.resourceCacheSize,
            assetsElem->FirstChildElement("ResourceCacheSize"));
        ParseValueFromXmlElem(&m_GameOptions.tempDir,
            assetsElem->FirstChildElement("TempDir"));
        ParseValueFromXmlElem(&m_GameOptions.savesFile,
            assetsElem->FirstChildElement("SavesFile"));
    }

    //-------------------------------------------------------------------------
    // Font
    //-------------------------------------------------------------------------
    TiXmlElement* fontRootElem = configRoot->FirstChildElement("Font");
    if (fontRootElem)
    {
        for (TiXmlElement* fontElem = fontRootElem->FirstChildElement("Font");
            fontElem != NULL;
            fontElem = fontElem->NextSiblingElement("Font"))
        {
            if (fontElem->GetText())
            {
                m_GameOptions.fontNames.push_back(fontElem->GetText());
            }
        }

        TiXmlElement* consoleFontElem = fontRootElem->FirstChildElement("ConsoleFont");
        if (consoleFontElem)
        {
            consoleFontElem->Attribute("size", (int*)&m_GameOptions.consoleFontSize);
            if (const char* fontName = consoleFontElem->Attribute("font"))
            {
                m_GameOptions.consoleFontName = fontName;
            }
        }
    }

    //-------------------------------------------------------------------------
    // Console
    //-------------------------------------------------------------------------
    if (TiXmlElement* pConsoleRootElem = configRoot->FirstChildElement("Console"))
    {
        ParseValueFromXmlElem(&m_GameOptions.consoleConfig.backgroundImagePath,
            pConsoleRootElem->FirstChildElement("BackgroundImagePath"));
        ParseValueFromXmlElem(&m_GameOptions.consoleConfig.stretchBackgroundImage,
            pConsoleRootElem->FirstChildElement("StretchBackgroundImage"));
        ParseValueFromXmlElem(&m_GameOptions.consoleConfig.widthRatio,
            pConsoleRootElem->FirstChildElement("WidthRatio"));
        ParseValueFromXmlElem(&m_GameOptions.consoleConfig.heightRatio,
            pConsoleRootElem->FirstChildElement("HeightRatio"));
        ParseValueFromXmlElem(&m_GameOptions.consoleConfig.lineSeparatorHeight,
            pConsoleRootElem->FirstChildElement("LineSeparatorHeight"));
        ParseValueFromXmlElem(&m_GameOptions.consoleConfig.commandPromptOffsetY,
            pConsoleRootElem->FirstChildElement("CommandPromptOffsetY"));
        ParseValueFromXmlElem(&m_GameOptions.consoleConfig.consoleAnimationSpeed,
            pConsoleRootElem->FirstChildElement("ConsoleAnimationSpeed"));
        if (TiXmlElement* pElem = pConsoleRootElem->FirstChildElement("FontColor"))
        {
            int r, g, b;
            pElem->Attribute("r", &r);
            pElem->Attribute("g", &g);
            pElem->Attribute("b", &b);
            m_GameOptions.consoleConfig.fontColor.r = r;
            m_GameOptions.consoleConfig.fontColor.g = g;
            m_GameOptions.consoleConfig.fontColor.b = b;
        }
        ParseValueFromXmlElem(&m_GameOptions.consoleConfig.fontHeight,
            pConsoleRootElem->FirstChildElement("FontHeight"));
        ParseValueFromXmlElem(&m_GameOptions.consoleConfig.leftOffset,
            pConsoleRootElem->FirstChildElement("LeftOffset"));
        ParseValueFromXmlElem(&m_GameOptions.consoleConfig.commandPrompt,
            pConsoleRootElem->FirstChildElement("CommandPrompt"));
        ParseValueFromXmlElem(&m_GameOptions.consoleConfig.fontPath,
            pConsoleRootElem->FirstChildElement("FontPath"));
    }
    else
    {
        LOG_ERROR("Console configuration is missing.");
        return false;
    }
    //-------------------------------------------------------------------------
    // Global options
    //-------------------------------------------------------------------------
    if (TiXmlElement* pGlobalOptionsRootElem = configRoot->FirstChildElement("GlobalOptions"))
    {
        ParseValueFromXmlElem(&m_GlobalOptions.cpuDelayMs, 
            pGlobalOptionsRootElem->FirstChildElement("CpuDelay"));
        ParseValueFromXmlElem(&m_GlobalOptions.maxJumpSpeed,
            pGlobalOptionsRootElem->FirstChildElement("MaxJumpSpeed"));
        ParseValueFromXmlElem(&m_GlobalOptions.maxFallSpeed,
            pGlobalOptionsRootElem->FirstChildElement("MaxFallSpeed"));
        ParseValueFromXmlElem(&m_GlobalOptions.idleSoundQuoteIntervalMs,
            pGlobalOptionsRootElem->FirstChildElement("IdleSoundQuoteInterval"));
        ParseValueFromXmlElem(&m_GlobalOptions.platformSpeedModifier,
            pGlobalOptionsRootElem->FirstChildElement("PlatformSpeedModifier"));
        ParseValueFromXmlElem(&m_GlobalOptions.maxJumpHeight,
            pGlobalOptionsRootElem->FirstChildElement("MaxJumpHeight"));
        ParseValueFromXmlElem(&m_GlobalOptions.powerupMaxJumpHeight,
            pGlobalOptionsRootElem->FirstChildElement("PowerupMaxJumpHeight"));
        ParseValueFromXmlElem(&m_GlobalOptions.skipMenu,
            pGlobalOptionsRootElem->FirstChildElement("SkipMenu"));
    }

    return true;
}

void BaseGameApp::SaveGameOptions(const char* outConfigFile)
{
    LOG_ERROR("Not implemented yet!");
    return;
}

//=====================================================================================================================
// Private implementations
//=====================================================================================================================

//---------------------------------------------------------------------------------------------------------------------
// BaseGameApp::RegisterEngineEvents
//---------------------------------------------------------------------------------------------------------------------
void BaseGameApp::RegisterEngineEvents()
{
    /*REGISTER_EVENT(EventData_Environment_Loaded);
    REGISTER_EVENT(EventData_New_Actor);
    REGISTER_EVENT(EventData_Move_Actor);
    REGISTER_EVENT(EventData_Destroy_Actor);
    REGISTER_EVENT(EventData_Request_New_Actor);
    REGISTER_EVENT(EventData_Network_Player_Actor_Assignment);
    REGISTER_EVENT(EventData_Attach_Actor);
    REGISTER_EVENT(EventData_Collideable_Tile_Created);
    REGISTER_EVENT(EventData_Start_Climb);
    REGISTER_EVENT(EventData_Actor_Fire);
    REGISTER_EVENT(EventData_Actor_Attack);
    REGISTER_EVENT(EventData_New_HUD_Element);
    REGISTER_EVENT(EventData_New_Life);
    REGISTER_EVENT(EventData_Updated_Score);
    REGISTER_EVENT(EventData_Updated_Lives);
    REGISTER_EVENT(EventData_Updated_Health);
    REGISTER_EVENT(EventData_Updated_Ammo);
    REGISTER_EVENT(EventData_Updated_Ammo_Type);
    REGISTER_EVENT(EventData_Request_Change_Ammo_Type);
    REGISTER_EVENT(EventData_Teleport_Actor);*/
}

//---------------------------------------------------------------------------------------------------------------------
// BaseGameApp::InitializeDisplay
//
// Initializes SDL2 main game window and creates SDL2 renderer
//---------------------------------------------------------------------------------------------------------------------
bool BaseGameApp::InitializeDisplay(GameOptions& gameOptions)
{
    LOG(">>>>> Initializing display...");

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
        LOG_ERROR("Failed to initialize SDL2 library");
        return false;
    }

    m_pWindow = SDL_CreateWindow(VGetGameTitle(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        gameOptions.windowWidth, gameOptions.windowHeight, SDL_WINDOW_SHOWN);
    if (m_pWindow == NULL)
    {
        LOG_ERROR("Failed to create main window");
        return false;
    }

    if (gameOptions.isFullscreen)
    {
        SDL_SetWindowFullscreen(m_pWindow, SDL_WINDOW_FULLSCREEN);
        SDL_GetWindowSize(m_pWindow, &m_GameOptions.windowWidth, &m_GameOptions.windowHeight);
    }
    else if (gameOptions.isFullscreenDesktop)
    {
        SDL_SetWindowFullscreen(m_pWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_GetWindowSize(m_pWindow, &m_GameOptions.windowWidth, &m_GameOptions.windowHeight);
    }

    m_WindowSize.Set(gameOptions.windowWidth, gameOptions.windowHeight);

    uint32 rendererFlags = SDL_RENDERER_ACCELERATED;
    if (gameOptions.useVerticalSync)
    {
        rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
    }

    m_pRenderer = SDL_CreateRenderer(m_pWindow, -1, rendererFlags);
    if (m_pRenderer == NULL)
    {
        LOG_ERROR("Failed to create SDL2 Renderer. Error: %s" + std::string(SDL_GetError()));
        return false;
    }

    SDL_RenderSetScale(m_pRenderer, (float)gameOptions.scale, (float)gameOptions.scale);

    LOG("Display successfully initialized.");

    return true;
}

//---------------------------------------------------------------------------------------------------------------------
// BaseGameApp::InitializeAudio
//
// Initializes SDL Mixer as audio device
//---------------------------------------------------------------------------------------------------------------------
bool BaseGameApp::InitializeAudio(GameOptions& gameOptions)
{
    LOG(">>>>> Initializing audio...");

    m_pAudio = new Audio();
    if (!m_pAudio->Initialize(gameOptions))
    {
        LOG_ERROR("Failed to initialize SDL Mixer audio subsystem");
        return false;
    }

    LOG("Audio successfully initialized.");

    return true;
}

//---------------------------------------------------------------------------------------------------------------------
// BaseGameApp::InitializeResources
//
// Register CLAW.REZ resource file as resource cache for assets the game is going to use
//---------------------------------------------------------------------------------------------------------------------
bool BaseGameApp::InitializeResources(GameOptions& gameOptions)
{
    LOG(">>>>> Initializing resource cache...");

    if (gameOptions.rezArchivePath.empty())
    {
        LOG_ERROR("No specified assets resource files in configuration.");
        return false;
    }

    IResourceFile* rezArchive = new ResourceRezArchive(gameOptions.rezArchivePath);

    m_pResourceCache = new ResourceCache(gameOptions.resourceCacheSize, rezArchive, ORIGINAL_RESOURCE);
    if (!m_pResourceCache->Init())
    {
        LOG_ERROR("Failed to initialize resource cachce from resource file: " + std::string(gameOptions.rezArchivePath));
        return false;
    }

    m_pResourceCache->RegisterLoader(DefaultResourceLoader::Create());
    m_pResourceCache->RegisterLoader(XmlResourceLoader::Create());
    m_pResourceCache->RegisterLoader(WwdResourceLoader::Create());
    m_pResourceCache->RegisterLoader(PalResourceLoader::Create());
    m_pResourceCache->RegisterLoader(PidResourceLoader::Create());
    m_pResourceCache->RegisterLoader(AniResourceLoader::Create());
    m_pResourceCache->RegisterLoader(WavResourceLoader::Create());
    m_pResourceCache->RegisterLoader(MidiResourceLoader::Create());
    m_pResourceCache->RegisterLoader(PcxResourceLoader::Create());

    IResourceFile* pCustomArchive = new ResourceZipArchive(gameOptions.customArchivePath);
    ResourceCache* pCustomCache = new ResourceCache(50, pCustomArchive, CUSTOM_RESOURCE);
    if (!pCustomCache->Init())
    {
        LOG_ERROR("Failed to initialize resource cachce from resource file: " + gameOptions.customArchivePath);
        return false;
    }

    pCustomCache->RegisterLoader(DefaultResourceLoader::Create());
    pCustomCache->RegisterLoader(XmlResourceLoader::Create());
    pCustomCache->RegisterLoader(WavResourceLoader::Create());
    pCustomCache->RegisterLoader(PcxResourceLoader::Create());
    pCustomCache->RegisterLoader(PngResourceLoader::Create());

    m_pResourceMgr = new ResourceMgrImpl();
    m_pResourceMgr->VAddResourceCache(m_pResourceCache);
    m_pResourceMgr->VAddResourceCache(pCustomCache);

    LOG("Resource cache successfully initialized");

    return true;
}

//---------------------------------------------------------------------------------------------------------------------
// BaseGameApp::InitializeFont
//---------------------------------------------------------------------------------------------------------------------
bool BaseGameApp::InitializeFont(GameOptions& gameOptions)
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, ">>>>> Initializing font...");

    if (TTF_Init() < 0)
    {
        LOG_ERROR("Failed to initialize SDL TTF font subsystem");
        return false;
    }

    m_pConsoleFont = TTF_OpenFont(gameOptions.consoleFontName.c_str(), gameOptions.consoleFontSize);
    if (m_pConsoleFont == NULL)
    {
        LOG_ERROR("Failed to load TTF font");
        return false;
    }

    LOG("Font successfully initialized...");

    return true;
}

//---------------------------------------------------------------------------------------------------------------------
// BaseGameApp::InitializeLocalization
//---------------------------------------------------------------------------------------------------------------------
bool BaseGameApp::InitializeLocalization(GameOptions& gameOptions)
{
    return true;
}

//---------------------------------------------------------------------------------------------------------------------
// BaseGameApp::InitializeEventMgr
//---------------------------------------------------------------------------------------------------------------------
bool BaseGameApp::InitializeEventMgr()
{
    m_pEventMgr = new EventMgr("BaseGameApp Event Mgr", true);
    if (!m_pEventMgr)
    {
        LOG_ERROR("Failed to create EventMgr.");
        return false;
    }

    return true;
}

Point BaseGameApp::GetScale()
{
    float scaleX, scaleY;
    uint32 windowFlags = GetWindowFlags();
    Point scale(1.0, 1.0);

    SDL_RenderGetScale(m_pRenderer, &scaleX, &scaleY);
    
    scale.Set((double)scaleX, (double)scaleY);

    return scale;
}

uint32 BaseGameApp::GetWindowFlags()
{
    return SDL_GetWindowFlags(m_pWindow);
}

//=====================================================================================================================
// Events
//=====================================================================================================================


void BaseGameApp::RegisterAllDelegates()
{
    IEventMgr::Get()->VAddListener(MakeDelegate(
        this, &BaseGameApp::QuitGameDelegate), EventData_Quit_Game::sk_EventType);
}

void BaseGameApp::RemoveAllDelegates()
{
    IEventMgr::Get()->VRemoveListener(MakeDelegate(
        this, &BaseGameApp::QuitGameDelegate), EventData_Quit_Game::sk_EventType);
}

void BaseGameApp::QuitGameDelegate(IEventDataPtr pEventData)
{
    Terminate();
    exit(0);
}

//=====================================================================================================================
// XML config management
//=====================================================================================================================

TiXmlElement* CreateDefaultDisplayConfig()
{
    TiXmlElement* display = new TiXmlElement("Display");

    XML_ADD_2_PARAM_ELEMENT("Size", "width", ToStr(1280).c_str(), "height", ToStr(768).c_str(), display);
    XML_ADD_TEXT_ELEMENT("Scale", "1", display);
    XML_ADD_TEXT_ELEMENT("UseVerticalSync", "true", display);
    XML_ADD_TEXT_ELEMENT("IsFullscreen", "false", display);
    XML_ADD_TEXT_ELEMENT("IsFullscreenDesktop", "false", display);

    return display;
}

TiXmlElement* CreateDefaultAudioConfig()
{
    TiXmlElement* audio = new TiXmlElement("Audio");

    XML_ADD_TEXT_ELEMENT("Frequency", "44100", audio);
    XML_ADD_TEXT_ELEMENT("SoundChannels", "1", audio);
    XML_ADD_TEXT_ELEMENT("MixingChannels", "24", audio);
    XML_ADD_TEXT_ELEMENT("ChunkSize", "2048", audio);
    XML_ADD_TEXT_ELEMENT("SoundVolume", "50", audio);
    XML_ADD_TEXT_ELEMENT("MusicVolume", "50", audio);
    XML_ADD_TEXT_ELEMENT("MusicRpcServerPath", "MidiProc.exe", audio);

    return audio;
}

TiXmlElement* CreateDefaultFontConfig()
{
    TiXmlElement* font = new TiXmlElement("Font");

    XML_ADD_TEXT_ELEMENT("Font", "clacon.ttf", font);
    XML_ADD_2_PARAM_ELEMENT("ConsoleFont", "font", "clacon.ttf", "size", "20", font);

    return font;
}

TiXmlElement* CreateDefaultAssetsConfig()
{
    TiXmlElement* assets = new TiXmlElement("Assets");

    XML_ADD_TEXT_ELEMENT("RezArchive", "CLAW.REZ", assets);
    XML_ADD_TEXT_ELEMENT("ResourceCacheSize", "50", assets);
    XML_ADD_TEXT_ELEMENT("TempDir", ".", assets);
    XML_ADD_TEXT_ELEMENT("SavesFile", "SAVES.XML", assets);

    return assets;
}

TiXmlElement* CreateDefaultConsoleConfig()
{
TiXmlElement* pConsoleConfig = new TiXmlElement("Console");

    // Assume that the default constructor has default values set
    ConsoleConfig defaultConfig;

    XML_ADD_TEXT_ELEMENT("BackgroundImagePath",
        defaultConfig.backgroundImagePath.c_str(), pConsoleConfig);
    XML_ADD_TEXT_ELEMENT("StretchBackgroundImage",
        ToStr(defaultConfig.stretchBackgroundImage).c_str(), pConsoleConfig);
    XML_ADD_TEXT_ELEMENT("WidthRatio",
        ToStr(defaultConfig.widthRatio).c_str(), pConsoleConfig);
    XML_ADD_TEXT_ELEMENT("HeightRatio",
        ToStr(defaultConfig.heightRatio).c_str(), pConsoleConfig);
    XML_ADD_TEXT_ELEMENT("LineSeparatorHeight",
        ToStr(defaultConfig.lineSeparatorHeight).c_str(), pConsoleConfig);
    XML_ADD_TEXT_ELEMENT("CommandPromptOffsetY",
        ToStr(defaultConfig.commandPromptOffsetY).c_str(), pConsoleConfig);
    XML_ADD_TEXT_ELEMENT("ConsoleAnimationSpeed",
        ToStr(defaultConfig.consoleAnimationSpeed).c_str(), pConsoleConfig);
    XML_ADD_TEXT_ELEMENT("FontPath",
        defaultConfig.fontPath.c_str(), pConsoleConfig);

    TiXmlElement* pColorElem = new TiXmlElement("FontColor");
    pColorElem->SetAttribute("r", defaultConfig.fontColor.r);
    pColorElem->SetAttribute("g", defaultConfig.fontColor.g);
    pColorElem->SetAttribute("b", defaultConfig.fontColor.b);
    pConsoleConfig->LinkEndChild(pColorElem);

    XML_ADD_TEXT_ELEMENT("FontHeight",
        ToStr(defaultConfig.fontHeight).c_str(), pConsoleConfig);
    XML_ADD_TEXT_ELEMENT("LeftOffset",
        ToStr(defaultConfig.leftOffset).c_str(), pConsoleConfig);
    XML_ADD_TEXT_ELEMENT("CommandPrompt",
        defaultConfig.commandPrompt.c_str(), pConsoleConfig);

    return pConsoleConfig;
}

TiXmlDocument BaseGameApp::CreateAndReturnDefaultConfig(const char* inConfigFile)
{
    TiXmlDocument xmlConfig;

    //----- [Configuration]
    TiXmlElement* root = new TiXmlElement("Configuration");
    xmlConfig.LinkEndChild(root);

    root->LinkEndChild(CreateDefaultDisplayConfig());
    root->LinkEndChild(CreateDefaultAudioConfig());
    root->LinkEndChild(CreateDefaultFontConfig());
    root->LinkEndChild(CreateDefaultAssetsConfig());
    root->LinkEndChild(CreateDefaultConsoleConfig());

    xmlConfig.SaveFile(inConfigFile);

    return xmlConfig;
}
