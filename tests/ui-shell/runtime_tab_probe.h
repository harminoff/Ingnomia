#pragma once

#include <RmlUi/Core.h>
#include <array>
#include <string>

// Opt-in production-document probe. Events enter the real Rml context; this
// does not claim physical Qt input or simulation command coverage.
inline std::string verifyNewGameTabs( Rml::ElementDocument& document )
{
    auto& context = *document.GetContext();
    constexpr std::array names{ "world", "settlement", "terrain", "review" };
    std::array<Rml::Element*, 4> tabs{}, panels{};
    for( int i = 0; i < 4; ++i )
    {
        tabs[i] = document.GetElementById( Rml::String("new-tab-") + names[i] );
        panels[i] = document.GetElementById( Rml::String("new-panel-") + names[i] );
        if( !tabs[i] || !panels[i] ) return "FAIL missing tab or panel";
    }
    std::string failures;
    int checks = 0;
    const auto check = [&]( bool passed, const char* label ) {
        ++checks;
        if( !passed ) failures += std::string(label) + ";";
    };
    const auto state = [&]( int selected ) {
        context.Update();
        for( int i = 0; i < 4; ++i )
        {
            check( tabs[i]->IsClassSet("is-selected") == (i == selected), "selection" );
            check( tabs[i]->GetAttribute<Rml::String>("aria-selected", "") == (i == selected ? "true" : "false"), "association" );
            check( (tabs[i]->GetComputedValues().tab_index() == Rml::Style::TabIndex::Auto) == (i == selected), "roving focus" );
            check( panels[i]->IsVisible() == (i == selected), "page visibility" );
        }
    };
    const auto key = [&]( Rml::Input::KeyIdentifier value, int modifiers = 0 ) {
        const bool consumed = !context.ProcessKeyDown(value, modifiers);
        context.ProcessKeyUp(value, modifiers);
        context.Update();
        return consumed;
    };
    tabs[0]->Click();
    tabs[0]->Focus();
    state(0);
    check(key(Rml::Input::KI_DOWN), "Down consumed");
    state(1);
    check(context.GetFocusElement() == tabs[1], "Down focus");
    key(Rml::Input::KI_END);
    state(3);
    key(Rml::Input::KI_DOWN);
    state(0);
    key(Rml::Input::KI_UP);
    state(3);
    key(Rml::Input::KI_HOME);
    state(0);
    tabs[1]->SetProperty("display", "none");
    tabs[2]->SetAttribute("disabled", true);
    context.Update();
    key(Rml::Input::KI_DOWN);
    state(3);
    tabs[2]->Click();
    state(3);
    tabs[1]->RemoveProperty("display");
    tabs[2]->RemoveAttribute("disabled");
    context.Update();
    key(Rml::Input::KI_HOME);
    state(0);
    key(Rml::Input::KI_DOWN);
    state(1);
    // The next Tab stop must enter the visible page, skipping the other tabs.
    key(Rml::Input::KI_TAB);
    auto* focused = context.GetFocusElement();
    bool inSelectedPage = false;
    for( auto* node = focused; node; node = node->GetParentNode() )
        if( node == panels[1] ) inSelectedPage = true;
    check(inSelectedPage, "Tab enters selected page");
    check(focused && focused->IsVisible(), "hidden page focus excluded");
    // Show selected page, keyboard focus and default command simultaneously.
    if(auto* back = document.GetElementById("shell-back"))
    {
        const auto before = back->GetBox().GetSize();
        back->Focus(true);
        context.Update();
        check(back->GetBox().GetSize() == before, "focus preserves button size");
        check(context.GetFocusElement() == back, "command focus");
        check(back->IsPseudoClassSet("focus-visible"), "keyboard focus cue enabled");
    }
    return failures.empty() ? "PASS checks=" + std::to_string(checks) : "FAIL " + failures;
}
