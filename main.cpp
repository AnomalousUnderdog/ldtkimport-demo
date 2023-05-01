#include <string>
#include <unordered_map>
#include <iostream>
#include <sstream>

#include <SFML/Graphics.hpp>
#include <SFML/Window/VideoMode.hpp>

#include "ldtkimport/LdtkDefFile.h"
#include "ldtkimport/Level.h"

using namespace ldtkimport::RunSettings;

struct TileSetImage
{
   sf::Texture image;
   std::unordered_map<ldtkimport::tileid_t, sf::IntRect> tiles;
};

struct LdtkAssets
{
   ldtkimport::LdtkDefFile ldtk;
   std::unordered_map<ldtkimport::uid_t, TileSetImage> tilesetImages;
   sf::Sprite sprite;

   bool load(
#if !defined(NDEBUG) && LDTK_IMPORT_DEBUG_RULE > 0
      ldtkimport::RulesLog &rulesLog,
#endif
      std::string filename)
   {
      bool loadSuccess = ldtk.loadFromFile(
#if !defined(NDEBUG) && LDTK_IMPORT_DEBUG_RULE > 0
         rulesLog,
#endif
         filename.c_str(), false);

      if (!loadSuccess)
      {
         std::cerr << "Could not load: " << filename << std::endl;
         return false;
      }

      // Create TileSetImage for each tileset in the ldtkFile,
      // go through each active rule,
      // check the tileIds and create the IntRect for each.

      size_t lastSlashIdx = filename.find_last_of("\\/");

      // Loop through all tilesets and get the filename
      for (auto tileset = ldtk.tilesetCBegin(), end = ldtk.tilesetCEnd(); tileset != end; ++tileset)
      {
         if (tileset->imagePath.empty())
         {
            continue;
         }

         std::string imagePath;
         if (lastSlashIdx != std::string::npos)
         {
            imagePath = filename.substr(0, lastSlashIdx + 1) + tileset->imagePath;
         }
         else
         {
            imagePath = tileset->imagePath;
         }

         std::cout << "Loading: " << imagePath << std::endl;

         TileSetImage tileSetImage;
         if (!tileSetImage.image.loadFromFile(imagePath))
         {
            std::cerr << "Failed to load: " << imagePath << std::endl;
            return false;
         }

         tilesetImages.insert(std::make_pair(tileset->uid, tileSetImage));
      }

      // Assign the IntRects
      // To get which IntRects should be used, we'll have to go through all rules of all layers.
      // This allows us to skip creating IntRects for unused tiles.
      for (auto layer = ldtk.layerCBegin(), layerEnd = ldtk.layerCEnd(); layer != layerEnd; ++layer)
      {
         const ldtkimport::TileSet *tileset = nullptr;
         if (!ldtk.getTileset(layer->tilesetDefUid, tileset))
         {
            std::cerr << "TileSet " << layer->tilesetDefUid << " was not found in ldtk file" << std::endl;
            continue;
         }
         // be extra sure
         ASSERT(tileset != nullptr, "result arg of getTileset should not be null if return value is true");

         if (tilesetImages.count(tileset->uid) == 0)
         {
            std::cerr << "TileSet " << tileset->uid << " was not found in tilesetImages" << std::endl;
            continue;
         }

         auto &tilesetImage = tilesetImages[tileset->uid];
         const double cellPixelSize = layer->cellPixelSize;

         for (auto ruleGroup = layer->ruleGroups.cbegin(), ruleGroupEnd = layer->ruleGroups.cend(); ruleGroup != ruleGroupEnd; ++ruleGroup)
         {
            for (auto rule = ruleGroup->rules.cbegin(), ruleEnd = ruleGroup->rules.cend(); rule != ruleEnd; ++rule)
            {
               auto &tileIds = rule->tileIds;
               for (auto tile = tileIds.cbegin(), tileEnd = tileIds.cend(); tile != tileEnd; ++tile)
               {
                  ldtkimport::tileid_t tileId = (*tile);

                  if (tilesetImage.tiles.count(tileId) > 0)
                  {
                     // this tileId is already assigned, skip it
                     continue;
                  }

                  int16_t tileX, tileY;
                  tileset->getCoordinates(tileId, tileX, tileY);

                  tilesetImage.tiles.insert(std::make_pair(tileId, sf::IntRect(tileX * cellPixelSize, tileY * cellPixelSize, cellPixelSize, cellPixelSize)));
               } // for Tiles
            } // for Rule
         } // for RuleGroup
      } // for Layer

      return true;
   }

   void drawTiles(const ldtkimport::tiles_t *tilesToDraw, uint8_t idxToStartDrawing, ldtkimport::dimensions_t cellPixelSize, ldtkimport::dimensions_t cellPixelHalfSize, int x, int y, int cellX, int cellY, TileSetImage &tilesetImage, sf::RenderWindow &window, sf::Sprite &sprite)
   {
      for (int tileIdx = idxToStartDrawing; tileIdx >= 0; --tileIdx)
      {
         const auto &tile = (*tilesToDraw)[tileIdx];
         float offsetX = tile.getOffsetX(cellPixelHalfSize);
         float offsetY = tile.getOffsetY(cellPixelHalfSize);
         float scaleX;
         float scaleY;
         float pivotX;
         float pivotY;

         if (tile.isFlippedX())
         {
            scaleX = -1;
            pivotX = cellPixelSize;
         }
         else
         {
            scaleX = 1;
            pivotX = 0;
         }

         if (tile.isFlippedY())
         {
            scaleY = -1;
            pivotY = cellPixelSize;
         }
         else
         {
            scaleY = 1;
            pivotY = 0;
         }

         sprite.setTextureRect(tilesetImage.tiles[tile.tileId]);
         sprite.setPosition(x + (cellX * cellPixelSize) + offsetX, y + (cellY * cellPixelSize) + offsetY);
         sprite.setOrigin(pivotX, pivotY);
         sprite.setScale(scaleX, scaleY);
         window.draw(sprite);
      }
   }

   void draw(int x, int y, const ldtkimport::Level &level, sf::RenderWindow &window)
   {
      auto cellCountX = level.getWidth();
      auto cellCountY = level.getHeight();

      for (int layerNum = ldtk.getLayerCount(); layerNum > 0; --layerNum)
      {
         const auto &layer = ldtk.getLayer(layerNum - 1);
         const auto &tileGrid = level.getTileGrid(layerNum - 1);

         ldtkimport::TileSet *tileset = nullptr;
         if (!ldtk.getTileset(layer.tilesetDefUid, tileset))
         {
            continue;
         }
         // be extra sure
         ASSERT(tileset != nullptr, "result arg of getTileset should not be null if return value is true");

         if (tilesetImages.count(tileset->uid) == 0)
         {
            continue;
         }

         auto &tilesetImage = tilesetImages[tileset->uid];
         sprite.setTexture(tilesetImage.image);

         const auto cellPixelSize = layer.cellPixelSize;
         const float halfGridSize = cellPixelSize * 0.5f;

         /// @todo probably need to do this vertically too (for offset down tiles)
         const ldtkimport::tiles_t *tilesDelayedDraw = nullptr;
         uint8_t idxOfDelayedDraw = -1;
         uint8_t rulePriorityOfDelayedDraw = UINT8_MAX;
         int cellXOfDelayedDraw;
         int cellYOfDelayedDraw;

         for (int cellY = 0; cellY < cellCountY; ++cellY)
         {
            for (int cellX = 0; cellX < cellCountX; ++cellX)
            {
               // these are the tiles in this cell
               auto &tiles = tileGrid(cellX, cellY);

               // we draw the tiles in reverse
               uint8_t tileIdx = tiles.size()-1;
               for (auto tile = tiles.crbegin(), tileEnd = tiles.crend(); tile != tileEnd; ++tile)
               {
                  if (tile->hasOffsetRight() && (cellX < cellCountX - 1) && tileGrid(cellX + 1, cellY).size() > 0)
                  {
                     // this tile might need to be drawn on top of the tiles to the right,
                     // so delay drawing this tile and continue to the next tiles first
                     tilesDelayedDraw = &tiles;
                     idxOfDelayedDraw = tileIdx;
                     rulePriorityOfDelayedDraw = tile->priority;
                     cellXOfDelayedDraw = cellX;
                     cellYOfDelayedDraw = cellY;
                     break;
                  }

                  if (tilesDelayedDraw != nullptr && cellX != cellXOfDelayedDraw && rulePriorityOfDelayedDraw > tile->priority)
                  {
                     // now draw the tiles we delayed drawing
                     // we'll draw the right-offset'ed tile now (plus other tiles on top of it) since there's a higher priority tile that will come next
                     drawTiles(tilesDelayedDraw, idxOfDelayedDraw, cellPixelSize, halfGridSize, x, y, cellXOfDelayedDraw, cellYOfDelayedDraw, tilesetImage, window, sprite);
                     tilesDelayedDraw = nullptr;
                  }

                  float offsetX = tile->getOffsetX(halfGridSize);
                  float offsetY = tile->getOffsetY(halfGridSize);
                  float scaleX;
                  float scaleY;
                  float pivotX;
                  float pivotY;

                  if (tile->isFlippedX())
                  {
                     scaleX = -1;
                     pivotX = cellPixelSize;
                  }
                  else
                  {
                     scaleX = 1;
                     pivotX = 0;
                  }

                  if (tile->isFlippedY())
                  {
                     scaleY = -1;
                     pivotY = cellPixelSize;
                  }
                  else
                  {
                     scaleY = 1;
                     pivotY = 0;
                  }

                  sprite.setTextureRect(tilesetImage.tiles[tile->tileId]);
                  sprite.setPosition(x + (cellX * cellPixelSize) + offsetX, y + (cellY * cellPixelSize) + offsetY);
                  sprite.setOrigin(pivotX, pivotY);
                  sprite.setScale(scaleX, scaleY);
                  window.draw(sprite);
                  --tileIdx;
               } // for tiles

               if (tilesDelayedDraw != nullptr && cellX != cellXOfDelayedDraw && rulePriorityOfDelayedDraw < tiles.front().priority)
               {
                  // now draw the tiles we delayed drawing
                  // now this right-offset'ed tile (plus other tiles on top of it) will be drawn on top of all the tiles just drawn
                  drawTiles(tilesDelayedDraw, idxOfDelayedDraw, cellPixelSize, halfGridSize, x, y, cellXOfDelayedDraw, cellYOfDelayedDraw, tilesetImage, window, sprite);
                  tilesDelayedDraw = nullptr;
               }

            } // for cellX
         } // for cellY
      } // for Layer
   }
};

struct CellInfo
{
   const TileSetImage *tileSetImage;
   sf::Vector2f pos;
   const ldtkimport::TileInCell &tileInfo;
};

int main()
{
   // gets rid of annoying "Failed to set DirectInput device axis mode: 1" spam message
   sf::err().rdbuf(nullptr);

   sf::RenderWindow window(sf::VideoMode(1110, 680), "LDtk Import Demo");

   window.setFramerateLimit(60);

#if !defined(NDEBUG) && LDTK_IMPORT_DEBUG_RULE > 0
   ldtkimport::RulesLog rulesLog;
#endif
   LdtkAssets demoLdtk;

   bool loadSuccess = demoLdtk.load(
#if !defined(NDEBUG) && LDTK_IMPORT_DEBUG_RULE > 0
      rulesLog,
#endif
      "assets/Demo.ldtk");

   if (!loadSuccess)
   {
      return EXIT_FAILURE;
   }

   // I hardcode getting the cell pixel size from the first layer
   // because I know the ldtk file used in this demo has at least 1 layer,
   // but proper code should check if the file is empty.
   const int cellPixelSize = demoLdtk.ldtk.layerCBegin()->cellPixelSize;

   ldtkimport::Level level;
   level.setIntGrid(50, 30, {
      0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1,1,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,
      1,1,1,1,1,1,1,1,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,1,1,
      1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,1,1,1,1,0,0,0,0,0,
      0,0,0,1,1,1,1,1,1,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      1,1,1,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,
      0,1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1,1,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,1,1,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,3,3,3,3,
      3,3,3,3,3,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,
      1,1,1,0,0,0,1,1,1,1,3,3,3,3,3,3,3,3,3,3,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,
      0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,1,0,0,0,1,1,1,1,3,3,3,3,3,3,3,3,3,3,
      1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,1,0,0,
      0,1,1,1,1,3,3,3,3,3,3,3,3,3,3,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,
      0,1,1,1,0,0,0,0,0,0,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,0,0,0,0,0,0,1,1,1,0,0,0,0,0,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,0,
      0,0,0,0,0,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
      1,1,1,1,1,1,1,1,1,1,0,1,1,1,0,0,0,0,0,0,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,0,0,0,0,0,0,
      1,1,1,0,0,0,0,0,1,1,1,1,1,1,2,2,2,2,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,0,
      0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,1,1,
      1,1,2,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
      1,1,1,1,1,1,1,1,1,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
      0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,1,1,1,1,1,1,1,
      1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
      1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,
      1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
      0,0,0,0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
      3,3,3,3,3,3,3,3,3,3,3,3,3,3,0,0,0,0,0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,
      3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,0,0,0,0,0,0,
      0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
      3,3,3,3,3,3,3,3,3,0,0,0,0,0,0,0,0,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
      3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,0,0,0,0,0,0 });

   const int levelPixelWidth = level.getWidth() * cellPixelSize;
   const int levelPixelHeight = level.getHeight() * cellPixelSize;

   demoLdtk.ldtk.runRules(
#if !defined(NDEBUG) && LDTK_IMPORT_DEBUG_RULE > 0
      rulesLog,
#endif
      level);

   const auto &gotBgColor = demoLdtk.ldtk.getBgColor8();
   sf::Color bgColor(gotBgColor.r, gotBgColor.g, gotBgColor.b);

   sf::Font font;
   if (!font.loadFromFile("assets/FiraCode-Regular.ttf"))
   {
      std::cerr << "Could not load: Fira Code font" << std::endl;
      return EXIT_FAILURE;
   }

   sf::Text creditMessage(L"Press spacebar to randomize. Left click on a cell to show diagnostic info.\n\nRogue Fantasy Catacombs Tileset by Szadi art https://szadiart.itch.io/rogue-fantasy-catacombs\nFira Code font OFL-1.1 license (C) 2014 The Fira Code Project Authors https://github.com/tonsky/FiraCode", font, 12);
   creditMessage.setPosition(5, window.getSize().y - creditMessage.getLocalBounds().height - 5);

   sf::Text mouseInfoText("", font, 12);
   mouseInfoText.setPosition(levelPixelWidth + 5, 5);

   sf::Text cellInfoText("", font, 12);
   cellInfoText.setPosition(levelPixelWidth + 75, 65);

   sf::Texture tileBg;
   if (!tileBg.loadFromFile("assets/TileBg.png"))
   {
      std::cerr << "Could not load: TileBg.png" << std::endl;
      return EXIT_FAILURE;
   }
   
   sf::Sprite tileBgSprite;
   tileBgSprite.setTexture(tileBg);

   sf::Vector2i mousePos;
   sf::Vector2i cellPos;

   sf::RectangleShape hoveredCell(sf::Vector2f(cellPixelSize, cellPixelSize));
   hoveredCell.setFillColor(sf::Color(0, 0, 0, 0));
   hoveredCell.setOutlineColor(sf::Color(200, 200, 200, 200));
   hoveredCell.setOutlineThickness(1);

   sf::RectangleShape clickedCell(sf::Vector2f(cellPixelSize, cellPixelSize));
   clickedCell.setFillColor(sf::Color(10, 100, 10, 127));
   clickedCell.setOutlineColor(sf::Color(5, 30, 5, 200));
   clickedCell.setOutlineThickness(1);

   std::vector<CellInfo> cellInfo;
   cellInfo.reserve(4);
   sf::Sprite tile;

   while (window.isOpen())
   {
      sf::Event event;
      while (window.pollEvent(event))
      {
         switch (event.type)
         {
            case sf::Event::KeyPressed:
            {
               if (event.key.code == sf::Keyboard::Space)
               {
                  demoLdtk.ldtk.runRules(
#if !defined(NDEBUG) && LDTK_IMPORT_DEBUG_RULE > 0
                     rulesLog,
#endif
                     level, RandomizeSeeds | FasterStampBreakOnMatch);
               }
            } // fall through so that new random generated level will also refresh diagnostic info
            case sf::Event::MouseButtonPressed:
            {
               mousePos = sf::Mouse::getPosition(window);

               // don't update mouse cell position if triggered by random generation keypress
               if (event.type != sf::Event::KeyPressed && mousePos.x < levelPixelWidth && mousePos.y < levelPixelHeight)
               {
                  cellPos.x = mousePos.x / cellPixelSize;
                  cellPos.y = mousePos.y / cellPixelSize;
               }

               std::stringstream mouseInfoString;
               mouseInfoString << "Mouse Pos: " << mousePos.x << ", " << mousePos.y << std::endl;
               mouseInfoString << "Cell Pos: " << cellPos.x << ", " << cellPos.y << std::endl;

               clickedCell.setPosition(cellPos.x * cellPixelSize, cellPos.y * cellPixelSize);

               cellInfo.clear();
               int lineCount = 0;

               // --------------------------------------
               // Get the IntGridValue of the cell

               const auto &intGrid = level.getIntGrid();
               const auto intGridValueAtCell = intGrid(cellPos.x, cellPos.y);
               const ldtkimport::IntGridValue *intGridValueDef = nullptr;

               // Note: I hardcode to layer index 2 because I know that's where the intgrid is in the ldtk file for this demo.
               // TODO: I should add the layer def uid to the IntGrid
               if (demoLdtk.ldtk.getLayer(2).getIntGridValue(intGridValueAtCell, intGridValueDef))
               {
                  mouseInfoString << "IntGridValue: " << intGridValueAtCell << " " << intGridValueDef->name << std::endl;
               }
               else
               {
                  mouseInfoString << "IntGridValue: " << intGridValueAtCell << std::endl;
               }
               mouseInfoText.setString(mouseInfoString.str());

               // --------------------------------------

               std::stringstream cellInfoString;

               // TileGrids store the results of the rule pattern matching process.
               // They correspond to each Layer in a LdtkDefFile.
               for (int tileGridIdx = 0, tileGridEnd = level.getTileGridCount(); tileGridIdx < tileGridEnd; ++tileGridIdx)
               {
                  const auto &tileGrid = level.getTileGrid(tileGridIdx);

                  const auto &tiles = tileGrid(cellPos.x, cellPos.y);
                  if (tiles.size() == 0)
                  {
                     continue;
                  }

#if !defined(NDEBUG) && LDTK_IMPORT_DEBUG_RULE > 0
                  const auto &tileGridLog = rulesLog.tileGrid[tileGridIdx];
                  const auto &rulesInCell = tileGridLog[ldtkimport::GridUtility::getIndex(cellPos.x, cellPos.y, tileGrid.getWidth())];
#endif

                  const TileSetImage *tilesetImage = nullptr;

                  // Get the Layer that corresponds to this TileGrid, so we can display the Layer name.
                  // Normally the order of layers match the order of tilegrids,
                  // but to be safe we get by Layer Uid.
                  const ldtkimport::Layer *layer = nullptr;
                  if (demoLdtk.ldtk.getLayer(tileGrid.getLayerUid(), layer))
                  {
                     cellInfoString << layer->name << ": " << tiles.size() << std::endl;

                     ldtkimport::TileSet *tileset = nullptr;
                     if (!demoLdtk.ldtk.getTileset(layer->tilesetDefUid, tileset))
                     {
                        continue;
                     }
                     // be extra sure
                     ASSERT(tileset != nullptr, "result arg of getTileset should not be null if return value is true");

                     if (demoLdtk.tilesetImages.count(tileset->uid) == 0)
                     {
                        continue;
                     }

                     tilesetImage = &demoLdtk.tilesetImages[tileset->uid];
                  }
                  else
                  {
                     // Can't find Layer for this TileGrid, so just display the TileGrid index.
                     cellInfoString << "TileGrid " << tileGridIdx << ": " << tiles.size() << std::endl;
                  }

                  ++lineCount;

#if !defined(NDEBUG) && LDTK_IMPORT_DEBUG_RULE > 0
                  ASSERT(rulesInCell.size() == tiles.size(),
                  "rulesInCell size should match tiles size. rulesInCell.size(): " << rulesInCell.size() << " tiles.size(): " << tiles.size() <<
                  " at (" << cellPos.x << ", " << cellPos.y << ")");
#endif

                  for (int tileIdx = 0, tileEnd = tiles.size(); tileIdx < tileEnd; ++tileIdx)
                  {
                     const auto &tile = tiles[tileIdx];

                     cellInfo.push_back(CellInfo{tilesetImage, sf::Vector2f(levelPixelWidth + 20, cellInfoText.getPosition().y + (lineCount * 15)), tile});

                     cellInfoString << (tileIdx+1) << ") Tile Id " << tile.tileId << std::endl;

#if !defined(NDEBUG) && LDTK_IMPORT_DEBUG_RULE > 0
                     cellInfoString << "   Rule Uid: " << rulesInCell[tileIdx] << std::endl;
                     ++lineCount;

                     const ldtkimport::RuleGroup *ruleGroup;
                     if (demoLdtk.ldtk.getRuleGroupOfRule(rulesInCell[tileIdx], ruleGroup))
                     {
                        cellInfoString << "   RuleGroup: " << ruleGroup->name << std::endl;
                        ++lineCount;
                     }
#endif
                     cellInfoString << "   Priority: " << +(tile.priority) << std::endl;

                     // --------------------------------------

                     cellInfoString << "   Offsets:";
                     if (tile.hasOffsetUp())
                     {
                        cellInfoString << " up";
                     }
                     else if (tile.hasOffsetDown())
                     {
                        cellInfoString << " down";
                     }

                     if (tile.hasOffsetLeft())
                     {
                        cellInfoString << " left";
                     }
                     else if (tile.hasOffsetRight())
                     {
                        cellInfoString << " right";
                     }
                     cellInfoString << std::endl;

                     // --------------------------------------

                     cellInfoString << "   Flipped:";
                     if (tile.isFlippedX())
                     {
                        cellInfoString << " X";
                     }
                     if (tile.isFlippedY())
                     {
                        cellInfoString << " Y";
                     }
                     cellInfoString << std::endl;

                     // --------------------------------------

                     lineCount += 4;

                     if (tile.isFinal())
                     {
                        cellInfoString << "   Final" << std::endl;
                        ++lineCount;
                     }
                     cellInfoString << std::endl;
                     ++lineCount;
                  }
               }

               cellInfoText.setString(cellInfoString.str());
            }
            case sf::Event::MouseMoved:
            {
               mousePos = sf::Mouse::getPosition(window);

               if (mousePos.x < levelPixelWidth && mousePos.y < levelPixelHeight)
               {
                  hoveredCell.setPosition((mousePos.x / cellPixelSize) * cellPixelSize, (mousePos.y / cellPixelSize) * cellPixelSize);
               }
               break;
            }
            case sf::Event::Resized:
            {
               sf::FloatRect visibleArea(0, 0, event.size.width, event.size.height);
               window.setView(sf::View(visibleArea));
               break;
            }
            case sf::Event::Closed:
            {
               window.close();
               break;
            }
         }
      }

      window.clear(bgColor);
      demoLdtk.draw(0, 0, level, window);
      window.draw(creditMessage);
      window.draw(mouseInfoText);
      window.draw(cellInfoText);
      window.draw(clickedCell);
      window.draw(hoveredCell);

      const float infoCellScale = 3;

      for (auto c = cellInfo.cbegin(), end = cellInfo.cend(); c != end; ++c)
      {
         tile.setTexture(c->tileSetImage->image);
         float scaleX;
         float scaleY;

         sf::Vector2f pos(c->pos);

         if (c->tileInfo.isFlippedX())
         {
            scaleX = -infoCellScale;
            pos.x += cellPixelSize * infoCellScale;
         }
         else
         {
            scaleX = infoCellScale;
         }

         if (c->tileInfo.isFlippedY())
         {
            scaleY = -infoCellScale;
            pos.y += cellPixelSize * infoCellScale;
         }
         else
         {
            scaleY = infoCellScale;
         }
         tile.setTextureRect(c->tileSetImage->tiles.at(c->tileInfo.tileId));
         tile.setPosition(pos);
         tile.setScale(scaleX, scaleY);
         tileBgSprite.setPosition(c->pos);
         window.draw(tileBgSprite);
         window.draw(tile);
      }

      window.display();
   }

   return EXIT_SUCCESS;
}
