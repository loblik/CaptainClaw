#include "RenderComponent.h"
#include "../../GameApp/BaseGameApp.h"
#include "../../GameApp/BaseGameLogic.h"
#include "../../Graphics2D/Image.h"

#include "../../Resource/Loaders/PidLoader.h"

#include "PositionComponent.h"

#include "../../Scene/ActorSceneNode.h"
#include "../../Scene/TilePlaneSceneNode.h"
#include "../../Scene/HUDSceneNode.h"

#include "../../Events/Events.h"
#include "../Actor.h"

#include <algorithm>
#include <cctype>

const char* ActorRenderComponent::g_Name = "ActorRenderComponent";
const char* TilePlaneRenderComponent::g_Name = "TilePlaneRenderComponent";
const char* HUDRenderComponent::g_Name = "HUDRenderComponent";

//=================================================================================================
// BaseRenderComponent Implementation
//
//=================================================================================================

bool BaseRenderComponent::VInit(TiXmlElement* pXmlData)
{
    assert(pXmlData != NULL);

    WapPal* palette = g_pApp->GetCurrentPalette();

    for (TiXmlElement* pImagePathElem = pXmlData->FirstChildElement("ImagePath");
        pImagePathElem; pImagePathElem = pImagePathElem->NextSiblingElement("ImagePath"))
    {
        if (palette == NULL)
        {
            LOG_ERROR("Attempting to create BaseRenderComponent without existing palette");
            return false;
        }

        const char* imagesPath = pImagePathElem->GetText();
        assert(imagesPath != NULL);

        // Get all files residing in given directory
        // !!! THIS ASSUMES THAT WE ONLY WANT IMAGES FROM THIS DIRECTORY. IT IGNORES ALL NESTED DIRECTORIES !!!
        // Maybe add recursive algo to libwap
        std::string imageDir = std::string(imagesPath);
        //imageDir = imageDir.substr(0, imageDir.find("*")); // Get rid of everything after '*' including '*'
        imageDir = imageDir.substr(0, imageDir.find_last_of("/")); // Get rid of filenames - get just path to the final directory
        std::vector<std::string> matchingPathNames =
            g_pApp->GetResourceCache()->GetAllFilesInDirectory(imageDir.c_str());

        // Remove all images which dont conform to the given pattern
        // This affects probably only object with "DoNothing" logic
        // Compute everything in lowercase to assure compatibility with everything in the engine
        std::string imageDirLowercase(imagesPath);
        std::transform(imageDirLowercase.begin(), imageDirLowercase.end(), imageDirLowercase.begin(), (int(*)(int)) std::tolower);
        for (auto iter = matchingPathNames.begin(); iter != matchingPathNames.end(); /*++iter*/)
        {
            if (!WildcardMatch(imageDirLowercase.c_str(), (*iter).c_str()))
            {
                iter = matchingPathNames.erase(iter);
            }
            else
            {
                iter++;
            }
        }

        for (std::string imagePath : matchingPathNames)
        {
            // Only load known image formats
            if (!WildcardMatch("*.pid", imagePath.c_str()))
            {
                continue;
            }

            shared_ptr<Image> image = PidResourceLoader::LoadAndReturnImage(imagePath.c_str(), palette);
            if (!image)
            {
                LOG_WARNING("Failed to load image: " + imagePath);
                return false;
            }

            std::string imageNameKey = StripPathAndExtension(imagePath);

            // Check if we dont already have the image loaded
            if (m_ImageMap.count(imageNameKey) > 0)
            {
                LOG_WARNING("Trying to load existing image: " + imagePath);
                continue;
            }

            // HACK: all animation frames should be in format frameXXX
            /*if (imageNameKey.find("chest") != std::string::npos)
            {
                imageNameKey.replace(0, 5, "frame");
            }
            // HACK: all animation frames should be in format frameXXX (length = 8)
            if (imageNameKey.find("frame") != std::string::npos && imageNameKey.length() != 8)
            {
                int imageNameNumStr = std::stoi(std::string(imageNameKey).erase(0, 5));
                imageNameKey = "frame" + Util::ConvertToThreeDigitsString(imageNameNumStr);
            }*/
            // Just reconstruct it...
            if (imageNameKey.length() > 3 /* Hack for checkpointflag */ || 
                std::string(pXmlData->Parent()->ToElement()->Attribute("Type")) == "GAME_CHECKPOINTFLAG")
            {
                std::string tmp = imageNameKey;
                tmp.erase(std::remove_if(tmp.begin(), tmp.end(), (int(*)(int))std::isalpha), tmp.end());
                if (!tmp.empty())
                {
                    int imageNum = std::stoi(tmp);
                    imageNameKey = "frame" + Util::ConvertToThreeDigitsString(imageNum);
                }
                else
                {
                    //LOG(imagePath);
                }
            }

            m_ImageMap.insert(std::make_pair(imageNameKey, image));
        }
    }

    if (m_ImageMap.empty())
    {
        LOG_WARNING("Image map for render component is empty. Actor type: " + std::string(pXmlData->Parent()->ToElement()->Attribute("Type")));
    }

    /*for (auto it : m_ImageMap)
    {
        LOG(it.first);
    }*/

    return VDelegateInit(pXmlData);
}

TiXmlElement* BaseRenderComponent::VGenerateXml()
{
    return NULL;
}

void BaseRenderComponent::VPostInit()
{
    shared_ptr<SceneNode> pNode = GetSceneNode();
    if (pNode)
    {
        shared_ptr<EventData_New_Render_Component> pEvent(new EventData_New_Render_Component(_owner->GetGUID(), pNode));
        IEventMgr::Get()->VTriggerEvent(pEvent);
    }
}

void BaseRenderComponent::VOnChanged()
{

}

shared_ptr<SceneNode> BaseRenderComponent::GetSceneNode()
{
    if (!m_pSceneNode)
    {
        m_pSceneNode = VCreateSceneNode();
    }

    return m_pSceneNode;
}

weak_ptr<Image> BaseRenderComponent::GetImage(std::string imageName)
{
    if (m_ImageMap.count(imageName) > 0)
    {
        return m_ImageMap[imageName];
    }

    return weak_ptr<Image>();
}

weak_ptr<Image> BaseRenderComponent::GetImage(uint32 imageId)
{
    return weak_ptr<Image>();
}

bool BaseRenderComponent::HasImage(std::string imageName)
{
    if (m_ImageMap.count(imageName) > 0)
    {
        return true;
    }

    return false;
}

bool BaseRenderComponent::HasImage(int32 imageId)
{
    return false;
}

//=================================================================================================
// [ActorComponent::BaseRenderComponent::ActorRenderComponent]
// 
//      ActorRenderComponent Implementation
//
//=================================================================================================

ActorRenderComponent::ActorRenderComponent()
{
    // Everything is visible by default, should be explicitly stated that its not visible
    m_IsVisible = true;
    m_IsMirrored = false;
    m_IsInverted = false;
    m_ZCoord = 0;
}

bool ActorRenderComponent::VDelegateInit(TiXmlElement* pXmlData)
{
    if (TiXmlElement* pElem = pXmlData->FirstChildElement("Visible"))
    {
        if (std::string(pElem->GetText()) == "true")
        {
            m_IsVisible = true;
        }
        else
        {
            m_IsVisible = false;
        }
    }
    if (pXmlData->FirstChildElement("Mirrored") &&
        std::string(pXmlData->FirstChildElement("Mirrored")->GetText()) == "true")
    {
        m_IsMirrored = true;
    }
    if (pXmlData->FirstChildElement("Inverted") &&
        std::string(pXmlData->FirstChildElement("Inverted")->GetText()) == "true")
    {
        m_IsInverted = true;
    }
    if (TiXmlElement* pElem = pXmlData->FirstChildElement("ZCoord"))
    {
        m_ZCoord = std::stoi(pElem->GetText());
    }

    if (m_IsVisible)
    {
        if (m_ImageMap.empty())
        {
            LOG_WARNING("Creating actor render component without valid image.");
            return true;
        }
        m_CurrentImage = m_ImageMap.begin()->second;
    }

    return true;
}

SDL_Rect ActorRenderComponent::VGetPositionRect() const
{
    SDL_Rect positionRect = { 0 };

    if (!m_IsVisible)
    {
        return positionRect;
    }

    shared_ptr<PositionComponent> pPositionComponent = _owner->GetPositionComponent();
    if (!pPositionComponent)
    {
        return positionRect;
    }

    if (!m_CurrentImage)
    {
        return positionRect;
    }

    positionRect = {
        (int)pPositionComponent->GetX() - m_CurrentImage->GetWidth() / 2 + m_CurrentImage->GetOffsetX(),
        (int)pPositionComponent->GetY() - m_CurrentImage->GetHeight() / 2 + m_CurrentImage->GetOffsetY(),
        m_CurrentImage->GetWidth(),
        m_CurrentImage->GetHeight()
    };

    return positionRect;
}

shared_ptr<SceneNode> ActorRenderComponent::VCreateSceneNode()
{
    shared_ptr<PositionComponent> pPositionComponent = 
        MakeStrongPtr(_owner->GetComponent<PositionComponent>(PositionComponent::g_Name));
    if (!pPositionComponent)
    {
        // can't render without a transform
        return shared_ptr<SceneNode>();
    }

    Point pos(pPositionComponent->GetX(), pPositionComponent->GetY());
    shared_ptr<SceneNode> pActorNode(new SDL2ActorSceneNode(_owner->GetGUID(), this, RenderPass_Actor, pos, m_ZCoord));

    return pActorNode;
}

void ActorRenderComponent::VCreateInheritedXmlElements(TiXmlElement* pBaseElement)
{

}

void ActorRenderComponent::SetImage(std::string imageName)
{
    if (m_ImageMap.count(imageName) > 0)
    {
        m_CurrentImage = m_ImageMap[imageName];
    }
    else
    {
        // Known... Treasure chest HUD
        if (imageName == "frame000")
        {
            return;
        }
        LOG_ERROR("Trying to set nonexistant image: " + imageName + " to render component of actor: " +
            _owner->GetName());
    }
}

//=================================================================================================
// [ActorComponent::BaseRenderComponent::TilePlaneRenderComponent]
// 
//      TilePlaneRenderComponent Implementation
//
//=================================================================================================

void TilePlaneRenderComponent::ProcessMainPlaneTiles(const TileList& tileList)
{
    const int tilesCount = tileList.size();
    int currentTileIdx = 0;
    
    while (currentTileIdx < tilesCount)
    {
        TileList continuousTiles = GetAllContinuousTiles(tileList, currentTileIdx);
        assert(continuousTiles.size() >= 1);

        // Discard all empty tiles
        if (continuousTiles[0] != -1)
        {
            // Do stuff with continuous tiles
            //-----
            //-----
            //-----
            //

            if (continuousTiles.size() > 1)
            {
                LOG("Continuous tiles on Y: " + ToStr(GetTileInfo(tileList, currentTileIdx).y));
                std::string contTilesString;
                for (int tileId : continuousTiles)
                {
                    contTilesString += ToStr(tileId) + "-";
                }
                LOG(contTilesString);
            }

            int tileWidth = m_PlaneProperties.tilePixelWidth;
            TileInfo firstTileInfo = GetTileInfo(tileList, currentTileIdx);
            int currentX = firstTileInfo.x;
            int currentY = firstTileInfo.y;
            for (int tileId : continuousTiles)
            {
                shared_ptr<EventData_Collideable_Tile_Created> pEvent(
                    new EventData_Collideable_Tile_Created(tileId, currentX, currentY));
                IEventMgr::Get()->VTriggerEvent(pEvent);

                currentX += tileWidth;
            }
        }

        currentTileIdx += continuousTiles.size();
    }
}

TileList TilePlaneRenderComponent::GetAllContinuousTiles(const TileList& tileList, int fromTileIdx)
{
    const int tilesCount = tileList.size();

    TileList continuousTileList;

    // First tile always belongs in the list
    TileInfo currentTile = GetTileInfo(tileList, fromTileIdx);
    continuousTileList.push_back(currentTile.tileId);

    // We already have the first tile, check next one
    int currentTileIdx = fromTileIdx + 1;

    // Continue adding tiles as long as its ID and Y position matches the first tile
    while (true)
    {
        // Keep track of the idx so we do not step out of bounds
        if (currentTileIdx >= tilesCount)
        {
            break;
        }

        TileInfo nextTile = GetTileInfo(tileList, currentTileIdx);
        if ((nextTile.tileId == currentTile.tileId) && (nextTile.y == currentTile.y))
        {
            continuousTileList.push_back(nextTile.tileId);
            currentTile = nextTile;
            currentTileIdx++;
        }
        else
        {
            break;
        }
    }

    return continuousTileList;
}

TileInfo TilePlaneRenderComponent::GetTileInfo(const TileList& tileList, int tileIdx)
{
    assert(tileIdx < tileList.size());

    TileInfo tileInfo;

    tileInfo.tileId = tileList[tileIdx];
    tileInfo.x = (tileIdx * m_PlaneProperties.tilePixelWidth) % m_PlaneProperties.planePixelWidth;
    tileInfo.y = ((tileIdx / m_PlaneProperties.tilesOnAxisX) * m_PlaneProperties.tilePixelHeight) % m_PlaneProperties.planePixelHeight;

    return tileInfo;
}

bool TilePlaneRenderComponent::VDelegateInit(TiXmlElement* pXmlData)
{
    TiXmlElement* pPlaneProperties = pXmlData->FirstChildElement("PlaneProperties");
    if (!pPlaneProperties)
    {
        LOG_ERROR("Could not locate PlaneProperties.");
        return false;
    }

    // If anyone knows a better way, tell me
    //-------------------------------------------------------------------------
    // Fill plane properties
    //-------------------------------------------------------------------------
    if (TiXmlElement* node = pPlaneProperties->FirstChildElement("PlaneName"))
    {
        m_PlaneProperties.name = node->GetText();
    }
    if (TiXmlElement* node = pPlaneProperties->FirstChildElement("MainPlane"))
    {
        m_PlaneProperties.isMainPlane = std::string(node->GetText()) == "true";
    }
    if (TiXmlElement* node = pPlaneProperties->FirstChildElement("NoDraw"))
    {
        m_PlaneProperties.isDrawable = std::string(node->GetText()) == "true";
    }
    if (TiXmlElement* node = pPlaneProperties->FirstChildElement("WrappedX"))
    {
        m_PlaneProperties.isWrappedX = std::string(node->GetText()) == "true";
    }
    if (TiXmlElement* node = pPlaneProperties->FirstChildElement("WrappedY"))
    {
        m_PlaneProperties.isWrappedY = std::string(node->GetText()) == "true";
    }
    if (TiXmlElement* node = pPlaneProperties->FirstChildElement("TileAutoSized"))
    {
        m_PlaneProperties.isTileAutosized = std::string(node->GetText()) == "true";
    }
    if (TiXmlElement* node = pPlaneProperties->FirstChildElement("TilePixelSize"))
    {
        node->Attribute("width", &m_PlaneProperties.tilePixelWidth);
        node->Attribute("height", &m_PlaneProperties.tilePixelHeight);
    }
    if (TiXmlElement* node = pPlaneProperties->FirstChildElement("PlanePixelSize"))
    {
        node->Attribute("width", &m_PlaneProperties.planePixelWidth);
        node->Attribute("height", &m_PlaneProperties.planePixelHeight);
    }
    if (TiXmlElement* node = pPlaneProperties->FirstChildElement("MoveSpeedPercentage"))
    {
        node->Attribute("x", &m_PlaneProperties.movementPercentX);
        node->Attribute("y", &m_PlaneProperties.movementPercentY);
    }
    if (TiXmlElement* node = pPlaneProperties->FirstChildElement("FillColor"))
    {
        m_PlaneProperties.fillColor = std::stoi(node->GetText());
    }
    if (TiXmlElement* node = pPlaneProperties->FirstChildElement("ZCoord"))
    {
        m_PlaneProperties.zCoord = std::stoi(node->GetText());
    }
    
    m_PlaneProperties.tilesOnAxisX = m_PlaneProperties.planePixelWidth / m_PlaneProperties.tilePixelWidth;
    m_PlaneProperties.tilesOnAxisY = m_PlaneProperties.planePixelHeight / m_PlaneProperties.tilePixelHeight;

    // Filler tile - for the background plane

    WapPal* pPallette = g_pApp->GetCurrentPalette();
    assert(m_PlaneProperties.fillColor >= 0 && m_PlaneProperties.fillColor <= 255);
    WAP_ColorRGBA wapColor = pPallette->colors[m_PlaneProperties.fillColor];
    SDL_Color fillColor = { wapColor.r, wapColor.g, wapColor.b, wapColor.a };

    m_pFillImage.reset(Image::CreateImageFromColor(
        fillColor,
        m_PlaneProperties.tilePixelWidth,
        m_PlaneProperties.tilePixelHeight,
        g_pApp->GetRenderer()));
    assert(m_pFillImage != nullptr);

    //-------------------------------------------------------------------------
    // Fill plane tiles
    //-------------------------------------------------------------------------

    TiXmlElement* pTileElements = pXmlData->FirstChildElement("Tiles");
    if (!pTileElements)
    {
        LOG_ERROR("Tiles are missing.");
        return false;
    }
    PROFILE_CPU("PLANE CREATION");
    int32 tileIdx = 0;

    TileList tileList;
    for (TiXmlElement* pTileNode = pTileElements->FirstChildElement(); 
        pTileNode != NULL; 
        pTileNode = pTileNode->NextSiblingElement())
    {
        std::string tileFileName(pTileNode->GetText());

        // Temporarily keep track of the tiles stored
        int32 tileId = std::stoi(tileFileName);
        tileList.push_back(tileId);

#if 0
        //------ HACKERINO, should be in separate component, but...
        //    Sends event that tile on the main (action) plane was created
        //    It will be added into physics subsystem then but it is handled elsewhere. This just sends event that some
        //    tile has been created. 
        if (m_PlaneProperties.isMainPlane)
        {
            int32 tileId = std::stoi(tileFileName);

            // Temporary - I guess ?
            tileList.push_back(tileId);


            if (tileId != -1) // Tiles with id == -1 are empty tiles (= air)
            {
                // THIS IS THE ORIGINAL CODE WHICH DOES NOT DEAL WITH MERGING CONTINUOUS TILES
                /*
                int32 posX = (tileIdx * m_PlaneProperties.tilePixelWidth) % m_PlaneProperties.planePixelWidth;
                int32 posY = ((tileIdx / m_PlaneProperties.tilesOnAxisX) * m_PlaneProperties.tilePixelHeight) % m_PlaneProperties.planePixelHeight;
                shared_ptr<EventData_Collideable_Tile_Created> pEvent(new EventData_Collideable_Tile_Created(tileId, posX, posY));
                IEventMgr::Get()->VTriggerEvent(pEvent);
                */

                int32 posX = (tileIdx * m_PlaneProperties.tilePixelWidth) % m_PlaneProperties.planePixelWidth;
                int32 posY = ((tileIdx / m_PlaneProperties.tilesOnAxisX) * m_PlaneProperties.tilePixelHeight) % m_PlaneProperties.planePixelHeight;

                tilesInRow.push_back({ tileId, posX, posY });
                lastPos = posY;
            }

            // This block of uncanny code deals with merging continuous tiles if possible
            if (tileId == -1 || (prevTileId != tileId))
            {
                if (tilesInRow.size() > 1)
                {
                    std::string outString;
                    for (auto tile : tilesInRow)
                    {
                        outString += ToStr(tile.tileId) + "-";
                    }

                    /*LOG("Row Y: " + ToStr(lastPos));
                    LOG("Tiles: " + outString);*/

                    //-----

                    //if (tilesInRow.size() == 2 && tilesInRow[0].tileId != tilesInRow[1].tileId)

                    // First and last are different - resolve last one separately and pop it
                    if (tilesInRow.front().tileId != tilesInRow.back().tileId)
                    {
                        TileInfo tile = tilesInRow.back();
                        shared_ptr<EventData_Collideable_Tile_Created> pEvent(
                            new EventData_Collideable_Tile_Created(tile.tileId, tile.x, tile.y));
                        IEventMgr::Get()->VTriggerEvent(pEvent);

                        tilesInRow.pop_back();
                    }
                    // If there were only 2 tiles to begin with, it can happen that tehre can be only 1 now
                    if (tilesInRow.size() == 1)
                    {
                        shared_ptr<EventData_Collideable_Tile_Created> pEvent(
                            new EventData_Collideable_Tile_Created(tilesInRow[0].tileId, tilesInRow[0].x, tilesInRow[0].y));
                        IEventMgr::Get()->VTriggerEvent(pEvent);
                    }
                    else
                    {
                        // Sanity check
                        for (TileInfo& tile : tilesInRow)
                        {
                            assert(tile.tileId == tilesInRow[0].tileId);
                        }

                        // Try to merge here, multiple same tiles in the row
                        auto findIt = pTilePrototypeMap->find(tilesInRow[0].tileId);
                        assert(findIt != pTilePrototypeMap->end());
                        TileCollisionPrototype tileProto = findIt->second;

                        int countAttrTiles = 0;
                        bool canBeMerged = true;
                        TileCollisionRectangle mergedRect;
                        for (TileCollisionRectangle& colRect : tileProto.collisionRectangles)
                        {
                            if (colRect.collisionType != CollisionType_None)
                            {
                                countAttrTiles++;
                                mergedRect = colRect;

                                if (colRect.collisionRect.w != m_PlaneProperties.tilePixelWidth)
                                {
                                    canBeMerged = false;
                                }
                            }
                        }

                        if (countAttrTiles != 1)
                        {
                            canBeMerged = false;
                        }

                        if (canBeMerged)
                        {
                            LOG("Can be merged !");
                            for (TileInfo& tile : tilesInRow)
                            {
                                shared_ptr<EventData_Collideable_Tile_Created> pEvent(
                                    new EventData_Collideable_Tile_Created(tile.tileId, tile.x, tile.y));
                                IEventMgr::Get()->VTriggerEvent(pEvent);
                            }
                        }
                        else
                        {
                            for (TileInfo& tile : tilesInRow)
                            {
                                shared_ptr<EventData_Collideable_Tile_Created> pEvent(
                                    new EventData_Collideable_Tile_Created(tile.tileId, tile.x, tile.y));
                                IEventMgr::Get()->VTriggerEvent(pEvent);
                            }
                        }
                    }
                }
                else
                {
                    for (TileInfo& tile : tilesInRow)
                    {
                        shared_ptr<EventData_Collideable_Tile_Created> pEvent(
                            new EventData_Collideable_Tile_Created(tile.tileId, tile.x, tile.y));
                        IEventMgr::Get()->VTriggerEvent(pEvent);
                    }
                }

                tilesInRow.clear();
            }
            prevTileId = tileId;
        }
#endif

        // Convert to three digits, e.g. "2" -> "002" or "15" -> "015"
        if (tileFileName.length() == 1) 
        { 
            tileFileName = "00" + tileFileName; 
        }
        else if (tileFileName.length() == 2 &&
            !(g_pApp->GetGameLogic()->GetCurrentLevelData()->GetLevelNumber() == 1 && tileFileName == "74")) 
        { 
            tileFileName = "0" + tileFileName; 
        }

        auto findIt = m_ImageMap.find(tileFileName);
        if (findIt != m_ImageMap.end())
        {
            m_TileImageList.push_back(findIt->second.get());
        }
        else if (tileFileName == "0-1" || tileFileName == "-1")
        {
            m_TileImageList.push_back(NULL);
        }
        else if (m_PlaneProperties.name == "Background") // Use fill color, only aplicable to background
        {
            assert(m_pFillImage != nullptr);

            m_TileImageList.push_back(m_pFillImage.get());
        }
        else
        {
            LOG_ERROR("Could not find plane tile: " + tileFileName);
            return false;
        }

        tileIdx++;
    }

    if (m_PlaneProperties.isMainPlane)
    {
        ProcessMainPlaneTiles(tileList);
    }

    if (m_TileImageList.empty())
    {
        LOG_ERROR("No tiles on plane were loaded.");
        return false;
    }

    int planeWidth = m_PlaneProperties.planePixelWidth;
    int planeHeight = m_PlaneProperties.planePixelHeight;
    if (m_PlaneProperties.isWrappedX)
    {
        planeWidth = INT32_MAX;
    }
    if (m_PlaneProperties.isWrappedY)
    {
        planeHeight = INT32_MAX;
    }

    m_PositionRect = { 0, 0, planeWidth, planeHeight };

    return true;
}

SDL_Rect TilePlaneRenderComponent::VGetPositionRect() const
{
    return m_PositionRect;
}

shared_ptr<SceneNode> TilePlaneRenderComponent::VCreateSceneNode()
{
    shared_ptr<PositionComponent> pPositionComponent =
        MakeStrongPtr(_owner->GetComponent<PositionComponent>(PositionComponent::g_Name));
    if (!pPositionComponent)
    {
        // can't render without a transform
        return shared_ptr<SceneNode>();
    }

    RenderPass renderPass;
    if (m_PlaneProperties.name == "Background")
    {
        renderPass = RenderPass_Background;
    }
    else if (m_PlaneProperties.name == "Action")
    {
        renderPass = RenderPass_Action;
    }
    else if (m_PlaneProperties.name == "Front")
    {
        renderPass = RenderPass_Foreground;
    }
    else
    {
        LOG_ERROR("Unknown plane name: " + m_PlaneProperties.name + " - cannot assign corrent render pass");
        return shared_ptr<SceneNode>();
    }

    Point pos(pPositionComponent->GetX(), pPositionComponent->GetY());
    shared_ptr<SceneNode> pTilePlaneNode(new SDL2TilePlaneSceneNode(_owner->GetGUID(), this, renderPass, pos));

    return pTilePlaneNode;
}

void TilePlaneRenderComponent::VCreateInheritedXmlElements(TiXmlElement* pBaseElement)
{

}

//=================================================================================================
// [ActorComponent::BaseRenderComponent::ActorRenderComponent::HUDRenderComponent]
// 
//      HUDRenderComponent Implementation
//
//=================================================================================================

HUDRenderComponent::HUDRenderComponent()
    :
    m_IsAnchoredRight(false),
    m_IsAnchoredBottom(false)
{ }

bool HUDRenderComponent::VDelegateInit(TiXmlElement* pXmlData)
{
    if (!ActorRenderComponent::VDelegateInit(pXmlData))
    {
        return false;
    }

    if (TiXmlElement* pElem = pXmlData->FirstChildElement("AnchorRight"))
    {
        m_IsAnchoredRight = std::string(pElem->GetText()) == "true";
    }
    if (TiXmlElement* pElem = pXmlData->FirstChildElement("AnchorBottom"))
    {
        m_IsAnchoredBottom = std::string(pElem->GetText()) == "true";
    }
    if (TiXmlElement* pElem = pXmlData->FirstChildElement("HUDElementKey"))
    {
        m_HUDElementKey = pElem->GetText();
    }

    return true;
}

SDL_Rect HUDRenderComponent::VGetPositionRect() const
{
    // HACK: Always visible
    return { 0, 0, 1000000, 1000000 };
}

shared_ptr<SceneNode> HUDRenderComponent::VCreateSceneNode()
{
    shared_ptr<PositionComponent> pPositionComponent =
        MakeStrongPtr(_owner->GetComponent<PositionComponent>(PositionComponent::g_Name));
    if (!pPositionComponent)
    {
        // can't render without a transform
        return shared_ptr<SceneNode>();
    }

    Point pos(pPositionComponent->GetX(), pPositionComponent->GetY());
    shared_ptr<SDL2HUDSceneNode> pHUDNode(new SDL2HUDSceneNode(_owner->GetGUID(), this, RenderPass_HUD, pos, IsVisible()));

    shared_ptr<EventData_New_HUD_Element> pEvent(new EventData_New_HUD_Element(_owner->GetGUID(), m_HUDElementKey, pHUDNode));
    IEventMgr::Get()->VTriggerEvent(pEvent);

    return pHUDNode;
}

void HUDRenderComponent::VCreateInheritedXmlElements(TiXmlElement* pBaseElement)
{

}
