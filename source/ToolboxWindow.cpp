#include <cassert>
#include <cmath>
#include <Rocket/Core/Element.h>
#include "Game.h"
#include "ToolboxWindow.h"
#include <chrono>
#include <algorithm>
#include <vector>

using namespace OT;

// Define a constant for the long-press threshold
const double LONG_PRESS_THRESHOLD = 0.3; // seconds

void ToolboxWindow::advance(double dt)
{
	// Check if a button is being held and the long press hasn't been triggered yet
	if (pressedElement && !longPressTriggered) {
		auto now = std::chrono::steady_clock::now();
		std::chrono::duration<double> elapsed = now - mouseDownTime;

		if (elapsed.count() >= LONG_PRESS_THRESHOLD) {
			triggerLongPress(pressedElement);
		}
	}
}

void ToolboxWindow::close()
{
	buttons.clear();

	if (window) {
		window->RemoveReference();
		window->Close();
	}
	window = NULL;
}

void ToolboxWindow::reload()
{
	close();

	window = game->gui.loadDocument("toolbox.rml");
	assert(window);
	window->Show();

	// Add listeners for static buttons
	Rocket::Core::ElementList tmp;
	window->GetElementsByTagName(tmp, "button");
	for (int i = 0; i < tmp.size(); i++) {
		tmp[i]->AddEventListener("click", this);
		tmp[i]->AddEventListener("mousedown", this);
		tmp[i]->AddEventListener("mouseup", this);
		tmp[i]->AddEventListener("mousemove", this);
		if (tmp[i]->IsClassSet("tool")) buttons.insert(tmp[i]);
	}

	// --- DYNAMIC ITEM BUTTON CREATION ---

	// 1. Get and sort the prototypes
	std::vector<Item::AbstractPrototype*> sortedPrototypes = game->itemFactory.prototypes;

	// Custom display priority for toolbox categories. The enum values in
	// Item::Prototype.h are not in the visual order we want; define a mapping
	// so the UI shows Construction (Lobby) first, then Transport (Elevators),
	// then Food, Housing, Office, etc.
	auto category_priority = [](int cat) -> int {
		using namespace Item;
		switch (cat) {
			case CAT_CONSTRUCTION: return 0; // Lobby / construction tools
			case CAT_TRANSPORT:    return 1; // Elevators, stairs, escalators
			case CAT_FOOD:         return 2; // FastFood, Restaurant
			case CAT_HOUSING:      return 3; // Condo
			case CAT_OFFICE:       return 4; // Office
			case CAT_ENTERTAINMENT:return 5; // Cinema, PartyHall
			case CAT_HOTEL:        return 6;
			case CAT_UTILITIES:    return 7;
			default:               return 100;
		}
	};

	// Explicit placement list for the top-left of the toolbox. Entries are
	// matched either by exact id or by simple substring for categories like
	// elevators. Items in this list will be ordered before others.
	const std::vector<std::string> explicit_order = { "lobby", "elevator", "stairs", "fastfood", "condo", "office" };

	auto get_explicit_rank = [&explicit_order](const std::string &id) -> int {
		for (size_t i = 0; i < explicit_order.size(); ++i) {
			const std::string &pat = explicit_order[i];
			if (pat == "elevator") {
				if (id.find("elevator") != std::string::npos) return (int)i;
			} else {
				if (id == pat) return (int)i;
			}
		}
		return INT_MAX;
	};
 
	std::sort(sortedPrototypes.begin(), sortedPrototypes.end(),
		[&category_priority,&get_explicit_rank](const Item::AbstractPrototype* a, const Item::AbstractPrototype* b) {
			int ra = get_explicit_rank(a->id);
			int rb = get_explicit_rank(b->id);
			if (ra != rb) return ra < rb;

			// If neither (or both) are in the explicit list, fall back to
			// category priority and toolboxOrder to keep ordering stable.
			int pa = category_priority(a->toolboxCategory);
			int pb = category_priority(b->toolboxCategory);
			if (pa != pb) return pa < pb;
			return a->toolboxOrder < b->toolboxOrder;
		}
	);

	// 2. Get the container element
	Rocket::Core::Element* itemContainer = window->GetElementById("item-container");
	assert(itemContainer);

	// 3. Clear any existing items (for reloads)
	while (itemContainer->HasChildNodes()) {
		itemContainer->RemoveChild(itemContainer->GetChild(0));
	}

	// 4. Create and append new buttons
	for (Item::AbstractPrototype* prototype : sortedPrototypes) {
		// Skip items that shouldn't be in the toolbox (like Floor)
		if (prototype->id == "floor") continue;

		char id[128];
		snprintf(id, 128, "item-%s", prototype->id.c_str());

	// Generate style for the icon using the same RML format (no url())
	// include repeat/size to match toolbox.rml defaults
	char style[512];
	// The project uses a custom RML property `background-image-s` to select a slice
	// from a sprite sheet (start_px end_px). Compute the slice for this prototype.
	int start_px = prototype->icon * 32;
	int end_px = start_px + 32;
	// Generate style matching toolbox.rml: set the same background-image resource,
	// provide the background-image-s range in pixels, keep no-repeat and preserve
	// background-size so the sprite stays at native resolution.
	snprintf(style, 512, "button#%s { height:32px; background-image: /simtower/ui/toolbox/items; background-image-s: %dpx %dpx; background-repeat: no-repeat; background-size: 100%% 100%%; }", id, start_px, end_px);

		Rocket::Core::StyleSheet * sheet = Rocket::Core::Factory::InstanceStyleSheetString(style);
		assert(sheet && "unable to instantiate stylesheet");
		window->SetStyleSheet(window->GetStyleSheet()->CombineStyleSheet(sheet));
		sheet->RemoveReference();

		// Create the button element
		Rocket::Core::XMLAttributes attributes;
		attributes.Set("class", "item");
		attributes.Set("id", id);

		Rocket::Core::Element* button = Rocket::Core::Factory::InstanceElement(itemContainer, "button", "button", attributes);
		assert(button && "unable to instantiate button");

		button->AddEventListener("click", this);
		button->AddEventListener("mousedown", this);
		button->AddEventListener("mouseup", this);
		button->AddEventListener("mousemove", this);

		itemContainer->AppendChild(button);
		buttons.insert(button);

		// mark locked buttons initially based on game's rating
		if (prototype->unlockRating > game->rating) {
			button->SetClass("locked", true);
		}

		button->RemoveReference(); // container now owns it
	}

	updateSpeed();
	updateTool();
	updateAvailability();
}

void ToolboxWindow::ProcessEvent(Rocket::Core::Event& event) {
	const std::string type = event.GetType().CString();
	//LOG(DEBUG, "ToolboxWindow: %s event received", type.c_str());

	try {
		if (type == "mousedown") {
			handleMouseDown(event);
		} else if (type == "mouseup") {
			handleMouseUp(event);
		} else if (type == "mousemove") {
			handleMouseMove(event);
		} else if (type == "click") {
			handleClick(event);
		}
	} catch (const std::exception& e) {
		LOG(DEBUG, "ToolboxWindow: exception in ProcessEvent: %s", e.what());
	} catch (...) {
		LOG(DEBUG, "ToolboxWindow: unknown exception in ProcessEvent");
	}
}

void ToolboxWindow::handleMouseDown(Rocket::Core::Event &event) {
	Rocket::Core::Element* element = event.GetTargetElement();
	if (!element) {
		LOG(DEBUG, "ToolboxWindow: handleMouseDown - null element");
		return;
	}

	LOG(DEBUG, "ToolboxWindow: handleMouseDown on %s", element->GetTagName().CString());

	// Check for context menu dismissal
	if (isContextMenuOpen()) {
		// Check if click is outside the context menu
		bool clickInsideMenu = false;
		Rocket::Core::Element* parent = element;
		while(parent) {
			if (parent->IsClassSet("context-menu")) {
				clickInsideMenu = true;
				break;
			}
			parent = parent->GetParentNode();
		}

		if (!clickInsideMenu) {
			dismissContextMenu();
			return;
		}
	}

	// Restore layout if clicking on a different button than the one that triggered the long press
	if (longPressTriggered && pressedElement && element != pressedElement) {
		const std::string id = element->GetId().CString();
		// Don't restore if clicking on a transformed button (floor or escalator)
		if (id != "item-floor" && id != "item-escalator") {
			restoreOriginalLayout();
			longPressTriggered = false;
		}
	}

	// Start long-press timer for buttons
	if (element->GetTagName() == "button") {
		startLongPressTimer(element);
	}
}

void ToolboxWindow::handleMouseUp(Rocket::Core::Event &event) {
	cancelLongPressTimer();
	
	// If a long-press was active, treat this mouseup as the selection
	// for whatever element is currently under the mouse cursor
	if (longPressTriggered) {
		Rocket::Core::Element* element = event.GetTargetElement();
		if (element && element->GetTagName() == "button") {
			const char* eid = element->GetId().CString();
			LOG(DEBUG, "ToolboxWindow: handleMouseUp - releasing on '%s' after long-press", eid);
			
			// If the user released on the transformed Floor button, select it
			// and move it into the Lobby slot visually.
			if (strcmp(eid, "item-floor") == 0) {
				// Select floor
				game->selectTool("item-floor");
				
				// Find the top-left button (first child) and change it to Floor
				Rocket::Core::Element* itemContainer = window->GetElementById("item-container");
				if (itemContainer && itemContainer->GetNumChildren() > 0) {
					Rocket::Core::Element* topLeftButton = itemContainer->GetChild(0);
					if (topLeftButton) {
						// Find floor prototype icon
						Item::AbstractPrototype* floorProto = nullptr;
						for (Item::AbstractPrototype* prototype : game->itemFactory.prototypes) {
							if (prototype->id == "floor") { floorProto = prototype; break; }
						}
						if (floorProto) {
							topLeftButton->SetAttribute("id", "item-floor");
							char style[512];
							int start_px = floorProto->icon * 32;
							int end_px = start_px + 32;
							snprintf(style, 512, "button#item-floor { height:32px; background-image: /simtower/ui/toolbox/items; background-image-s: %dpx %dpx; background-repeat: no-repeat; background-size: 100%% 100%%; }", start_px, end_px);
							Rocket::Core::StyleSheet * sheet = Rocket::Core::Factory::InstanceStyleSheetString(style);
							if (sheet) { window->SetStyleSheet(window->GetStyleSheet()->CombineStyleSheet(sheet)); sheet->RemoveReference(); }
						}
					}
				}
				
				// Now restore other transforms (this will restore FastFood from any other Floor buttons)
				restoreOriginalLayout();
				updateTool();
			} else if (strcmp(eid, "item-lobby") == 0) {
				// User selected Lobby (likely from Floor's context menu) - select lobby and ensure it's in top-left position
				game->selectTool("item-lobby");
				
				// Find the top-left button (first child) and change it to Lobby
				Rocket::Core::Element* itemContainer = window->GetElementById("item-container");
				if (itemContainer && itemContainer->GetNumChildren() > 0) {
					Rocket::Core::Element* topLeftButton = itemContainer->GetChild(0);
					if (topLeftButton) {
						// Find lobby prototype icon
						Item::AbstractPrototype* lobbyProto = nullptr;
						for (Item::AbstractPrototype* prototype : game->itemFactory.prototypes) {
							if (prototype->id == "lobby") { lobbyProto = prototype; break; }
						}
						if (lobbyProto) {
							topLeftButton->SetAttribute("id", "item-lobby");
							char style[512];
							int start_px = lobbyProto->icon * 32;
							int end_px = start_px + 32;
							snprintf(style, 512, "button#item-lobby { height:32px; background-image: /simtower/ui/toolbox/items; background-image-s: %dpx %dpx; background-repeat: no-repeat; background-size: 100%% 100%%; }", start_px, end_px);
							Rocket::Core::StyleSheet * sheet = Rocket::Core::Factory::InstanceStyleSheetString(style);
							if (sheet) { window->SetStyleSheet(window->GetStyleSheet()->CombineStyleSheet(sheet)); sheet->RemoveReference(); }
						}
					}
				}
				
				// Now restore other transforms (this will restore FastFood from any other Lobby buttons)
				restoreOriginalLayout();
				updateTool();
			} else if (strcmp(eid, "item-escalator") == 0) {
				// User selected Escalator from Stairs context - select escalator and move it to Stairs position (position 3)
				game->selectTool("item-escalator");
				
				// Find the Stairs button (position 3) and change it to Escalator
				Rocket::Core::Element* itemContainer = window->GetElementById("item-container");
				if (itemContainer && itemContainer->GetNumChildren() > 2) {
					Rocket::Core::Element* stairsButton = itemContainer->GetChild(2); // Position 3 (0-indexed as 2)
					if (stairsButton) {
						// Find escalator prototype icon
						Item::AbstractPrototype* escalatorProto = nullptr;
						for (Item::AbstractPrototype* prototype : game->itemFactory.prototypes) {
							if (prototype->id == "escalator") { escalatorProto = prototype; break; }
						}
						if (escalatorProto) {
							stairsButton->SetAttribute("id", "item-escalator");
							char style[512];
							int start_px = escalatorProto->icon * 32;
							int end_px = start_px + 32;
							snprintf(style, 512, "button#item-escalator { height:32px; background-image: /simtower/ui/toolbox/items; background-image-s: %dpx %dpx; background-repeat: no-repeat; background-size: 100%% 100%%; }", start_px, end_px);
							Rocket::Core::StyleSheet * sheet = Rocket::Core::Factory::InstanceStyleSheetString(style);
							if (sheet) { window->SetStyleSheet(window->GetStyleSheet()->CombineStyleSheet(sheet)); sheet->RemoveReference(); }
						}
					}
				}
				
				// Now restore other transforms (this will restore Office to position 6)
				restoreOriginalLayout();
				updateTool();
			} else if (strcmp(eid, "item-stairs") == 0) {
				// User selected Stairs (from Escalator's context menu) - select stairs and move it to Stairs position (position 3)
				game->selectTool("item-stairs");
				
				// Find the Stairs button (position 3) and ensure it shows as Stairs
				Rocket::Core::Element* itemContainer = window->GetElementById("item-container");
				if (itemContainer && itemContainer->GetNumChildren() > 2) {
					Rocket::Core::Element* stairsButton = itemContainer->GetChild(2); // Position 3 (0-indexed as 2)
					if (stairsButton) {
						// Find stairs prototype icon
						Item::AbstractPrototype* stairsProto = nullptr;
						for (Item::AbstractPrototype* prototype : game->itemFactory.prototypes) {
							if (prototype->id == "stairs") { stairsProto = prototype; break; }
						}
						if (stairsProto) {
							stairsButton->SetAttribute("id", "item-stairs");
							char style[512];
							int start_px = stairsProto->icon * 32;
							int end_px = start_px + 32;
							snprintf(style, 512, "button#item-stairs { height:32px; background-image: /simtower/ui/toolbox/items; background-image-s: %dpx %dpx; background-repeat: no-repeat; background-size: 100%% 100%%; }", start_px, end_px);
							Rocket::Core::StyleSheet * sheet = Rocket::Core::Factory::InstanceStyleSheetString(style);
							if (sheet) { window->SetStyleSheet(window->GetStyleSheet()->CombineStyleSheet(sheet)); sheet->RemoveReference(); }
						}
					}
				}
				
				// Now restore other transforms (this will restore Office to position 6)
				restoreOriginalLayout();
				updateTool();
			} else {
				// Released on some other button - just restore layout
				restoreOriginalLayout();
			}
		} else {
			// Released outside any button - just restore layout
			restoreOriginalLayout();
		}

		// Prevent the following click event from re-processing selection
		ignoreNextClick = true;

		// Clear state
		pressedElement = nullptr;
		longPressTriggered = false;
		return;
	}

	// Otherwise, just clear the pressed element
	pressedElement = nullptr;
	longPressTriggered = false;
}

void ToolboxWindow::handleMouseMove(Rocket::Core::Event &event) {
	// If not holding a long-press, ignore hover selection
	if (!longPressTriggered) return;

	Rocket::Core::Element* element = event.GetTargetElement();
	if (!element) {
		LOG(DEBUG, "ToolboxWindow: handleMouseMove - null element");
		return;
	}

	LOG(DEBUG, "ToolboxWindow: handleMouseMove - element tag: %s, id: %s", 
		element->GetTagName().CString(), element->GetId().CString());

	// Only consider buttons
	if (element->GetTagName() != "button") {
		LOG(DEBUG, "ToolboxWindow: handleMouseMove - not a button");
		return;
	}

	const std::string id = element->GetId().CString();
	if (id.empty()) {
		LOG(DEBUG, "ToolboxWindow: handleMouseMove - empty id");
		return;
	}

	// While holding long-press, update the hovered selection visually by
	// calling selectItem with the hovered button id. This will call
	// game->selectTool but won't restore the toolbox until mouseup.
	LOG(DEBUG, "ToolboxWindow: handleMouseMove - hovering over '%s' during long-press", id.c_str());
	selectItem(id);
}void ToolboxWindow::handleClick(Rocket::Core::Event &event) {
	Rocket::Core::Element* element = event.GetTargetElement();
	if (!element) {
		LOG(DEBUG, "ToolboxWindow: handleClick - null element");
		return;
	}

	LOG(DEBUG, "ToolboxWindow: handleClick on %s", element->GetTagName().CString());

	const std::string id = element->GetId().CString();

	// If a long-press handled the selection on mouseup, ignore this click
	if (ignoreNextClick) {
		LOG(DEBUG, "ToolboxWindow: ignoring click because long-press already handled selection");
		ignoreNextClick = false;
		return;
	}
	LOG(DEBUG, "ToolboxWindow: handleClick - element id: '%s'", id.c_str());
	
	if (!id.empty()) {
		LOG(DEBUG, "ToolboxWindow: calling selectItem with id: '%s'", id.c_str());
		selectItem(id);
		
		// If a long-press was active, handle transformed selections specially
		if (longPressTriggered) {
			if (id == "item-floor") {
				LOG(DEBUG, "ToolboxWindow: selected transformed Floor, restoring layout and moving Floor to Lobby");
				// First restore other transformed buttons (fastfood -> floor)
				restoreOriginalLayout();

				// Now replace the Lobby button with Floor so the UI shows selection
				Rocket::Core::Element* lobbyButton = window->GetElementById("item-lobby");
				if (lobbyButton) {
					// Find the floor prototype to get its icon
					Item::AbstractPrototype* floorProto = nullptr;
					for (Item::AbstractPrototype* prototype : game->itemFactory.prototypes) {
						if (prototype->id == "floor") {
							floorProto = prototype;
							break;
						}
					}
					if (floorProto) {
						lobbyButton->SetAttribute("id", "item-floor");
						// Update the background image to show floor icon
						char style[512];
						int start_px = floorProto->icon * 32;
						int end_px = start_px + 32;
						snprintf(style, 512, "button#item-floor { height:32px; background-image: /simtower/ui/toolbox/items; background-image-s: %dpx %dpx; background-repeat: no-repeat; background-size: 100%% 100%%; }", start_px, end_px);
						Rocket::Core::StyleSheet * sheet = Rocket::Core::Factory::InstanceStyleSheetString(style);
						if (sheet) {
							window->SetStyleSheet(window->GetStyleSheet()->CombineStyleSheet(sheet));
							sheet->RemoveReference();
						}
					}
				}
				// Refresh selection classes to reflect new id
				updateTool();
			} else if (id == "item-escalator") {
				LOG(DEBUG, "ToolboxWindow: selected transformed escalator, restoring layout");
				restoreOriginalLayout();
			}
			// Clear the long-press flag now that we've handled the transformed selection
			longPressTriggered = false;
		}
	} else {
		LOG(DEBUG, "ToolboxWindow: element id is empty!");
	}
}

void ToolboxWindow::startLongPressTimer(Rocket::Core::Element* element) {
	pressedElement = element;
	mouseDownTime = std::chrono::steady_clock::now();
	longPressTriggered = false;
}

void ToolboxWindow::cancelLongPressTimer() {
	// Just clear the pressed element - mouseUp will handle the rest
	pressedElement = nullptr;
}

bool ToolboxWindow::isContextMenuOpen() {
	// Check if any context menu is visible
	Rocket::Core::Element* lobbyMenu = window->GetElementById("lobby-context");
	if (lobbyMenu) {
		const Rocket::Core::Property* prop = lobbyMenu->GetProperty("display");
		if (prop && prop->Get<Rocket::Core::String>() == "block") return true;
	}

	Rocket::Core::Element* elevatorMenu = window->GetElementById("elevator-context");
	if (elevatorMenu) {
		const Rocket::Core::Property* prop = elevatorMenu->GetProperty("display");
		if (prop && prop->Get<Rocket::Core::String>() == "block") return true;
	}

	Rocket::Core::Element* stairsMenu = window->GetElementById("stairs-context");
	if (stairsMenu) {
		const Rocket::Core::Property* prop = stairsMenu->GetProperty("display");
		if (prop && prop->Get<Rocket::Core::String>() == "block") return true;
	}

	Rocket::Core::Element* hotelMenu = window->GetElementById("hotelroom-context");
	if (hotelMenu) {
		const Rocket::Core::Property* prop = hotelMenu->GetProperty("display");
		if (prop && prop->Get<Rocket::Core::String>() == "block") return true;
	}

	return false;
}

void ToolboxWindow::dismissContextMenu() {
	// Hide all context menus
	Rocket::Core::Element* menus[] = {
		window->GetElementById("lobby-context"),
		window->GetElementById("elevator-context"),
		window->GetElementById("stairs-context"),
		window->GetElementById("hotelroom-context")
	};

	for (auto menu : menus) {
		if (menu) {
			menu->SetProperty("display", "none");
		}
	}
}

void ToolboxWindow::handleContextMenuClick(Rocket::Core::Element* element) {
	const std::string id = element->GetId().CString();
	LOG(DEBUG, "ToolboxWindow: context menu click on %s", id.c_str());

	if (id == "context-lobby-floor") {
		game->selectTool("floor");
	} else if (id == "context-lobby-stairs") {
		game->selectTool("stairs");
	}

	dismissContextMenu();
}

void ToolboxWindow::selectItem(const std::string& id) {
	LOG(DEBUG, "ToolboxWindow: selectItem called with id: '%s'", id.c_str());
	// Pass the full button ID (with "item-" prefix) to selectTool as expected
	game->selectTool(id.c_str());
	LOG(DEBUG, "ToolboxWindow: called game->selectTool with: '%s'", id.c_str());
}

void ToolboxWindow::restoreOriginalLayout() {
	LOG(DEBUG, "ToolboxWindow: restoreOriginalLayout called - clearing context state");
	// Find the FastFood prototype for restoration
	Item::AbstractPrototype* fastfoodProto = nullptr;
	for (Item::AbstractPrototype* prototype : game->itemFactory.prototypes) {
		if (prototype->id == "fastfood") {
			fastfoodProto = prototype;
			break;
		}
	}
	
	if (!fastfoodProto) return;
	
	// Get the item container to identify positions
	Rocket::Core::Element* itemContainer = window->GetElementById("item-container");
	if (!itemContainer) return;
	
	// Get the first button (top-left position) - this should be preserved as the selected tool
	Rocket::Core::Element* topLeftButton = nullptr;
	if (itemContainer->GetNumChildren() > 0) {
		topLeftButton = itemContainer->GetChild(0);
	}
	
	// Find all buttons and restore any Floor/Lobby buttons that are NOT in the top-left position
	Rocket::Core::ElementList allButtons;
	window->GetElementsByTagName(allButtons, "button");
	
	for (int i = 0; i < allButtons.size(); i++) {
		std::string buttonId = allButtons[i]->GetId().CString();
		
		// Skip the top-left button - it should remain as the selected tool
		if (allButtons[i] == topLeftButton) {
			continue;
		}
		
		// If we find a Floor button that's not in top-left position, restore it to FastFood
		if (buttonId == "item-floor") {
			allButtons[i]->SetAttribute("id", "item-fastfood");
			char style[512];
			int start_px = fastfoodProto->icon * 32;
			int end_px = start_px + 32;
			snprintf(style, 512, "button#item-fastfood { height:32px; background-image: /simtower/ui/toolbox/items; background-image-s: %dpx %dpx; background-repeat: no-repeat; background-size: 100%% 100%%; }", start_px, end_px);
			Rocket::Core::StyleSheet * sheet = Rocket::Core::Factory::InstanceStyleSheetString(style);
			if (sheet) {
				window->SetStyleSheet(window->GetStyleSheet()->CombineStyleSheet(sheet));
				sheet->RemoveReference();
			}
		}
		// If we find a Lobby button that's not in top-left position, restore it to FastFood
		else if (buttonId == "item-lobby") {
			allButtons[i]->SetAttribute("id", "item-fastfood");
			char style[512];
			int start_px = fastfoodProto->icon * 32;
			int end_px = start_px + 32;
			snprintf(style, 512, "button#item-fastfood { height:32px; background-image: /simtower/ui/toolbox/items; background-image-s: %dpx %dpx; background-repeat: no-repeat; background-size: 100%% 100%%; }", start_px, end_px);
			Rocket::Core::StyleSheet * sheet = Rocket::Core::Factory::InstanceStyleSheetString(style);
			if (sheet) {
				window->SetStyleSheet(window->GetStyleSheet()->CombineStyleSheet(sheet));
				sheet->RemoveReference();
			}
		}
	}

	// Restore Office button if it was changed to Escalator
	Rocket::Core::Element* escalatorButton = window->GetElementById("item-escalator");
	if (escalatorButton) {
		// Find the office prototype to get its icon
		Item::AbstractPrototype* officeProto = nullptr;
		for (Item::AbstractPrototype* prototype : game->itemFactory.prototypes) {
			if (prototype->id == "office") {
				officeProto = prototype;
				break;
			}
		}
		if (officeProto) {
			escalatorButton->SetAttribute("id", "item-office");
			// Update the background image to show office icon
			char style[512];
			int start_px = officeProto->icon * 32;
			int end_px = start_px + 32;
			snprintf(style, 512, "button#item-office { height:32px; background-image: /simtower/ui/toolbox/items; background-image-s: %dpx %dpx; background-repeat: no-repeat; background-size: 100%% 100%%; }", start_px, end_px);
			Rocket::Core::StyleSheet * sheet = Rocket::Core::Factory::InstanceStyleSheetString(style);
			if (sheet) {
				window->SetStyleSheet(window->GetStyleSheet()->CombineStyleSheet(sheet));
				sheet->RemoveReference();
			}
		}
	}
}

void ToolboxWindow::triggerLongPress(Rocket::Core::Element* element) {
	LOG(DEBUG, "ToolboxWindow: %s triggered long press", element->GetId().CString());
	if (!element) return;

	longPressTriggered = true;

	const char* elementId = element->GetId().CString();

	if (strcmp(elementId, "item-lobby") == 0) {
		// Change the FastFood button to show Floor instead (so Floor appears directly under Lobby)
		Rocket::Core::Element* fastfoodButton = window->GetElementById("item-fastfood");
		if (fastfoodButton) {
			// Change to floor icon (icon index 1)
			fastfoodButton->SetAttribute("id", "item-floor");
			// Update the background image to show floor icon
			char style[512];
			snprintf(style, 512, "button#item-floor { height:32px; background-image: /simtower/ui/toolbox/items; background-image-s: %dpx %dpx; background-repeat: no-repeat; background-size: 100%% 100%%; }", 32, 64);
			Rocket::Core::StyleSheet * sheet = Rocket::Core::Factory::InstanceStyleSheetString(style);
			if (sheet) {
				window->SetStyleSheet(window->GetStyleSheet()->CombineStyleSheet(sheet));
				sheet->RemoveReference();
			}
		}
	}
	else if (strcmp(elementId, "item-floor") == 0) {
		// Floor button was long-pressed - show the original lobby context menu
		// This happens when Floor has replaced Lobby and user wants to switch back
		// Change the FastFood button to show Lobby instead
		Rocket::Core::Element* fastfoodButton = window->GetElementById("item-fastfood");
		if (fastfoodButton) {
			// Find the lobby prototype to get its icon
			Item::AbstractPrototype* lobbyProto = nullptr;
			for (Item::AbstractPrototype* prototype : game->itemFactory.prototypes) {
				if (prototype->id == "lobby") {
					lobbyProto = prototype;
					break;
				}
			}
			if (lobbyProto) {
				fastfoodButton->SetAttribute("id", "item-lobby");
				// Update the background image to show lobby icon
				char style[512];
				int start_px = lobbyProto->icon * 32;
				int end_px = start_px + 32;
				snprintf(style, 512, "button#item-lobby { height:32px; background-image: /simtower/ui/toolbox/items; background-image-s: %dpx %dpx; background-repeat: no-repeat; background-size: 100%% 100%%; }", start_px, end_px);
				Rocket::Core::StyleSheet * sheet = Rocket::Core::Factory::InstanceStyleSheetString(style);
				if (sheet) {
					window->SetStyleSheet(window->GetStyleSheet()->CombineStyleSheet(sheet));
					sheet->RemoveReference();
				}
			}
		}
	}
	else if (strcmp(elementId, "item-elevator") == 0) {
		Rocket::Core::Element* menu = window->GetElementById("elevator-context");
		if (menu) {
			menu->SetProperty("display", "block");
			Rocket::Core::Vector2f offset(element->GetAbsoluteLeft(), window->GetAbsoluteTop() - 90);
			menu->SetOffset(offset, menu->GetOffsetParent());
		}
	}
	else if (strcmp(elementId, "item-stairs") == 0) {
		// Change the blank button to show escalator instead
		Rocket::Core::Element* blankButton = window->GetElementById("item-office");
		if (blankButton) {
			// Find the escalator prototype to get its icon
			Item::AbstractPrototype* escalatorProto = nullptr;
			for (Item::AbstractPrototype* prototype : game->itemFactory.prototypes) {
				if (prototype->id == "escalator") {
					escalatorProto = prototype;
					break;
				}
			}
			if (escalatorProto) {
				blankButton->SetAttribute("id", "item-escalator");
				// Update the background image to show escalator icon
				char style[512];
				int start_px = escalatorProto->icon * 32;
				int end_px = start_px + 32;
				snprintf(style, 512, "button#item-escalator { height:32px; background-image: /simtower/ui/toolbox/items; background-image-s: %dpx %dpx; background-repeat: no-repeat; background-size: 100%% 100%%; }", start_px, end_px);
				Rocket::Core::StyleSheet * sheet = Rocket::Core::Factory::InstanceStyleSheetString(style);
				if (sheet) {
					window->SetStyleSheet(window->GetStyleSheet()->CombineStyleSheet(sheet));
					sheet->RemoveReference();
				}
			}
		}
	}
	else if (strcmp(elementId, "item-escalator") == 0) {
		// Escalator button was long-pressed - show the original stairs context menu
		// This happens when Escalator has replaced Stairs and user wants to switch back
		// Change the Office button to show Stairs instead
		Rocket::Core::Element* officeButton = window->GetElementById("item-office");
		if (officeButton) {
			// Find the stairs prototype to get its icon
			Item::AbstractPrototype* stairsProto = nullptr;
			for (Item::AbstractPrototype* prototype : game->itemFactory.prototypes) {
				if (prototype->id == "stairs") {
					stairsProto = prototype;
					break;
				}
			}
			if (stairsProto) {
				officeButton->SetAttribute("id", "item-stairs");
				// Update the background image to show stairs icon
				char style[512];
				int start_px = stairsProto->icon * 32;
				int end_px = start_px + 32;
				snprintf(style, 512, "button#item-stairs { height:32px; background-image: /simtower/ui/toolbox/items; background-image-s: %dpx %dpx; background-repeat: no-repeat; background-size: 100%% 100%%; }", start_px, end_px);
				Rocket::Core::StyleSheet * sheet = Rocket::Core::Factory::InstanceStyleSheetString(style);
				if (sheet) {
					window->SetStyleSheet(window->GetStyleSheet()->CombineStyleSheet(sheet));
					sheet->RemoveReference();
				}
			}
		}
	}
	// Note: other context menus removed for simplified approach
}

void ToolboxWindow::updateSpeed()
{
	for (int i = 0; i < 4; i++) {
		char c[32];
		snprintf(c, 32, "speed%i", i);
		Rocket::Core::Element * button = window->GetElementById(c);
		assert(button);
		button->SetClass("selected", game->speedMode == i);
	}
}

void ToolboxWindow::updateTool()
{
	for (ElementSet::iterator b = buttons.begin(); b != buttons.end(); b++) {
		std::string buttonId = (*b)->GetId().CString();
		(*b)->SetClass("selected", buttonId == game->selectedTool);
	}
}

void ToolboxWindow::updateAvailability()
{
	// Iterate through prototypes and buttons, set 'locked' class if prototype requires higher rating
	for (int i = 0; i < game->itemFactory.prototypes.size(); i++) {
		Item::AbstractPrototype * prototype = game->itemFactory.prototypes[i];
		char id[128];
		snprintf(id, 128, "item-%s", prototype->id.c_str());
		Rocket::Core::Element * button = window->GetElementById(id);
		if (!button) continue;
		bool locked = (prototype->unlockRating > game->rating);
		button->SetClass("locked", locked);
	}
}