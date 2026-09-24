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
				for (auto* farm : game->fm()->allFarms()) {
					if (!farm || farm->countTiles() < 4) continue;
					const auto fields = farm->serialize().toMap().value("Fields").toList();
					if (fields.size() < 2) continue;
					const Position first(fields[0].toMap().value("Pos").toString());
					const Position second(fields[1].toMap().value("Pos").toString());
					qputenv("INGNOMIA_PROBE_FARM_TILE", QByteArray::number(first.toInt()));
					qputenv("INGNOMIA_PROBE_FARM_PLOT_A", QString("%1_%2_%3").arg(first.x).arg(first.y).arg(first.z).toUtf8());
					qputenv("INGNOMIA_PROBE_FARM_PLOT_B", QString("%1_%2_%3").arg(second.x).arg(second.y).arg(second.z).toUtf8());
					automationTrace(QString("Farm plan probe selected existing farm id=%1 plots=%2").arg(farm->id()).arg(fields.size()));
					connector->onSelectTile(first.toInt());
					return;
				}
				automationTrace("FAIL Farm plan probe found no existing multi-plot Farm");
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
			const bool products = MainWindow::getInstance().activateManagementElement("agriculture_view_products");
			if (qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_NATIVE_CLICK_PROBE") == "1") {
				const bool allPlots = MainWindow::getInstance().activateManagementElement("agriculture_plot_select_all");
				automationTrace(QString(products && allPlots ? "PASS" : "FAIL") + " Farm plan opened with all plots for pointer probe");
				return;
			}
			const bool filtered = MainWindow::getInstance().setManagementFormValueForProbe("agriculture_farm_search", "strawberry");
			const auto crop = std::string("agriculture_farm_crop_") + QByteArray("Strawberry").toHex().toStdString();
			const bool chosen = MainWindow::getInstance().activateManagementElement(crop);
			const auto first = std::string("agriculture_plot_") + qEnvironmentVariable("INGNOMIA_PROBE_FARM_PLOT_A").toStdString();
			const auto second = std::string("agriculture_plot_") + qEnvironmentVariable("INGNOMIA_PROBE_FARM_PLOT_B").toStdString();
			const bool selected = MainWindow::getInstance().activateManagementElement(first) && MainWindow::getInstance().activateManagementElement(second);
			const bool assigned = selected && MainWindow::getInstance().activateManagementElement("agriculture_plot_assign");
			const bool count = MainWindow::getInstance().setManagementFormValueForProbe("agriculture_plot_count", "2");
			const bool queued = count && MainWindow::getInstance().activateManagementElement("agriculture_plot_queue");
			automationTrace(QString(products && filtered && chosen && selected && assigned && queued ? "PASS" : "FAIL") + " Farm grid selects two plots and queues two Strawberry plantings on each");
			MainWindow::getInstance().activateManagementElement(second);
		}
        else if (qEnvironmentVariable("INGNOMIA_AUTOMATE_FARM_SEED_PROBE") == "1") {
            automationTrace(QString(MainWindow::getInstance().activateManagementElement("agriculture_view_products") ? "PASS" : "FAIL")
                + " Farm Products view opens");
            const bool filtered = MainWindow::getInstance().setManagementFormValueForProbe("agriculture_search", "strawberry");
            const auto row = std::string("m6a_agri_product_") + QByteArray("Strawberry").toHex().toStdString();
            const bool selected = filtered && MainWindow::getInstance().activateManagementElement(row);
            automationTrace(QString(selected ? "PASS" : "FAIL") + " Strawberry filtered and selected through Farm Products");
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
				for (const auto& value : fields) {
					const auto plot = value.toMap();
					const auto orders = plot.value("CropOrders").toList();
					if (plot.value("Crop").toString() == "Strawberry" && orders.size() == 1
						&& orders.first().toMap().value("Crop").toString() == "Strawberry"
						&& orders.first().toMap().value("Remaining").toInt() == 2) ++planned;
				}
				automationTrace(QString(planned == 2 ? "PASS" : "FAIL") + QString(" Farm plot assignments and queues serialized for %1 plots").arg(planned));
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
            const auto row = std::string("m6a_agri_product_") + QByteArray("Strawberry").toHex().toStdString();
            const bool selected = MainWindow::getInstance().activateManagementElement(row);
            const bool applied = selected && MainWindow::getInstance().activateManagementElement("agriculture_apply_product");
            automationTrace(QString(selected ? "PASS" : "FAIL") + " Strawberry selected through Farm Products");
            automationTrace(QString(applied ? "PASS" : "FAIL") + " selected crop applied through Farm Products");
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
    QTimer::singleShot(16000, &app, [] {
        if (Global::eventConnector) QMetaObject::invokeMethod(Global::eventConnector, "onExit", Qt::QueuedConnection);
    });
}

inline void scheduleWorkshopOrderProbe(QApplication& app)
{
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
