/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../../src/game/workshopmanager.h"
#include "../../src/game/farmingmanager.h"
#include "../../src/game/stockpile.h"
#include "../../src/game/inventory.h"
#include "../../src/game/world.h"
#include "../../src/gui/aggregatoragri.h"

#include <algorithm>

// Isolated fixtures are created only inside the explicitly supplied copied save.
inline void workshopLinkedStockpileFixture(EventConnector* connector)
{
    auto* game=connector->game();
    Position tile(qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_TILE").toUInt());
    auto* workshop=game ? game->wsm()->workshopAt(tile) : nullptr;
    if(!workshop) { automationTrace("FAIL workshop fixture missing"); return; }
    const auto input=workshop->inputPos();
    QList<Position> positions;
    for(int radius : {3,7,12}) {
        bool found=false;
        for(int dx=-radius;dx<=radius && !found;++dx) for(int dy=-radius;dy<=radius && !found;++dy) {
            if((std::max)(std::abs(dx),std::abs(dy))!=radius) continue;
            Position pos(input.x+dx,input.y+dy,input.z);
            if(game->w()->isWalkableGnome(pos) && game->w()->regionMap().checkConnectedRegions(input,pos)
                && !game->spm()->isStockPile(pos) && !game->wsm()->workshopAt(pos)) {
                positions.append(pos); found=true;
            }
        }
    }
    if(positions.size()!=3) { automationTrace("FAIL fixture needs three reachable tiles"); return; }
    QList<unsigned int> piles,items;
    for(auto pos : positions) {
        game->spm()->addStockpile(pos,{{pos,true}});
        auto* pile=game->spm()->getStockpileAtPos(pos);
        piles.append(pile->id());
        auto item=game->inv()->createItem(pos,"RawWood","Oak");
        pile->insertItem(pos,item); items.append(item);
    }
    const auto nearPile=piles[1],farPile=piles[2];
    workshop->setLinkedStockpiles({farPile,nearPile}); // Deliberately reversed to test distance ordering.
    auto choose=[&](int count, bool same=false, QStringList restrictions=QStringList{}, QString material="Oak") {
        return game->inv()->getWorkshopItems(input,"RawWood",material,count,same,restrictions,workshop->linkedStockpiles());
    };
    const auto verify=[](bool ok,const char* label) { automationTrace(QString(ok?"PASS ":"FAIL ")+label); };
    verify(choose(2)==QList<unsigned int>{items[1],items[2]},"nearest linked then farther linked, despite closer unlinked stock");
    auto fallback=choose(3);
    verify(fallback.size()==3 && fallback[0]==items[1] && fallback[1]==items[2]
        && game->inv()->isInStockpile(fallback[2])!=nearPile && game->inv()->isInStockpile(fallback[2])!=farPile,"unlinked fallback after linked stock");
    game->inv()->setInJob(items[1],999999);
    verify(choose(1)==QList<unsigned int>{items[2]},"reserved linked stock is skipped");
    game->inv()->setInJob(items[1],0);
    const auto pine=game->inv()->createItem(positions[1],"RawWood","Pine");
    game->spm()->getStockpile(nearPile)->insertItem(positions[1],pine);
    verify(choose(1,false,{"Pine"},"any")==QList<unsigned int>{pine},"material restrictions are preserved");
    verify(choose(1,false,{"Wood"},"any")==QList<unsigned int>{items[1]},"material-type restrictions accept their constituent materials");
    verify(choose(1,false,{"Metal"},"any").empty(),"incompatible material types are excluded");
    const auto same=choose(2,true,{"Oak","Pine"},"any");
    verify(same==QList<unsigned int>{items[1],items[2]},"same-material requirement spans linked locations");
    verify(choose(100000).empty() && game->inv()->isInJob(items[1])==0,"insufficient stock does not reserve partial requirements");
    verify(game->inv()->getPosition(items[1])==positions[1],"selection does not teleport linked items");
    auto saved=workshop->serialize().toMap();
    WorkshopProperties restored(saved);
    verify(restored.linkedStockpiles==QList<unsigned int>{farPile,nearPile},"multiple links survive save serialization");
    saved.remove("LinkedStockpiles"); saved["LinkedStockpile"]=nearPile;
    WorkshopProperties migrated(saved);
    verify(migrated.linkedStockpiles==QList<unsigned int>{nearPile},"legacy single link migrates");
    QSet<QString> names; bool unique=true;
    for(auto pileId : game->spm()->allStockpiles()) {
        auto* pile=game->spm()->getStockpile(pileId);
        unique=unique && !pile->name().isEmpty() && !names.contains(pile->name().toCaseFolded());
        names.insert(pile->name().toCaseFolded());
    }
    verify(unique,"loaded and created stockpiles have unique names");
    verify(game->spm()->getStockpile(nearPile)->name().startsWith("Stockpile"),"new stockpiles receive numbered names");
    workshop->setLinkedStockpiles({});
    qputenv("INGNOMIA_PROBE_LINK_NEAR",QByteArray::number(nearPile));
    qputenv("INGNOMIA_PROBE_LINK_FAR",QByteArray::number(farPile));
}

// Opt-in real-save proof, using the normal RmlUi bindings and queued commands.
// Supply INGNOMIA_DATA_FOLDER and a copied INGNOMIA_AUTOMATE_LOAD_PATH.
inline void scheduleFarmOpenProbe(QApplication& app)
{
    if (qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_OPEN_PROBE") != "1"
        || qEnvironmentVariableIsEmpty("INGNOMIA_DATA_FOLDER")
        || qEnvironmentVariableIsEmpty("INGNOMIA_AUTOMATE_LOAD_PATH")) return;
    const bool seedProbe = qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_SEED_PROBE") == "1";
	const bool planProbe = qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_PLAN_PROBE") == "1";
	if (qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_NATIVE_CLICK_PROBE") == "1")
		QTimer::singleShot(13500, &app, [] {
			qputenv("INGNOMIA_AUTOMATE_DETACHED_CAPTURE_PATH", qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_NATIVE_CLICK_CAPTURE_PATH").toUtf8());
			qputenv("INGNOMIA_AUTOMATE_DETACHED_LAYOUT_PATH", qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_NATIVE_CLICK_LAYOUT_PATH").toUtf8());
		});
	if (qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_NATIVE_CLICK_PROBE") == "1")
		QTimer::singleShot(12250, &app, [] {
			const bool clicked = MainWindow::getInstance().clickManagementFarmCropForProbe("Barley");
			const auto selected = MainWindow::getInstance().managementFarmSelectedCropForProbe();
			automationTrace(QString(clicked && selected == "Barley" ? "PASS" : "FAIL")
				+ QString(" Farm pointer selected Barley clicked=%1 selected=%2").arg(clicked).arg(QString::fromStdString(selected)));
		});
    if (seedProbe && Global::eventConnector)
        QObject::connect(Global::eventConnector->aggregatorAgri(), &AggregatorAgri::signalGlobalPlantInfo,
            &app, [](const QList<GuiPlant>& plants) {
                const auto strawberry = std::find_if(plants.begin(), plants.end(), [](const GuiPlant& plant) {
                    return plant.plantID == "Strawberry";
                });
                automationTrace(QString(strawberry != plants.end() && strawberry->seedCount > 0 ? "PASS" : "FAIL")
                    + QString(" Farm catalog has Strawberry seeds count=%1 rows=%2")
                        .arg(strawberry == plants.end() ? -1 : strawberry->seedCount).arg(plants.size()));
            }, Qt::QueuedConnection);
    QTimer::singleShot(9000, &app, [] {
        auto* connector = Global::eventConnector;
        if (!connector) { automationTrace("FAIL Farm probe has no EventConnector"); return; }
        QMetaObject::invokeMethod(connector, [connector] {
            auto* game = connector->game();
            if (!game || !game->w() || !game->fm()) { automationTrace("FAIL Farm probe has no loaded world"); return; }
            connector->onSetPause(true);
			if (qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_PLAN_PROBE") == "1") {
				Farm* farm = nullptr;
				for (auto* candidate : game->fm()->allFarms())
					if (candidate && candidate->countTiles() >= 4) { farm = candidate; break; }
				if (!farm) {
					const auto mask = TileFlag::TF_WORKSHOP + TileFlag::TF_STOCKPILE + TileFlag::TF_GROVE
						+ TileFlag::TF_FARM + TileFlag::TF_PASTURE + TileFlag::TF_ROOM;
					for (int z = Global::dimZ - 2; z > 0 && !farm; --z)
						for (int x = Global::dimX / 2 - 20; x < Global::dimX / 2 + 20 && !farm; ++x)
							for (int y = Global::dimY / 2 - 20; y < Global::dimY / 2 + 20 && !farm; ++y) {
								const Position first(x,y,z);
								const QList<Position> plots{first,first.eastOf(),first.southOf(),first.seOf()};
								const bool open = std::all_of(plots.begin(),plots.end(),[game,mask](const Position& tile) {
									return game->w()->isWalkableGnome(tile) && (game->w()->getTile(tile).flags-~mask)==TileFlag::TF_NONE;
								});
								if (!open) continue;
								QList<QPair<Position,bool>> fields;
								for (const auto& tile : plots) fields.append({tile,true});
								game->fm()->addFarm(first,fields);
								farm=game->fm()->getFarmAtPos(first);
								if (farm) automationTrace(QString("Farm plan probe created four-plot Farm id=%1").arg(farm->id()));
							}
				}
				if (!farm) { automationTrace("FAIL Farm plan probe found no multi-plot Farm or open four-tile patch"); return; }
				const auto fields = farm->serialize().toMap().value("Fields").toList();
				if (fields.size() < 2) { automationTrace("FAIL Farm plan probe requires at least two fields"); return; }
				const Position first(fields[0].toMap().value("Pos").toString());
				const Position second(fields[1].toMap().value("Pos").toString());
				qputenv("INGNOMIA_PROBE_FARM_TILE", QByteArray::number(first.toInt()));
				qputenv("INGNOMIA_PROBE_FARM_PLOT_A", QString("%1_%2_%3").arg(first.x).arg(first.y).arg(first.z).toUtf8());
				qputenv("INGNOMIA_PROBE_FARM_PLOT_B", QString("%1_%2_%3").arg(second.x).arg(second.y).arg(second.z).toUtf8());
				automationTrace(QString("Farm plan probe selected farm id=%1 plots=%2 targetA=%3 targetB=%4").arg(farm->id()).arg(fields.size()).arg(first.toInt()).arg(second.toInt()));
				connector->onSelectTile(first.toInt());
				return;
			}
            const auto mask = TileFlag::TF_WORKSHOP + TileFlag::TF_STOCKPILE + TileFlag::TF_GROVE
                + TileFlag::TF_FARM + TileFlag::TF_PASTURE + TileFlag::TF_ROOM;
            for (int z = Global::dimZ - 2; z > 0; --z) {
                for (int x = Global::dimX / 2 - 20; x < Global::dimX / 2 + 20; ++x) {
                    for (int y = Global::dimY / 2 - 20; y < Global::dimY / 2 + 20; ++y) {
                        Position tile(x, y, z);
                        if (!game->w()->isWalkableGnome(tile)
                            || (game->w()->getTile(tile).flags - ~mask) != TileFlag::TF_NONE) continue;
                        game->fm()->addFarm(tile, {{tile, true}});
                        qputenv("INGNOMIA_PROBE_FARM_TILE", QByteArray::number(tile.toInt()));
                        automationTrace(QString("Farm probe created tile=%1").arg(tile.toInt()));
                        connector->onSelectTile(tile.toInt());
                        return;
                    }
                }
            }
            automationTrace("FAIL Farm probe found no open tile");
        }, Qt::QueuedConnection);
    });
    QTimer::singleShot(11500, &app, [] {
        const bool ready = MainWindow::getInstance().managementFarmReadyForProbe();
        automationTrace(QString(ready ? "PASS" : "FAIL") + " Farm opens with ready data instead of Loading");
        if (qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_PLAN_PROBE") == "1") {
			const bool products = MainWindow::getInstance().activateManagementElement("agriculture_view_plots");
			if (qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_NATIVE_CLICK_PROBE") == "1") {
				const bool allPlots = MainWindow::getInstance().activateManagementElement("agriculture_plot_select_all");
				automationTrace(QString(products && allPlots ? "PASS" : "FAIL") + " Farm plan opened with all plots for pointer probe");
				return;
			}
			// Stage 11 property sheet: click, Ctrl+click, crop drop-down, Assign Crop, Queue (see ui-stage11/runtime_probe.h).
			automationTrace(QString(products ? "PASS" : "FAIL") + " Farm Plots page opens");
			stage11::farmPlanSteps();
		}
        else if (qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_SEED_PROBE") == "1") {
            automationTrace(QString(MainWindow::getInstance().activateManagementElement("agriculture_view_crops") ? "PASS" : "FAIL")
                + " Farm Crops page opens");
        }
        const auto capture = qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_CAPTURE_PATH");
        if (!capture.isEmpty()) qputenv("INGNOMIA_AUTOMATE_DETACHED_CAPTURE_PATH", capture.toUtf8());
    });
    QTimer::singleShot(12500, &app, [] {
        const auto tile = qEnvironmentVariable("INGNOMIA_PROBE_FARM_TILE").toUInt();
        if (tile && Global::eventConnector)
            QMetaObject::invokeMethod(Global::eventConnector, "onSelectTile", Qt::QueuedConnection, Q_ARG(unsigned int, tile));
    });
    QTimer::singleShot(14000, &app, [] {
		if (qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_NATIVE_CLICK_PROBE") == "1") {
			const auto selected = MainWindow::getInstance().managementFarmSelectedCropForProbe();
			automationTrace(QString(selected == "Barley" ? "PASS" : "FAIL")
				+ QString(" Farm crop selection survives live refresh selected=%1").arg(QString::fromStdString(selected)));
			return;
		}
        automationTrace(QString(MainWindow::getInstance().managementFarmReadyForProbe() ? "PASS" : "FAIL")
            + " reopening the same Farm remains ready");
    });
    if (planProbe && qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_NATIVE_CLICK_PROBE") != "1")
	{
		QTimer::singleShot(14500, &app, [] {
			auto* connector = Global::eventConnector;
			if (!connector) return;
			QMetaObject::invokeMethod(connector, [connector] {
				auto* game = connector->game();
				auto* farm = game && game->fm() ? game->fm()->getFarmAtPos(Position(qEnvironmentVariable("INGNOMIA_PROBE_FARM_TILE").toUInt())) : nullptr;
				if (!farm) { automationTrace("FAIL Farm plan model missing"); return; }
				const auto fields = farm->serialize().toMap().value("Fields").toList();
				int planned = 0;
				int untouched = 0;
				for (const auto& value : fields) {
					const auto plot = value.toMap();
					const auto orders = plot.value("CropOrders").toList();
					if (plot.value("Crop").toString() == "Strawberry" && orders.size() == 1
						&& orders.first().toMap().value("Crop").toString() == "Strawberry"
						&& orders.first().toMap().value("Remaining").toInt() == 2) ++planned;
					else if (plot.value("Crop").toString().isEmpty() && orders.isEmpty()) ++untouched;
				}
				automationTrace(QString(planned == 2 && untouched == fields.size()-2 ? "PASS" : "FAIL")
					+ QString(" Farm bulk plan applied to exactly %1 of %2 plots; %3 plots untouched").arg(planned).arg(fields.size()).arg(untouched));
				Farm restored(farm->serialize().toMap(), game);
				int restoredPlans = 0;
				for (const auto& value : restored.serialize().toMap().value("Fields").toList()) {
					const auto plot = value.toMap();
					if (plot.value("Crop").toString() == "Strawberry"
						&& plot.value("CropOrders").toList().size() == 1) ++restoredPlans;
				}
				automationTrace(QString(restoredPlans == 2 ? "PASS" : "FAIL") + QString(" Farm plot plans survive save-data deserialization for %1 plots").arg(restoredPlans));
				farm->onTick(0);
				int pending = 0;
				for (const auto& value : farm->serialize().toMap().value("Fields").toList())
					if (value.toMap().value("PendingCrop").toString() == "Strawberry") ++pending;
				automationTrace(QString(pending > 0 ? "PASS" : "WARN") + QString(" Farm scheduling created %1 Strawberry planting jobs").arg(pending));
			}, Qt::QueuedConnection);
		});
		QTimer::singleShot(13000, &app, [&app] {
			if (!Global::eventConnector) return;
			auto updates = QSharedPointer<int>::create(0);
			QObject::connect(Global::eventConnector->aggregatorAgri(), &AggregatorAgri::signalUpdateFarm,
				&app, [updates](const GuiFarmInfo&) { ++*updates; }, Qt::QueuedConnection);
			QTimer::singleShot(2700, &app, [updates] {
				automationTrace(QString(*updates > 0 ? "PASS" : "FAIL")
					+ QString(" Farm emitted %1 live updates without Refresh").arg(*updates));
			});
		});
	}
    else if (seedProbe)
    {
        QTimer::singleShot(13500, &app, [] {
            const auto row = std::string("agriculture_farm_crop_") + QByteArray("Strawberry").toHex().toStdString();
            const bool selected = MainWindow::getInstance().activateManagementElement(row);
            const bool applied = selected && MainWindow::getInstance().activateManagementElement("agriculture_apply");
            automationTrace(QString(selected ? "PASS" : "FAIL") + " Strawberry chosen as the default crop on the Crops page");
            automationTrace(QString(applied ? "PASS" : "FAIL") + " default crop applied with Apply");
        });
        QTimer::singleShot(14500, &app, [] {
            auto* connector = Global::eventConnector;
            if (!connector) return;
            QMetaObject::invokeMethod(connector, [connector] {
                auto* game = connector->game();
                auto* farm = game && game->fm() ? game->fm()->getFarmAtPos(Position(qEnvironmentVariable("INGNOMIA_PROBE_FARM_TILE").toUInt())) : nullptr;
                automationTrace(QString(farm && farm->plantType() == "Strawberry" ? "PASS" : "FAIL")
                    + " Farm persisted selected Strawberry crop in the game model");
                if (farm && farm->plantType() == "Strawberry" && game->inv()) {
                    const auto seed = DB::select("SeedItemID", "Plants", "Strawberry").toString();
                    const auto material = DB::select("Material", "Plants", "Strawberry").toString();
                    const auto seedId = game->inv()->getClosestItem(Position(qEnvironmentVariable("INGNOMIA_PROBE_FARM_TILE").toUInt()), true, seed, material);
                    automationTrace(QString(seedId ? "PASS" : "FAIL") + QString(" Farm planting lookup finds a reachable Strawberry seed id=%1").arg(seedId));
                }
            }, Qt::QueuedConnection);
        });
    }
    const bool stage11Live = qEnvironmentVariable("INGNOMIA_AUTOMATE_AGRICULTURE_STAGE11_LIVE") == "1";
    if (stage11Live && planProbe) stage11::schedule(app, 15500);
    QTimer::singleShot(stage11Live ? 36500 : 16000, &app, [] {
        if (Global::eventConnector) QMetaObject::invokeMethod(Global::eventConnector, "onExit", Qt::QueuedConnection);
    });
}

inline void scheduleWorkshopOrderProbe(QApplication& app)
{
    if(qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_STAGE10_LIVE")=="1") {
        QTimer::singleShot(13000,&app,[]{auto& w=MainWindow::getInstance();w.activateManagementElement("workshop_view_craft");w.setManagementFormValueForProbe("workshop_order_count","3");w.activateManagementElement("workshop_queue_once");w.activateManagementElement("workshop_queue_once");});
        QTimer::singleShot(14000,&app,[]{auto* c=Global::eventConnector;QMetaObject::invokeMethod(c,[c]{stage10BackendProbe(c);},Qt::QueuedConnection);});
        scheduleStage10Captures(app,16500);
    }
    const bool stagedEditProbe=qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_STAGED_EDIT_PROBE")=="1";
    if((qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_OPEN_PROBE")=="1" || stagedEditProbe)
        && !qEnvironmentVariableIsEmpty("INGNOMIA_DATA_FOLDER") && !qEnvironmentVariableIsEmpty("INGNOMIA_AUTOMATE_LOAD_PATH")) {
        QTimer::singleShot(9000,&app,[] {
            auto* connector=Global::eventConnector;
            if(!connector) { automationTrace("FAIL workshop probe has no EventConnector"); return; }
            QMetaObject::invokeMethod(connector,[connector] {
                auto* game=connector->game();
                if(!game || !game->w() || !game->wsm()) { automationTrace("FAIL workshop probe has no loaded world"); return; }
                connector->onSetPause(true);
                for(int z=Global::dimZ-2;z>0;--z) for(int x=Global::dimX/2-20;x<Global::dimX/2+20;++x) for(int y=Global::dimY/2-20;y<Global::dimY/2+20;++y) {
                    Position tile(x,y,z);
                    if(!game->w()->isWalkableGnome(tile) || game->wsm()->isWorkshop(tile)
                        || game->spm()->isStockPile(tile) || !DB::workshop("Carpenter")) continue;
                    auto* workshop=game->wsm()->addWorkshop("Carpenter",tile,0);
                    if(!workshop || workshop->tiles().isEmpty()) { automationTrace("FAIL Carpenter workshop object missing components"); return; }
                    for(auto component : workshop->tiles()) game->w()->setTileFlag(component,TileFlag::TF_WORKSHOP);
                    qputenv("INGNOMIA_PROBE_WORKSHOP_TILE",QByteArray::number(tile.toInt()));
                    automationTrace(QString("PASS created probe Carpenter workshop id=%1 tile=%2 footprint=%3").arg(workshop->id()).arg(tile.toInt()).arg(workshop->tiles().size()));
                    connector->onSelectTile(tile.toInt());
                    return;
                }
                automationTrace("FAIL workshop probe found no open tile or Carpenter definition");
            },Qt::QueuedConnection);
        });
        QTimer::singleShot(11500,&app,[] {
            if(qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_STAGED_EDIT_PROBE")=="1") {
                auto& window=MainWindow::getInstance();
                const bool settings=window.activateManagementElement("workshop_view_settings");
                const auto before=window.workshopSettingsStatusForProbe();
                const bool entered=window.setManagementFormValueForProbe("workshop_name","Draft Carpenter");
                const auto draft=window.workshopSettingsStatusForProbe();
                const bool staged=settings && entered && before==draft && before.find("name=Draft Carpenter")==std::string::npos;
                automationTrace(QString(staged?"PASS ":"FAIL ")+"Workshop name remains a local draft until Apply");
                const bool applied=staged && window.activateManagementElement("workshop_apply_basics");
                automationTrace(QString(applied?"PASS ":"FAIL ")+"Workshop Apply dispatched one staged basics edit");
				QTimer::singleShot(750,qApp,[applied] {
                    auto& current=MainWindow::getInstance();
                    const auto status=current.workshopSettingsStatusForProbe();
                    const bool committed=applied && status.find("name=Draft Carpenter")!=std::string::npos;
                    automationTrace(QString(committed?"PASS ":"FAIL ")+"Workshop authoritative snapshot reflects the applied name");
                    const auto path=qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_CAPTURE_PATH");
                    const bool captured=current.requestManagementCaptureForProbe("workshop",path);
                    automationTrace(QString(captured?"PASS ":"FAIL ")+"Workshop staged-edit capture requested");
                });
                return;
            }
            const auto path=qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_CAPTURE_PATH");
            const bool opened=MainWindow::getInstance().requestManagementCaptureForProbe("workshop",path);
            automationTrace(QString(opened?"PASS ":"FAIL ")+"Workshop manager opened and capture requested");
        });
        QTimer::singleShot(qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_STAGE10_LIVE")=="1"?37000:15000,&app,[] { if(Global::eventConnector) QMetaObject::invokeMethod(Global::eventConnector,"onExit",Qt::QueuedConnection); });
        return;
    }
    if(qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_OPEN_PROBE")=="1"
        && !qEnvironmentVariableIsEmpty("INGNOMIA_DATA_FOLDER") && !qEnvironmentVariableIsEmpty("INGNOMIA_AUTOMATE_LOAD_PATH")) {
        QTimer::singleShot(9000,&app,[] {
            auto* connector=Global::eventConnector;
            if(!connector) { automationTrace("FAIL stockpile probe has no EventConnector"); return; }
            QMetaObject::invokeMethod(connector,[connector] {
                auto* game=connector->game();
                if(!game || !game->w() || !game->spm() || !game->inv()) { automationTrace("FAIL stockpile probe has no loaded world"); return; }
                connector->onSetPause(true);
                const auto mask=TileFlag::TF_WORKSHOP+TileFlag::TF_STOCKPILE+TileFlag::TF_GROVE+TileFlag::TF_FARM+TileFlag::TF_PASTURE+TileFlag::TF_ROOM;
                for(int z=Global::dimZ-2;z>0;--z) for(int x=Global::dimX/2-20;x<Global::dimX/2+20;++x) for(int y=Global::dimY/2-20;y<Global::dimY/2+20;++y) {
                    Position tile(x,y,z);
                    if(!game->w()->isWalkableGnome(tile) || (game->w()->getTile(tile).flags-~mask)!=TileFlag::TF_NONE) continue;
                    game->spm()->addStockpile(tile,{{tile,true}});
                    auto* pile=game->spm()->getStockpileAtPos(tile);
                    const auto item=game->inv()->createItem(tile,"RawWood","Oak");
                    const bool inserted=pile && item && pile->insertItem(tile,item);
                    qputenv("INGNOMIA_PROBE_STOCKPILE_TILE",QByteArray::number(tile.toInt()));
                    automationTrace(QString(inserted?"PASS ":"FAIL ")+QString("created one-field Stockpile at %1 with item=%2").arg(tile.toInt()).arg(item));
                    connector->onSelectTile(tile.toInt());
                    return;
                }
                automationTrace("FAIL stockpile probe found no open tile");
            },Qt::QueuedConnection);
        });
        QTimer::singleShot(11500,&app,[] {
            const auto path=qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_CAPTURE_PATH");
            const bool opened=MainWindow::getInstance().requestManagementCaptureForProbe("stockpile",path);
            automationTrace(QString(opened?"PASS ":"FAIL ")+"Stockpile manager opened and capture requested");
        });
        if(qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_FORM_PROBE")=="1") {
            QTimer::singleShot(12000,&app,[]{
                auto& w=MainWindow::getInstance();w.activateManagementElement("stockpile_view_settings");
                w.setManagementFormValueForProbe("stockpile_name","Stage 05 supplies");
                w.setManagementFormValueForProbe("stockpile_priority","999999");w.activateManagementElement("stockpile_apply");w.activateManagementElement("stockpile_review_cancel");
            });
            QTimer::singleShot(12600,&app,[]{auto* c=Global::eventConnector;QMetaObject::invokeMethod(c,[c]{
                Position tile(qEnvironmentVariable("INGNOMIA_PROBE_STOCKPILE_TILE").toUInt());auto* pile=c->game()->spm()->getStockpileAtPos(tile);
                automationTrace(QString(pile&&pile->name()!="Stage 05 supplies"?"PASS ":"FAIL ")+"invalid priority blocked authoritative Apply");
                if(pile)qputenv("INGNOMIA_PROBE_PULL_BEFORE",pile->pullsOthers()?"1":"0");
            },Qt::QueuedConnection);});
            QTimer::singleShot(13200,&app,[]{auto& w=MainWindow::getInstance();w.setManagementFormValueForProbe("stockpile_priority","1");w.activateManagementElement("stockpile_apply");});
            // Hauling options are pending until Apply (Stage 21a).
            QTimer::singleShot(14000,&app,[]{auto& w=MainWindow::getInstance();w.activateManagementElement("stockpile_toggle_pull");w.activateManagementElement("stockpile_apply");});
            QTimer::singleShot(15000,&app,[]{auto* c=Global::eventConnector;QMetaObject::invokeMethod(c,[c]{
                Position tile(qEnvironmentVariable("INGNOMIA_PROBE_STOCKPILE_TILE").toUInt());auto* pile=c->game()->spm()->getStockpileAtPos(tile);
                const bool ok=pile&&pile->name()=="Stage 05 supplies"&&pile->priority()==0&&pile->pullsOthers()!=(qEnvironmentVariable("INGNOMIA_PROBE_PULL_BEFORE")=="1");
                automationTrace(QString(ok?"PASS ":"FAIL ")+"form name priority and hauling reached authoritative Stockpile");
            },Qt::QueuedConnection);});
            QTimer::singleShot(15600,&app,[]{MainWindow::getInstance().requestManagementCaptureForProbe("stockpile",qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_FORM_CAPTURE_PATH"));});
        }
        if(qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_STAGE09_LIVE")=="1") {
            // Stage 21a sheet: allow-list check boxes and General settings are pending until Apply; templates act at once.
            const auto schedule=[&app](int delay,std::function<void()> fn){QTimer::singleShot(delay,Qt::PreciseTimer,&app,std::move(fn));};
            const auto shot=[](const char* name){const auto dir=qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_STAGE21_DIR");if(dir.isEmpty())return;const bool ok=MainWindow::getInstance().requestManagementCaptureForProbe("stockpile",dir+"/"+name+".png");automationTrace(QString(ok?"PASS ":"FAIL ")+"capture requested "+name);};
            const auto act=[](const char* id){automationTrace(QString(MainWindow::getInstance().activateManagementElement(id)?"PASS ":"FAIL ")+"activated "+id);};
            schedule(12400,[shot]{shot("general-invalid");});
            schedule(16000,[act]{auto& w=MainWindow::getInstance();act("stockpile_view_allow");w.setManagementStockpileSearchForProbe("wood");automationTrace(QString::fromStdString(w.stockpileStage09Probe("scope")));act("stockpile_block_bulk");});
            schedule(16500,[shot]{shot("allow-pending");});
            schedule(17000,[act]{act("stockpile_apply");});
            schedule(17500,[act]{auto& w=MainWindow::getInstance();automationTrace(QString::fromStdString(w.stockpileStage09Probe("blocked")));w.setManagementFormValueForProbe("stockpile_template_name","Stage09 wood");act("stockpile_template_save");});
            schedule(18000,[shot]{shot("allow");});
            schedule(18500,[act]{auto& w=MainWindow::getInstance();automationTrace(QString::fromStdString(w.stockpileStage09Probe("scope")));act("stockpile_allow_bulk");act("stockpile_apply");});
            schedule(19500,[act]{auto& w=MainWindow::getInstance();automationTrace(QString::fromStdString(w.stockpileStage09Probe("allowed")));act("stockpile_template_save");});
            schedule(20000,[shot]{shot("template-replace");});
            schedule(20500,[act]{act("stockpile_review_cancel");});
            const auto compare=[](bool equal){auto*c=Global::eventConnector;QMetaObject::invokeMethod(c,[c,equal]{
                Position tile(qEnvironmentVariable("INGNOMIA_PROBE_STOCKPILE_TILE").toUInt());auto* pile=c->game()->spm()->getStockpileAtPos(tile);
                QJsonDocument doc;IO::loadFile(IO::getDataFolder()+"/settings/stockpile-filter-templates.json",doc);
                bool match=false;for(const auto& entry:doc.toVariant().toList())if(entry.toMap().value("Name").toString()=="Stage09 wood")match=pile&&entry.toMap().value("Filter").toMap()==pile->filter().serialize();
                automationTrace(QString(match==equal?"PASS ":"FAIL ")+(equal?"confirmed template replacement persisted current rules":"No on the replacement preserved the saved rules"));
            },Qt::QueuedConnection);};
            schedule(21000,[compare]{compare(false);});
            schedule(22000,[act]{act("stockpile_template_save");act("stockpile_review_accept");});
            schedule(23500,[compare]{compare(true);automationTrace(QString::fromStdString(MainWindow::getInstance().stockpileStage09Probe("inspector")));});
            schedule(24000,[act]{act("stockpile_view_settings");act("stockpile_toggle_suspended");});
            schedule(24400,[shot]{shot("general-pending");});
            schedule(24800,[act]{act("stockpile_apply");});
            schedule(25500,[]{auto&w=MainWindow::getInstance();automationTrace(QString::fromStdString(w.stockpileStage09Probe("agree")));automationTrace(QString::fromStdString(w.stockpileStage09Probe("toggle-inspector")));});
            schedule(27000,[]{auto&w=MainWindow::getInstance();automationTrace(QString::fromStdString(w.stockpileStage09Probe("agree")));w.requestManagementCaptureForProbe("stockpile",qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_STAGE09_CAPTURE"));});
            schedule(27600,[shot]{shot("general");});
            schedule(28000,[act]{act("stockpile_view_contents");});
            schedule(28400,[shot]{shot("contents");});
        }
        const int exitDelay=qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_STAGE09_LIVE")=="1"?29000:qEnvironmentVariable("INGNOMIA_AUTOMATE_INVENTORY_HISTORY")=="1"?30000:qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_FORM_PROBE")=="1"?18000:15000;
        QTimer::singleShot(exitDelay,&app,[] { if(Global::eventConnector) QMetaObject::invokeMethod(Global::eventConnector,"onExit",Qt::QueuedConnection); });
        return;
    }
    if(qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_LINKS")=="1"
        && !qEnvironmentVariableIsEmpty("INGNOMIA_DATA_FOLDER") && !qEnvironmentVariableIsEmpty("INGNOMIA_AUTOMATE_LOAD_PATH")) {
        const auto status=[] { automationTrace(QString::fromStdString(MainWindow::getInstance().workshopSettingsStatusForProbe())); };
        QTimer::singleShot(9000,&app,[] {
            auto* c=Global::eventConnector;
            QMetaObject::invokeMethod(c,[c] {
                c->onSetPause(true); workshopLinkedStockpileFixture(c);
                c->onSelectTile(qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_TILE").toUInt());
            },Qt::QueuedConnection);
        });
        QTimer::singleShot(11000,&app,[status] {
            auto& w=MainWindow::getInstance(); w.activateManagementElement("stockpile_close");
            w.activateManagementElement("workshop_view_settings");
            w.setManagementFormValueForProbe("workshop_priority","1"); status();
        });
        QTimer::singleShot(12000,&app,[status] {
            status(); auto& w=MainWindow::getInstance();
            w.setManagementFormValueForProbe("workshop_stockpile_choice",qEnvironmentVariable("INGNOMIA_PROBE_LINK_FAR").toStdString());
            automationTrace(QString("link far clicked=%1").arg(w.activateManagementElement("workshop_link_add")));
        });
        QTimer::singleShot(13500,&app,[status] {
            status(); auto& w=MainWindow::getInstance();
            w.setManagementFormValueForProbe("workshop_stockpile_choice",qEnvironmentVariable("INGNOMIA_PROBE_LINK_NEAR").toStdString());
            automationTrace(QString("link near clicked=%1").arg(w.activateManagementElement("workshop_link_add")));
            w.activateManagementElement("workshop_priority_down");
        });
        QTimer::singleShot(15000,&app,[status] {
            status(); auto* c=Global::eventConnector;
            QMetaObject::invokeMethod(c,[c] {
                Position p(qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_TILE").toUInt());
                auto* ws=c->game()->wsm()->workshopAt(p);
                const auto expected=QList<unsigned int>{qEnvironmentVariable("INGNOMIA_PROBE_LINK_FAR").toUInt(),qEnvironmentVariable("INGNOMIA_PROBE_LINK_NEAR").toUInt()};
                automationTrace(QString(ws && ws->linkedStockpiles()==expected?"PASS ":"FAIL ")+"UI linked both selected stockpiles through queued simulation commands");
            },Qt::QueuedConnection);
            const auto path=qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_CAPTURE_PATH");
            if(!path.isEmpty()) qputenv("INGNOMIA_AUTOMATE_DETACHED_CAPTURE_PATH",path.toUtf8());
        });
        QTimer::singleShot(17000,&app,[status] {
            auto& w=MainWindow::getInstance(); w.activateManagementElement("workshop_view_craft");
            w.activateManagementElement("workshop_queue_once"); status();
        });
        QTimer::singleShot(18500,&app,[status] { status(); });
        QTimer::singleShot(20000,&app,[] { QMetaObject::invokeMethod(Global::eventConnector,"onExit",Qt::QueuedConnection); });
        return;
    }
    if (qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_SETTINGS") == "1"
        && !qEnvironmentVariableIsEmpty("INGNOMIA_DATA_FOLDER") && !qEnvironmentVariableIsEmpty("INGNOMIA_AUTOMATE_LOAD_PATH")) {
        const auto status = [] {
            automationTrace(QString::fromStdString(MainWindow::getInstance().workshopSettingsStatusForProbe()));
        };
        QTimer::singleShot(9000, &app, [] {
            auto* c=Global::eventConnector;
            QMetaObject::invokeMethod(c, [c] {
                c->onSetPause(true);
                c->onSelectTile(qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_TILE").toUInt());
            }, Qt::QueuedConnection);
        });
        QTimer::singleShot(10500, &app, [status] { MainWindow::getInstance().activateManagementElement("workshop_view_settings"); status(); });
        QTimer::singleShot(11000, &app, [] { QMetaObject::invokeMethod(Global::eventConnector,"onSelectTile",Qt::QueuedConnection,Q_ARG(unsigned int,qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_TILE").toUInt())); });
        QTimer::singleShot(12000, &app, [] { MainWindow::getInstance().activateManagementElement("stockpile_close"); });
        QTimer::singleShot(13000, &app, [] { MainWindow::getInstance().activateManagementElement("workshop_toggle_generated"); });
        QTimer::singleShot(14000, &app, [status] { status(); MainWindow::getInstance().activateManagementElement("workshop_toggle_auto_missing"); });
        QTimer::singleShot(15000, &app, [status] { status(); MainWindow::getInstance().activateManagementElement("workshop_toggle_suspended"); });
        QTimer::singleShot(16000, &app, [status] {
            status(); auto* c=Global::eventConnector;
            QMetaObject::invokeMethod(c, [c] {
                auto* g=c->game(); if(!g) return;
                Position tile(qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_TILE").toUInt());
                auto* ws=g->wsm()->workshopAt(tile); if(!ws) return;
                const auto in=ws->inputPos();
                if(!ws->getPossibleStockpile()) {
                    for(auto pos : {in.northOf(),in.westOf(),in.southOf(),in.eastOf()}) {
                        if(!g->spm()->isStockPile(pos)) { g->spm()->addStockpile(pos,{{pos,true}}); break; }
                    }
                }
                automationTrace(QStringLiteral("eligible_stockpile=%1 paused=%2").arg(ws->getPossibleStockpile()).arg(g->paused()));
            }, Qt::QueuedConnection);
        });
        QTimer::singleShot(18000, &app, [status] { status(); MainWindow::getInstance().activateManagementElement("workshop_toggle_linked"); });
        QTimer::singleShot(19500, &app, [status] {
            status(); auto* c=Global::eventConnector;
            QMetaObject::invokeMethod(c, [c] {
                Position tile(qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_TILE").toUInt());
                if(auto* ws=c->game()->wsm()->workshopAt(tile)) automationTrace(QStringLiteral("authoritative linked=%1 generated=%2 auto=%3 active=%4").arg(ws->linkedStockpile()).arg(ws->isAcceptingGenerated()).arg(ws->getAutoCraftMissing()).arg(ws->active()));
            }, Qt::QueuedConnection);
            const auto capture=qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_CAPTURE_PATH");
            if(!capture.isEmpty()) qputenv("INGNOMIA_AUTOMATE_DETACHED_CAPTURE_PATH",capture.toUtf8());
        });
        QTimer::singleShot(21000, &app, [] { MainWindow::getInstance().activateManagementElement("workshop_toggle_linked"); });
        QTimer::singleShot(22500, &app, [status] { status(); QMetaObject::invokeMethod(Global::eventConnector,"onExit",Qt::QueuedConnection); });
        return;
    }
    if (qEnvironmentVariable("INGNOMIA_AUTOMATE_INDEPENDENT_WINDOWS") == "1"
        && !qEnvironmentVariableIsEmpty("INGNOMIA_DATA_FOLDER")) {
        QTimer::singleShot(9000, &app, [] {
            const auto tile=qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_TILE").toUInt();
            if(Global::eventConnector) QMetaObject::invokeMethod(Global::eventConnector,"onSelectTile",Qt::QueuedConnection,Q_ARG(unsigned int,tile));
        });
        QTimer::singleShot(11500, &app, [] {
            const auto tile=qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_TILE").toUInt();
            if(Global::eventConnector) QMetaObject::invokeMethod(Global::eventConnector,"onSelectTile",Qt::QueuedConnection,Q_ARG(unsigned int,tile));
        });
        QTimer::singleShot(14000, &app, [] {
            automationTrace(QStringLiteral("independent_windows opened=%1").arg(MainWindow::getInstance().verifyManagementWindowsForProbe(false)));
        });
        QTimer::singleShot(16000, &app, [] {
            automationTrace(QStringLiteral("independent_windows close_isolated=%1").arg(MainWindow::getInstance().verifyManagementWindowsForProbe(true)));
            const auto capture=qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_CAPTURE_PATH");
            if(!capture.isEmpty()) qputenv("INGNOMIA_AUTOMATE_DETACHED_CAPTURE_PATH",capture.toUtf8());
            MainWindow::getInstance().activateManagementElement("workshop_view_settings");
        });
        QTimer::singleShot(22000, &app, [] {
            if(Global::eventConnector) QMetaObject::invokeMethod(Global::eventConnector,"onExit",Qt::QueuedConnection);
        });
        QTimer::singleShot(18000, &app, [] {
            const auto tile=qEnvironmentVariable("INGNOMIA_AUTOMATE_STOCKPILE_TILE").toUInt();
            if(Global::eventConnector) QMetaObject::invokeMethod(Global::eventConnector,"onSelectTile",Qt::QueuedConnection,Q_ARG(unsigned int,tile));
        });
        QTimer::singleShot(19500, &app, [] {
            automationTrace(QStringLiteral("independent_windows reopened=%1").arg(MainWindow::getInstance().verifyManagementWindowsForProbe(false)));
        });
        return;
    }
    if (qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_ORDER") != "1") return;
    if (qEnvironmentVariableIsEmpty("INGNOMIA_DATA_FOLDER") || qEnvironmentVariableIsEmpty("INGNOMIA_AUTOMATE_LOAD_PATH")) return;
    QTimer::singleShot(13500, &app, [] {
        auto& window = MainWindow::getInstance();
        const bool count = window.setManagementFormValueForProbe("workshop_order_count", "23");
        const bool mode = window.activateManagementElement("workshop_order_mode_maintain");
        const bool added = count && mode && window.activateManagementElement("workshop_queue_once");
        automationTrace(QStringLiteral("workshop_order count=%1 mode=%2 added=%3").arg(count).arg(mode).arg(added));
    });
    QTimer::singleShot(15000, &app, [] {
        auto& window = MainWindow::getInstance();
        const bool queue = window.activateManagementElement("workshop_view_queue");
        const bool count = window.setManagementFormValueForProbe("workshop_job_count", "37");
        const bool applied = queue && count && window.activateManagementElement("workshop_job_apply");
        automationTrace(QStringLiteral("workshop_edit queue=%1 count=%2 applied=%3").arg(queue).arg(count).arg(applied));
    });
    QTimer::singleShot(16500, &app, [] {
        const auto capture = qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_CAPTURE_PATH");
        if (!capture.isEmpty()) qputenv("INGNOMIA_AUTOMATE_DETACHED_CAPTURE_PATH", capture.toUtf8());
        MainWindow::getInstance().activateManagementElement("workshop_view_queue");
        if (Global::eventConnector) QMetaObject::invokeMethod(Global::eventConnector, "onSaveGame", Qt::QueuedConnection);
        automationTrace(QStringLiteral("workshop_save dispatched=true"));
    });
    QTimer::singleShot(22000, &app, [] {
        if (Global::eventConnector) QMetaObject::invokeMethod(Global::eventConnector, "onExit", Qt::QueuedConnection);
    });
}
