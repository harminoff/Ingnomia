/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../../src/game/workshopmanager.h"
#include "../../src/game/stockpilemanager.h"
#include "../../src/game/stockpile.h"
#include "../../src/game/inventory.h"
#include "../../src/game/gnomemanager.h"
#include "../../src/game/gnometrader.h"
#include "../../src/gui/aggregatorworkshop.h"
#include "../../src/game/world.h"

// Opens each probe workshop through the production tile-selection route and captures
// every supported pane. Captures land in INGNOMIA_AUTOMATE_WORKSHOP_STAGE10_DIR.
inline void scheduleStage10Captures(QApplication& app,int start)
{
 // Precise timers: coarse timers may drift ~5%, which reorders steps this far after launch.
 const auto at=[&app](int delay,std::function<void()> fn){QTimer::singleShot(delay,Qt::PreciseTimer,&app,std::move(fn));};
 const auto dir=qEnvironmentVariable("INGNOMIA_AUTOMATE_WORKSHOP_STAGE10_DIR");
 const auto capture=[dir](const char* name){const bool ok=MainWindow::getInstance().requestManagementCaptureForProbe("workshop",dir+"/"+name+".png");automationTrace(QString(ok?"PASS ":"FAIL ")+"capture requested "+name);};
 const auto state=[]{automationTrace(QString::fromStdString(MainWindow::getInstance().workshopStage10Probe("state")));};
 const auto open=[](const char* env){auto* c=Global::eventConnector;const auto tile=qEnvironmentVariable(env).toUInt();QMetaObject::invokeMethod(c,[c,tile]{c->onSelectTile(tile);},Qt::QueuedConnection);};
 const auto click=[](const char* id){automationTrace(QString(MainWindow::getInstance().activateManagementElement(id)?"PASS ":"FAIL ")+"activated "+id);};
 // The backend probe opened the market through the aggregator; return to the Carpenter first.
 at(start-600,[=]{open("INGNOMIA_PROBE_WORKSHOP_TILE");});
 at(start+400,[=]{click("workshop_view_queue");automationTrace(QString::fromStdString(MainWindow::getInstance().workshopStage10Probe("select-first-job")));});
 at(start+800,[=]{state();capture("queue");});
 at(start+1800,[=]{click("workshop_view_settings");});
 at(start+2400,[=]{state();capture("settings");});
 at(start+2700,[=]{click("workshop_view_stockpiles");});
 at(start+3200,[=]{capture("stockpiles");});
 at(start+3700,[=]{click("workshop_view_settings");});
 // Restock settlement goods the earlier exchange consumed, so both ledgers have rows.
 at(start+3000,[]{auto* c=Global::eventConnector;QMetaObject::invokeMethod(c,[c]{auto* g=c->game();Position base(qEnvironmentVariable("INGNOMIA_PROBE_WORKSHOP_TILE").toUInt());int made=0;
  for(int n=0;n<8;++n){Position tile=base+Position(0,5+n,0);auto* pile=g->spm()->getStockpileAtPos(tile);if(!pile)continue;for(int k=0;k<3;++k){auto id=g->inv()->createItem(tile,"RawWood",n%2?"Oak":"Pine");if(pile->insertItem(tile,id))++made;}}
  automationTrace(QString(made>0?"PASS ":"FAIL ")+QString("restocked %1 settlement items").arg(made));},Qt::QueuedConnection);});
 at(start+3400,[=]{open("INGNOMIA_STAGE10_BUTCHER_TILE");});
 at(start+4600,[=]{state();capture("butcher");});
 at(start+5600,[=]{open("INGNOMIA_STAGE10_FISHERY_TILE");});
 at(start+6800,[=]{state();capture("fishery");});
 at(start+7800,[=]{open("INGNOMIA_STAGE10_MARKET_TILE");});
 at(start+9200,[=]{state();automationTrace(QString::fromStdString(MainWindow::getInstance().workshopStage10Probe("select-first-trade-row")));});
 const auto probe=[](const char* a){automationTrace(QString::fromStdString(MainWindow::getInstance().workshopStage10Probe(a)));};
 at(start+9800,[=]{MainWindow::getInstance().setManagementFormValueForProbe("workshop_trade_count","2");click("workshop_trade_set");});
 at(start+10600,[=]{probe("offers");capture("trade-unbalanced");probe("select-first-player-row");});
 at(start+11400,[=]{MainWindow::getInstance().setManagementFormValueForProbe("workshop_trade_count","3");click("workshop_trade_set");});
 at(start+12200,[=]{probe("offers");state();capture("trade");});
 at(start+13200,[=]{click("workshop_trade_execute");});
 at(start+14000,[=]{state();capture("trade-review");});
 at(start+15000,[=]{click("workshop_review_cancel");});
 at(start+15800,[=]{state();probe("offers");capture("trade-cancelled");});
}

inline void stage10BackendProbe(EventConnector* c)
{
 auto* g=c->game();auto* a=c->aggregatorWorkshop();
 const auto check=[](bool ok,const QString& message){automationTrace(QString(ok?"PASS ":"FAIL ")+message);};
 auto* ws=g->wsm()->workshopAt(Position(qEnvironmentVariable("INGNOMIA_PROBE_WORKSHOP_TILE").toUInt()));
 if(!ws){check(false,"Stage10 workshop exists");return;}
 auto jobs=ws->jobList();check(jobs.size()==1,"two UI Add activations created exactly one authoritative order");
 if(jobs.empty())return;
 const auto first=jobs.first();QStringList mats;for(const auto& input:first.requiredItems)mats.append(input.materialSID);a->onCraftItem(ws->id(),first.craftID,0,4,mats);
 jobs=ws->jobList();check(jobs.size()==2,"second authoritative order created");
 if(jobs.size()!=2)return;
 const auto second=jobs.last().id;
 a->onCraftJobCommand(ws->id(),second,"Top");a->onCraftJobParams(ws->id(),second,1,9,true,true);
 jobs=ws->jobList();check(jobs.first().id==second && jobs.first().numItemsToCraft==9 && jobs.first().paused,"reordered stable job received exact parameters");
 a->onCraftJobCommand(ws->id(),second,"Cancel");a->onCraftJobParams(ws->id(),second,0,99,false,false);
 jobs=ws->jobList();check(jobs.size()==1 && jobs.first().id==first.id && jobs.first().numItemsToCraft==first.numItemsToCraft,"removed job cannot edit neighboring order");
 a->onCraftItem(ws->id(),"invalid-recipe",0,3,{});check(ws->jobList().size()==1,"invalid recipe rejected");
 // Probe-created models get their footprint flags so the production tile-selection route opens them.
 const auto place=[&](const char* type,Position at,const char* env){auto* w=g->wsm()->addWorkshop(type,at,0);if(w){for(auto tile:w->tiles())g->w()->setTileFlag(tile,TileFlag::TF_WORKSHOP);qputenv(env,QByteArray::number(at.toInt()));}return w;};
 Position p=ws->pos()+Position(8,0,0);auto* butcher=place("Butcher",p,"INGNOMIA_STAGE10_BUTCHER_TILE");
 a->onSetButcherOptions(butcher->id(),true,false);auto butcherMap=butcher->serialize().toMap();WorkshopProperties butcherSaved(butcherMap);check(butcherSaved.butcherCorpses && !butcherSaved.butcherExcess,"Butcher options serialized");
 p=ws->pos()+Position(12,0,0);auto* fisher=place("Fishery",p,"INGNOMIA_STAGE10_FISHERY_TILE");
 a->onSetFisherOptions(fisher->id(),true,false);auto fishMap=fisher->serialize().toMap();WorkshopProperties fishSaved(fishMap);check(fishSaved.fish && !fishSaved.processFish,"Fishery options serialized");
 QList<unsigned int> piles;
 for(int n=0;n<8;++n){Position tile=ws->pos()+Position(0,5+n,0);g->spm()->addStockpile(tile,{{tile,true}});auto* pile=g->spm()->getStockpileAtPos(tile);if(!pile)continue;piles.append(pile->id());auto id=g->inv()->createItem(tile,"RawWood",n%2?"Oak":"Pine");pile->insertItem(tile,id);}
 if(piles.size()>1){a->onSetStockpileLink(ws->id(),piles[0],true);a->onSetStockpileLink(ws->id(),piles[1],true);a->onSetStockpileLink(ws->id(),piles[0],false);auto savedMap=ws->serialize().toMap();WorkshopProperties saved(savedMap);check(saved.linkedStockpiles.contains(piles[1]) && !saved.linkedStockpiles.contains(piles[0]),"direct link and unlink persist exact stockpile IDs");
  a->onSetStockpileLink(ws->id(),piles[1],true);check(ws->linkedStockpiles().count(piles[1])==1,"re-linking an already linked stockpile keeps one link");}
 if(piles.size()>2){const auto gone=piles.takeLast();g->spm()->removeStockpile(gone);a->onSetStockpileLink(ws->id(),gone,true);check(!ws->linkedStockpiles().contains(gone),"a deleted stockpile cannot be linked");}
 p=ws->pos()+Position(16,0,0);auto* market=place("MarketStall",p,"INGNOMIA_STAGE10_MARKET_TILE");auto traderID=g->gm()->addTrader(p,market->id(),"");market->assignGnome(traderID);auto* trader=g->gm()->trader(traderID);
 if(!trader){check(false,"merchant created");return;}
 trader->inventory().clear();for(const auto& mat:{QString("Oak"),QString("Pine")}){TraderItem item;item.type="Item";item.itemSID="Plank";item.materialSID=mat;item.quality=2;item.value=1;item.amount=10;trader->inventory().append(item);}
 quint64 revision=0;QList<GuiTradeItem> sells,buys;int sellerValue=0,buyerValue=0,rejections=0;
 auto snap=QObject::connect(a,&AggregatorWorkshop::signalTradeSnapshot,a,[&](unsigned int,unsigned int,quint64 r,const QList<GuiTradeItem>& s,const QList<GuiTradeItem>& b,int sv,int bv){revision=r;sells=s;buys=b;sellerValue=sv;buyerValue=bv;},Qt::DirectConnection);
 auto rejected=QObject::connect(a,&AggregatorWorkshop::signalWorkshopRejected,a,[&](unsigned int,const QString&){++rejections;},Qt::DirectConnection);
 a->onOpenWorkshopInfo(market->id());a->onRequestAllTradeItems(market->id());check(sells.size()==2 && buys.size()>=2,"targeted snapshot contains multiple merchant and settlement rows");
 const auto offer=[&](bool seller,const GuiTradeItem& r,int n){a->onSetTradeOffer(market->id(),traderID,revision,seller,r.itemSID,r.materialSIDorGender,r.quality,n);};
 auto desired=sells;for(const auto& row:desired)offer(true,row,1);auto available=buys;for(const auto& row:available)offer(false,row,row.count);
 check(sellerValue==2 && buyerValue>=2,"exact offers refresh backed totals on both sides");
 const auto stale=revision;offer(true,sells[0],2);const auto before=trader->inventory()[0].amount;a->onReviewedTrade(market->id(),traderID,stale);check(rejections==1 && trader->inventory()[0].amount==before,"stale reviewed revision exchanges nothing");
 trader->inventory()[0].value=7;a->onReviewedTrade(market->id(),traderID,revision);check(rejections==2 && trader->inventory()[0].amount==before,"changed merchant price rejected before exchange");trader->inventory()[0].value=1;
 const auto reviewed=revision;a->onReviewedTrade(market->id(),traderID,reviewed);check(trader->inventory()[0].amount==before-2 && trader->inventory()[1].amount==9 && sellerValue==0 && buyerValue==0,"reviewed trade exchanges exact quantities and clears totals");
 a->onReviewedTrade(market->id(),traderID,reviewed);check(rejections==3 && trader->inventory()[0].amount==before-2,"duplicate commit cannot exchange again");
 market->assignGnome(0);a->onReviewedTrade(market->id(),traderID,revision);check(rejections==4,"missing merchant rejects commit");market->assignGnome(traderID);
 a->onRequestAllTradeItems(market->id());
 QObject::disconnect(snap);QObject::disconnect(rejected);
 qputenv("INGNOMIA_STAGE10_MARKET",QByteArray::number(market->id()));
}
