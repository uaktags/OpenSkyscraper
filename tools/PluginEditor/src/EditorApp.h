/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// PluginEditor — standalone editor for the OpenSkyscraper plugin format.
///
/// Loads YootTower .t2p plugins (read-only) or native .osp-plugin directories,
/// lets the user edit the Tenant-class fields (identity, placement, price,
/// toolbox icon, ingame sprite, occupant animation), and saves ONLY to the
/// OpenSkyscraper .osp-plugin format. See doc/openskyscraper/plugin-format.md.
///
/// This is the Tenant vertical slice: one plugin class round-tripped end to
/// end. Mover/World/OuterObj/ToolPalette classes are out of scope for now.
#pragma once
#include "OspPlugin.h"
#include "PluginObjmap.h"
#include "PluginResources.h"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Window.hpp>

#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/TGUI.hpp>

#include <memory>
#include <optional>
#include <string>

namespace editors
{
	/// UI color palette. Two themes: a dark editor theme (default) and a
	/// bright theme for screenshots / accessibility / light-preference users.
	/// Selected at startup via the OSP_EDITOR_THEME env var
	/// ("light" | "dark" | "auto"; auto == dark).
	struct Theme
	{
		static const Theme & active();  ///< reads OSP_EDITOR_THEME once

		sf::Color windowBg;        ///< sf::RenderWindow clear color
		sf::Color panelBg;         ///< inspector / preview client background
		sf::Color titleBar;        ///< child-window title bar
		sf::Color border;
		sf::Color labelText;       ///< field captions
		sf::Color sectionText;     ///< blue section headers
		sf::Color noteText;        ///< dim helper notes
		sf::Color editBg;          ///< editbox background
		sf::Color statusText;
		sf::Color btnBg;           ///< asset op buttons
		sf::Color canvasBg;        ///< preview canvas clear color
		sf::Color placeholder;     ///< "no sheet" placeholder rect
		sf::Color rectOutline;     ///< OBJMAP overlay rect outline
	};
	/// The editor's top-level controller: owns the SFML window, the TGUI gui,
	/// the in-memory OspPlugin being edited, and the menu/panel wiring.
	/// Mirrors the game's Application in shape but is far smaller — no game
	/// state, no managers, just load → edit → save.
	class EditorApp
	{
	public:
		int run();

		/// Global UI scale. All widget sizes/positions are multiplied by this
		/// (mirrors the game's uiScale convention). 1.5 gives comfortable
		/// legibility on standard-density displays with TGUI's default font.
		/// Public so the buildUi helper can read it.
		static constexpr float kUiScale = 1.5f;

	private:

		// Window + GUI (constructed in run()).
		sf::RenderWindow window;
		tgui::Gui gui{window};

		// The document under edit. Null when nothing is loaded.
		std::optional<OT::OspPlugin> doc;
		bool dirty = false;
		std::string loadedFromPath;  // OSP dir last loaded/saved (for Save w/o As)

		// Sprite-sheet textures derived from the doc (rebuilt on load/edit).
		sf::Texture toolboxTex, ingameTex, occupantsTex, walkingTex;
		bool toolboxValid = false, ingameValid = false, occupantsValid = false, walkingValid = false;

		// UI handles (created once in buildUi).
		tgui::MenuBar::Ptr menu;
		tgui::ChildWindow::Ptr inspector;
		tgui::ChildWindow::Ptr previewWindow;
		tgui::CanvasSFML::Ptr canvas;
		tgui::Label::Ptr statusLabel;
		// Inspector field handles (so we can read them back on save).
		tgui::EditBox::Ptr itemTypeEdit, nameEdit, idEdit, gameLevelEdit, markerEdit;
		tgui::EditBox::Ptr placeWEdit, placeHEdit, placeBaseXEdit, placeBaseYEdit;  // DPDF
		tgui::EditBox::Ptr priceBaseEdit, priceF1Edit, priceF2Edit, priceF3Edit, priceF4Edit, priceF5Edit;
		tgui::EditBox::Ptr transportPriceEdit, transportQuarterlyEdit;  // MVDF (Mover plugins)
		tgui::Label::Ptr transportHeader;  // shown only when class == Mover
		tgui::ComboBox::Ptr sheetSelector;        // which sheet the canvas shows
		tgui::CheckBox::Ptr showRectsCheck;       // overlay OBJMAP rects
		tgui::Button::Ptr importBtn, exportBtn, maskBtn, clearMaskBtn;
		int shownSheet = 1;                        // 0=toolbox 1=ingame 2=occupants 3=walking
		bool eyedropperActive = false;             // next canvas click picks a mask color

		// --- lifecycle ---
		void buildUi();
		void refreshFromDoc();        // push doc values into the inspector fields
		void pullFromInspector();     // read inspector fields back into the doc
		void refreshTextures();       // rebuild sf::Textures from the doc images
		void renderPreview();         // redraw the canvas
		void setStatus(const std::string & msg, bool isError = false);
		void updateTitle();

		// --- actions ---
		void actionOpenT2p();
		void actionOpenOsp();
		void actionSave();
		void actionSaveAs();
		void actionConvert();         // only meaningful after loading a t2p
		void newTenant();
		void closeDoc();

		// --- asset operations on the currently-shown sheet ---
		void actionImportPng();
		void actionExportPng();
		void actionPickMask();        // toggle eyedropper mode
		void actionClearMask();
		OT::OspSpriteSheet * currentSheet();
		void onCanvasClick(tgui::Vector2f pos);  // eyedropper: pos is canvas-local

		// --- helpers ---
		bool ensureLoadedForSave();
		void uploadImage(const sf::Image & img, sf::Texture & tex, bool & valid);
		void loadSheetIntoDoc(int which, const std::string & pngPath);
	};
}
