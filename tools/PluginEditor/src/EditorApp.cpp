/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "EditorApp.h"

#include "NativeFileDialog.h"
#include "OspPlugin.h"
#include "PluginManager.h"
#include "PluginObjmap.h"
#include "PluginResources.h"
#include "ItemTypeCatalog.h"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>

#include <TGUI/Widgets/Button.hpp>
#include <TGUI/Backend/Renderer/SFML-Graphics/CanvasSFML.hpp>
#include <TGUI/Widgets/CheckBox.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>
#include <TGUI/Widgets/ComboBox.hpp>
#include <TGUI/Widgets/EditBox.hpp>
#include <TGUI/Widgets/MessageBox.hpp>
#include <TGUI/Widgets/Panel.hpp>
#include <TGUI/Widgets/ScrollablePanel.hpp>

#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace editors
{
	using OT::NativeFileDialog::Result;

	// -------------------------------------------------------------------------
	// Theme — two palettes, selected via OSP_EDITOR_THEME env var.
	// -------------------------------------------------------------------------
	const Theme & Theme::active()
	{
		static Theme t;
		static bool init = false;
		if (init) return t;
		init = true;
		const char * e = std::getenv("OSP_EDITOR_THEME");
		const bool light = (e && std::string(e) == "light");
		if (light)
		{
			t.windowBg    = sf::Color(236, 238, 242);
			t.panelBg     = sf::Color(250, 250, 252);
			t.titleBar    = sf::Color(210, 214, 222);
			t.border      = sf::Color(150, 156, 168);
			t.labelText   = sf::Color(30, 34, 42);
			t.sectionText = sf::Color(20, 90, 160);
			t.noteText    = sf::Color(110, 110, 120);
			t.editBg      = sf::Color(255, 255, 255);
			t.statusText  = sf::Color(40, 44, 52);
			t.btnBg       = sf::Color(70, 120, 190);
			t.canvasBg    = sf::Color(220, 222, 228);
			t.placeholder = sf::Color(200, 202, 210);
			t.rectOutline = sf::Color(200, 40, 40, 220);
		}
		else  // dark (default)
		{
			t.windowBg    = sf::Color(24, 26, 32);
			t.panelBg     = sf::Color(28, 31, 38);
			t.titleBar    = sf::Color(42, 46, 54);
			t.border      = sf::Color(70, 76, 86);
			t.labelText   = sf::Color(210, 216, 224);
			t.sectionText = sf::Color(120, 170, 230);
			t.noteText    = sf::Color(140, 140, 150);
			t.editBg      = sf::Color(40, 44, 52);
			t.statusText  = sf::Color(180, 186, 194);
			t.btnBg       = sf::Color(48, 90, 140);
			t.canvasBg    = sf::Color(32, 35, 42);
			t.placeholder = sf::Color(50, 54, 62);
			t.rectOutline = sf::Color(80, 200, 120, 200);
		}
		return t;
	}

	namespace
	{
		std::string classLabel(OT::PluginDesc::Kind k)
		{
			switch (k)
			{
				case OT::PluginDesc::TenantPlugin:   return "Tenant";
				case OT::PluginDesc::MoverPlugin:    return "Mover";
				case OT::PluginDesc::ToolPalette:    return "ToolPalette";
				case OT::PluginDesc::WorldPlugin:    return "World";
				case OT::PluginDesc::OuterObjPlugin: return "OuterObj";
				default:                             return "Unknown";
			}
		}

		/// Helper to build a labeled EditBox row inside a vertical container.
		/// All dimensions are pre-scaled by kUiScale so callers pass 1x values.
		/// `tooltip` (optional) attaches a hover tooltip to the caption label.
		tgui::EditBox::Ptr makeField(tgui::Panel::Ptr panel, const char * caption,
		                             float x, float y, float w, const char * def,
		                             const char * tooltip = nullptr)
		{
			const float s = EditorApp::kUiScale;
			auto lbl = tgui::Label::create(caption);
			lbl->setPosition(x * s, y * s);
			lbl->setTextSize(int(13 * s));
			lbl->getRenderer()->setTextColor(Theme::active().labelText);
			if (tooltip && tooltip[0])
			{
				auto tip = tgui::Label::create(tooltip);
				tip->setTextSize(int(13 * s));
				tip->getRenderer()->setBackgroundColor(sf::Color(250, 245, 200));
				tip->getRenderer()->setTextColor(sf::Color(30, 30, 30));
				tip->getRenderer()->setBorders({1, 1, 1, 1});
				lbl->setToolTip(tip);
			}
			panel->add(lbl);
			auto edit = tgui::EditBox::create();
			edit->setPosition(x * s, (y + 18) * s);
			edit->setSize(w * s, 24 * s);
			edit->setTextSize(int(14 * s));
			edit->setText(def);
			edit->getRenderer()->setBackgroundColor(Theme::active().editBg);
			panel->add(edit);
			return edit;
		}
	}

	int EditorApp::run()
	{
		// Load a proper UI font (TGUI's built-in default is tiny). TGUI's Font
		// constructor THROWS if the path doesn't resolve, so guard each attempt.
		const char * fontCandidates[] = {
			"data/fonts/Telex-Regular.ttf",
			"../data/fonts/Telex-Regular.ttf",
			"../../data/fonts/Telex-Regular.ttf",
		};
		for (const char * p : fontCandidates)
		{
			try { tgui::Font f(p); if (f) { gui.setFont(f); break; } }
			catch (const tgui::Exception &) { /* try next candidate */ }
		}
		// If none loaded we fall back to TGUI's default; sizes below are still
		// large enough to be legible.

		// Size the window to fit the scaled layout (inspector 340 + preview 752
		// = ~1092 wide at 1x; scaled up with comfortable padding).
		window.create(sf::VideoMode({static_cast<unsigned>(1120 * kUiScale),
		                             static_cast<unsigned>(740 * kUiScale)}),
		              "OpenSkyscraper Plugin Editor");
		window.setVerticalSyncEnabled(true);
		gui.setTarget(window);

		buildUi();
		setStatus("Ready. Use File \u25B8 Open .t2p or Open .osp-plugin to begin.");
		updateTitle();

		// A document requested on the command line (main() validated the
		// path kind). Runs after buildUi so the widgets/status exist.
		if (!cliOpenPath.empty())
		{
			const bool isT2p = cliOpenPath.size() >= 4
				&& cliOpenPath.compare(cliOpenPath.size() - 4, 4, ".t2p") == 0;
			const bool ok = isT2p ? openT2pPath(cliOpenPath) : openOspPath(cliOpenPath);
			if (!ok)
			{
				setStatus("Failed to open " + cliOpenPath, true);
				std::cerr << "PluginEditor: failed to open " << cliOpenPath << "\n";
				// Headless mode must not fake success with a shot of an
				// empty editor.
				if (!cliScreenshotPath.empty()) return 1;
			}
		}
		while (window.isOpen())
		{
			while (const std::optional event = window.pollEvent())
			{
				if (const auto *resized = event->getIf<sf::Event::Resized>())
				{
					sf::View view(sf::FloatRect({0, 0},
						{static_cast<float>(resized->size.x), static_cast<float>(resized->size.y)}));
					window.setView(view);
					gui.setAbsoluteView({0.f, 0.f,
						static_cast<float>(resized->size.x), static_cast<float>(resized->size.y)});
				}
				else if (event->is<sf::Event::Closed>())
				{
					window.close();
				}
				gui.handleEvent(*event);
			}

			window.clear(Theme::active().windowBg);
			gui.draw();
			window.display();

			// Headless capture (--screenshot): the first fully rendered frame
			// is the proof that the document loaded and drew; save it and
			// exit (mirrors the game's startup frame capture).
			if (!cliScreenshotPath.empty())
			{
				sf::Texture frame(window.getSize());
				frame.update(window);
				if (!frame.copyToImage().saveToFile(cliScreenshotPath))
				{
					std::cerr << "PluginEditor: unable to save frame capture to "
					          << cliScreenshotPath << "\n";
					return 1;
				}
				return 0;
			}
		}
		return 0;
	}

	void EditorApp::buildUi()
	{
		const float s = kUiScale;  // local alias for brevity

		// --- Menu bar ---
		menu = tgui::MenuBar::create();
		menu->setHeight(26 * s);
		menu->setTextSize(int(15 * s));
		menu->addMenu("File");
		menu->addMenuItem("New Tenant...");
		menu->addMenuItem("Open .t2p...");
		menu->addMenuItem("Open .osp-plugin...");
		menu->addMenuItem("-");
		menu->addMenuItem("Save");
		menu->addMenuItem("Save As...");
		menu->addMenuItem("-");
		menu->addMenuItem("Close");
		menu->addMenuItem("Quit");
		menu->addMenu("Plugin");
		menu->addMenuItem("Convert (t2p \u2192 osp)");
		menu->onMenuItemClick([this](const std::vector<tgui::String> & items) {
			const auto & item = items.back();
			// Destructive/navigation actions go through the unsaved-changes
			// guard first; the pending action only runs after a clean
			// Save, an explicit Discard, or when nothing is dirty.
			if (item == "New Tenant...") confirmThen([this] { newTenant(); });
			else if (item == "Open .t2p...") confirmThen([this] { actionOpenT2p(); });
			else if (item == "Open .osp-plugin...") confirmThen([this] { actionOpenOsp(); });
			else if (item == "Save") actionSave();
			else if (item == "Save As...") actionSaveAs();
			else if (item == "Convert (t2p \u2192 osp)") actionConvert();
			else if (item == "Close") confirmThen([this] { closeDoc(); });
			else if (item == "Quit") confirmThen([this] { window.close(); });
		});
		// Theme the menu bar (its default renderer is dark regardless of theme).
		{
			auto r = menu->getRenderer();
			r->setBackgroundColor(Theme::active().titleBar);
			r->setTextColor(Theme::active().labelText);
			r->setSelectedBackgroundColor(Theme::active().btnBg);
		}
		gui.add(menu);

		// --- Inspector panel (left) ---
		const Theme & th = Theme::active();
		// Place the panels a few px below the menu bar so their title text
		// doesn't crowd the menu items.
		const float panelY = (26 + 6) * s;
		inspector = tgui::ChildWindow::create();
		inspector->setTitle("Inspector");
		inspector->setTitleButtons(tgui::ChildWindow::TitleButton::None);
		inspector->setPosition({0, panelY});
		inspector->setClientSize({340 * s, 700 * s});
		{
			auto r = inspector->getRenderer();
			r->setBackgroundColor(th.panelBg);
			r->setTitleBarColor(th.titleBar);
			r->setBorderColor(th.border);
			r->setBorders(1);
			r->setTitleBarHeight(int(22 * s));
		}
		auto panel = tgui::ScrollablePanel::create();
		panel->getRenderer()->setBackgroundColor(sf::Color::Transparent);
		panel->setPosition({4 * s, 4 * s});
		panel->setSize({324 * s, 660 * s});
		// Content is taller than the viewport (16+ fields × 48px); the panel scrolls.
		panel->setContentSize({310 * s, 900 * s});
		inspector->add(panel);

		const float x = 4, w = 260;
		float y = 4;
		auto section = [&](const char * title) {
			auto lbl = tgui::Label::create(title);
			lbl->setPosition({x * s, y * s});
			lbl->setTextSize(int(15 * s));
			lbl->getRenderer()->setTextColor(th.sectionText);
			panel->add(lbl);
			y += 24;
		};

		section("Identity");
		itemTypeEdit  = makeField(panel, "Type code", x, y, 120, "NERH",
			"4-character YootTower item-type code (mItemType).\n"
			"Opaque identifier carried over from the .t2p source;\n"
			"e.g. NERH=Apartment, GRUB=Burger, RATS=Stairs.\n"
			"Editing it only re-tags the saved plugin."); y += 48;
		nameEdit      = makeField(panel, "Display name", x, y, w, "",
			"The in-game name shown in the toolbox and inspector."); y += 48;
		gameLevelEdit = makeField(panel, "Stars required", x, y, 80, "1",
			"Minimum tower star rating at which this item unlocks\n"
			"(1 = Star 1 ... 5 = Star 5)."); y += 48;
		idEdit        = makeField(panel, "Internal ID (mID)", x, y, w, "0",
			"Numeric cross-reference key into the YootTower category\n"
			"(TNDF/MVDF) tables. Opaque; usually preserved from source."); y += 48;
		markerEdit    = makeField(panel, "Category marker", x, y, 120, "****",
			"4-character grouping code. \"****\" = default;\n"
			"\"NORF\" marks the hotel-room family.\n"
			"Opaque; preserve from source unless you know the group."); y += 48;

		section("Placement (footprint)");
		placeWEdit    = makeField(panel, "Footprint width", x, y, 80, "0",
			"DPDF cell width (e.g. 70 = 1 floor tile).\n"
			"How wide the item is when placed."); y += 48;
		placeHEdit    = makeField(panel, "Footprint height", x, y, 80, "0",
			"DPDF cell height (e.g. 50 = 1 story).\n"
			"How tall the item is when placed."); y += 48;
		placeBaseXEdit = makeField(panel, "Base offset X", x, y, 80, "0",
			"DPDF baseX anchor (opaque placement param)."); y += 48;
		placeBaseYEdit = makeField(panel, "Base offset Y", x, y, 80, "0",
			"DPDF baseY anchor (opaque placement param)."); y += 48;

		section("Price (shops only)");
		priceBaseEdit = makeField(panel, "Construction cost", x, y, w, "0",
			"PLDF basePrice: the one-time cost to build this item."); y += 48;
		priceF1Edit   = makeField(panel, "Price factor 1", x, y, 80, "0",
			"PLDF f1. Semantic meaning not yet decoded from T2DLL.dll\n"
			"(likely an income/expense multiplier)."); y += 48;
		priceF2Edit   = makeField(panel, "Price factor 2", x, y, 80, "0",
			"PLDF f2. Meaning not yet decoded (income/expense)."); y += 48;
		priceF3Edit   = makeField(panel, "Price factor 3", x, y, 80, "0",
			"PLDF f3. Meaning not yet decoded (income/expense)."); y += 48;
		priceF4Edit   = makeField(panel, "Price factor 4", x, y, 80, "0",
			"PLDF f4. Meaning not yet decoded (income/expense)."); y += 48;
		priceF5Edit   = makeField(panel, "Price factor 5", x, y, 80, "0",
			"PLDF f5. Meaning not yet decoded (income/expense)."); y += 48;

		// Transport (MVDF) — Mover plugins only. Hidden until a Mover loads.
		transportHeader = tgui::Label::create("Transport pricing (movers only)");
		transportHeader->setPosition({x * s, y * s});
		transportHeader->setTextSize(int(15 * s));
		transportHeader->getRenderer()->setTextColor(th.sectionText);
		transportHeader->setVisible(false);
		panel->add(transportHeader);
		y += 24;
		transportPriceEdit    = makeField(panel, "Build price", x, y, w, "",
			"MVDF build price string, e.g.\n"
			"\"$5000\" (stairs) or \"$150000:$50000/car\" (elevator)."); y += 48;
		transportQuarterlyEdit = makeField(panel, "Quarterly cost", x, y, w, "",
			"MVDF recurring maintenance, e.g. \"$12000/Q\"."); y += 48;
		transportPriceEdit->setVisible(false);
		transportQuarterlyEdit->setVisible(false);

		// Any inspector edit marks the doc dirty — this is what engages the
		// unsaved-changes guard after field edits. refreshFromDoc() pushes
		// doc values through the same signal, so it suppresses the flag.
		auto wireDirty = [this](tgui::EditBox::Ptr e) {
			e->onTextChange([this] {
				if (!refreshingFields) { dirty = true; updateTitle(); }
			});
		};
		wireDirty(itemTypeEdit); wireDirty(nameEdit); wireDirty(gameLevelEdit);
		wireDirty(idEdit); wireDirty(markerEdit);
		wireDirty(placeWEdit); wireDirty(placeHEdit);
		wireDirty(placeBaseXEdit); wireDirty(placeBaseYEdit);
		wireDirty(priceBaseEdit); wireDirty(priceF1Edit); wireDirty(priceF2Edit);
		wireDirty(priceF3Edit); wireDirty(priceF4Edit); wireDirty(priceF5Edit);
		wireDirty(transportPriceEdit); wireDirty(transportQuarterlyEdit);

		gui.add(inspector);

		// --- Preview window (right) ---
		previewWindow = tgui::ChildWindow::create();
		previewWindow->setTitle("Sprite Preview");
		previewWindow->setTitleButtons(tgui::ChildWindow::TitleButton::None);
		previewWindow->setPosition({344 * s, panelY});
		previewWindow->setClientSize({768 * s, 700 * s});
		{
			auto r = previewWindow->getRenderer();
			r->setBackgroundColor(th.windowBg);
			r->setTitleBarColor(th.titleBar);
			r->setBorderColor(th.border);
			r->setBorders(1);
			r->setTitleBarHeight(int(22 * s));
		}

		sheetSelector = tgui::ComboBox::create();
		sheetSelector->setPosition({8 * s, 4 * s});
		sheetSelector->setSize({200 * s, 26 * s});
		sheetSelector->setTextSize(int(14 * s));
		sheetSelector->addItem("Toolbox (100)");
		sheetSelector->addItem("Ingame (1000)");
		sheetSelector->addItem("Occupants (1100)");
		sheetSelector->addItem("Walking (200)");
		sheetSelector->setSelectedItemByIndex(1);
		sheetSelector->onItemSelect([this](const tgui::String &) {
			shownSheet = sheetSelector->getSelectedItemIndex();
			if (shownSheet < 0) shownSheet = 1;
			renderPreview();
		});
		previewWindow->add(sheetSelector);

		showRectsCheck = tgui::CheckBox::create("Overlay OBJMAP rects");
		showRectsCheck->setPosition({220 * s, 6 * s});
		showRectsCheck->setTextSize(int(14 * s));
		showRectsCheck->setChecked(true);
		showRectsCheck->onChange([this](bool) { renderPreview(); });
		previewWindow->add(showRectsCheck);

		// Asset operation buttons (act on the currently-shown sheet).
		auto mkBtn = [&](const char * label, float bx) -> tgui::Button::Ptr {
			auto b = tgui::Button::create(label);
			b->setPosition({bx * s, 3 * s});
			b->setSize({110 * s, 28 * s});
			b->setTextSize(int(14 * s));
			b->getRenderer()->setBackgroundColor(th.btnBg);
			previewWindow->add(b);
			return b;
		};
		importBtn    = mkBtn("Import PNG", 420);
		exportBtn    = mkBtn("Export PNG", 540);
		maskBtn      = mkBtn("Pick Mask", 660);
		clearMaskBtn = mkBtn("Clear Mask", 780);
		importBtn->onPress([this] { actionImportPng(); });
		exportBtn->onPress([this] { actionExportPng(); });
		maskBtn->onPress([this] { actionPickMask(); });
		clearMaskBtn->onPress([this] { actionClearMask(); });

		canvas = tgui::CanvasSFML::create({740.f * s, 650.f * s});
		canvas->setPosition({8 * s, 40 * s});
		canvas->onClick([this](tgui::Vector2f pos) { onCanvasClick(pos); });
		previewWindow->add(canvas);

		gui.add(previewWindow);

		// --- Status bar ---
		statusLabel = tgui::Label::create("Ready.");
		statusLabel->setPosition({8 * s, 744 * s});
		statusLabel->setTextSize(int(14 * s));
		statusLabel->getRenderer()->setTextColor(Theme::active().statusText);
		gui.add(statusLabel);
	}

	void EditorApp::setStatus(const std::string & msg, bool isError)
	{
		if (statusLabel)
		{
			statusLabel->setText(msg);
			// Error text is red in both themes; normal text uses the theme status color.
			statusLabel->getRenderer()->setTextColor(isError ? sf::Color(205, 60, 60) : Theme::active().statusText);
		}
	}

	void EditorApp::updateTitle()
	{
		std::string t = "OpenSkyscraper Plugin Editor";
		if (doc)
		{
			t += " - ";
			t += (doc->attr.name.empty() ? std::string("<unnamed>") : doc->attr.name);
			if (dirty) t += " *";
		}
		window.setTitle(t);
	}

	void EditorApp::uploadImage(const sf::Image & img, sf::Texture & tex, bool & valid)
	{
		valid = false;
		if (img.getSize().x == 0 || img.getSize().y == 0) return;
		if (tex.loadFromImage(img)) valid = true;
	}

	void EditorApp::refreshTextures()
	{
		if (!doc) { toolboxValid = ingameValid = occupantsValid = walkingValid = false; return; }
		uploadImage(doc->toolbox.image,  toolboxTex,  toolboxValid);
		uploadImage(doc->ingame.image,   ingameTex,   ingameValid);
		uploadImage(doc->occupants.image, occupantsTex, occupantsValid);
		uploadImage(doc->walking.image,   walkingTex,   walkingValid);
	}

	void EditorApp::refreshFromDoc()
	{
		if (!doc) return;
		// Mirroring doc values into the fields fires onTextChange; keep the
		// dirty flag out of it so opening a document doesn't count as editing.
		refreshingFields = true;
		const OT::OspPlugin & p = *doc;
		itemTypeEdit->setText(p.attr.itemType.str());
		// Annotate the Type code with the catalog lookup so an author sees the
		// shipped name (e.g. "Apartment") for known codes.
		if (const OT::ItemTypeEntry * e = OT::lookupItemType(p.attr.itemType.str()))
		{
			std::string tip = std::string("Known YootTower item: ") + e->name + " (" + e->cls + ")";
			auto t = tgui::Label::create(tip);
			t->setTextSize(int(13 * kUiScale));
			t->getRenderer()->setBackgroundColor(sf::Color(250, 245, 200));
			t->getRenderer()->setTextColor(sf::Color(30, 30, 30));
			t->getRenderer()->setBorders({1, 1, 1, 1});
			itemTypeEdit->setToolTip(t);
		}
		else
		{
			itemTypeEdit->setToolTip(nullptr);  // unknown / hand-authored code
		}
		nameEdit->setText(p.attr.name);
		gameLevelEdit->setText(std::to_string(p.attr.gameLevel));
		idEdit->setText(std::to_string(p.attr.id));
		markerEdit->setText(p.attr.marker.str());
		placeWEdit->setText(std::to_string(p.placement.w));
		placeHEdit->setText(std::to_string(p.placement.h));
		placeBaseXEdit->setText(std::to_string(p.placement.baseX));
		placeBaseYEdit->setText(std::to_string(p.placement.baseY));
		priceBaseEdit->setText(std::to_string(p.price.basePrice));
		priceF1Edit->setText(std::to_string(p.price.f1));
		priceF2Edit->setText(std::to_string(p.price.f2));
		priceF3Edit->setText(std::to_string(p.price.f3));
		priceF4Edit->setText(std::to_string(p.price.f4));
		priceF5Edit->setText(std::to_string(p.price.f5));

		// Transport (MVDF) — show only for Mover plugins.
		const bool isMover = (p.pluginClass == OT::PluginDesc::MoverPlugin);
		if (transportHeader)    transportHeader->setVisible(isMover);
		if (transportPriceEdit)    { transportPriceEdit->setVisible(isMover);    transportPriceEdit->setText(p.transport.priceString); }
		if (transportQuarterlyEdit){ transportQuarterlyEdit->setVisible(isMover); transportQuarterlyEdit->setText(p.transport.quarterlyString); }
		refreshingFields = false;
	}

	void EditorApp::pullFromInspector()
	{
		if (!doc) return;
		OT::OspPlugin & p = *doc;
		auto get4 = [](tgui::EditBox::Ptr e, OT::FourCC & out) {
			std::string s = e->getText().toStdString();
			for (int i = 0; i < 4; ++i) out.c[i] = (i < int(s.size()) ? s[i] : ' ');
		};
		auto getU = [](tgui::EditBox::Ptr e) -> uint32_t {
			return uint32_t(std::strtoul(e->getText().toStdString().c_str(), nullptr, 10));
		};
		get4(itemTypeEdit, p.attr.itemType);
		p.attr.name = nameEdit->getText().toStdString();
		p.attr.gameLevel = getU(gameLevelEdit);
		p.attr.id = getU(idEdit);
		get4(markerEdit, p.attr.marker);
		p.placement.w     = getU(placeWEdit);
		p.placement.h     = getU(placeHEdit);
		p.placement.baseX = getU(placeBaseXEdit);
		p.placement.baseY = getU(placeBaseYEdit);
		if (p.placement.w > 0 || p.placement.h > 0) p.placement.valid = true;
		p.price.basePrice = getU(priceBaseEdit);
		p.price.f1 = getU(priceF1Edit);
		p.price.f2 = getU(priceF2Edit);
		p.price.f3 = getU(priceF3Edit);
		p.price.f4 = getU(priceF4Edit);
		p.price.f5 = getU(priceF5Edit);
		if (p.price.valid || p.price.basePrice > 0) p.price.valid = true;
		// Transport (MVDF) — read back only the editable price strings.
		if (p.pluginClass == OT::PluginDesc::MoverPlugin)
		{
			p.transport.priceString    = transportPriceEdit->getText().toStdString();
			p.transport.quarterlyString = transportQuarterlyEdit->getText().toStdString();
			if (!p.transport.priceString.empty() || !p.transport.quarterlyString.empty())
				p.transport.valid = true;
		}
		p.attr.valid = true;
	}

	void EditorApp::renderPreview()
	{
		if (!canvas) return;
		canvas->clear(Theme::active().canvasBg);

		sf::Texture * tex = nullptr;
		const std::vector<OT::ObjmapBlock> * blocks = nullptr;
		bool ok = false;
		switch (shownSheet)
		{
			case 0: tex = &toolboxTex;  ok = toolboxValid;  blocks = doc ? &doc->toolbox.blocks  : nullptr; break;
			case 1: tex = &ingameTex;   ok = ingameValid;   blocks = doc ? &doc->ingame.blocks   : nullptr; break;
			case 2: tex = &occupantsTex; ok = occupantsValid; blocks = nullptr; break;
			case 3: tex = &walkingTex;  ok = walkingValid;  blocks = doc ? &doc->walking.blocks  : nullptr; break;
		}

		if (!ok || !tex)
		{
			// No sheet loaded: draw a placeholder rect in the canvas center.
			const sf::Vector2f cs = canvas->getSize();
			sf::RectangleShape r({cs.x * 0.5f, cs.y * 0.15f});
			r.setPosition({cs.x * 0.25f, cs.y * 0.42f});
			r.setFillColor(Theme::active().placeholder);
			canvas->draw(r);
			canvas->display();
			return;
		}

		// Fit the sheet into the canvas (scale down large sheets). Leave a
		// margin so the overlay rects aren't flush against the edge.
		sf::Sprite sprite(*tex);
		const sf::Vector2u sz = tex->getSize();
		const sf::Vector2f cs = canvas->getSize();
		const float margin = 12.f;
		const float maxW = cs.x - 2 * margin, maxH = cs.y - 2 * margin;
		float scale = std::min(maxW / float(sz.x), maxH / float(sz.y));
		if (scale > 4) scale = 4;   // cap upscale so tiny icons aren't huge
		sprite.setScale({scale, scale});
		sprite.setPosition({margin, margin});
		canvas->draw(sprite);

		// Overlay the OBJMAP rects (scaled to match).
		if (showRectsCheck && showRectsCheck->isChecked() && blocks)
		{
			for (const OT::ObjmapBlock & b : *blocks)
			{
				if (b.rect.width() <= 0 || b.rect.height() <= 0) continue;
				sf::RectangleShape rr({b.rect.width() * scale, b.rect.height() * scale});
				rr.setPosition({margin + b.rect.left * scale, margin + b.rect.top * scale});
				rr.setFillColor(sf::Color::Transparent);
				rr.setOutlineColor(Theme::active().rectOutline);
				rr.setOutlineThickness(1);
				canvas->draw(rr);
			}
		}
		canvas->display();
	}

	// -----------------------------------------------------------------------
	// Actions
	// -----------------------------------------------------------------------
	void EditorApp::actionOpenT2p()
	{
		std::string path;
		Result r = OT::NativeFileDialog::openPluginFile("Open YootTower plugin (.t2p)", path);
		if (r != Result::Selected) { if (r == Result::Unavailable) setStatus("No file dialog available.", true); return; }
		openT2pPath(path);
	}

	bool EditorApp::openT2pPath(const std::string & path)
	{
		OT::Plugin plugin(OT::Path(path).name(), OT::Path(path));
		if (!plugin.ensureLoaded())
		{
			setStatus("Failed to load PE: " + path, true);
			return false;
		}
		OT::OspPlugin p;
		OT::OspResult cr = OT::convertT2pToOsp(plugin, p);
		if (!cr.ok) { setStatus("Convert failed: " + cr.message, true); return false; }

		doc = std::move(p);
		loadedFromPath.clear();   // converted from t2p — Save needs Save As first
		dirty = false;
		refreshTextures();
		refreshFromDoc();
		renderPreview();
		setStatus("Loaded " + path + " (read-only source). Use Save As to write an .osp-plugin.");
		updateTitle();
		return true;
	}

	void EditorApp::actionOpenOsp()
	{
		std::string path;
		Result r = OT::NativeFileDialog::openDirectory("Open OpenSkyscraper plugin directory", path);
		if (r != Result::Selected) { if (r == Result::Unavailable) setStatus("No directory dialog available.", true); return; }
		openOspPath(path);
	}

	bool EditorApp::openOspPath(const std::string & path)
	{
		OT::OspPlugin p;
		OT::OspResult lr = OT::loadOspPlugin(path, p);
		if (!lr.ok) { setStatus("Open failed: " + lr.message, true); return false; }

		doc = std::move(p);
		loadedFromPath = path;
		dirty = false;
		refreshTextures();
		refreshFromDoc();
		renderPreview();
		setStatus("Opened " + path);
		updateTitle();
		return true;
	}

	bool EditorApp::ensureLoadedForSave()
	{
		if (!doc) { setStatus("Nothing to save.", true); return false; }
		pullFromInspector();
		return true;
	}

	void EditorApp::actionSave()
	{
		if (!ensureLoadedForSave()) return;
		if (loadedFromPath.empty()) { actionSaveAs(); return; }
		OT::OspResult r = OT::saveOspPlugin(loadedFromPath, *doc, true);
		if (!r.ok) { setStatus("Save failed: " + r.message, true); return; }
		dirty = false;
		setStatus("Saved to " + loadedFromPath);
		updateTitle();
	}

	void EditorApp::actionSaveAs()
	{
		if (!ensureLoadedForSave()) return;
		std::string path;
		std::string def = doc->attr.name;
		if (def.empty()) def = "NewPlugin";
		def += ".osp-plugin";
		Result r = OT::NativeFileDialog::saveDirectory("Save OpenSkyscraper plugin as directory", def, path);
		if (r != Result::Selected) { if (r == Result::Unavailable) setStatus("No directory dialog available.", true); return; }

		OT::OspResult sr = OT::saveOspPlugin(path, *doc, true);
		if (!sr.ok) { setStatus("Save failed: " + sr.message, true); return; }
		loadedFromPath = path;
		dirty = false;
		setStatus("Saved to " + path);
		updateTitle();
	}

	void EditorApp::actionConvert()
	{
		// "Convert" is the same as Save As for a t2p-sourced doc: write the
		// current in-memory OspPlugin (sourceFormat=yoottower-t2p) to a fresh
		// .osp-plugin directory. Offered as its own menu item for clarity.
		if (!doc) { setStatus("Open a .t2p first.", true); return; }
		actionSaveAs();
	}

	void EditorApp::newTenant()
	{
		OT::OspPlugin p;
		p.sourceFormat = "scratch";
		p.pluginClass = OT::PluginDesc::TenantPlugin;
		p.attr.valid = true;
		p.attr.itemType.c[0]='N'; p.attr.itemType.c[1]='E'; p.attr.itemType.c[2]='R'; p.attr.itemType.c[3]='H';
		p.attr.marker.c[0]='*'; p.attr.marker.c[1]='*'; p.attr.marker.c[2]='*'; p.attr.marker.c[3]='*';
		p.attr.name = "New Tenant";
		p.attr.gameLevel = 1;
		doc = std::move(p);
		loadedFromPath.clear();
		dirty = true;
		toolboxValid = ingameValid = occupantsValid = walkingValid = false;
		refreshFromDoc();
		renderPreview();
		setStatus("New Tenant created. Import sprite sheets and Save As.");
		updateTitle();
	}

	void EditorApp::closeDoc()
	{
		doc.reset();
		loadedFromPath.clear();
		dirty = false;
		toolboxValid = ingameValid = occupantsValid = walkingValid = false;
		renderPreview();
		updateTitle();
	}

	// -----------------------------------------------------------------------
	// Unsaved-changes guard. Wraps a pending action: with a dirty document
	// the user is asked "Save changes?" first — Save routes through
	// actionSave() and only continues on success, Discard continues without
	// saving, Cancel aborts the pending action.
	// -----------------------------------------------------------------------
	void EditorApp::confirmThen(const std::function<void()> & action)
	{
		if (!doc || !dirty) { action(); return; }

		// A second trigger while the dialog is open replaces the pending
		// action instead of stacking dialogs.
		pendingAction = action;
		if (confirmBox)
		{
			gui.remove(confirmBox);
			confirmBox = nullptr;
		}
		confirmBox = tgui::MessageBox::create("Unsaved changes",
		                                      "Save changes before continuing?",
		                                      {"Save", "Discard", "Cancel"});
		confirmBox->setPosition({"(parent.width - width) / 2",
		                         "(parent.height - height) / 2"});
		gui.add(confirmBox);
		confirmBox->onButtonPress([this](const tgui::String & button) {
			tgui::MessageBox::Ptr box = confirmBox;
			confirmBox = nullptr;
			if (box) gui.remove(box);
			if (button == "Cancel") { pendingAction = nullptr; return; }
			if (button == "Save")
			{
				actionSave();
				if (dirty) { pendingAction = nullptr; return; }  // save failed/cancelled
			}
			// Discard — or a successful Save — proceeds with the action.
			std::function<void()> run = pendingAction;
			pendingAction = nullptr;
			if (run) run();
		});
	}

	void EditorApp::loadSheetIntoDoc(int which, const std::string & pngPath)
	{
		if (!doc) return;
		OT::OspSpriteSheet * sh = nullptr;
		const char * rel = nullptr;
		// New plugins follow the shipped OpenTower convention: sheets live
		// under their numeric RT_BITMAP ids (the game-side names).
		if (which == 0) { sh = &doc->toolbox;  rel = "sprites/100.png"; }
		else if (which == 1) { sh = &doc->ingame;   rel = "sprites/1000.png"; }
		else if (which == 2) { sh = &doc->occupants; rel = "sprites/1100.png"; }
		else { sh = &doc->walking; rel = "sprites/200.png"; }
		if (!sh->image.loadFromFile(pngPath)) { setStatus("Failed to load PNG: " + pngPath, true); return; }
		sh->present = true;
		sh->relPath = rel;
		if (sh->mask) sh->image.createMaskFromColor(*sh->mask);
		dirty = true;
		refreshTextures();
		renderPreview();
		setStatus("Imported " + pngPath);
		updateTitle();
	}

	// -----------------------------------------------------------------------
	// Asset operations on the currently-shown sheet
	// -----------------------------------------------------------------------
	OT::OspSpriteSheet * EditorApp::currentSheet()
	{
		if (!doc) return nullptr;
		if (shownSheet == 0) return &doc->toolbox;
		if (shownSheet == 1) return &doc->ingame;
		if (shownSheet == 2) return &doc->occupants;
		return &doc->walking;
	}

	void EditorApp::actionImportPng()
	{
		if (!doc) { setStatus("Open or create a plugin first.", true); return; }
		std::string path;
		Result r = OT::NativeFileDialog::openPngFile("Import PNG sprite sheet", path);
		if (r != Result::Selected) { if (r == Result::Unavailable) setStatus("No file dialog available.", true); return; }
		OT::OspSpriteSheet * sh = currentSheet();
		if (!sh) return;
		const char * rel = (shownSheet == 0 ? "sprites/100.png"
		                    : shownSheet == 1 ? "sprites/1000.png"
		                    : shownSheet == 2 ? "sprites/1100.png" : "sprites/200.png");
		// Reload from the unmasked source so a re-import isn't doubly-keyed.
		sf::Image raw;
		if (!raw.loadFromFile(path)) { setStatus("Failed to load PNG: " + path, true); return; }
		sh->image = raw;
		sh->present = true;
		sh->relPath = rel;
		if (sh->mask) sh->image.createMaskFromColor(*sh->mask);
		dirty = true;
		refreshTextures();
		renderPreview();
		setStatus("Imported " + path + " into " + rel);
		updateTitle();
	}

	void EditorApp::actionExportPng()
	{
		OT::OspSpriteSheet * sh = currentSheet();
		if (!sh || !sh->present || sh->image.getSize().x == 0)
		{
			setStatus("No sheet to export.", true);
			return;
		}
		std::string def = doc && !doc->attr.name.empty() ? doc->attr.name : "sheet";
		def += shownSheet == 0 ? "_toolbox" : shownSheet == 1 ? "_ingame"
		     : shownSheet == 2 ? "_occupants" : "_walking";
		def += ".png";
		std::string path;
		Result r = OT::NativeFileDialog::savePngFile("Export PNG sprite sheet", def, path);
		if (r != Result::Selected) { if (r == Result::Unavailable) setStatus("No file dialog available.", true); return; }
		// Export the masked image (transparent pixels already keyed out).
		if (!sh->image.saveToFile(path)) { setStatus("Failed to write PNG: " + path, true); return; }
		setStatus("Exported " + path);
	}

	void EditorApp::actionPickMask()
	{
		if (!doc) { setStatus("Open or create a plugin first.", true); return; }
		eyedropperActive = true;
		setStatus("Eyedropper: click a pixel in the preview to set the transparent color.");
	}

	void EditorApp::actionClearMask()
	{
		OT::OspSpriteSheet * sh = currentSheet();
		if (!sh) return;
		sh->mask.reset();
		sh->maskHow = OT::MaskDiscovery::None;
		dirty = true;
		setStatus("Cleared transparent color for the current sheet.");
		updateTitle();
	}

	void EditorApp::onCanvasClick(tgui::Vector2f pos)
	{
		if (!eyedropperActive || !doc) return;
		eyedropperActive = false;
		OT::OspSpriteSheet * sh = currentSheet();
		if (!sh || sh->image.getSize().x == 0) { setStatus("No sheet to sample.", true); return; }

		// The preview draws the sheet scaled, offset by `margin` within the
		// canvas (see renderPreview). Invert that to recover the source pixel.
		const sf::Vector2u sz = sh->image.getSize();
		const sf::Vector2f cs = canvas->getSize();
		const float margin = 12.f;
		const float maxW = cs.x - 2 * margin, maxH = cs.y - 2 * margin;
		float scale = std::min(maxW / float(sz.x), maxH / float(sz.y));
		if (scale > 4) scale = 4;
		float px = (pos.x - margin) / scale;
		float py = (pos.y - margin) / scale;
		unsigned ix = static_cast<unsigned>(px);
		unsigned iy = static_cast<unsigned>(py);
		if (ix >= sz.x || iy >= sz.y) { setStatus("Click within the sprite.", true); return; }

		// Sample from the masked-out image is wrong (those pixels are now
		// transparent). Re-import logic keeps the raw pixels only at convert
		// time; for an imported sheet the mask hasn't been applied yet if the
		// user clears it first. To stay correct, sample from the texture we
		// show (post-mask) — but the keyed color is by definition transparent
		// there. So we require the mask to be cleared before picking. Tell the
		// user to Clear Mask first if a mask is set.
		if (sh->mask)
		{
			setStatus("Clear Mask first, then Pick Mask to sample a color.", true);
			return;
		}
		sf::Color c = sh->image.getPixel({ix, iy});
		sh->mask = c;
		sh->maskHow = OT::MaskDiscovery::Manual;
		sh->image.createMaskFromColor(c);
		dirty = true;
		refreshTextures();
		renderPreview();
		char buf[64];
		std::snprintf(buf, sizeof(buf), "Set transparent color to (%d, %d, %d).", c.r, c.g, c.b);
		setStatus(buf);
		updateTitle();
	}
}
