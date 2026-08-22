#include "gui/ui/navigation/WorkbenchCoordinator.h"
#include <cstdlib>
#include <iostream>
using namespace ingnomia::ui;using namespace ingnomia::ui::navigation;
void require(bool value,const char*message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
int main(){
	bool familyB=false,familyC=false;FocusToken restored{},activeFocus{};RouteId activeRoute;
	WorkbenchCoordinator* coordinator{};
	WorkbenchPorts ports;
	ports.openPopulation=[&](FocusToken focus){familyB=true;activeFocus=focus;activeRoute=RouteId{"workbench.population"};return true;};
	ports.openInventory=[&](FocusToken focus){familyB=true;activeFocus=focus;activeRoute=RouteId{"workbench.inventory"};return true;};
	ports.openMilitary=[&](FocusToken focus){familyC=true;activeFocus=focus;activeRoute=RouteId{"workbench.military"};return true;};
	ports.openDiplomacy=[&](FocusToken focus){familyC=true;activeFocus=focus;activeRoute=RouteId{"workbench.diplomacy"};return true;};
	ports.closePopulationInventory=[&]{familyB=false;if(coordinator)(void)coordinator->close(activeRoute,activeFocus);};ports.closeMilitaryDiplomacy=[&]{familyC=false;if(coordinator)(void)coordinator->close(activeRoute,activeFocus);};ports.restoreFocus=[&](FocusToken value){restored=value;};
	WorkbenchCoordinator value(std::move(ports));coordinator=&value;(void)coordinator;
	require(!value.open(Workbench::Population,FocusToken{1}),"menu rejects workbench");
	require(value.enterGame(),"enter game");
	require(value.open(Workbench::Population,FocusToken{1})&&familyB&&!familyC,"population opens");
	require(value.open(Workbench::Population,FocusToken{1})&&!familyB&&!value.state().workbench&&restored.value==1,"clicking the active population button closes it and restores focus");
	require(value.open(Workbench::Population,FocusToken{1})&&familyB,"population reopens after toggle close");
	restored={};
	require(value.open(Workbench::Inventory,FocusToken{2})&&familyB&&value.state().workbenches.size()==2,"population and inventory remain open together");
	require(value.open(Workbench::Military,FocusToken{3})&&familyB&&familyC&&value.state().workbenches.size()==3,"cross-family workbench remains open");
	require(!restored,"opening another window does not restore intermediate focus");
	require(value.close(RouteId{"workbench.inventory"},FocusToken{2})&&restored.value==2&&value.state().workbenches.size()==2,"individual close restores its launcher focus");familyB=false;
	require(value.open(Workbench::Diplomacy,FocusToken{4}),"open diplomacy");
	require(value.handleEscape(false,false,false,false,false)&&value.state().workbench&&value.state().workbenches.size()==2,"Escape closes only the frontmost workbench");
	(void)value.close(RouteId{"workbench.military"},FocusToken{3});
	(void)value.close(RouteId{"workbench.population"},FocusToken{1});
	require(!value.handleEscape(false,true,false,false,false),"required modal blocks Escape");
	value.leaveGame();require(!value.open(Workbench::Population,FocusToken{1}),"menu transition rejects HUD workbench open");
	std::cout<<"Workbench routing, multi-window stacking, Escape, and focus tests passed\n";
}
