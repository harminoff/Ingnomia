/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "gui/ui/screens/management6b/Management6BRmlBinding.h"
#include "gui/ui/screens/management6c/Management6CRmlBinding.h"
#include "gui/ui/screens/hud/HudRmlBinding.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>

using namespace ingnomia::ui;
using namespace ingnomia::ui::management6c;
namespace {
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
    management6b::CommandResult dispatch(const UiActionEnvelope&) override { return {}; }
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
void click(Rml::Context& context, Rml::Element* e) {
    check(e && e->IsVisible(true), "Click target must be visible");
    auto p = e->GetAbsoluteOffset(Rml::BoxArea::Border);
    context.ProcessMouseMove(static_cast<int>(p.x + e->GetOffsetWidth() / 2), static_cast<int>(p.y + e->GetOffsetHeight() / 2), 0);
    context.ProcessMouseButtonDown(0, 0); context.ProcessMouseButtonUp(0, 0); update(context);
}
}
int main(int argc, char** argv) {
    check(argc == 2, "Supply the actual content/rmlui directory");
    Files files; files.root = std::filesystem::absolute(argv[1]);
    Renderer renderer; Rml::SystemInterface system;
    Rml::SetFileInterface(&files); Rml::SetRenderInterface(&renderer); Rml::SetSystemInterface(&system);
    check(Rml::Initialise(), "RmlUi initializes");
    check(Rml::LoadFontFace("fonts/LatoLatin-Regular.ttf"), "Load the packaged font");
    auto* context = Rml::CreateContext("management-layout-tests", {1120, 760});
    context->SetDefaultScrollBehavior(Rml::ScrollBehavior::Instant, 1.f);
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
                for (auto name : names) check(inViewport(doc->GetElementById(std::string(prefix) + "_tab_" + name), size),
                    std::string("Tab clipped: ") + name + " at " + std::to_string(size.x) + " scale " + std::to_string(scale));
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
        controller.applyMissions({WorldEpoch{1}, Revision{2}, {}}); update(*context);
        check(diplomacy->GetElementById("diplomacy_empty")->GetOffsetHeight() < 180,
            "Empty missions stays a compact panel");
        click(*context, diplomacy->GetElementById("diplomacy_empty_neighbors"));
        check(controller.state().view == View::Neighbors, "Empty missions leads to Neighbors through pointer input");
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
            && blockedRow->GetAttribute<Rml::String>("aria-disabled", "") == "true"
            && blockedAction->GetAttribute<Rml::String>("aria-disabled", "") == "true",
            "Unavailable non-workshop pieces remain disabled");
        const auto dispatchesBeforeBlockedClick = port.sent.size();
        click(*context, blockedAction);
        check(port.sent.size() == dispatchesBeforeBlockedClick,
            "Unavailable non-workshop Build actions ignore pointer input");
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
        controller.toggleInventoryExpanded(category.id);
        controller.toggleInventoryExpanded(group.id);
        update(*context);
        auto* rows = inventory->GetElementById("inventory_rows");
        check(rows && rows->GetNumChildren() == 3
            && rows->GetChild(2)->GetAttribute<Rml::String>("aria-expanded", "") == "false",
            "Expanded group renders its item parent collapsed");
        controller.toggleInventoryExpanded(item.id);
        update(*context);
        check(rows->GetNumChildren() == 4
            && rows->GetChild(2)->GetAttribute<Rml::String>("aria-expanded", "") == "true",
            "Expanded item parent renders its material child");
        auto* itemRow = rows->GetChild(2);
        auto* materialRow = rows->GetChild(3);
        auto* itemName = itemRow->GetChild(0);
        auto* materialName = materialRow->GetChild(0);
        auto* itemIcon = itemName->GetChild(0);
        auto* itemContent = itemName->GetChild(1);
        auto* materialIcon = materialName->GetChild(0);
        const float itemIconX = itemIcon->GetAbsoluteOffset(Rml::BoxArea::Border).x;
        const float materialIconX = materialIcon->GetAbsoluteOffset(Rml::BoxArea::Border).x;
        check(itemIconX >= itemRow->GetAbsoluteOffset(Rml::BoxArea::Border).x + 16.f
            && materialIconX > itemIconX
            && itemContent->GetAbsoluteOffset(Rml::BoxArea::Border).x > itemIconX,
            "Inventory item icon and name share the depth-indented Item cell");
        auto* search = rmlui_dynamic_cast<Rml::ElementFormControl*>(inventory->GetElementById("inventory_search"));
        check(search != nullptr, "Bound inventory search is a native form control");
        search->Focus(); context->ProcessTextInput("a"); update(*context);
        check(search->GetValue() == "a" && context->GetFocusElement() == search,
            "Inventory search retains focus after its first live-filter refresh");
        context->ProcessTextInput("x"); update(*context);
        check(search->GetValue() == "ax" && context->GetFocusElement() == search,
            "Inventory search accepts the next character without another click");
        controller.setInventoryFilter(""); controller.setInventoryOwnedOnly(true); update(*context);
        check(!inventory->GetElementById("inventory_workbench")->IsClassSet("is-filtered"),
            "Owned-only inventory keeps hierarchical indentation styling");
        binding.shutdown(); update(*context);
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
        auto* search = rmlui_dynamic_cast<Rml::ElementFormControl*>(inventory->GetElementById("inventory_search"));
        check(search != nullptr, "Inventory search is a native form control");
        search->Focus(); context->ProcessTextInput("ax"); update(*context);
        check(search->GetValue() == "ax", "Inventory search accepts initial text");
        context->ProcessKeyDown(Rml::Input::KI_BACK, 0); context->ProcessKeyUp(Rml::Input::KI_BACK, 0); update(*context);
        check(search->GetValue() == "a", "Inventory search accepts Backspace deletion");
        context->ProcessTextInput("xe"); update(*context);
        check(search->GetValue() == "axe", "Inventory search accepts replacement text after deletion");
        auto* tabs = inventory->GetElementById("inventory_category_tabs");
        auto* filterHeading = inventory->GetElementById("inventory_filters_heading");
        auto* ownedFilter = inventory->GetElementById("inventory_filter_owned");
        auto* sortName = inventory->GetElementById("inventory_sort_name");
        auto* sortStock = inventory->GetElementById("inventory_sort_stock");
        auto* sortTotal = inventory->GetElementById("inventory_sort_total");
        auto* categoryHeading = inventory->GetElementById("inventory_categories_heading");
        auto* rail = inventory->GetElementById("inventory_filters_heading")->GetParentNode()->GetParentNode();
        check(tabs && filterHeading && ownedFilter && sortName && sortStock && sortTotal && categoryHeading && rail,
            "Inventory split navigation rail includes filters and all three sort choices");
        tabs->SetInnerRML("<button id='inventory_category_all' class='c-tabs__tab l-m6b-tab'>All</button><button id='inventory_category_materials' class='c-tabs__tab l-m6b-tab is-selected'>Materials</button><button class='c-tabs__tab l-m6b-tab'>Grown</button><button class='c-tabs__tab l-m6b-tab'>Workshop</button><button class='c-tabs__tab l-m6b-tab'>Food</button><button class='c-tabs__tab l-m6b-tab'>Drinks</button><button class='c-tabs__tab l-m6b-tab'>Containers</button><button class='c-tabs__tab l-m6b-tab'>Furniture</button><button class='c-tabs__tab l-m6b-tab'>Cloth</button><button id='inventory_category_armor' class='c-tabs__tab l-m6b-tab'>Armor</button>");
        update(*context);
        auto* all = inventory->GetElementById("inventory_category_all");
        auto* materials = inventory->GetElementById("inventory_category_materials");
        check(filterHeading->GetAbsoluteOffset(Rml::BoxArea::Border).y
                < ownedFilter->GetAbsoluteOffset(Rml::BoxArea::Border).y
            && ownedFilter->GetAbsoluteOffset(Rml::BoxArea::Border).y
                < sortName->GetAbsoluteOffset(Rml::BoxArea::Border).y
            && sortName->GetAbsoluteOffset(Rml::BoxArea::Border).y
                < sortStock->GetAbsoluteOffset(Rml::BoxArea::Border).y
            && sortStock->GetAbsoluteOffset(Rml::BoxArea::Border).y
                < sortTotal->GetAbsoluteOffset(Rml::BoxArea::Border).y
            && sortTotal->GetAbsoluteOffset(Rml::BoxArea::Border).y
                < categoryHeading->GetAbsoluteOffset(Rml::BoxArea::Border).y
            && categoryHeading->GetAbsoluteOffset(Rml::BoxArea::Border).y
                < all->GetAbsoluteOffset(Rml::BoxArea::Border).y,
            "Filters and Categories are labeled and stacked in the left rail");
        check(all && materials
            && std::abs(materials->GetAbsoluteOffset(Rml::BoxArea::Border).x - all->GetAbsoluteOffset(Rml::BoxArea::Border).x) <= 2.f
            && materials->GetAbsoluteOffset(Rml::BoxArea::Border).y > all->GetAbsoluteOffset(Rml::BoxArea::Border).y,
            "Inventory categories render as one vertical property-tab column");
        check(all->GetAbsoluteOffset(Rml::BoxArea::Border).x
                + all->GetOffsetWidth()
                < rail->GetAbsoluteOffset(Rml::BoxArea::Border).x + rail->GetOffsetWidth(),
            "Inventory category tabs retain a visible right edge inside the rail");
        auto* armor = inventory->GetElementById("inventory_category_armor");
        check(armor && armor->GetAbsoluteOffset(Rml::BoxArea::Border).y + armor->GetOffsetHeight()
            <= tabs->GetAbsoluteOffset(Rml::BoxArea::Border).y + tabs->GetOffsetHeight(),
            "Narrow inventory window exposes the complete vertical category list");
        auto* columnHead = inventory->GetElementById("inventory_column_head");
        const auto columnRml = columnHead ? columnHead->GetInnerRML() : std::string{};
        check(columnHead && columnHead->GetNumChildren() == 3
            && columnRml.find("Item") < columnRml.find("In Stock")
            && columnRml.find("In Stock") < columnRml.find("Total"),
            "Inventory list shows only Item, In Stock, and Total columns");
        check(search->GetOffsetWidth() <= 260.f
            && search->GetAbsoluteOffset(Rml::BoxArea::Border).x
                > tabs->GetAbsoluteOffset(Rml::BoxArea::Border).x + tabs->GetOffsetWidth(),
            "Search stays above the item list in the compact content pane");
        check(inventory->GetElementById("inventory_history") == nullptr
            && inventory->GetElementById("inventory_page_status") == nullptr
            && inventory->GetElementById("inventory_toggle_watch") == nullptr,
            "Inventory history and paging footer are absent");
        inventory->Close(); update(*context);
    }
    Rml::RemoveContext("management-layout-tests"); Rml::Shutdown();
    std::cout << "RmlUi layout matrix, pointer and wheel navigation, native choice input, and mission presentation passed\n";
}
