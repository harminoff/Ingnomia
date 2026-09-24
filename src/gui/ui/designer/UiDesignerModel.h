/*
 * This file is part of Ingnomia.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ingnomia::ui::designer
{

enum class Mode
{
	Play,
	Design,
};

struct ComponentDefinition
{
	std::string id;
	std::string label;
	std::string tagName;
};

struct ElementSelection
{
	std::string id;
	std::string tagName;
	float left{};
	float top{};
	float width{};
	float height{};
	bool editable{};
	bool generated{};
};

struct Bounds
{
	float left{};
	float top{};
	float width{};
	float height{};
};

struct AddedComponent
{
	std::string id;
	std::string componentId;
	std::string tagName;
	std::string parentId;
	float left{};
	float top{};
	float width{};
	float height{};
};

class UiDesignerModel final
{
public:
	using ChangeHandler = std::function<void()>;

	UiDesignerModel();

	Mode mode() const noexcept { return mode_; }
	bool designMode() const noexcept { return mode_ == Mode::Design; }
	void setChangeHandler( ChangeHandler handler ) { changeHandler_ = std::move( handler ); }

	void enterDesign();
	void leaveDesign();

	const ElementSelection& selection() const noexcept { return selection_; }
	bool hasSelection() const noexcept { return !selection_.id.empty(); }
	bool select( ElementSelection selection );
	void clearSelection();

	bool updateBounds( Bounds bounds );
	bool undo();
	std::size_t undoDepth() const noexcept { return undoStack_.size(); }
	void recordAddedComponent( AddedComponent component );
	const std::vector<AddedComponent>& addedComponents() const noexcept { return addedComponents_; }

	const std::vector<ComponentDefinition>& components() const noexcept { return components_; }
	std::string projectJson( std::string_view surface ) const;

private:
	void notifyChanged();
	void pushUndo();

	Mode mode_ = Mode::Play;
	ElementSelection selection_;
	std::vector<ElementSelection> undoStack_;
	std::vector<ComponentDefinition> components_;
	std::vector<AddedComponent> addedComponents_;
	ChangeHandler changeHandler_;
};

} // namespace ingnomia::ui::designer
