#pragma once

// Explicit opt-in, isolated-profile regression probe. Runs real selection/job
// and worker task code; captures the production OpenGL framebuffer, not a mock.
#include "base/db.h"
#include "base/selection.h"
#include "game/gnome.h"
#include "game/newgamesettings.h"
#include "game/world.h"
#include "gfx/sprite.h"
#include "gfx/spritefactory.h"
#include "gui/aggregatorselection.h"
#include <atomic>
#include <cmath>
#include <memory>
#include "layer_scroll_probe.h"

class PlacementProbeWorker : public Gnome
{
public:
    PlacementProbeWorker(Position& pos, Game* game) : Gnome(pos, "Placement probe", Gender::MALE, game) {}
    bool execute(const QSharedPointer<Job>& job)
    {
        m_job = job;
        m_jobID = job->id();
        m_workPosition = m_position;
        m_currentTask = DB::selectRows("Jobs_Tasks", "ID", job->type()).first();
        if (job->type() == "DigStairsDown") return constructDugStairs();
        if (job->type() == "DigRampDown") return constructDugRamp();
        if (job->type() == "DigHole") return digHole();
        return mineWall();
    }
};

inline void schedulePlacementProbe(QApplication& app, GameManager* manager)
{
    const QString folder = qEnvironmentVariable("INGNOMIA_PLACEMENT_PROBE");
    if (folder.isEmpty() || qEnvironmentVariable("INGNOMIA_DATA_FOLDER").isEmpty()) return;
    QDir().mkpath(folder);
    struct State {
        std::atomic_bool ready{false};
        bool checkedPreview = false;
        Position target;
        QSharedPointer<Job> job;
        std::unique_ptr<PlacementProbeWorker> worker;
    };
    auto state = std::make_shared<State>();
    auto report = [folder](const QString& text) {
        QFile file(folder + "/result.txt");
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) file.write((text + "\n").toUtf8());
    };
    auto publish = [manager]() {
        manager->eventConnector()->aggregatorRenderer()->onUpdateAnyTileInfo(manager->game()->w()->updatedTiles());
    };
    QTimer::singleShot(2500, manager, [manager,state,report,publish]() {
        Global::newGameSettings->setSeed("1342516210");
        Global::newGameSettings->setKingdomName("PlacementProbe");
        manager->startNewGame();
        manager->setPaused(true);
        // Allow the initial renderer/atlas upload to finish before adding the
        // isolated fixture's sprites. Startup and rendering use separate threads.
        QTimer::singleShot(2000,manager,[manager,state,report,publish]() {
        auto* game = manager->game();
        auto* world = game->w();
        QObject::connect(manager->eventConnector()->aggregatorSelection(),
            &AggregatorSelection::signalUpdateSelection,manager,
            [weakState=std::weak_ptr<State>(state),game,report](const QMap<unsigned int,SelectionData>& data,bool) {
                // Do not retain the test worker past Game's destruction at exit.
                const auto state = weakState.lock();
                if (!state || state->checkedPreview || data.isEmpty()) return;
                const QString action = Global::sel->action();
                if (action != "DigStairsDown" && action != "DigRampDown" && action != "DigHole") return;
                const Position selected = Global::sel->getSelection().first().first;
                const bool wallTop = !Global::wallsLowered && (game->w()->getTile(selected).wallType & WT_SOLIDWALL);
                auto* marker = game->sf()->createSprite(wallTop ? "SelectionWallTop" : "SelectionFloorTop", {"None"});
                // Check actual sprite pixels too: the old WallTop asset still
                // contained cube sides, despite its name.
                const QImage markerImage = marker->pixmap("Spring",0,0).toImage();
                const int topRow = wallTop ? 16 : 32;
                bool flatDiamond = true;
                int visiblePixels = 0;
                for (int y=0; y<markerImage.height(); ++y)
                for (int x=0; x<markerImage.width(); ++x) {
                    if (!markerImage.pixelColor(x,y).alpha()) continue;
                    ++visiblePixels;
                    if (y < topRow || y >= topRow+16) flatDiamond = false;
                }
                const bool expectsItem = action == "DigStairsDown" || action == "DigRampDown";
                const auto item = expectsItem ? game->sf()->createSprite(action == "DigStairsDown" ? "Stairs" : "Ramp", {"None"}) : nullptr;
                bool markerPresent = false;
                bool itemAligned = !expectsItem;
                for (const auto& preview : data) {
                    if (preview.spriteID == marker->uID && preview.pos == selected)
                        markerPresent = flatDiamond && visiblePixels > 0 && preview.previewYOffset == 0.f;
                    if (item && preview.spriteID == item->uID && preview.pos == selected.belowOf()) {
                        const float itemTopHeight = 28.f + 20.f*(preview.pos.z-selected.z) + preview.previewYOffset;
                        itemAligned = std::abs(itemTopHeight-(wallTop ? 28.f : 12.f)) < 0.001f
                            && preview.localRot == Global::sel->rotation();
                    }
                }
                const bool previewValid = data.size() == (expectsItem ? 2 : 1) && markerPresent && itemAligned;
                report(QString("%1 marker+item preview sprites=%2 at=%3 item=%4 rotation=%5")
                    .arg(previewValid ? "PASS" : "FAIL").arg(data.size()).arg(selected.toString())
                    .arg(expectsItem).arg(Global::sel->rotation()));
                state->checkedPreview = true;
            });
        const int z = Global::newGameSettings->ground() - 5;
        state->target = Position(50,48,z);
        const bool openFloor = qEnvironmentVariable("INGNOMIA_PLACEMENT_SURFACE") == "floor";
        const auto floor = game->sf()->createSprite("RoughFloor", {"Granite"})->uID;
        const auto wall = game->sf()->createSprite("RoughWall", {"Granite"})->uID;
        for (int level=z-2; level<=z+1; ++level)
        for (int y=36; y<=60; ++y) for (int x=38; x<=62; ++x) {
            Tile& tile = world->getTile(x,y,level);
            tile = Tile{};
            tile.floorType = FT_SOLIDFLOOR;
            tile.floorSpriteUID = floor;
            tile.wallType = static_cast<WallType>(WT_SOLIDWALL | WT_MOVEBLOCKING | WT_VIEWBLOCKING);
            tile.wallSpriteUID = wall;
            tile.wallMaterial = DBH::materialUID("Granite");
            tile.floorMaterial = tile.wallMaterial;
            tile.flags += TileFlag::TF_UNDISCOVERED;
            if (openFloor && level == z) {
                tile.wallType = WT_NOWALL;
                tile.wallSpriteUID = 0;
                tile.flags = TileFlag::TF_WALKABLE;
            }
            // Expose a lower-layer landmark for stationary-canvas captures.
            if (qEnvironmentVariableIsSet("INGNOMIA_LAYER_SCROLL_PROBE")
                && level >= z && x >= 46 && x <= 54 && y >= 44 && y <= 52)
                tile = Tile{};
            world->addToUpdateList(x,y,level);
        }
        Position workerPos(45,45,z);
        if (!qEnvironmentVariableIsSet("INGNOMIA_LAYER_SCROLL_PROBE"))
            state->worker = std::make_unique<PlacementProbeWorker>(workerPos,game);
        publish();
        report("READY target=" + state->target.toString());
        state->ready = true;
        });
    });
    auto run = [&app,folder,state,manager,report,publish]() {
    QTimer::singleShot(1000,&app,[state]() {
        if (!state->ready) return;
        auto& window = MainWindow::getInstance();
        window.showNormal();
        window.resize(1200,750);
        Global::wallsLowered = false;
        window.onUiSetViewLevel(state->target.z);
        window.renderer()->onCenterCameraPosition(state->target);
        window.renderer()->setScale(3.0f);
        window.pushRenderParams();
    });
    if (qEnvironmentVariableIsSet("INGNOMIA_LAYER_SCROLL_PROBE")) {
        scheduleLayerScrollProbe(app,folder,state->target);
        return;
    }
    QTimer::singleShot(3000,&app,[state,manager,report]() {
        if (!state->ready) return;
        auto& window = MainWindow::getInstance();
        const int mx = window.width()/2 + 48;
        const int my = window.height()/2 - 36;
        QMetaObject::invokeMethod(manager,[state,manager,report,mx,my]() {
            const QString requestedAction = qEnvironmentVariable("INGNOMIA_PLACEMENT_ACTION");
            Global::sel->setAction(requestedAction.isEmpty() ? "DigStairsDown" : requestedAction);
            const int requestedRotation = (qEnvironmentVariableIntValue("INGNOMIA_PLACEMENT_ROTATION")%4+4)%4;
            while (Global::sel->rotation() != requestedRotation) Global::sel->rotate();
            manager->eventConnector()->aggregatorSelection()->onMouse(mx,my,false,false);
            const Position selected = Global::sel->getSelection().first().first;
            const auto& tile = manager->game()->w()->getTile(selected);
            const int surfaceHeight = (tile.wallType & WT_SOLIDWALL) ? 28 : 12;
            // Independently forward-project the surface actually removed by
            // the job; this fails with the old floor-only inverse, by 48 px.
            const int dx = selected.x-state->target.x, dy = selected.y-state->target.y;
            const int surfaceX = 600 + 3*(16*(dx-dy)+16);
            const int surfaceY = 375 + 3*(8*(dx+dy)-surfaceHeight);
            report(QString("mouse=%1,%2 selection=%3 visibleSurface=%4,%5 %6")
                .arg(mx).arg(my).arg(selected.toString()).arg(surfaceX).arg(surfaceY)
                .arg(surfaceX == mx && surfaceY == my ? "PASS alignment" : "FAIL alignment"));
        },Qt::QueuedConnection);
    });
    auto capture = [&app,folder,state](int delay,const QString& name) {
        QTimer::singleShot(delay,&app,[folder,state,name]() {
            if (!state->ready) return;
            qputenv("INGNOMIA_UI_CAPTURE",(folder+"/"+name+".png").toUtf8());
            qputenv("INGNOMIA_UI_CAPTURE_FRAME","1");
            MainWindow::getInstance().armUiCapture();
        });
    };
    capture(6000,"preview");
    QTimer::singleShot(8000,manager,[state,manager,report,publish]() {
        if (!state->ready) return;
        const Position selected = Global::sel->getSelection().first().first;
        manager->eventConnector()->aggregatorSelection()->onLeftClick(false,false);
        if (Global::sel->action() != "DigStairsDown")
            manager->eventConnector()->aggregatorSelection()->onLeftClick(false,false);
        state->job = manager->game()->jm()->getJobAtPos(selected);
        report(state->job ? "JOB " + state->job->pos().toString() : "FAIL no job");
        publish();
    });
    capture(10000,"job");
    QTimer::singleShot(12000,manager,[state,manager,report,publish]() {
        if (!state->ready || !state->job) return;
        const Position p = state->job->pos();
        const bool success = state->worker->execute(state->job);
        manager->game()->jm()->finishJob(state->job->id());
        report(QString("EXECUTE success=%1 target=%2 floor=%3 wall=%4 below=%5")
            .arg(success).arg(p.toString()).arg(int(manager->game()->w()->getTile(p).floorType))
            .arg(int(manager->game()->w()->getTile(p).wallType)).arg(int(manager->game()->w()->getTile(p.belowOf()).wallType)));
        publish();
    });
    capture(15000,"completed-preview");
    QTimer::singleShot(17000,manager,[state,manager]() {
        if (state->ready) manager->eventConnector()->aggregatorSelection()->onCancelSelection();
    });
    capture(19000,"completed");
    QTimer::singleShot(22000,&app,[report]() {
        report("COMPLETE");
        QMetaObject::invokeMethod(Global::eventConnector,"onExit",Qt::QueuedConnection);
    });
    };
    auto* readyTimer = new QTimer(&app);
    QObject::connect(readyTimer,&QTimer::timeout,&app,[readyTimer,state,run]() {
        if (!state->ready) return;
        readyTimer->stop();
        run();
        readyTimer->deleteLater();
    });
    readyTimer->start(1000);
}
