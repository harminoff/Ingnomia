/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "DebugController.h"

#include <functional>
#include <memory>
#include <vector>
namespace Rml
{
class Context;
class Element;
class ElementDocument;
class Event;
class EventListener;
} // namespace Rml
namespace ingnomia::ui::debug
{
class DebugRmlBinding final : public ViewPort
{
public:
	explicit DebugRmlBinding( Rml::Context& );
	~DebugRmlBinding() override;
	bool initialize( DebugController& );
	bool reloadDocument();
	void shutdown();
	void stateChanged( const DebugState& ) override;
	std::size_t listenerCount() const
	{
		return listeners_.size();
	}
	Rml::ElementDocument* document() const
	{
		return document_;
	}

private:
	class Callback;
	void bind( const char*, std::function<void()> );
	void bindEvent( const char*, const char*, std::function<void( Rml::Event& )> );
	Rml::Context& context_;
	DebugController* controller_ {};
	Rml::ElementDocument* document_ {};
	struct Listener
	{
		Rml::Element* element {};
		std::string event;
		std::unique_ptr<Callback> callback;
	};
	std::vector<Listener> listeners_;
};
} // namespace ingnomia::ui::debug
