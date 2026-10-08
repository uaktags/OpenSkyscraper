/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// PluginEditor — standalone editor for YootTower plugins and the
/// OpenSkyscraper plugin format (OSP).
///
/// Opens YootTower .t2p plugins (PE and SP) and .osp-plugin directories and
/// edits every resource of every plugin class: Tenant, Mover, OuterObj,
/// ToolPalette, World, Movie, HallEvent and Advertise. The window has three
/// panes:
///
///  * Resources: a tree of the plugin's resources grouped by role
///    (identity, definition, tables, world, bitmaps, sprite maps, sounds,
///    text).
///  * Fields: a form for the selected resource built from its Json view
///    (ResourceForm + FieldSchema): validated fields, enum dropdowns, bit
///    checkboxes, tables with add/remove rows, advanced fields on demand.
///    Text resources get a text editor, raw CResFile resources their int
///    slots, bitmaps import/export and sounds playback.
///  * Preview: the sprite sheet with its OBJMAP rects (select a part, edit
///    its rect), the BINF/FINF grid (click to paint), palettes and the sound
///    list.
///
/// Saves to OSP v3 (lossless) and exports real .t2p plugins
/// (PluginT2pExport.h). See doc/openskyscraper/plugin-format.md.
#pragma once
#include "EditorTheme.h"
#include "OspPlugin.h"
#include "PluginObjmap.h"
#include "PluginResources.h"
#include "ResourceForm.h"

#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Window.hpp>

#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/TGUI.hpp>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace editors
{
	/// The editor's top-level controller: owns the SFML window, the TGUI gui,
	/// the in-memory OspPlugin being edited, and the menu/panel wiring.
	class EditorApp
	{
	public:
		int run();

		/// Startup plumbing set by main() before run():
		std::string cliOpenPath;            ///< .t2p or .osp-plugin to open
		std::string cliScreenshotPath;      ///< capture a frame and exit
		std::string cliSelect;              ///< "TYPE:ID" resource to select ("TNDF:1", "RT_BITMAP:1000")
		std::vector<std::string> cliSets;   ///< "TYPE:ID:path=value" edits applied after opening
		std::string cliExportT2p;           ///< write the edited plugin as .t2p and exit
		std::string cliSaveOsp;             ///< write the edited plugin as .osp-plugin and exit
		bool cliAdvanced = false;           ///< start with advanced fields shown
		int cliPart = -1;                   ///< OBJMAP part to select in the preview

		/// Global UI scale (mirrors the game's uiScale convention).
		static constexpr float kUiScale = 1.5f;

	private:
		sf::RenderWindow window;
		tgui::Gui gui{window};

		// The document under edit. Null when nothing is loaded.
		std::optional<OT::OspPlugin> doc;
		bool dirty = false;
		std::string loadedFromPath;  // OSP dir last loaded/saved (for Save w/o As)
		tgui::MessageBox::Ptr confirmBox;
		std::function<void()> pendingAction;

		// --- panes ---
		tgui::MenuBar::Ptr menu;
		tgui::ChildWindow::Ptr treeWindow, formWindow, previewWindow;
		tgui::TreeView::Ptr tree;
		tgui::CheckBox::Ptr advancedCheck;
		tgui::Label::Ptr formHeader;
		tgui::ScrollablePanel::Ptr formPanel;
		tgui::Group::Ptr previewTools;     // per-mode controls above the canvas
		tgui::CanvasSFML::Ptr canvas;
		tgui::Label::Ptr statusLabel;
		ResourceForm form;

		// --- selection ---
		int selected = -1;                 // index into doc->resources, -1 = none
		std::string selectedGroup;         // tree group when a group node is selected
		std::map<std::string, int> treeIndex;  // "Group/Label" -> resource index
		int legacySheet = -1;              // v2 sprite sheet shown (0..3), -1 = none

		// --- preview state ---
		enum class Preview { None, Bitmap, Grid, Palette, Sounds, Legacy };
		Preview preview = Preview::None;
		int previewBitmap = -1;            // resource index of the bitmap shown
		int previewObjmap = -1;            // resource index of its OBJMAP
		std::vector<OT::ObjmapBlock> blocks;
		int selectedBlock = -1;
		sf::Texture previewTex;
		bool previewTexValid = false;
		float previewScale = 1.f, previewX = 0, previewY = 0;
		float gridCellH = 1.f;              // BINF cells keep the tile aspect
		int zoomChoice = 0;                // 0 = fit, else integer zoom
		int gridResource = -1;             // BINF/FINF resource index
		int32_t paintValue = 1;
		sf::Texture tileTex;               // BINF tiles (bitmap 1000)
		bool tileTexValid = false;
		std::unique_ptr<sf::SoundBuffer> soundBuffer;
		std::unique_ptr<sf::Sound> sound;
		bool eyedropperActive = false;

		// --- lifecycle ---
		void buildUi();
		void rebuildTree();
		void selectResource(int index);
		void selectGroup(const std::string & group);
		void showFormFor(int index);
		void setStatus(const std::string & msg, bool isError = false);
		void updateTitle();
		void markDirty();

		// --- resource helpers ---
		std::string resourceLabel(int index) const;
		std::string resourceGroup(int index) const;
		std::string schemaFor(const OT::OspResource & r) const;
		bool isBitmapResource(int index) const;
		int findResourceIndex(const std::string & type, int id) const;
		int bitmapIndexForId(int id) const;
		bool applyJson(int index, const OT::Json & candidate, std::string & error);
		bool parseResourceRef(const std::string & ref, int & index) const;
		bool applyCliSets();

		// --- preview ---
		void setPreviewBitmap(int bitmapIndex, int objmapIndex);
		void setPreviewGrid(int index);
		void setPreviewPalette(int index);
		void setPreviewSounds();
		void setPreviewLegacy(int sheet);
		void clearPreviewTools();
		void renderPreview();
		void onCanvasClick(tgui::Vector2f pos);
		void buildBlockTools();
		bool applyBlockRect(const OT::ObjmapRect & rect);
		void playSound(int index);

		// --- forms for non-Json resources ---
		void showTextForm(int index);
		void showRawForm(int index);
		void showBitmapForm(int index);
		void showSoundForm(int index);

		// --- actions ---
		void actionOpenT2p();
		void actionOpenOsp();
		bool openT2pPath(const std::string & path);
		bool openOspPath(const std::string & path);
		void openDocument(OT::OspPlugin && p, const std::string & ospPath, const std::string & message);
		void confirmThen(const std::function<void()> & action);
		void actionSave();
		void actionSaveAs();
		void actionExportT2p();
		bool exportT2pTo(const std::string & path);
		void newTenant();
		void closeDoc();
		void actionImportPng();
		void actionExportPng();
	};
}
