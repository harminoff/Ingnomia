/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include <RmlUi/Core.h>
#include <algorithm>

namespace ingnomia::ui::connected_tabs
{
inline bool enabled(Rml::Element* element)
{
    return element && element->IsVisible(true) && !element->HasAttribute("disabled") && !element->IsPseudoClassSet("disabled");
}
inline Rml::ElementList tabs(Rml::Element& strip)
{
    Rml::ElementList result;
    strip.GetElementsByTagName(result, "button");
    std::erase_if(result, [](auto* e) { return e->GetAttribute<Rml::String>("role", "") != "tab"; });
    return result;
}
inline Rml::Element* stripFor(Rml::Element* tab)
{
    for(auto* parent = tab ? tab->GetParentNode() : nullptr; parent; parent = parent->GetParentNode())
        if(parent->IsClassSet("c-connected-tabs")) return parent;
    return nullptr;
}
inline bool contains(Rml::Element* root, Rml::Element* node)
{
    for(; node; node = node->GetParentNode()) if(node == root) return true;
    return false;
}
inline void select(Rml::Element& strip, Rml::Element* selected)
{
    auto* document = strip.GetOwnerDocument();
    for(auto* tab : tabs(strip))
    {
        const bool active = tab == selected;
        tab->SetClass("is-selected", active);
        tab->SetAttribute("aria-selected", active ? "true" : "false");
        tab->SetAttribute("tab-index", active ? "0" : "-1");
        tab->SetProperty("tab-index", active ? "auto" : "none");
        // A tab without aria-controls owns no panel; GetElementById("") would return an unrelated element.
        const auto controls = tab->GetAttribute<Rml::String>("aria-controls", "");
        if(auto* panel = controls.empty() ? nullptr : document->GetElementById(controls))
        {
            panel->SetClass("u-hidden", !active);
            panel->SetAttribute("aria-hidden", active ? "false" : "true");
            if(active) panel->RemoveProperty("display"); else panel->SetProperty("display", "none");
        }
    }
}
// Called after layout so dynamic visibility has resolved. Only a change in
// availability triggers selection/focus repair and the owner's click callback.
inline bool reconcile(Rml::Context& context)
{
    Rml::ElementList strips;
    context.GetRootElement()->GetElementsByClassName(strips, "c-connected-tabs");
    bool changed = false;
    for(auto* strip : strips)
    {
        if(!strip->IsVisible(true)) continue;
        auto choices = tabs(*strip);
        auto current = std::find_if(choices.begin(), choices.end(), [](auto* t) { return t->IsClassSet("is-selected"); });
        if(current != choices.end() && enabled(*current)) continue;
        auto first = std::find_if(choices.begin(), choices.end(), enabled);
        auto* replacement = first == choices.end() ? nullptr : *first;
        if(current == choices.end() && !replacement) continue;
        auto* focus = context.GetFocusElement();
        auto* oldPanel = current == choices.end() ? nullptr : strip->GetOwnerDocument()->GetElementById((*current)->GetAttribute<Rml::String>("aria-controls", ""));
        const bool repairFocus = focus && (!focus->IsVisible(true) ||
            (current != choices.end() && (focus == *current || contains(oldPanel, focus) || focus == strip->GetOwnerDocument() || focus == context.GetRootElement())));
        select(*strip, replacement);
        if(replacement) {
            auto observer = replacement->GetObserverPtr();
            replacement->Click();
            if(repairFocus && observer && observer->IsVisible(true)) observer->Focus(true);
        }
        else if(repairFocus) strip->Focus(true);
        changed = true;
    }
    return changed;
}
inline bool key(Rml::Context& context, int key, int modifiers)
{
    if(modifiers & (Rml::Input::KM_ALT | Rml::Input::KM_META)) return false;
    auto* focus = context.GetFocusElement();
    auto* document = focus ? focus->GetOwnerDocument() : nullptr;
    if(!document) return false;
    // Ctrl+Tab and Ctrl+Page Down go to the next tab, Ctrl+Shift+Tab and Ctrl+Page Up to the previous one (PDF p.147, p.400).
    const bool pageKey = (key == Rml::Input::KI_NEXT || key == Rml::Input::KI_PRIOR) && (modifiers & Rml::Input::KM_CTRL);
    const bool cycle = (key == Rml::Input::KI_TAB && (modifiers & Rml::Input::KM_CTRL)) || pageKey;
    if(pageKey) modifiers = key == Rml::Input::KI_PRIOR ? (modifiers | Rml::Input::KM_SHIFT) : (modifiers & ~Rml::Input::KM_SHIFT);
    Rml::Element* strip = stripFor(focus);
    if(!strip && cycle)
    {
        Rml::ElementList strips; document->GetElementsByClassName(strips, "c-connected-tabs");
        for(auto* candidate : strips)
            for(auto* tab : tabs(*candidate))
                if(contains(document->GetElementById(tab->GetAttribute<Rml::String>("aria-controls", "")), focus)) strip = candidate;
        std::erase_if(strips, [](auto* candidate) { return !candidate->IsVisible(true); });
        if(!strip && strips.size() == 1) strip = strips.front();
    }
    if(!strip || !strip->IsVisible(true)) return false;
    const bool vertical = strip->GetAttribute<Rml::String>("aria-orientation", "horizontal") == "vertical";
    const int previous = vertical ? Rml::Input::KI_UP : Rml::Input::KI_LEFT;
    const int next = vertical ? Rml::Input::KI_DOWN : Rml::Input::KI_RIGHT;
    if(!cycle && ((modifiers & Rml::Input::KM_CTRL) || (key != previous && key != next && key != Rml::Input::KI_HOME && key != Rml::Input::KI_END))) return false;
    auto choices = tabs(*strip); std::erase_if(choices, [](auto* e) { return !enabled(e); });
    if(choices.empty()) return true;
    auto current = std::find(choices.begin(), choices.end(), focus);
    if(current == choices.end()) current = std::find_if(choices.begin(), choices.end(), [](auto* e) { return e->IsClassSet("is-selected"); });
    const int count = static_cast<int>(choices.size());
    int index = current == choices.end() ? 0 : static_cast<int>(current - choices.begin());
    if(!cycle && key == Rml::Input::KI_HOME) index = 0;
    else if(!cycle && key == Rml::Input::KI_END) index = count - 1;
    else index = (index + ((cycle ? bool(modifiers & Rml::Input::KM_SHIFT) : key == previous) ? count - 1 : 1)) % count;
    auto* destination = choices[index];
    auto observer = destination->GetObserverPtr();
    select(*strip, destination); destination->Click();
    if(observer && observer->IsVisible(true)) observer->Focus(true);
    return true;
}
}
