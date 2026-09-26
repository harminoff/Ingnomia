/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include <RmlUi/Core.h>
#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <vector>
namespace ingnomia::ui::report {
inline std::string foldedLabel(std::string s){for(auto&c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
inline std::string filterOptionMarkup( std::vector<std::string> values, const std::vector<std::string>& selected, std::string_view idPrefix )
{
	std::ranges::stable_sort( values, []( const auto& left, const auto& right ) { return foldedLabel( left ) < foldedLabel( right ); } );
	values.erase( std::unique( values.begin(), values.end(), []( const auto& left, const auto& right ) { return foldedLabel( left ) == foldedLabel( right ); } ), values.end() );
	std::string out = "<button id='" + std::string( idPrefix ) + "_option_all' type='button' class='c-excel-filter-combo__option" + std::string( selected.empty() ? " is-selected" : "" ) + "' role='option' aria-selected='" + ( selected.empty() ? std::string( "true" ) : std::string( "false" ) ) + "' data-filter-value=''><span class='c-excel-filter-combo__check'>" + ( selected.empty() ? std::string( "[x]" ) : std::string( "[ ]" ) ) + "</span> All</button>";
	std::size_t optionIndex = 0;
	for ( const auto& value : values )
	{
		if ( value.empty() ) continue;
		const bool active = std::ranges::any_of( selected, [&]( const auto& candidate ) { return foldedLabel( candidate ) == foldedLabel( value ); } );
		out += "<button id='" + std::string( idPrefix ) + "_option_" + std::to_string( ++optionIndex ) + "' type='button' class='c-excel-filter-combo__option" + std::string( active ? " is-selected" : "" ) + "' role='option' aria-selected='" + ( active ? std::string( "true" ) : std::string( "false" ) ) + "' data-filter-value='" + Rml::StringUtilities::EncodeRml( value ) + "'><span class='c-excel-filter-combo__check'>" + ( active ? std::string( "[x]" ) : std::string( "[ ]" ) ) + "</span> " + Rml::StringUtilities::EncodeRml( value ) + "</button>";
	}
	out += "<button type='button' class='c-excel-filter-combo__option' data-filter-value='' data-filter-reset='true'>Reset column</button>";
	return out;
}
// Only option catalogs are shared: routes keep their own filter payload semantics.
inline void projectOptions(Rml::Element* list,std::string& previous,const std::string& markup){
 if(!list||previous==markup)return;
 auto*focus=list->GetContext()->GetFocusElement();std::string value;bool restore=false;
 if(focus&&list->Contains(focus)){value=focus->GetAttribute<Rml::String>("data-filter-value","");restore=true;}
 const float top=list->GetScrollTop();list->SetInnerRML(markup);list->SetScrollTop(top);previous=markup;
 if(restore)for(int i=0;i<list->GetNumChildren();++i){auto*e=list->GetChild(i);if(e->GetAttribute<Rml::String>("data-filter-value","")==value){e->Focus();break;}}
}
inline void filterKey(Rml::Event&e,Rml::Element*input,Rml::Element*list,const std::function<void(bool)>&setOpen){
 if(!input||!list)return;const int key=e.GetParameter<int>("key_identifier",0);
 auto*focus=e.GetTargetElement();const bool inside=list->Contains(focus);
 if(key==Rml::Input::KI_ESCAPE || (inside&&key==Rml::Input::KI_TAB)){setOpen(false);input->Focus();if(key!=Rml::Input::KI_TAB)e.StopPropagation();return;}
 if(key!=Rml::Input::KI_DOWN&&key!=Rml::Input::KI_UP&&key!=Rml::Input::KI_HOME&&key!=Rml::Input::KI_END)return;
 if(!inside&&(key==Rml::Input::KI_HOME||key==Rml::Input::KI_END))return;
 setOpen(true);std::vector<Rml::Element*>options;for(int i=0;i<list->GetNumChildren();++i)if(list->GetChild(i)->HasAttribute("data-filter-value"))options.push_back(list->GetChild(i));
 if(options.empty())return;auto found=std::find(options.begin(),options.end(),focus);int index=found==options.end()?-1:int(found-options.begin());
 if(key==Rml::Input::KI_HOME)index=0;else if(key==Rml::Input::KI_END)index=int(options.size())-1;else index=std::clamp(index+(key==Rml::Input::KI_UP?-1:1),0,int(options.size())-1);
 options[index]->Focus();options[index]->ScrollIntoView();e.StopPropagation();
}
inline void sortIndicator(Rml::Element*button,Rml::Element*mark,bool selected,bool descending){
 if(button){button->SetClass("is-selected",selected);button->SetAttribute("aria-pressed",selected?"true":"false");if(auto*p=button->GetParentNode())p->SetAttribute("aria-sort",selected?(descending?"descending":"ascending"):"none");}
 if(mark){mark->SetInnerRML("");mark->SetClass("c-report-sort",selected);mark->SetClass("is-descending",selected&&descending);}
}
}
