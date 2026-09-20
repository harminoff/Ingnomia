#pragma once

// Explicit opt-in comparison in an isolated profile. Uses one generated world
// for both shader versions, so terrain, sprites and camera frames match exactly.
#include "game/newgamesettings.h"
#include "game/world.h"
#include "gfx/spritefactory.h"
#include "gfx/sprite.h"
#include <memory>

inline void scheduleLightingProbe(QApplication& app, GameManager* manager)
{
    const QString folder = qEnvironmentVariable("INGNOMIA_LIGHTING_PROBE");
    const QString baseline = qEnvironmentVariable("INGNOMIA_LIGHTING_BASELINE_CONTENT");
    if (folder.isEmpty() || qEnvironmentVariable("INGNOMIA_DATA_FOLDER").isEmpty()) return;
    QDir().mkpath(folder);
    const QString content = Global::cfg->get("dataPath").toString();
    struct State {
        bool ready = false;
        int surface = 92;
        int cave = 87;
        unsigned int torchSprite = 0;
        unsigned int caveId = 0;
        QVector<QPair<unsigned int, Position>> outdoor;
    };
    auto state = std::make_shared<State>();
    auto report = [folder](const QString& text) {
        QFile file(folder + "/result.txt");
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            file.write((text + "\n").toUtf8());
    };
    auto publish = [manager]() {
        if (manager->game()) manager->eventConnector()->aggregatorRenderer()->onUpdateAnyTileInfo(manager->game()->w()->updatedTiles());
    };
    QTimer::singleShot(2500, manager, [manager, state, report, publish]() {
        Global::newGameSettings->setSeed("1342516210");
        Global::newGameSettings->setKingdomName("LightingProbe");
        manager->startNewGame();
        manager->setPaused(true);
        auto* game = manager->game();
        auto* world = game->w();
        state->surface = Global::newGameSettings->ground();
        state->cave = state->surface - 5;
        if (Global::dimX < 70 || Global::dimY < 70 || state->cave < 2) return;
        state->torchSprite = game->sf()->createSprite("GroundTorch", {"AppleWood"})->uID;
        const auto floor = game->sf()->createSprite("RoughFloor", {"Granite"})->uID;
        const auto wall = game->sf()->createSprite("RoughWall", {"Granite"})->uID;
        // Two roofed rooms divided by a view-blocking wall. A hidden patch in
        // the dark room checks that the shader does not expose unknown tiles.
        for (int z = state->cave - 1; z <= state->cave + 1; ++z)
        for (int y = 38; y <= 58; ++y) for (int x = 36; x <= 64; ++x) {
            Tile& tile = world->getTile(x,y,z);
            tile = Tile{};
            tile.floorType = FT_SOLIDFLOOR;
            tile.floorSpriteUID = floor;
            bool solid = z != state->cave || x == 36 || x == 64 || y == 38 || y == 58 || x == 50;
            if (solid) {
                tile.wallType = static_cast<WallType>(WT_SOLIDWALL | WT_MOVEBLOCKING | WT_VIEWBLOCKING);
                tile.wallSpriteUID = wall;
            } else tile.flags += TileFlag::TF_WALKABLE;
            if (x >= 55 && x <= 59 && y >= 51 && y <= 55) tile.flags += TileFlag::TF_UNDISCOVERED;
            world->addToUpdateList(x,y,z);
        }
        state->caveId = GameState::createID();
        world->getTile(47,47,state->cave).wallSpriteUID = state->torchSprite;
        for (const Position target : {Position(51,50,state->surface), Position(40,53,state->surface)}) {
            Position best;
            int distance = 1000000;
            for (int y=35; y<=64; ++y) for (int x=35; x<=64; ++x) {
                const auto& tile = world->getTile(x,y,state->surface);
                if (!(tile.floorType & FT_SOLIDFLOOR) || tile.wallSpriteUID || tile.fluidLevel || (tile.flags & TileFlag::TF_UNDISCOVERED)) continue;
                Position p(x,y,state->surface);
                if (p.distSquare(target) < distance) { best=p; distance=p.distSquare(target); }
            }
            if (distance < 1000000) {
                world->getTile(best).wallSpriteUID = state->torchSprite;
                world->addToUpdateList(best);
                state->outdoor.append(qMakePair(GameState::createID(),best));
                report(QString("outdoor source=%1,%2,%3").arg(best.x).arg(best.y).arg(best.z));
            }
        }
        state->ready = true;
        publish();
        report("READY same-world comparison; minimum light=" + Global::cfg->get("lightMin").toString());
    });
    auto lights = [manager,state,publish,report](bool outdoors, bool cave) {
        if (!state->ready) return;
        auto* world = manager->game()->w();
        for (const auto& source : state->outdoor) {
            world->removeLight(source.first);
            if (outdoors) world->addLight(source.first,source.second,21);
        }
        world->removeLight(state->caveId);
        if (cave) world->addLight(state->caveId,Position(47,47,state->cave),21);
        publish();
        const int nearLight = world->getTile(48,47,state->cave).lightLevel;
        const int shadow = world->getTile(52,47,state->cave).lightLevel;
        report(QString("lights outdoor=%1 cave=%2 near=%3 behindWall=%4 hidden=%5")
            .arg(outdoors).arg(cave).arg(nearLight).arg(shadow)
            .arg(bool(world->getTile(56,53,state->cave).flags & TileFlag::TF_UNDISCOVERED)));
        if (shadow != 0 || (cave && nearLight <= 0)) report("FAIL light-field fixture");
    };
    auto day = [manager,state](bool daylight) {
        if (!state->ready) return;
        GameState::daylight = daylight;
        GameState::hour = daylight ? 12 : 0;
        GameState::minute = 0;
        manager->game()->sendTime();
    };
    auto capture = [&app,folder,state](int delay, QString name, bool cave, bool rotate = false) {
        QTimer::singleShot(delay, &app, [folder,state,name,cave,rotate]() {
            if (!state->ready) return;
            auto& window = MainWindow::getInstance();
            const int z = cave ? state->cave : qMin(Global::dimZ - 2, state->surface + 5);
            if (rotate) window.renderer()->rotate(1);
            window.onUiSetViewLevel(z);
            window.renderer()->onCenterCameraPosition(rotate ? Position(Global::dimY-49,50,z) : Position(50,48,z));
            window.renderer()->setScale(cave ? 1.5f : 1.3f);
            qputenv("INGNOMIA_UI_CAPTURE",(folder+"/"+name+".png").toUtf8());
            qputenv("INGNOMIA_UI_CAPTURE_FRAME","1");
            window.armUiCapture();
        });
    };
    for (int phase=0; phase<2; ++phase) {
        const int offset = phase*23000;
        const QString prefix = phase ? "after" : "before";
        QTimer::singleShot(9000+offset,&app,[baseline,content,phase]() {
            Global::cfg->set("dataPath",phase || baseline.isEmpty() ? content : baseline);
            MainWindow::getInstance().renderer()->reloadShaders();
        });
        QTimer::singleShot(10000+offset,manager,[day,lights]() { lights(true,true); day(true); });
        capture(13000+offset,prefix+"-day",false);
        QTimer::singleShot(15000+offset,manager,[day]() { day(false); });
        capture(18000+offset,prefix+"-night-lit",false);
        QTimer::singleShot(20000+offset,manager,[lights]() { lights(false,true); });
        capture(23000+offset,prefix+"-night",false);
        capture(25000+offset,prefix+"-cave-lit",true);
        QTimer::singleShot(27000+offset,manager,[lights]() { lights(false,false); });
        capture(30000+offset,prefix+"-cave-dark",true);
    }
    QTimer::singleShot(55000,manager,[lights]() { lights(false,true); });
    capture(58000,"after-cave-rotated",true,true);
    QTimer::singleShot(61000,&app,[content,report]() {
        Global::cfg->set("dataPath",content);
        report("COMPLETE");
        if (Global::eventConnector) QMetaObject::invokeMethod(Global::eventConnector,"onExit",Qt::QueuedConnection);
    });
}

