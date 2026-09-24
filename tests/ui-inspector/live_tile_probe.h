#pragma once

// Explicit opt-in only: generates a disposable world in the supplied data root,
// exercises real GUI mouse routing and game-thread mutations, then exits.
#include "game/newgamesettings.h"
#include "game/world.h"
#include "game/inventory.h"
#include "game/jobmanager.h"
#include "game/job.h"
#include "gfx/spritefactory.h"
#include "gui/ui/runtime/RmlUiDetachedWindow.h"
#include <QWheelEvent>
#include <atomic>
#include <memory>

inline void scheduleLiveTileProbe(QApplication& app, GameManager* manager)
{
    const auto folder=qEnvironmentVariable("INGNOMIA_LIVE_TILE_PROBE");
    if(folder.isEmpty() || qEnvironmentVariable("INGNOMIA_DATA_FOLDER").isEmpty()) return;
    QDir().mkpath(folder);
    auto report=[folder](bool ok,const QString& message) {
        QFile file(folder+"/result.txt");
        if(file.open(QIODevice::WriteOnly|QIODevice::Append))
            file.write(((ok?"PASS ":"FAIL ")+message+"\n").toUtf8());
    };
    auto liveWindow=[]() -> QWindow* {
        for(auto* window:QGuiApplication::topLevelWindows())
            if(window->title()==QStringLiteral("Tile inspection")) return window;
        return nullptr;
    };
    auto capture=[folder,liveWindow](const char* name) {
        const QString path=folder+"/"+QString::fromLatin1(name)+".png";
        if(auto* window=dynamic_cast<ingnomia::ui::RmlUiDetachedWindow*>(liveWindow()))
            window->requestAutomationCapture(path);
    };
    struct State { std::atomic_uint tile{0}; unsigned first{}, item{}; Position target,other; std::atomic_bool outlined{false}; };
    auto state=std::make_shared<State>();
    QObject::connect(manager->eventConnector()->aggregatorSelection(),&AggregatorSelection::signalInspectTile,
        manager,[state](unsigned tile){state->tile=tile;});
    QObject::connect(manager->eventConnector()->aggregatorSelection(),&AggregatorSelection::signalUpdateSelection,
        manager,[state](const QMap<unsigned,SelectionData>& cells,bool){state->outlined=cells.size()==1;});
    QTimer::singleShot(2500,manager,[manager,state,report,capture,liveWindow,&app] {
        Global::newGameSettings->setSeed("1342516210");
        Global::newGameSettings->setKingdomName("InspectionProbe");
        manager->startNewGame();
        manager->setPaused(true);
        QTimer::singleShot(2000,manager,[manager,state,report,capture,liveWindow,&app] {
            auto* game=manager->game();
            const int z=Global::newGameSettings->ground()-5;
            const auto floor=game->sf()->createSprite("RoughFloor",{"Granite"})->uID;
            for(int y=35;y<=65;++y) for(int x=35;x<=65;++x) {
                for(int level=z;level<=z+1;++level) game->w()->getTile(x,y,level)=Tile{};
                auto& tile=game->w()->getTile(x,y,z);
                tile.floorType=FT_SOLIDFLOOR;
                tile.floorMaterial=DBH::materialUID("Granite");
                tile.floorSpriteUID=floor;
                tile.flags=TileFlag::TF_WALKABLE;
                game->w()->addToUpdateList(Position(x,y,z));
            }
            manager->eventConnector()->aggregatorRenderer()->onUpdateAnyTileInfo(game->w()->updatedTiles());
            manager->eventConnector()->aggregatorRenderer()->onCenterCamera(Position(50,50,z));
            QTimer::singleShot(1000,&app,[manager,state,report,capture,liveWindow,&app] {
                auto& window=MainWindow::getInstance();
                report(window.activateHudElement("hud_tool_inspect"),"Tools Inspect toggles through HUD binding");
                report(liveWindow()&&liveWindow()->isVisible()&&!liveWindow()->parent(),"tile inspector opens as an independent native window");
                auto move=[](int dx) {
                    auto& w=MainWindow::getInstance();
                    const QPointF p(w.width()/2+dx,w.height()/2);
                    QMouseEvent event(QEvent::MouseMove,p,w.mapToGlobal(p.toPoint()),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
                    QCoreApplication::sendEvent(&w,&event);
                };
                auto click=[](int dx) {
                    auto& w=MainWindow::getInstance();
                    const QPointF p(w.width()/2+dx,w.height()/2);
                    QMouseEvent press(QEvent::MouseButtonPress,p,w.mapToGlobal(p.toPoint()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                    QCoreApplication::sendEvent(&w,&press);
                    QMouseEvent release(QEvent::MouseButtonRelease,p,w.mapToGlobal(p.toPoint()),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                    QCoreApplication::sendEvent(&w,&release);
                };
                QTimer::singleShot(300,&app,[move]{move(0);});
                QTimer::singleShot(550,&app,[state,report]{report(state->tile==0 && !state->outlined,"hover alone does not select a tile");});
                QTimer::singleShot(650,&app,[click]{click(0);});
                QTimer::singleShot(950,&app,[state,move]{state->first=state->tile;move(64);});
                QTimer::singleShot(1150,&app,[state,report]{report(state->first!=0 && state->tile==state->first && state->outlined,"moving the mouse retains the clicked tile and outline");});
                QTimer::singleShot(1300,&app,[click]{click(64);});
                QTimer::singleShot(1700,manager,[manager,state,report,capture,liveWindow,&app] {
                    auto* game=manager->game();
                    report(state->tile!=0 && state->tile!=state->first,"clicking another tile changes the inspected world tile");
                    report(state->outlined,"one world cell is outlined");
                    if(!state->tile) { QMetaObject::invokeMethod(manager->eventConnector(),"onExit",Qt::QueuedConnection); return; }
                    state->target=Position(state->tile.load());
                    state->other=Position(state->target.x+1,state->target.y,state->target.z);
                    game->spm()->addStockpile(state->target,{{state->target,true},{state->other,true}});
                    state->item=game->inv()->createItem(state->target,"RawWood","Oak");
                    game->spm()->getStockpileAtPos(state->target)->insertItem(state->target,state->item);
                    manager->eventConnector()->aggregatorTileInfo()->onUpdateTileInfo(state->tile);
                    QTimer::singleShot(500,&app,[manager,state,report,capture,liveWindow,&app] {
                        auto& w=MainWindow::getInstance();
                        bool stockpileWindowVisible=false;
                        for(auto* window:QGuiApplication::topLevelWindows())
                            if(window->isVisible()&&window->title().contains(QStringLiteral("Stockpile"),Qt::CaseInsensitive)) stockpileWindowVisible=true;
                        report(!stockpileWindowVisible,"inspecting a stockpile does not open its management window");
                        const QPointF p(260,220);
                        QMouseEvent event(QEvent::MouseMove,p,w.mapToGlobal(p.toPoint()),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
                        QCoreApplication::sendEvent(&w,&event);
                        report(!w.activateInspectorElement("inspector_refresh") && !w.activateInspectorElement("inspector_locate"),"Refresh and Center are absent from live inspection");
                        capture("live-stockpile");
                        QTimer::singleShot(200,manager,[manager,state] {
                            auto* game=manager->game();
                            game->inv()->createItem(state->target,"RawWood","Pine");
                            game->inv()->createItem(state->target,"RawWood","Birch");
                            game->inv()->createItem(state->target,"RawStone","Granite");
                            game->inv()->createItem(state->target,"RawStone","Basalt");
                            manager->eventConnector()->aggregatorTileInfo()->onUpdateTileInfo(state->tile);
                        });
                        QTimer::singleShot(450,&app,[capture] { capture("live-overflow-top"); });
                        QTimer::singleShot(650,&app,[capture,report,liveWindow] {
                            auto& window=MainWindow::getInstance();
                            auto* inspector=liveWindow();
                            const QPointF p(200,190);
                            const int beforeLevel=GameState::viewLevel;
                            const float beforeScale=window.renderer()->scale();
                            if(inspector) {
                                QMouseEvent pointer(QEvent::MouseMove,p,inspector->mapToGlobal(p.toPoint()),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
                                QCoreApplication::sendEvent(inspector,&pointer);
                                QWheelEvent wheel(p,inspector->mapToGlobal(p.toPoint()),QPoint(),QPoint(0,-480),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
                                QCoreApplication::sendEvent(inspector,&wheel);
                            }
                            report(GameState::viewLevel==beforeLevel && window.renderer()->scale()==beforeScale,"wheel over the list does not change map level or zoom");
                            capture("live-overflow-scrolled");
                        });
                        QTimer::singleShot(1100,&app,[manager,state,report,capture,liveWindow,&app] {
                            report(state->tile==state->target.toInt(),"moving into the panel retains the target");
                            report(MainWindow::getInstance().activateInspectorElement("live_tile_delete_stockpile"),"Delete stockpile action is visible and dispatched");
                            QTimer::singleShot(400,manager,[manager,state,report,capture,liveWindow,&app] {
                                auto* game=manager->game();
                                report(!game->spm()->getStockpileAtPos(state->target) && !game->spm()->getStockpileAtPos(state->other)
                                    && !(game->w()->getTile(state->target).flags&TileFlag::TF_STOCKPILE)
                                    && !(game->w()->getTile(state->other).flags&TileFlag::TF_STOCKPILE),"entire stockpile removed and both tile flags cleared");
                                report(game->inv()->getPosition(state->item)==state->target && !game->inv()->isInStockpile(state->item),"items remain on their tile and are released from stockpile");
                                QTimer::singleShot(300,&app,[manager,state,report,capture,liveWindow,&app] {
                                    report(!MainWindow::getInstance().activateInspectorElement("live_tile_delete_stockpile"),"deleted stockpile action disappears automatically");
                                    report(MainWindow::getInstance().activateInspectorElement("live_tile_remove_floor"),"Remove floor dispatches on retained tile");
                                    QTimer::singleShot(400,manager,[manager,state,report,capture,liveWindow,&app] {
                                        auto job=manager->game()->jm()->getJobAtPos(state->target);
                                        report(job && job->type()=="RemoveFloor","Remove floor creates the authoritative job at inspected tile");
                                        QTimer::singleShot(300,&app,[manager,state,report,capture,liveWindow,&app] {
                                             report(MainWindow::getInstance().activateInspectorElement("live_tile_cancel_job"),"live panel automatically exposes Cancel job");
                                             capture("live-floor-job");
                                             QTimer::singleShot(200,&app,[state,report,capture,liveWindow] {
                                                 auto* w=liveWindow();
                                                 if(!w) return;
                                                 const QPoint before=w->position();
                                                 const QPointF from(110,18);
                                                 const int beyondCanvas=qMax(250,MainWindow::getInstance().geometry().right()-w->geometry().right()+60);
                                                 const QPointF to(from.x()+beyondCanvas,88);
                                                 QMouseEvent press(QEvent::MouseButtonPress,from,w->mapToGlobal(from.toPoint()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                                                 QCoreApplication::sendEvent(w,&press);
                                                 QMouseEvent drag(QEvent::MouseMove,to,w->mapToGlobal(to.toPoint()),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
                                                 QCoreApplication::sendEvent(w,&drag);
                                                 QMouseEvent release(QEvent::MouseButtonRelease,to,w->mapToGlobal(to.toPoint()),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                                                 QCoreApplication::sendEvent(w,&release);
                                                 report(w->position()!=before,"native title drag moves the inspector window");
                                                 report(w->geometry().right()>MainWindow::getInstance().geometry().right(),"native title drag moves the inspector beyond the game canvas");
                                                 report(state->tile==state->target.toInt(),"dragging the title keeps the inspected tile selected");
                                                 capture("live-dragged");
                                                 QTimer::singleShot(300,w,[w,before] { w->setPosition(before+QPoint(250,70)); });
                                             });
                                             QTimer::singleShot(700,&app,[manager,state,report,capture,liveWindow,&app] {
                                                report(MainWindow::getInstance().activateHudElement("hud_tool_inspect"),"Inspect closes through the sidebar");
                                                QTimer::singleShot(350,&app,[manager,state,report,capture,liveWindow,&app] {
                                                    report(liveWindow()&&!liveWindow()->isVisible()&&!state->outlined,"closing hides the native inspector and clears its outline");
                                                    report(MainWindow::getInstance().activateHudElement("hud_tool_inspect"),"Inspect reopens through the sidebar");
                                                    report(liveWindow()&&liveWindow()->isVisible(),"the parked native inspector reopens");
                                                    auto& w=MainWindow::getInstance();
                                                    const QPointF p(w.width()/2+64,w.height()/2);
                                                    QMouseEvent press(QEvent::MouseButtonPress,p,w.mapToGlobal(p.toPoint()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                                                    QCoreApplication::sendEvent(&w,&press);
                                                    QMouseEvent release(QEvent::MouseButtonRelease,p,w.mapToGlobal(p.toPoint()),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                                                    QCoreApplication::sendEvent(&w,&release);
                                                    QTimer::singleShot(450,&app,[manager,state,report,capture,liveWindow] {
                                                        report(state->tile==state->target.toInt()&&state->outlined,"a reopened inspector can select its previous tile");
                                                        capture("live-reopened");
                                                        QTimer::singleShot(250,&MainWindow::getInstance(),[manager,state,report] {
                                                            report(MainWindow::getInstance().activateInspectorElement("live_tile_replace_floor"),"Replace floor opens the replacement picker");
                                                        });
                                                        QTimer::singleShot(600,manager,[manager,state,report] {
                                                            bool buildVisible=false;
                                                            for(auto* window:QGuiApplication::topLevelWindows())
                                                                if(window->title()==QStringLiteral("Build")&&window->isVisible()) buildVisible=true;
                                                            report(buildVisible,"replacement floor catalog is visible in a native window");
                                                            report(!state->outlined,"replacement workflow clears the inspection outline");
                                                            manager->eventConnector()->onExit();
                                                        });
                                                    });
                                                });
                                            });
                                        });
                                    });
                                });
                            });
                        });
                    });
                });
            });
        });
    });
}
