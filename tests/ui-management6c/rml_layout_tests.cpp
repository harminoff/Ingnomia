/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "gui/ui/screens/management6b/Management6BRmlBinding.h"
#include "gui/ui/screens/management6a/Management6ARmlBinding.h"
#include "gui/ui/screens/management6c/Management6CRmlBinding.h"
#include "gui/ui/screens/hud/HudRmlBinding.h"
#include "gui/ui/runtime/RmlUiQtInputAdapter.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <tuple>

using namespace ingnomia::ui;
using namespace ingnomia::ui::management6c;
namespace {
struct StockpilePort : management6a::CommandPort {
    std::vector<UiActionEnvelope> sent;
    management6a::CommandResult dispatch(const UiActionEnvelope& action, management6a::DispatchOrigin) override { sent.push_back(action); return {}; }
};
struct StockpileScreen : management6a::ViewPort { void stateChanged(const management6a::Management6AState&) override {} };
void check(bool ok, const std::string& message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
struct Files : Rml::FileInterface {
    std::filesystem::path root;
    Rml::FileHandle Open(const Rml::String& name) override {
        auto path = std::filesystem::path(name);
        if (!path.has_root_name()) path = root / path.relative_path();
        return reinterpret_cast<Rml::FileHandle>(std::fopen(path.string().c_str(), "rb"));
    }
    void Close(Rml::FileHandle f) override { std::fclose(reinterpret_cast<FILE*>(f)); }
    size_t Read(void* p, size_t size, Rml::FileHandle f) override { return std::fread(p, 1, size, reinterpret_cast<FILE*>(f)); }
    bool Seek(Rml::FileHandle f, long n, int from) override { return std::fseek(reinterpret_cast<FILE*>(f), n, from) == 0; }
    size_t Tell(Rml::FileHandle f) override { return static_cast<size_t>(std::ftell(reinterpret_cast<FILE*>(f))); }
};
struct Renderer : Rml::RenderInterface {
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override { return 1; }
    void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
    void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
    Rml::TextureHandle LoadTexture(Rml::Vector2i&, const Rml::String&) override { return 0; }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override { return 1; }
    void ReleaseTexture(Rml::TextureHandle) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(Rml::Rectanglei) override {}
};
struct Screen : ViewPort { void stateChanged(const Management6CState&) override {} };
struct Port : CommandPort {
    std::vector<UiActionEnvelope> sent;
    CommandResult dispatch(const UiActionEnvelope& action, DispatchOrigin) override { sent.push_back(action); return {}; }
};
struct InventoryScreen : management6b::ViewPort { void stateChanged(const management6b::Management6BState&) override {} };
struct InventoryPort : management6b::CommandPort {
    std::vector<UiActionEnvelope> sent;
    management6b::CommandResult dispatch(const UiActionEnvelope& action) override { sent.push_back(action); return {}; }
    management6b::CommandResult dispatchConfirmed(const UiActionEnvelope& action) override { sent.push_back(action); return {}; }
};
struct HudScreen : hud::HudViewPort { void stateChanged(const hud::HudState&) override {} };
struct HudPort : hud::HudCommandPort {
    std::vector<UiActionEnvelope> sent;
    hud::CommandResult dispatch(const UiActionEnvelope& action) override { sent.push_back(action); return {}; }
    hud::CommandResult requestBuildItems(BuildSelection, std::string_view) override { return {}; }
};
void update(Rml::Context& context) { context.Update(); context.Update(); }
bool inViewport(Rml::Element* e, Rml::Vector2i size) {
    if (!e || !e->IsVisible(true)) return false;
    const auto p = e->GetAbsoluteOffset(Rml::BoxArea::Border);
    return p.x >= -1 && p.y >= -1 && e->GetOffsetWidth() > 0 && e->GetOffsetHeight() > 0
        && p.x + e->GetOffsetWidth() <= size.x + 1 && p.y + e->GetOffsetHeight() <= size.y + 1;
}
bool inside(Rml::Element* child, Rml::Element* parent) {
    if (!child || !parent || !child->IsVisible(true)) return false;
    const auto c = child->GetAbsoluteOffset(Rml::BoxArea::Border);
    const auto p = parent->GetAbsoluteOffset(Rml::BoxArea::Border);
    return c.x >= p.x - 1 && c.y >= p.y - 1
        && c.x + child->GetOffsetWidth() <= p.x + parent->GetOffsetWidth() + 1
        && c.y + child->GetOffsetHeight() <= p.y + parent->GetOffsetHeight() + 1;
}
void click(Rml::Context& context, Rml::Element* e) {
    check(e && e->IsVisible(true), "Click target must be visible");
    auto p = e->GetAbsoluteOffset(Rml::BoxArea::Border);
    context.ProcessMouseMove(static_cast<int>(p.x + e->GetOffsetWidth() / 2), static_cast<int>(p.y + e->GetOffsetHeight() / 2), 0);
    context.ProcessMouseButtonDown(0, 0); context.ProcessMouseButtonUp(0, 0); update(context);
}
}
#include "workshop_layout_tests.h"
int main(int argc, char** argv) {
	check(argc <= 2, "Usage: ui_management6c_rml_layout [content/rmlui directory]");
	const auto executableDirectory = std::filesystem::absolute(std::filesystem::path(argv[0])).parent_path();
	Files files;
	files.root = argc == 2 ? std::filesystem::absolute(argv[1])
		: executableDirectory.parent_path() / "content" / "rmlui";
    Renderer renderer; Rml::SystemInterface system;
    Rml::SetFileInterface(&files); Rml::SetRenderInterface(&renderer); Rml::SetSystemInterface(&system);
    check(Rml::Initialise(), "RmlUi initializes");
    check(Rml::LoadFontFace("fonts/LatoLatin-Regular.ttf"), "Load the packaged font");
    auto* context = Rml::CreateContext("management-layout-tests", {1120, 760});
    context->SetDefaultScrollBehavior(Rml::ScrollBehavior::Instant, 1.f);
    workshopLayoutTests(*context);
    context->SetDimensions({1120,760});
    {
        Port port; Screen screen; Management6CController controller(port,screen);
        Management6CRmlBinding military(*context), diplomacy(*context);
        military.setWindowSurface(false); diplomacy.setWindowSurface(true);
        check(military.initialize(controller) && diplomacy.initialize(controller),"Independent military/diplomacy bindings load");
        controller.addViewPort(military); controller.addViewPort(diplomacy); controller.beginWorld(WorldEpoch{82});
        MilitaryRoleRow retainedRole; retainedRole.id = MilitaryRoleId{82}; retainedRole.name = "Retained role";
        controller.applyRoles({WorldEpoch{82}, Revision{1}, {retainedRole}});
        military.openMilitary(View::Roles,FocusToken{1}); diplomacy.openDiplomacy(View::Neighbors,FocusToken{2}); update(*context);
        check(military.militaryDocument()->IsVisible() && diplomacy.diplomacyDocument()->IsVisible(),"Both management surfaces remain visible");
        check(!military.militaryDocument()->GetElementById("military_tab_neighbors")->IsVisible(true)
            && !diplomacy.diplomacyDocument()->GetElementById("diplomacy_tab_roles")->IsVisible(true),"Native windows only expose their own workbench tabs");
        check(military.militaryDocument()->GetElementById("military_role_rows")->IsVisible(true),"Military retains its selected tab while diplomacy opens");
        auto* roleTab = military.militaryDocument()->GetElementById("military_tab_roles");
        roleTab->DispatchEvent("mouseover", Rml::Dictionary{}); update(*context);
        check(military.militaryDocument()->GetElementById("military_tooltip")->IsClassSet("is-visible")
            && inViewport(military.militaryDocument()->GetElementById("military_tooltip"), context->GetDimensions())
            && military.militaryDocument()->GetElementById("military_tooltip")->GetInnerRML().find("civilian behavior") != std::string::npos,
            "Military side-menu tooltip appears on hover");
        roleTab->DispatchEvent("mouseout", Rml::Dictionary{}); update(*context);
        check(!military.militaryDocument()->GetElementById("military_tooltip")->IsClassSet("is-visible"),
            "Military side-menu tooltip hides on pointer exit");
        check(military.activateElement("military_tab_priorities"), "Military side menu dispatches");
        check(controller.state().militaryView == View::Priorities && controller.state().diplomacyView == View::Neighbors,
            "Military side menu preserves independent Diplomacy view");
        check(diplomacy.activateElement("diplomacy_tab_missions"), "Diplomacy side menu dispatches");
        check(controller.state().militaryView == View::Priorities && controller.state().diplomacyView == View::Missions,
            "Diplomacy side menu preserves independent Military view");
        diplomacy.closeDiplomacy(); update(*context);
        check(military.militaryDocument()->IsVisible() && !diplomacy.diplomacyDocument()->IsVisible(),"Closing diplomacy preserves military");
        controller.removeViewPort(military); controller.removeViewPort(diplomacy);
        military.shutdown(); diplomacy.shutdown(); update(*context);
    }
    float buildCloseWidth = 0.f;
    float buildCloseHeight = 0.f;
    {
        Port port; Screen screen; Management6CController controller(port, screen);
        Management6CRmlBinding binding(*context);
        check(binding.initialize(controller), "Load actual screen documents");
        controller.addViewPort(binding); controller.beginWorld(WorldEpoch{1});
        MilitaryRoleRow role; role.id = MilitaryRoleId{71}; role.name = "Shieldbearer";
        for (auto slot : {UniformSlot::HeadArmor, UniformSlot::ChestArmor, UniformSlot::ArmArmor,
            UniformSlot::HandArmor, UniformSlot::LegArmor, UniformSlot::FootArmor,
            UniformSlot::LeftHandHeld, UniformSlot::RightHandHeld, UniformSlot::Back})
            role.uniform.push_back({slot, "slot", CatalogId{"none"}, CatalogId{"any"},
                {CatalogId{"none"}, CatalogId{"PlateArmor"}}, {CatalogId{"any"}, CatalogId{"Iron"}}});
        MilitaryRoster roster;
        SquadRow squad; squad.id = SquadId{10}; squad.name = "Guard";
        squad.members = {{CreatureId{501}, "Mira", MilitaryRoleId{71}}};
        for (int i = 0; i < 13; ++i) squad.priorities.push_back({CatalogId{"target" + std::to_string(i)}, "Target " + std::to_string(i), MilitaryAttitude::Defend});
        roster.squads.push_back(squad); roster.unassigned.push_back({CreatureId{502}, "Nia", {}});
        controller.applyMilitary({WorldEpoch{1}, Revision{1}, roster});
        auto scout = role; scout.id = MilitaryRoleId{72}; scout.name = "Scout";
        controller.applyRoles({WorldEpoch{1}, Revision{1}, {role, scout}});
        NeighborRow neighbor; neighbor.id = NeighborId{22}; neighbor.discovered = true;
        neighbor.name = "The Shrieked Land, a gnome kingdom"; neighbor.distance = "About four days";
        neighbor.attitude = "Very friendly"; neighbor.wealth = "Rich"; neighbor.economy = "Animal breeding";
        neighbor.military = "Very weak"; neighbor.canSendEmissary = true;
        controller.applyNeighbors({WorldEpoch{1}, Revision{1}, {neighbor}});
        controller.applyAvailableGnomes({WorldEpoch{1}, Revision{1}, {{CreatureId{501}, "Mira"}, {CreatureId{502}, "Nia"}}});
        MissionRow mission; mission.id = MissionId{31}; mission.type = MissionType::Emissary; mission.action = MissionAction::Improve;
        mission.step = MissionStep::Returned; mission.target = NeighborId{22}; mission.participants = {CreatureId{501}};
        mission.result = {true, 72}; controller.applyMissions({WorldEpoch{1}, Revision{1}, {mission}});
        const std::array<const char*, 5> names{"squads", "roles", "priorities", "neighbors", "missions"};
        const std::array<const char*, 5> rows{"military_squad_10", "military_role_71", "military_squad_10", "diplomacy_neighbor_22", "diplomacy_mission_31"};
        for (auto size : {Rml::Vector2i{1120,760}, Rml::Vector2i{960,640}, Rml::Vector2i{720,460}})
        for (float scale : {1.f,1.25f,1.5f,2.f}) {
            context->SetDimensions(size); context->SetDensityIndependentPixelRatio(scale);
            for (int i = 0; i < 5; ++i) {
                controller.open(static_cast<View>(i)); update(*context);
                const auto prefix = i < 3 ? "military" : "diplomacy";
                auto* doc = i < 3 ? binding.militaryDocument() : binding.diplomacyDocument();
                if (size.x / scale > 760) {
                    check(doc->GetElementById(std::string(prefix) + "_rail")->GetOffsetWidth() >= 100,
                        "Military and Diplomacy side rails retain readable width");
                    for (int tab = i < 3 ? 0 : 3; tab < (i < 3 ? 3 : 5); ++tab)
                        check(inViewport(doc->GetElementById(std::string(prefix) + "_tab_" + names[tab]), size),
                            std::string("Side menu clipped: ") + names[tab] + " at " + std::to_string(size.x) + " scale " + std::to_string(scale));
                } else {
                    check(inViewport(doc->GetElementById(std::string(prefix) + "_views_toggle"), size),
                        std::string("Compact Views control clipped: ") + names[i]);
                }
                check(inViewport(doc->GetElementById(std::string(prefix) + "_close"), size), "Close clipped");
                if (size.x / scale <= 760) {
                    click(*context, doc->GetElementById(rows[i]));
                    auto* back = doc->GetElementById(std::string(prefix) + "_back");
                    if (!inViewport(back, size)) {
                        const auto pos = back->GetAbsoluteOffset();
                        auto* selected = doc->GetElementById(rows[i]);
                        const auto hitpos = selected->GetAbsoluteOffset();
                        auto* hit = context->GetElementAtPoint({hitpos.x + selected->GetOffsetWidth()/2, hitpos.y + selected->GetOffsetHeight()/2});
                        for (auto id : {"military_tabs", "military_main", "military_primary_toolbar", "military_squad_rows", "military_squad_10", "squad_add"}) {
                            auto* element=doc->GetElementById(id); if(!element) continue; const auto origin=element->GetAbsoluteOffset();
                            std::cerr << id << "=" << origin.x << ',' << origin.y << ' ' << element->GetOffsetWidth() << 'x' << element->GetOffsetHeight() << '\n';
                        }
                        std::cerr << "compact view=" << i << " size=" << size.x << ',' << size.y << " scale=" << scale
                            << " back=" << back->IsVisible(true) << ',' << pos.x << ',' << pos.y << ',' << back->GetOffsetWidth() << ',' << back->GetOffsetHeight()
                            << " body=" << doc->GetElementById(std::string(prefix)+"_body")->GetClassNames() << " hit=" << (hit ? hit->GetId() : "none") << '\n';
                    }
                    check(inViewport(back, size), "Compact detail exposes reachable Back");
                    const std::array<const char*,5> panes{"military_squad_detail", "military_role_detail", "military_priority_detail", "diplomacy_neighbor_detail", "diplomacy_mission_detail"};
                    const std::array<const char*,5> endings{"military_unassigned_502", "military_uniform_Back", "military_priority_target12", "mission_start", "mission_timing_field"};
                    auto* pane = doc->GetElementById(panes[i]);
                    auto* ending = doc->GetElementById(endings[i]);
                    check(pane && ending, "Compact scroll targets exist");
                    auto origin = pane->GetAbsoluteOffset(Rml::BoxArea::Border);
                    context->ProcessMouseMove(static_cast<int>(origin.x + pane->GetOffsetWidth() - 20 * scale),
                        static_cast<int>(origin.y + pane->GetOffsetHeight()/2), 0);
                    for (int step=0; step<30 && !inViewport(ending,size); ++step) {
                        context->ProcessMouseWheel(3.f,0); update(*context);
                    }
                    check(inViewport(ending,size), std::string("Wheel reaches end of compact detail: ") + names[i]
                        + " at " + std::to_string(size.x) + " scale " + std::to_string(scale));
                    click(*context, back);
                    if(!inViewport(doc->GetElementById(rows[i]),size)) {
                        auto* row=doc->GetElementById(rows[i]); auto pos=row->GetAbsoluteOffset();
                        std::cerr << "back view=" << i << " size=" << size.x << ',' << size.y << " scale=" << scale << " row=" << row->IsVisible(true) << ',' << pos.x << ',' << pos.y << ',' << row->GetOffsetWidth() << ',' << row->GetOffsetHeight() << " body=" << doc->GetElementById(std::string(prefix)+"_body")->GetClassNames() << '\n';
                    }
                    check(inViewport(doc->GetElementById(rows[i]), size), "Back restores a reachable selected row");
                }
            }
        }
        context->SetDimensions({960,640}); context->SetDensityIndependentPixelRatio(1.f);
        controller.open(View::Roles); update(*context);
        auto* military = binding.militaryDocument();
        for (auto name : {"HeadArmor","ChestArmor","ArmArmor","HandArmor","LegArmor","FootArmor","LeftHandHeld","RightHandHeld","Back"})
            check(inViewport(military->GetElementById(std::string("military_uniform_") + name), {960,640}), "All nine uniform slots fit at default size");
        click(*context, military->GetElementById("military_uniform_Back"));
        check(controller.state().selectedUniformSlot == UniformSlot::Back, "Pointer hit testing selects a uniform slot");
        click(*context, military->GetElementById("military_uniform_type_choice"));
        context->ProcessKeyDown(Rml::Input::KI_DOWN,0); context->ProcessKeyUp(Rml::Input::KI_DOWN,0); update(*context);
        context->ProcessKeyDown(Rml::Input::KI_RETURN,0); context->ProcessKeyUp(Rml::Input::KI_RETURN,0); update(*context);
        check(port.sent.back().id.value == "military.set_uniform_slot", "Native select keyboard change dispatches equipment action");
        check(std::get<SetUniformSlotPayload>(port.sent.back().payload).slot == UniformSlot::Back, "Equipment change targets selected slot");
        controller.open(View::Squads); controller.selectMember(CreatureId{501}); update(*context);
        check(!military->GetElementById("member_assign_squad")->IsVisible(true), "Hide assignment into the citizen's current squad");
        controller.selectMember(CreatureId{502}); update(*context);
        check(military->GetElementById("member_assign_squad")->IsVisible(true)
            && !military->GetElementById("member_remove")->IsVisible(true), "Unassigned citizen shows the applicable squad action");
        check(military->GetElementById("military_member_501")->GetChild(1)->GetOffsetWidth() > 40,
            "Roster role names have readable width");
        auto* choice = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>(military->GetElementById("military_member_role_choice"));
        check(choice != nullptr, "Roster has a native role selector"); choice->SetValue("72"); update(*context);
        check(port.sent.back().id.value == "military.assign_role" && std::get<AssignRolePayload>(port.sent.back().payload).creature == CreatureId{502}, "Roster selector targets chosen citizen");
        controller.open(View::Priorities); update(*context);
        click(*context, military->GetElementById("military_priority_target5"));
        click(*context, military->GetElementById("attitude_attack"));
        check(port.sent.back().id.value == "military.set_attitude"
            && std::get<SetAttitudePayload>(port.sent.back().payload).targetType == CatalogId{"target5"},
            "Priority editor action applies to the pointer-selected target");
        controller.open(View::Missions); update(*context);
        auto* diplomacy = binding.diplomacyDocument();
        check(diplomacy->GetElementById("mission_participants")->GetInnerRML().find("Mira") != std::string::npos, "Mission uses a citizen name");
        check(diplomacy->GetElementById("mission_timing")->GetInnerRML().find("72 hours total") != std::string::npos, "Returned mission uses completion time");
        auto activeMission = mission; activeMission.step = MissionStep::Travel; activeMission.elapsedHours = 9;
        controller.applyMissions({WorldEpoch{1}, Revision{2}, {activeMission}}); update(*context);
        check(diplomacy->GetElementById("mission_timing")->GetInnerRML().find("Elapsed 9 hours") != std::string::npos,
            "Active mission shows elapsed rather than completed time");
        controller.applyMissions({WorldEpoch{1}, Revision{3}, {}}); update(*context);
        check(diplomacy->GetElementById("diplomacy_empty")->GetOffsetHeight() < 180,
            "Empty missions stays a compact panel");
        click(*context, diplomacy->GetElementById("diplomacy_empty_neighbors"));
        check(controller.state().view == View::Neighbors, "Empty missions leads to Neighbors through pointer input");
        context->SetDimensions({720,460}); context->SetDensityIndependentPixelRatio(2.f);
        controller.open(View::Roles); update(*context);
        military->GetElementById("military_tab_squads")->SetInnerRML("Squads and all assigned citizens in the kingdom");
        military->GetElementById("military_tab_roles")->SetInnerRML("Roles and uniforms with legal materials for all equipment slots");
        military->GetElementById("military_tab_priorities")->SetInnerRML("Target priorities and responses to every encountered threat");
        check(binding.activateElement("military_views_toggle"), "Long-label rail opens"); update(*context);
        auto* longRail = military->GetElementById("military_rail");
        longRail->SetScrollTop(10000.f); update(*context);
        check(inside(military->GetElementById("military_tab_priorities"), longRail),
            "Long localized side-menu label remains reachable by rail scroll at 200% scale");
        controller.removeViewPort(binding); binding.shutdown();
    }
    {
        context->SetDimensions({360, 900}); context->SetDensityIndependentPixelRatio(1.f);
        auto* hud = context->LoadDocument("screens/game_hud.rml");
        check(hud != nullptr, "Load actual HUD document"); hud->Show();
        auto* shelf = hud->GetElementById("hud_tool_shelf");
        check(shelf != nullptr, "HUD sidebar exists");
        shelf->SetProperty("transform", "none"); update(*context);
        const float rootHeight = shelf->GetOffsetHeight();
        check(shelf->GetOffsetWidth() >= 160 && rootHeight >= 400,
            "Root sidebar background and hit box wrap the visible menu");
        auto shelfTop = shelf->GetAbsoluteOffset(Rml::BoxArea::Border).y;
        check(std::abs(shelfTop + rootHeight / 2.f - 450.f) <= 2.f,
            "Root sidebar is vertically centered");
        auto hitTarget = [&](const char* id) {
            auto* target = hud->GetElementById(id); check(target && target->IsVisible(true), std::string("Visible HUD target: ") + id);
            const auto p = target->GetAbsoluteOffset(Rml::BoxArea::Border);
            auto* hit = context->GetElementAtPoint({p.x + target->GetOffsetWidth()/2, p.y + target->GetOffsetHeight()/2});
            for (auto* element = hit; element; element = element->GetParentNode()) if (element == target) return true;
            return false;
        };
        check(hitTarget("hud_open_inventory") && hitTarget("hud_overlay_axles"),
            "Root sidebar buttons are pointer-hit-testable");
        hud->GetElementById("hud_sidebar_root")->SetClass("is-hidden", true);
        hud->GetElementById("hud_mine_menu")->SetClass("is-hidden", false); update(*context);
        const float mineHeight = shelf->GetOffsetHeight();
        check(mineHeight >= 250 && mineHeight < rootHeight,
            "Sidebar resizes from the root menu to the shorter Mine menu");
        shelfTop = shelf->GetAbsoluteOffset(Rml::BoxArea::Border).y;
        check(std::abs(shelfTop + mineHeight / 2.f - 450.f) <= 2.f,
            "Mine submenu is vertically centered");
        check(hitTarget("hud_mine_walls") && hitTarget("hud_mine_back"),
            "Mine actions and Back remain pointer-hit-testable");
        hud->Close(); update(*context);
    }
    {
        context->SetDimensions({600, 900}); context->SetDensityIndependentPixelRatio(1.f);
        HudPort port; HudScreen screen; hud::HudController controller(port, screen);
        hud::HudRmlBinding binding(*context);
        check(binding.initialize(controller), "Load bound HUD sidebar");
        controller.addViewPort(binding); controller.beginWorld(WorldEpoch{4});
        auto* hud = binding.document();
        auto* shelf = hud->GetElementById("hud_tool_shelf");
        auto* buildButton = hud->GetElementById("hud_tool_build");
        auto* deconstructButton = hud->GetElementById("hud_tool_deconstruct");
        auto* mineButton = hud->GetElementById("hud_tool_mine");
        auto* tooltip = hud->GetElementById("hud_sidebar_tooltip");
        check(shelf && buildButton && deconstructButton && mineButton && tooltip,
            "HUD sidebar exposes Build, Deconstruct, Mine, and its tooltip");
        shelf->SetProperty("transform", "none"); update(*context);
        const auto buildPosition = buildButton->GetAbsoluteOffset(Rml::BoxArea::Border);
        const float buildCenterY = buildPosition.y + buildButton->GetOffsetHeight() / 2.f;
        context->ProcessMouseMove(static_cast<int>(buildPosition.x + buildButton->GetOffsetWidth() / 2.f),
            static_cast<int>(buildCenterY), 0); update(*context);
        check(tooltip->IsClassSet("is-visible")
            && std::abs(tooltip->GetAbsoluteOffset(Rml::BoxArea::Border).y - buildCenterY) <= 2.f,
            "Sidebar tooltip is vertically centered on its hovered button");
        auto hoverTooltip = [&](const char* buttonId, const char* expected) {
            auto* button = hud->GetElementById(buttonId);
            check(button && button->IsVisible(true), std::string("Visible tooltip target: ") + buttonId);
            const auto position = button->GetAbsoluteOffset(Rml::BoxArea::Border);
            context->ProcessMouseMove(static_cast<int>(position.x + button->GetOffsetWidth() / 2.f),
                static_cast<int>(position.y + button->GetOffsetHeight() / 2.f), 0); update(*context);
            check(tooltip->IsClassSet("is-visible") && tooltip->GetInnerRML().find(expected) != std::string::npos,
                std::string("Tooltip describes ") + buttonId);
        };
        hoverTooltip("hud_tool_inspect", "Click a tile to inspect");
        for (const auto& [menuButton, actionButton, backButton, expected] : {
            std::tuple{"hud_tool_mine", "hud_mine_walls", "hud_mine_back", "Mark walls for mining"},
            std::tuple{"hud_tool_agriculture", "hud_tool_fell_tree", "hud_agriculture_back", "Mark trees to be cut down"},
            std::tuple{"hud_tool_designations", "hud_tool_stockpile", "hud_designations_back", "Mark an area for storing items"},
            std::tuple{"hud_tool_jobs", "hud_tool_suspend_job", "hud_jobs_back", "Pause a job without removing it"}
        }) {
            click(*context, hud->GetElementById(menuButton)); update(*context);
            hoverTooltip(actionButton, expected);
            click(*context, hud->GetElementById(backButton)); update(*context);
        }
        click(*context, deconstructButton);
        check(controller.state().tool.active && controller.state().tool.active->value == "deconstruct"
            && deconstructButton->IsClassSet("is-selected")
            && deconstructButton->GetAttribute<Rml::String>("aria-pressed", "") == "true",
            "Sidebar Deconstruct activates directly and shows its pressed state");
        controller.removeViewPort(binding); binding.shutdown(); update(*context);
    }    {
        context->SetDimensions({400, 720}); context->SetDensityIndependentPixelRatio(1.f);
        HudPort port; HudScreen screen; hud::HudController controller(port, screen);
        hud::HudRmlBinding binding(*context, hud::HudRmlBinding::Presentation::OrdersTools,
            hud::HudRmlBinding::ToolPanel::Build);
        check(binding.initialize(controller), "Load actual detached Build binding");
        controller.addViewPort(binding); controller.beginWorld(WorldEpoch{3});
        hud::BuildCatalogRow wood; wood.id = CatalogId{"CrudeWorkbench"}; wood.name = "Crude workbench"; wood.type = "Wood";
        hud::BuildCatalogRow stone; stone.id = CatalogId{"Stonecutter"}; stone.name = "Stonecutter"; stone.type = "Stone";
        hud::BuildCatalogRow furnace; furnace.id = CatalogId{"Furnace"}; furnace.name = "Furnace"; furnace.type = "Wood";
        furnace.kind = BuildKind::Workshop; furnace.available = false; furnace.unavailableReason = "Missing stone";
        hud::BuildCatalogRow blocked; blocked.id = CatalogId{"BlockedItem"}; blocked.name = "Blocked item"; blocked.type = "Wood";
        blocked.available = false; blocked.unavailableReason = "Missing block";
        controller.setBuildCatalog({wood, stone, furnace, blocked});
        check(binding.activateElement("hud_build_workshop"), "Activate Workshops category"); update(*context);
        auto* build = binding.document();
        auto* woodButton = build->GetElementById("hud_build_type_Wood");
        auto* stoneButton = build->GetElementById("hud_build_type_Stone");
        auto* catalog = build->GetElementById("hud_build_catalog");
        auto* buildClose = build->GetElementById("hud_build_close");
        check(buildClose != nullptr, "Build window exposes its shared Close button");
        buildCloseWidth = buildClose->GetOffsetWidth(); buildCloseHeight = buildClose->GetOffsetHeight();
        check(woodButton && stoneButton && catalog, "Build type rail creates pointer targets");
        check(woodButton->IsClassSet("is-selected") && catalog->GetInnerRML().find("Crude workbench") != std::string::npos,
            "Build type rail initially selects and renders Wood");
        click(*context, stoneButton);
        stoneButton = build->GetElementById("hud_build_type_Stone");
        check(stoneButton && stoneButton->IsClassSet("is-selected")
            && catalog->GetInnerRML().find("Stonecutter") != std::string::npos
            && catalog->GetInnerRML().find("Crude workbench") == std::string::npos,
            "Clicking Stone updates the selected type and catalog");
        click(*context, build->GetElementById("hud_build_type_Wood"));
        woodButton = build->GetElementById("hud_build_type_Wood");
        check(woodButton && woodButton->IsClassSet("is-selected")
            && catalog->GetInnerRML().find("Crude workbench") != std::string::npos
            && catalog->GetInnerRML().find("Stonecutter") == std::string::npos,
            "Generated type buttons remain clickable after a catalog refresh");
        auto* unavailableRow = build->GetElementById("hud_build_Furnace");
        auto* unavailableAction = build->GetElementById("hud_build_action_Build_Furnace");
        check(unavailableRow && unavailableAction
            && unavailableRow->GetAttribute<Rml::String>("aria-disabled", "") == "false"
            && unavailableAction->GetAttribute<Rml::String>("aria-disabled", "") == "false"
            && unavailableAction->IsClassSet("can-place-blueprint")
            && catalog->GetInnerRML().find("Place blueprint") != std::string::npos,
            "Unavailable workshops expose an enabled blueprint action");
        const auto dispatchesBeforeBlueprintClick = port.sent.size();
        click(*context, unavailableAction);
        check(port.sent.size() == dispatchesBeforeBlueprintClick + 1
            && std::get<ChooseBuildPayload>(port.sent.back().payload).item == CatalogId{"Furnace"},
            "Unavailable workshop blueprint dispatches placement");
        auto* blockedRow = build->GetElementById("hud_build_BlockedItem");
        auto* blockedAction = build->GetElementById("hud_build_action_Build_BlockedItem");
        check(blockedRow && blockedAction
            && blockedRow->GetAttribute<Rml::String>("aria-disabled", "") == "false"
            && blockedAction->GetAttribute<Rml::String>("aria-disabled", "") == "false"
            && blockedAction->IsClassSet("can-place-blueprint"),
            "Unavailable non-workshop pieces expose an enabled blueprint action");
        const auto dispatchesBeforeBlockedClick = port.sent.size();
        click(*context, blockedAction);
        check(port.sent.size() == dispatchesBeforeBlockedClick + 1
            && std::get<ChooseBuildPayload>(port.sent.back().payload).item == CatalogId{"BlockedItem"},
            "Unavailable non-workshop blueprint dispatches placement");
        controller.removeViewPort(binding); binding.shutdown(); update(*context);
    }    {
        context->SetDimensions({400, 700}); context->SetDensityIndependentPixelRatio(1.f);
        InventoryPort port; InventoryScreen screen; management6b::Management6BController controller(port, screen);
        management6b::Management6BRmlBinding binding(*context);
        check(binding.initialize(controller), "Load actual inventory binding");
        controller.addViewPort(binding); controller.beginWorld(WorldEpoch{2}); controller.openInventory(); update(*context);
        auto* inventory = binding.inventoryDocument();
        management6b::InventoryRow category;
        category.id = {CatalogId{"Drinks"}, {}, {}, {}, InventoryDepth::Category};
        category.name = "Drinks";
        management6b::InventoryRow group;
        group.id = {CatalogId{"Drinks"}, CatalogId{"alcoholic"}, {}, {}, InventoryDepth::Group};
        group.name = "Alcoholic";
        management6b::InventoryRow item;
        item.id = {CatalogId{"Drinks"}, CatalogId{"alcoholic"}, CatalogId{"beer"}, {}, InventoryDepth::Item};
        item.name = "Beer"; item.spriteSheet = "inventory_Carrot.tga";
        item.spriteWidth = item.spriteHeight = item.spriteSheetWidth = item.spriteSheetHeight = 40;
        management6b::InventoryRow material;
        material.id = {CatalogId{"Drinks"}, CatalogId{"alcoholic"}, CatalogId{"beer"}, CatalogId{"wheat"}, InventoryDepth::Material};
        material.name = "Wheat beer"; material.spriteSheet = "inventory_Carrot.tga";
        material.spriteWidth = material.spriteHeight = material.spriteSheetWidth = material.spriteSheetHeight = 40;
        check(controller.applyInventory({WorldEpoch{2}, Revision{1}, {category, group, item, material}}),
            "Apply hierarchical inventory fixture");
		auto* categoryToggle = inventory->GetElementById("inventory_filter_category_toggle");
		click(*context, categoryToggle);
		auto* categoryOption = inventory->GetElementById("inventory_filter_category_options_option_1");
		check(categoryOption && categoryOption->IsVisible(true), "Inventory category option opens for pointer input");
		click(*context, categoryOption);
		check(controller.state().inventoryColumnSelections[0].size() == 1
			&& controller.state().inventoryColumnSelections[0].front() == "Drinks",
			"Physical mouse press toggles an inventory multi-select option");
		controller.toggleInventoryColumnSelection(0, "");
		click(*context, categoryToggle);
        update(*context);
        auto* rows = inventory->GetElementById("inventory_rows");
		check(rows && rows->GetNumChildren() == 1 && rows->GetChild(0)->GetNumChildren() == 6,
			"Flat inventory renders one leaf row across six columns");
		auto* flatRow = rows->GetChild(0);
		check(flatRow->GetChild(0)->GetInnerRML().find("Drinks") != std::string::npos
			&& flatRow->GetChild(1)->GetInnerRML().find("Alcoholic") != std::string::npos
			&& flatRow->GetChild(2)->GetInnerRML().find("Beer") != std::string::npos
			&& flatRow->GetChild(3)->GetInnerRML().find("Wheat beer") != std::string::npos,
			"Flat inventory projects Category, Type, Item, and Material labels");
		check(flatRow->GetChild(2)->GetNumChildren() == 2,
			"Flat inventory Item cell retains its icon and label");
		auto* filter = rmlui_dynamic_cast<Rml::ElementFormControl*>(inventory->GetElementById("inventory_filter_category"));
		check(filter != nullptr, "Bound inventory column filter is a native form control");
		filter->Focus(); context->ProcessTextInput("D"); update(*context);
		check(filter->GetValue() == "D" && context->GetFocusElement() == filter,
			"Inventory column filter retains focus after live filtering");
		context->ProcessTextInput("r"); update(*context);
		check(filter->GetValue() == "Dr" && context->GetFocusElement() == filter,
			"Inventory column filter accepts consecutive characters");
        controller.setInventoryColumnFilter(0, "");
        context->SetDimensions({720, 720});
        std::vector<management6b::InventoryRow> many;
        for (int i = 0; i < 30; ++i) {
            const auto suffix = std::to_string(i);
            auto c = category; c.id.category.value += suffix; c.name += suffix;
            auto g = group; g.id.category = c.id.category; g.id.group.value += suffix; g.name += suffix;
            auto it = item; it.id.category = c.id.category; it.id.group = g.id.group; it.id.item.value += suffix; it.name += suffix;
            auto m = material; m.id.category = c.id.category; m.id.group = g.id.group; m.id.item = it.id.item; m.id.material.value += suffix; m.name += suffix;
            m.total = i; m.stockpiled = i;
            many.insert(many.end(), {c, g, it, m});
        }
        many.push_back(many.back());
        check(controller.applyInventory({WorldEpoch{2}, Revision{2}, many}), "Apply long dropdown fixture");
        check(controller.visibleInventory().size() == 30, "Repeated inventory identities render only once");
        for (const char* column : {"category", "group", "item", "material"}) {
            const std::string prefix = std::string("inventory_filter_") + column;
            click(*context, inventory->GetElementById(prefix + "_toggle"));
            auto* option = inventory->GetElementById(prefix + "_options_option_1");
            click(*context, option);
            check(option->GetAttribute<Rml::String>("aria-selected", "") == "true", "Long text dropdown option accepts a mouse click");
            click(*context, inventory->GetElementById(prefix + "_options_option_all"));
            click(*context, inventory->GetElementById(prefix + "_toggle"));
        }
        click(*context, inventory->GetElementById("inventory_filter_total_toggle"));
        check(inViewport(inventory->GetElementById("inventory_filter_total_options"), {720, 720}), "Rightmost quantity dropdown stays inside the window");
        click(*context, inventory->GetElementById("inventory_filter_total_options_option_1"));
        check(controller.visibleInventory().size() == 29, "Has quantity matches every positive value");
        click(*context, inventory->GetElementById("inventory_filter_total_options_option_2"));
        check(controller.visibleInventory().size() == 1 && controller.visibleInventory().front().total == 0, "None replaces Has and matches zero");
        click(*context, inventory->GetElementById("inventory_filter_total_options_option_all"));
        check(controller.visibleInventory().size() == 30, "All clears the quantity predicate");
        binding.shutdown(); update(*context);
    }
    {
        context->SetDimensions({720, 720}); context->SetDensityIndependentPixelRatio(1.f);
        StockpilePort port; StockpileScreen screen; management6a::Management6AController controller(port, screen);
        management6a::Management6ARmlBinding binding(*context);
        check(binding.initialize(controller), "Load stockpile binding");
        controller.addViewPort(binding); controller.beginWorld(WorldEpoch{3});
        management6a::StockpileSnapshot stockpile; stockpile.id = StockpileId{7}; stockpile.name = "Supplies"; stockpile.maxPriority = 3;
        for (int i = 0; i < 100; ++i) {
            const auto suffix = std::to_string(i);
            const CatalogId cat{"Category" + suffix}, group{"Type" + suffix}, item{"Item" + suffix}, mat{"Material" + suffix};
            stockpile.filters.push_back({{stockpile.id, cat, {}, {}, {}, FilterDepth::Category}, cat.value, management6a::TriState::Off});
            stockpile.filters.push_back({{stockpile.id, cat, group, {}, {}, FilterDepth::Group}, group.value, management6a::TriState::Off});
            stockpile.filters.push_back({{stockpile.id, cat, group, item, {}, FilterDepth::Item}, item.value, management6a::TriState::Off});
            stockpile.filters.push_back({{stockpile.id, cat, group, item, mat, FilterDepth::Material}, mat.value, management6a::TriState::Off});
            stockpile.contents.push_back({{cat, group, item, mat, FilterDepth::Material}, mat.value, static_cast<unsigned>(i), static_cast<unsigned>(i)});
        }
        stockpile.filters.push_back(stockpile.filters.back());
        stockpile.contents.push_back(stockpile.contents.back());
        controller.showStockpile(stockpile, Revision{1}, WorldPosition{1, 2, 3}); update(*context);
        check(controller.state().stockpile.visibleFilters.size() == 100 && controller.state().stockpile.visibleContents.size() == 100, "Repeated stockpile identities render only once in both lists");
        auto* document = binding.stockpileDocument();
        for (const bool allow : {false, true}) {
            click(*context, document->GetElementById(allow ? "stockpile_view_allow" : "stockpile_view_contents"));
            for (std::size_t column = 0; column < 4; ++column) {
                const char* names[] = {"category", "group", "item", "material"};
                const auto prefix = std::string(allow ? "stockpile_allow_filter_" : "stockpile_content_filter_") + names[column];
                click(*context, document->GetElementById(prefix + "_toggle"));
                click(*context, document->GetElementById(prefix + "_options_option_1"));
                check((allow ? controller.state().stockpile.allowColumnSelections[column] : controller.state().stockpile.contentColumnSelections[column]).size() == 1, "Stockpile long dropdown accepts mouse selection");
                click(*context, document->GetElementById(prefix + "_options_option_all"));
                click(*context, document->GetElementById(prefix + "_toggle"));
            }
        }
        check(!document->GetElementById("stockpile_filter_previous") && !document->GetElementById("stockpile_filter_next"), "Allow list has no pagination controls");
        auto* list = document->GetElementById("stockpile_filters");
        list->SetScrollTop(4800); update(*context);
        check(list->GetInnerRML().find("Material99") != std::string::npos, "Continuous Allow list scroll reaches its final rule");
        check(inViewport(document->GetElementById("stockpile_allow_bulk"), {720, 720}), "Bulk actions stay visible below scrolling list");
        auto* lastRule = list->GetChild(list->GetNumChildren() - 1);
        lastRule->Focus(); context->ProcessKeyDown(Rml::Input::KI_HOME, 0); context->ProcessKeyUp(Rml::Input::KI_HOME, 0); update(*context);
        check(controller.state().stockpile.selectedFilter == controller.state().stockpile.visibleFilters.front().id, "Home traverses the continuous Allow list");
        context->ProcessKeyDown(Rml::Input::KI_END, 0); context->ProcessKeyUp(Rml::Input::KI_END, 0); update(*context);
        check(controller.state().stockpile.selectedFilter == controller.state().stockpile.visibleFilters.back().id, "End traverses the continuous Allow list");
        click(*context, document->GetElementById("stockpile_view_settings"));
        auto* hauling = document->GetElementById("stockpile_toggle_pull");
        check(hauling && hauling->GetTagName() == "input" && hauling->GetAttribute<Rml::String>("type", "") == "checkbox", "Hauling uses a checkbox");
        click(*context, hauling);
        check(!port.sent.empty() && std::get<SetStockpileBasicsPayload>(port.sent.back().payload).pull != stockpile.pullFromOthers, "Hauling checkbox dispatches checked value");
        controller.removeViewPort(binding); binding.shutdown(); update(*context);
    }
    {
        context->SetDimensions({400, 700}); context->SetDensityIndependentPixelRatio(1.f);
        auto* inventory = context->LoadDocument("windows/inventory_browser.rml");
        check(inventory != nullptr, "Load actual inventory document"); inventory->Show();
        update(*context);
        auto* inventoryClose = inventory->GetElementById("inventory_close");
        check(inventoryClose && std::abs(inventoryClose->GetOffsetWidth() - buildCloseWidth) <= 1.f
            && std::abs(inventoryClose->GetOffsetHeight() - buildCloseHeight) <= 1.f,
            "Build and Inventory use the same Close button dimensions");
		auto* filter = rmlui_dynamic_cast<Rml::ElementFormControl*>(inventory->GetElementById("inventory_filter_item"));
		check(filter != nullptr, "Inventory Item filter is a native form control");
		filter->Focus(); context->ProcessTextInput("ax"); update(*context);
		check(filter->GetValue() == "ax", "Inventory Item filter accepts initial text");
        RmlUiQtInputAdapter qtInput(context);
        qtInput.keyDown(Qt::Key_Backspace, {}); qtInput.committedText(QString(QChar(0x08))); qtInput.keyUp(Qt::Key_Backspace, {}); update(*context);
		check(filter->GetValue() == "a", "Qt Backspace deletes without inserting its control glyph");
        context->ProcessTextInput("xe"); update(*context);
		check(filter->GetValue() == "axe", "Inventory filter accepts replacement text after deletion");
        qtInput.keyDown(Qt::Key_Left, {}); qtInput.keyUp(Qt::Key_Left, {});
        qtInput.keyDown(Qt::Key_Delete, {}); qtInput.committedText(QString(QChar(0x7f))); qtInput.keyUp(Qt::Key_Delete, {}); update(*context);
		check(filter->GetValue() == "ax", "Qt Delete removes text without inserting its control glyph");
        auto* columnHead = inventory->GetElementById("inventory_column_head");
        const auto columnRml = columnHead ? columnHead->GetInnerRML() : std::string{};
		check(columnHead && columnHead->GetNumChildren() == 6
			&& columnRml.find("Category") < columnRml.find("Type")
			&& columnRml.find("Type") < columnRml.find("Item")
			&& columnRml.find("Item") < columnRml.find("Material")
			&& columnRml.find("Material") < columnRml.find("In Stock")
			&& columnRml.find("In Stock") < columnRml.find("Total"),
			"Inventory exposes six filterable, sortable column headers");
		check(inventory->GetElementById("inventory_filter_category_toggle")
			&& inventory->GetElementById("inventory_filter_group_toggle")
			&& inventory->GetElementById("inventory_filter_item_toggle")
			&& inventory->GetElementById("inventory_filter_material_toggle")
			&& inventory->GetElementById("inventory_filter_stock_toggle")
			&& inventory->GetElementById("inventory_filter_total_toggle"),
			"Every inventory column exposes its combo filter toggle");
        check(inventory->GetElementById("inventory_history") == nullptr
            && inventory->GetElementById("inventory_page_status") == nullptr
            && inventory->GetElementById("inventory_toggle_watch") == nullptr,
            "Inventory history and paging footer are absent");
        inventory->Close(); update(*context);
    }
    {
        InventoryPort port; InventoryScreen screen; management6b::Management6BController controller(port, screen);
        management6b::Management6BRmlBinding binding(*context);
        check(binding.initialize(controller), "Load Population window");
        controller.addViewPort(binding); controller.beginWorld(WorldEpoch{91});
        check(binding.openPopulation(FocusToken{3}), "Open Population window");
        management6b::PopulationRow citizen{CreatureId{7}, "Ada", ProfessionId{"Miner"},
            {{CatalogId{"Mining"}, "Mining", "Industry", 4, 25.f, true}}};
        check(controller.applyPopulation({WorldEpoch{91}, Revision{1}, {citizen}}), "Population fixture loads");
        check(controller.applySkillCatalog(WorldEpoch{91}, {{CatalogId{"Mining"}, "Mining", "Industry"}}), "Skill catalog loads");
        check(controller.applyProfessions({WorldEpoch{91}, Revision{1},
            {{ProfessionId{"Gnomad"}, "Gnomad", {}}, {ProfessionId{"Miner"}, "Miner", {CatalogId{"Mining"}}}}}), "Profession fixture loads");
        management6b::ScheduleRow schedule; schedule.creature = CreatureId{7}; schedule.name = "Ada";
        check(controller.applySchedules({WorldEpoch{91}, Revision{1}, {schedule}}), "Schedule fixture loads");
        auto* population = binding.populationDocument();
        auto* skillsTab = population->GetElementById("population_tab_skills");
        skillsTab->DispatchEvent("focus", Rml::Dictionary{}); update(*context);
        check(population->GetElementById("population_tooltip")->IsClassSet("is-visible")
            && inViewport(population->GetElementById("population_tooltip"), context->GetDimensions())
            && population->GetElementById("population_tooltip")->GetInnerRML().find("Compare one skill") != std::string::npos,
            "Population side-menu tooltip appears on keyboard focus");
        skillsTab->DispatchEvent("blur", Rml::Dictionary{}); update(*context);
        check(!population->GetElementById("population_tooltip")->IsClassSet("is-visible"),
            "Population side-menu tooltip hides on blur");
        check(binding.activateElement("population_tab_skills") && controller.state().view == management6b::View::Skills,
            "Population side menu opens the focused Skills view");
        for (auto size : {Rml::Vector2i{960,640}, Rml::Vector2i{640,420}})
        for (float scale : {1.f, 1.25f, 1.5f, 2.f}) {
            context->SetDimensions(size); context->SetDensityIndependentPixelRatio(scale);
            for (auto view : {management6b::View::Citizens, management6b::View::Skills,
                    management6b::View::Professions, management6b::View::Schedules}) {
                controller.open(view); update(*context);
                check(inViewport(population->GetElementById("population_close"), size), "Population Close reachable at each scale");
                if (size.x / scale <= 760)
                    check(inViewport(population->GetElementById("population_views_toggle"), size), "Population compact Views reachable");
                else {
                    check(population->GetElementById("population_rail")->GetOffsetWidth() >= 100,
                        "Population side rail retains its readable width");
                    check(inViewport(population->GetElementById("population_tab_schedules"), size), "Population side rail reachable");
                }
                if (view == management6b::View::Schedules) {
                    auto* grid = population->GetElementById("schedule_rows");
                    auto* scheduleSection = population->GetElementById("population_schedules");
                    scheduleSection->SetScrollTop(10000.f); update(*context);
                    grid->SetScrollLeft(10000.f); update(*context);
                    const auto sectionEnd = scheduleSection->GetAbsoluteOffset(Rml::BoxArea::Border).y + scheduleSection->GetOffsetHeight();
                    const auto gridStart = grid->GetAbsoluteOffset(Rml::BoxArea::Border).y;
                    check(grid->GetOffsetWidth() >= 100 && grid->GetOffsetHeight() >= 30 && gridStart + 30 <= sectionEnd + 1,
                        "Schedule grid has a visible usable area at " + std::to_string(size.x) + "x" + std::to_string(size.y) + " scale " + std::to_string(scale));
                    check(inViewport(population->GetElementById("schedule_7_23"), size),
                        "Hour 23 remains reachable by horizontal scroll at " + std::to_string(size.x) + "x" + std::to_string(size.y) + " scale " + std::to_string(scale));
                    check(inside(population->GetElementById("schedule_7_23"), grid),
                        "Hour 23 is inside the schedule scrollport");
                    auto* finalAction = population->GetElementById("schedule_apply_column");
                    auto* actions = finalAction->GetParentNode();
                    actions->SetScrollLeft(10000.f); update(*context);
                    check(inside(finalAction, actions), "Final schedule action is reachable in its toolbar at " + std::to_string(size.x) + "x" + std::to_string(size.y) + " scale " + std::to_string(scale));
                }
                if (view == management6b::View::Professions) {
                    auto* detail = population->GetElementById("population_professions")->QuerySelector(".l-population-detail-pane");
                    check(detail != nullptr, "Profession detail scroll region exists");
                    detail->SetScrollTop(10000.f); update(*context);
                    check(inViewport(population->GetElementById("profession_delete"), size),
                        "Final profession action remains reachable at " + std::to_string(size.x) + "x" + std::to_string(size.y) + " scale " + std::to_string(scale));
                }
            }
        }
        context->SetDimensions({640,420}); context->SetDensityIndependentPixelRatio(2.f);
        controller.open(management6b::View::Citizens); update(*context);
        check(binding.activateElement("population_views_toggle")
            && population->GetElementById("population_shell")->IsClassSet("is-rail-open"),
            "Compact Views control opens the side rail");
        check(binding.activateElement("population_tab_professions")
            && controller.state().view == management6b::View::Professions
            && !population->GetElementById("population_shell")->IsClassSet("is-rail-open"),
            "Compact side rail selects a view and closes over content");
        context->SetDimensions({960,640}); context->SetDensityIndependentPixelRatio(1.f);
        controller.open(management6b::View::Skills); update(*context);
        check(population->GetElementById("skill_citizen_rows")->GetInnerRML().find("XP 25") != std::string::npos,
            "Focused skill shows citizen level and XP");
        click(*context, population->GetElementById("skill_enable_all"));
        check(!port.sent.empty() && port.sent.back().id.value == "population.set_skill_for_all", "Focused skill bulk action dispatches");
        controller.open(management6b::View::Schedules); controller.selectScheduleCell({CreatureId{7}, 23}); update(*context);
        click(*context, population->GetElementById("schedule_apply_column"));
        check(port.sent.back().id.value == "population.set_schedule_column", "Schedule column action dispatches");
        controller.open(management6b::View::Professions); controller.selectProfession(ProfessionId{"Miner"}); update(*context);
        check(population->GetElementById("profession_selected_skills")->GetInnerRML().find("Mining") != std::string::npos,
            "Profession editor retains assigned skill");
        check(controller.applyPopulation({WorldEpoch{91}, Revision{2}, {}}), "Empty Population refresh loads");
        controller.open(management6b::View::Skills); update(*context);
        check(population->GetElementById("skill_enable_all")->HasAttribute("disabled")
            && population->GetElementById("skill_disable_all")->HasAttribute("disabled"),
            "Skill bulk actions are unavailable without citizens");
        check(population->GetElementById("skill_citizen_rows")->GetInnerRML().find("No citizens") != std::string::npos,
            "Focused skill explains an empty citizen roster");
        context->SetDimensions({640,420}); context->SetDensityIndependentPixelRatio(2.f);
        controller.open(management6b::View::Professions); update(*context);
        for (const auto& [id, label] : {std::pair{"population_tab_citizens", "Citizens and all their daily needs and equipment"},
                {"population_tab_skills", "Skills and each citizen's experience and active status"},
                {"population_tab_professions", "Professions with every ordered skill assignment"},
                {"population_tab_schedules", "Schedules for every citizen and each of the 24 hours"}})
            population->GetElementById(id)->SetInnerRML(label);
        check(binding.activateElement("population_views_toggle"), "Population long-label rail opens"); update(*context);
        auto* longRail = population->GetElementById("population_rail");
        longRail->SetScrollTop(10000.f); update(*context);
        check(inside(population->GetElementById("population_tab_schedules"), longRail),
            "Long localized Population menu label remains reachable at 200% scale");
        controller.removeViewPort(binding); binding.shutdown(); update(*context);
    }
    Rml::RemoveContext("management-layout-tests"); Rml::Shutdown();
    std::cout << "RmlUi layout matrix, pointer and wheel navigation, native choice input, and mission presentation passed\n";
}
