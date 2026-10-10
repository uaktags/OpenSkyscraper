/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "EditorApp.h"

#include "NativeFileDialog.h"
#include "OspPlugin.h"
#include "PluginBitmapLoader.h"
#include "PluginManager.h"
#include "PluginObjmap.h"
#include "PluginResourceJson.h"
#include "PluginResources.h"
#include "PluginT2pExport.h"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace editors
{
	using OT::Json;
	using OT::NativeFileDialog::Result;
	using OT::ResourceCodec;

	namespace
	{
		constexpr float kMenuH = 26.f;
		constexpr float kTreeW = 240.f;
		constexpr float kFormW = 490.f;
		constexpr float kPreviewW = 460.f;
		constexpr float kPaneH = 690.f;
		constexpr float kToolsH = 130.f;

		const char * const kGroups[] = {
			"Identity", "Definition", "Tables", "World", "Bitmaps", "Sprite maps (OBJMAP)", "Sounds",
			"Text and version", "Other", "Sprite sheets (OSP v2)",
		};

		std::string upperType(const OT::DecodedResource & r)
		{
			return OT::resourceTypeDisplayName(r.type);
		}

		uint32_t le32(const std::string & s, size_t p)
		{
			if (p + 4 > s.size()) return 0;
			return uint32_t(uint8_t(s[p])) | (uint32_t(uint8_t(s[p + 1])) << 8) | (uint32_t(uint8_t(s[p + 2])) << 16)
			     | (uint32_t(uint8_t(s[p + 3])) << 24);
		}

		/// "fmt " fields and the data length of a RIFF WAVE.
		struct WaveInfo
		{
			bool ok = false;
			unsigned channels = 0, rate = 0, bits = 0;
			uint32_t dataBytes = 0;
			double seconds() const { return rate && channels && bits ? double(dataBytes) / (rate * channels * (bits / 8.0)) : 0; }
		};

		WaveInfo waveInfo(const std::string & b)
		{
			WaveInfo w;
			if (b.size() < 12 || b.compare(0, 4, "RIFF") != 0 || b.compare(8, 4, "WAVE") != 0) return w;
			size_t p = 12;
			while (p + 8 <= b.size())
			{
				const std::string id = b.substr(p, 4);
				const uint32_t len = le32(b, p + 4);
				if (id == "fmt " && p + 24 <= b.size())
				{
					w.channels = uint8_t(b[p + 10]) | (uint8_t(b[p + 11]) << 8);
					w.rate = le32(b, p + 12);
					w.bits = uint8_t(b[p + 22]) | (uint8_t(b[p + 23]) << 8);
				}
				else if (id == "data") w.dataBytes = len;
				p += 8 + len + (len & 1);
			}
			w.ok = w.rate != 0;
			return w;
		}

		sf::Color hashColor(int32_t v)
		{
			const uint32_t h = uint32_t(v) * 2654435761u;
			return sf::Color(uint8_t(60 + (h >> 24) % 160), uint8_t(60 + (h >> 16) % 160), uint8_t(60 + (h >> 8) % 160));
		}

		bool decodeDib(const std::string & bytes, sf::Image & img)
		{
			return OT::PluginBitmapLoader::decodeBitmapResource(bytes.data(), int(bytes.size()), img);
		}
	}

	// =====================================================================
	// Run loop
	// =====================================================================
	int EditorApp::run()
	{
		// TGUI's Font constructor THROWS if the path doesn't resolve.
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

		window.create(sf::VideoMode({static_cast<unsigned>((kTreeW + kFormW + kPreviewW + 30) * kUiScale),
		                             static_cast<unsigned>((kPaneH + kMenuH + 50) * kUiScale)}),
		              "OpenSkyscraper Plugin Editor");
		window.setVerticalSyncEnabled(true);
		gui.setTarget(window);

		buildUi();
		advancedCheck->setChecked(cliAdvanced);
		setStatus("Ready. Use File > Open .t2p or Open .osp-plugin to begin.");
		updateTitle();

		const bool headless = !cliScreenshotPath.empty() || !cliExportT2p.empty() || !cliSaveOsp.empty();
		if (!cliOpenPath.empty())
		{
			const bool isT2p = cliOpenPath.size() >= 4 && cliOpenPath.compare(cliOpenPath.size() - 4, 4, ".t2p") == 0;
			const bool ok = isT2p ? openT2pPath(cliOpenPath) : openOspPath(cliOpenPath);
			if (!ok)
			{
				std::cerr << "PluginEditor: failed to open " << cliOpenPath << "\n";
				if (headless) return 1;
			}
		}
		if (!cliSets.empty() && !applyCliSets() && headless) return 1;
		if (!cliSelect.empty())
		{
			int idx = -1;
			if (parseResourceRef(cliSelect, idx)) selectResource(idx);
			else if (doc) selectGroup(cliSelect);
			if (cliPart >= 0 && cliPart < int(blocks.size()))
			{
				selectedBlock = cliPart;
				buildBlockTools();
			}
		}
		if (!cliExportT2p.empty())
		{
			if (!exportT2pTo(cliExportT2p)) return 1;
			std::cout << "PluginEditor: wrote " << cliExportT2p << "\n";
		}
		if (!cliSaveOsp.empty())
		{
			if (!doc) return 1;
			const OT::OspResult r = OT::saveOspPlugin(cliSaveOsp, *doc, true);
			if (!r.ok) { std::cerr << "PluginEditor: " << r.message << "\n"; return 1; }
			std::cout << "PluginEditor: wrote " << cliSaveOsp << "\n";
		}
		if (cliScreenshotPath.empty() && (!cliExportT2p.empty() || !cliSaveOsp.empty())) return 0;

		int frame = 0;
		while (window.isOpen())
		{
			while (const std::optional event = window.pollEvent())
			{
				if (const auto * resized = event->getIf<sf::Event::Resized>())
				{
					sf::View view(sf::FloatRect({0, 0}, {float(resized->size.x), float(resized->size.y)}));
					window.setView(view);
					gui.setAbsoluteView({0.f, 0.f, float(resized->size.x), float(resized->size.y)});
				}
				else if (event->is<sf::Event::Closed>())
				{
					window.close();
				}
				gui.handleEvent(*event);
			}
			form.rebuildIfPending();
			renderPreview();

			window.clear(Theme::active().windowBg);
			gui.draw();
			window.display();

			// Headless capture: a few frames in, so the form and the canvas
			// have been laid out and drawn once.
			if (!cliScreenshotPath.empty() && ++frame >= 3)
			{
				sf::Texture shot(window.getSize());
				shot.update(window);
				if (!shot.copyToImage().saveToFile(cliScreenshotPath))
				{
					std::cerr << "PluginEditor: unable to save frame capture to " << cliScreenshotPath << "\n";
					return 1;
				}
				return 0;
			}
		}
		return 0;
	}

	// =====================================================================
	// UI construction
	// =====================================================================
	void EditorApp::buildUi()
	{
		const float s = kUiScale;
		const Theme & th = Theme::active();

		menu = tgui::MenuBar::create();
		menu->setHeight(kMenuH * s);
		menu->setTextSize(unsigned(15 * s));
		menu->addMenu("File");
		menu->addMenuItem("New Tenant...");
		menu->addMenuItem("Open .t2p...");
		menu->addMenuItem("Open .osp-plugin...");
		menu->addMenuItem("-");
		menu->addMenuItem("Save");
		menu->addMenuItem("Save As...");
		menu->addMenuItem("Export .t2p...");
		menu->addMenuItem("-");
		menu->addMenuItem("Close");
		menu->addMenuItem("Quit");
		menu->onMenuItemClick([this](const std::vector<tgui::String> & items) {
			const auto & item = items.back();
			if (item == "New Tenant...") confirmThen([this] { newTenant(); });
			else if (item == "Open .t2p...") confirmThen([this] { actionOpenT2p(); });
			else if (item == "Open .osp-plugin...") confirmThen([this] { actionOpenOsp(); });
			else if (item == "Save") actionSave();
			else if (item == "Save As...") actionSaveAs();
			else if (item == "Export .t2p...") actionExportT2p();
			else if (item == "Close") confirmThen([this] { closeDoc(); });
			else if (item == "Quit") confirmThen([this] { window.close(); });
		});
		applyMenuTheme(menu, th);
		gui.add(menu);

		const float panelY = (kMenuH + 6) * s;
		auto makeWindow = [&](const char * title, float x, float w) {
			auto win = tgui::ChildWindow::create();
			win->setTitle(title);
			win->setTitleButtons(tgui::ChildWindow::TitleButton::None);
			win->setPosition({x * s, panelY});
			win->setClientSize({w * s, kPaneH * s});
			applyChildWindowTheme(win, th, th.panelBg);
			win->getRenderer()->setBorders(1);
			win->getRenderer()->setTitleBarHeight(unsigned(22 * s));
			gui.add(win);
			return win;
		};

		// --- Resources tree ---
		treeWindow = makeWindow("Resources", 0, kTreeW);
		tree = tgui::TreeView::create();
		tree->setPosition({4 * s, 4 * s});
		tree->setSize({(kTreeW - 8) * s, (kPaneH - 40) * s});
		tree->setTextSize(unsigned(13 * s));
		tree->setItemHeight(unsigned(19 * s));
		applyTreeViewTheme(tree, th);
		tree->onItemSelect([this](const std::vector<tgui::String> & item) {
			if (item.empty() || !doc) return;
			if (item.size() == 1) { selectGroup(item[0].toStdString()); return; }
			auto it = treeIndex.find(item[0].toStdString() + "/" + item[1].toStdString());
			if (it != treeIndex.end())
			{
				if (it->second <= -10) setPreviewLegacy(-10 - it->second);
				else selectResource(it->second);
			}
		});
		treeWindow->add(tree);
		advancedCheck = tgui::CheckBox::create("Advanced fields");
		advancedCheck->setPosition({6 * s, (kPaneH - 30) * s});
		advancedCheck->setSize({16 * s, 16 * s});
		advancedCheck->setTextSize(unsigned(13 * s));
		applyCheckBoxTheme(advancedCheck, th);
		advancedCheck->onChange([this](bool) { if (selected >= 0) showFormFor(selected); });
		treeWindow->add(advancedCheck);

		// --- Fields ---
		formWindow = makeWindow("Fields", kTreeW + 4, kFormW);
		formHeader = tgui::Label::create("No plugin loaded.");
		formHeader->setPosition({6 * s, 4 * s});
		formHeader->setTextSize(unsigned(14 * s));
		formHeader->getRenderer()->setTextColor(th.sectionText);
		formWindow->add(formHeader);
		formPanel = tgui::ScrollablePanel::create();
		formPanel->getRenderer()->setBackgroundColor(sf::Color::Transparent);
		applyScrollablePanelTheme(formPanel, th);
		formPanel->setPosition({2 * s, 28 * s});
		formPanel->setSize({(kFormW - 4) * s, (kPaneH - 32) * s});
		formPanel->setHorizontalScrollbarPolicy(tgui::Scrollbar::Policy::Never);
		formWindow->add(formPanel);
		form.onStatus = [this](const std::string & m, bool err) { setStatus(m, err); };

		// --- Preview ---
		previewWindow = makeWindow("Preview", kTreeW + kFormW + 8, kPreviewW);
		previewTools = tgui::Group::create({kPreviewW * s, kToolsH * s});
		previewTools->setPosition({0, 0});
		previewWindow->add(previewTools);
		canvas = tgui::CanvasSFML::create({(kPreviewW - 8) * s, (kPaneH - kToolsH - 6) * s});
		canvas->setPosition({4 * s, (kToolsH + 2) * s});
		canvas->onClick([this](tgui::Vector2f pos) { onCanvasClick(pos); });
		previewWindow->add(canvas);

		statusLabel = tgui::Label::create("Ready.");
		statusLabel->setPosition({8 * s, (kPaneH + kMenuH + 16) * s});
		statusLabel->setTextSize(unsigned(14 * s));
		statusLabel->getRenderer()->setTextColor(th.statusText);
		gui.add(statusLabel);
	}

	void EditorApp::setStatus(const std::string & msg, bool isError)
	{
		if (!statusLabel) return;
		statusLabel->setText(msg);
		statusLabel->getRenderer()->setTextColor(isError ? Theme::active().errorText : Theme::active().statusText);
		if (isError) std::cerr << "PluginEditor: " << msg << "\n";
	}

	void EditorApp::updateTitle()
	{
		std::string t = "OpenSkyscraper Plugin Editor";
		if (doc)
		{
			t += " - " + (doc->attr.name.empty() ? std::string("<unnamed>") : doc->attr.name);
			if (!doc->sourceFile.empty()) t += " (" + doc->sourceFile + ")";
			if (dirty) t += " *";
		}
		window.setTitle(t);
	}

	void EditorApp::markDirty()
	{
		dirty = true;
		if (doc)
			for (const OT::OspResource & r : doc->resources)
				if (r.res.codec == ResourceCodec::Attr && r.res.id == 128) doc->attr = std::get<OT::PluginAttr>(r.res.value);
		updateTitle();
	}

	// =====================================================================
	// Resources
	// =====================================================================
	bool EditorApp::isBitmapResource(int i) const
	{
		if (!doc || i < 0 || size_t(i) >= doc->resources.size()) return false;
		const OT::OspResource & r = doc->resources[size_t(i)];
		if (r.res.codec != ResourceCodec::Raw) return false;
		return doc->container == OT::ResourceContainer::SP ? r.spType == 0 : r.res.type == "2";
	}

	int EditorApp::findResourceIndex(const std::string & type, int id) const
	{
		if (!doc) return -1;
		for (size_t i = 0; i < doc->resources.size(); ++i)
			if (doc->resources[i].res.id == id && doc->resources[i].res.type == type) return int(i);
		return -1;
	}

	int EditorApp::bitmapIndexForId(int id) const
	{
		if (!doc) return -1;
		for (size_t i = 0; i < doc->resources.size(); ++i)
			if (doc->resources[i].res.id == id && isBitmapResource(int(i))) return int(i);
		return -1;
	}

	std::string EditorApp::resourceGroup(int i) const
	{
		const OT::OspResource & r = doc->resources[size_t(i)];
		const std::string t = upperType(r.res);
		if (isBitmapResource(i)) return "Bitmaps";
		switch (r.res.codec)
		{
			case ResourceCodec::Attr:
			case ResourceCodec::Desc:          return "Identity";
			case ResourceCodec::TemplateDef:
			case ResourceCodec::Movie:
			case ResourceCodec::HallEvent:
			case ResourceCodec::Advertise:     return "Definition";
			case ResourceCodec::MemberTable:
			case ResourceCodec::DayParam:
			case ResourceCodec::PlaceParam:
			case ResourceCodec::PeopleAnime:
			case ResourceCodec::Silhouettes:
			case ResourceCodec::People:
			case ResourceCodec::Words:
			case ResourceCodec::StringTable:
			case ResourceCodec::SubPluginPeopleParam: return "Tables";
			case ResourceCodec::WorldRecord:
			case ResourceCodec::BkgndInfo:
			case ResourceCodec::FloorInfoMask:
			case ResourceCodec::PaletteAnime:
			case ResourceCodec::Palette:       return "World";
			case ResourceCodec::StringBlock:
			case ResourceCodec::VersionInfo:   return "Text and version";
			default: break;
		}
		if (t == "OBJMAP") return "Sprite maps (OBJMAP)";
		if (t == "WAVE") return "Sounds";
		if (t == "PGDF" || t == "PZDF") return "World";
		if (r.res.codec == ResourceCodec::Text) return "Text and version";
		return "Other";
	}

	std::string EditorApp::resourceLabel(int i) const
	{
		const OT::OspResource & r = doc->resources[size_t(i)];
		std::string t = upperType(r.res);
		if (t == "RT_BITMAP" || isBitmapResource(i)) t = "BITMAP";
		std::string label = t + " " + std::to_string(r.res.id);
		if (t == "TMDF") label += r.res.id == 1 ? " (employees)" : r.res.id == 2 ? " (customers)" : " (members)";
		if (r.res.codec == ResourceCodec::WorldRecord && t.size() == 4 && t[2] == 'E' && t[3] == 'V' && t != "T2EV")
			label += " (event)";
		if (isBitmapResource(i) && r.res.bytes.size() >= 12)
			label += "  " + std::to_string(int32_t(le32(r.res.bytes, 4))) + "x" + std::to_string(std::abs(int32_t(le32(r.res.bytes, 8))));
		return label;
	}

	std::string EditorApp::schemaFor(const OT::OspResource & r) const
	{
		if (r.res.codec == ResourceCodec::WorldRecord) return upperType(r.res);
		return OT::resourceCodecName(r.res.codec);
	}

	void EditorApp::rebuildTree()
	{
		tree->removeAllItems();
		treeIndex.clear();
		if (!doc) return;
		std::map<std::string, std::vector<int>> byGroup;
		for (size_t i = 0; i < doc->resources.size(); ++i) byGroup[resourceGroup(int(i))].push_back(int(i));
		bool hasBitmaps = !byGroup["Bitmaps"].empty();
		for (const char * g : kGroups)
		{
			const std::string group = g;
			if (group == "Sprite sheets (OSP v2)")
			{
				if (hasBitmaps) continue;
				const OT::OspSpriteSheet * sheets[4] = {&doc->toolbox, &doc->ingame, &doc->occupants, &doc->walking};
				const char * names[4] = {"Toolbox (100)", "Ingame (1000)", "Occupants (1100)", "Walking (200)"};
				for (int k = 0; k < 4; ++k)
				{
					if (!sheets[k]->present) continue;
					tree->addItem({group, names[k]});
					treeIndex[group + "/" + names[k]] = -10 - k;
				}
				continue;
			}
			for (int i : byGroup[group])
			{
				std::string label = resourceLabel(i);
				while (treeIndex.count(group + "/" + label)) label += "'";
				tree->addItem({group, label});
				treeIndex[group + "/" + label] = i;
			}
			if (group == "Sounds" && !byGroup[group].empty()) continue;
		}
		tree->collapseAll();
		for (const char * g : {"Identity", "Definition", "Tables", "World", "Sprite sheets (OSP v2)"})
			tree->expand({g});
	}

	bool EditorApp::parseResourceRef(const std::string & ref, int & index) const
	{
		if (!doc) return false;
		const size_t colon = ref.rfind(':');
		if (colon == std::string::npos) return false;
		const std::string type = ref.substr(0, colon);
		const int id = std::atoi(ref.c_str() + colon + 1);
		for (size_t i = 0; i < doc->resources.size(); ++i)
		{
			const OT::DecodedResource & r = doc->resources[i].res;
			if (r.id != id) continue;
			if (r.type == type || upperType(r) == type || (type == "BITMAP" && isBitmapResource(int(i))))
			{
				index = int(i);
				return true;
			}
		}
		return false;
	}

	bool EditorApp::applyCliSets()
	{
		for (const std::string & set : cliSets)
		{
			// TYPE:ID:path=value
			const size_t eq = set.find('=');
			const size_t c1 = set.find(':');
			const size_t c2 = c1 == std::string::npos ? c1 : set.find(':', c1 + 1);
			if (eq == std::string::npos || c2 == std::string::npos || c2 > eq)
			{
				setStatus("--set expects TYPE:ID:path=value, got " + set, true);
				return false;
			}
			int idx = -1;
			if (!parseResourceRef(set.substr(0, c2), idx))
			{
				setStatus("--set: no resource " + set.substr(0, c2), true);
				return false;
			}
			const OT::OspResource & r = doc->resources[size_t(idx)];
			if (r.res.codec == ResourceCodec::Raw || r.res.codec == ResourceCodec::Text)
			{
				setStatus("--set: " + set.substr(0, c2) + " has no fields", true);
				return false;
			}
			ResourceForm tmp;
			tmp.onCommit = [this, idx](const Json & cand, std::string & err) { return applyJson(idx, cand, err); };
			tmp.setDoc(OT::resourceToJson(r.res));
			std::string err;
			if (!tmp.setPath(set.substr(c2 + 1, eq - c2 - 1), set.substr(eq + 1), err))
			{
				setStatus("--set " + set + ": " + err, true);
				return false;
			}
			std::cout << "PluginEditor: set " << set << "\n";
		}
		if (selected >= 0) showFormFor(selected);
		return true;
	}

	bool EditorApp::applyJson(int index, const Json & candidate, std::string & error)
	{
		if (!doc || index < 0 || size_t(index) >= doc->resources.size()) return false;
		OT::DecodedResource copy = doc->resources[size_t(index)].res;
		if (!OT::resourceFromJson(candidate, copy, &error)) return false;
		doc->resources[size_t(index)].res = std::move(copy);
		markDirty();
		if (preview == Preview::Grid || preview == Preview::Palette) previewTexValid = false;
		return true;
	}

	// =====================================================================
	// Selection and forms
	// =====================================================================
	void EditorApp::selectGroup(const std::string & group)
	{
		selected = -1;
		selectedGroup = group;
		formPanel->removeAllWidgets();
		formHeader->setText(group);
		if (group == "Sounds")
		{
			setPreviewSounds();
			return;
		}
		clearPreviewTools();
		preview = Preview::None;
	}

	void EditorApp::selectResource(int index)
	{
		if (!doc || index < 0 || size_t(index) >= doc->resources.size()) return;
		selected = index;
		selectedGroup.clear();
		showFormFor(index);
		const OT::OspResource & r = doc->resources[size_t(index)];
		const std::string t = upperType(r.res);
		if (isBitmapResource(index))
			setPreviewBitmap(index, findResourceIndex("OBJMAP", r.res.id));
		else if (t == "OBJMAP")
			setPreviewBitmap(bitmapIndexForId(r.res.id), index);
		else if (r.res.codec == ResourceCodec::TemplateDef)
		{
			int b = bitmapIndexForId(1000);
			int id = 1000;
			if (b < 0) { b = bitmapIndexForId(100); id = 100; }
			setPreviewBitmap(b, findResourceIndex("OBJMAP", id));
		}
		else if (r.res.codec == ResourceCodec::Movie || r.res.codec == ResourceCodec::HallEvent
		         || r.res.codec == ResourceCodec::Advertise)
			setPreviewBitmap(bitmapIndexForId(128), -1);
		else if (r.res.codec == ResourceCodec::BkgndInfo || r.res.codec == ResourceCodec::FloorInfoMask)
			setPreviewGrid(index);
		else if (r.res.codec == ResourceCodec::Palette)
			setPreviewPalette(index);
		else if (t == "WAVE")
			setPreviewSounds();
		else if (r.res.codec == ResourceCodec::WorldRecord && t == "WORLDDEF")
			setPreviewBitmap(bitmapIndexForId(20000), -1);
		else
		{
			clearPreviewTools();
			preview = Preview::None;
		}
	}

	void EditorApp::showFormFor(int index)
	{
		const OT::OspResource & r = doc->resources[size_t(index)];
		const std::string name = upperType(r.res) + " " + std::to_string(r.res.id);
		std::string bytes;
		OT::encodeResource(r.res, bytes);
		formHeader->setText(name + "   " + OT::resourceCodecName(r.res.codec) + ", " + std::to_string(bytes.size()) + " bytes");
		if (r.res.codec == ResourceCodec::Text) { showTextForm(index); return; }
		if (r.res.codec == ResourceCodec::Raw)
		{
			if (isBitmapResource(index)) showBitmapForm(index);
			else if (upperType(r.res) == "WAVE") showSoundForm(index);
			else showRawForm(index);
			return;
		}
		form.onCommit = [this, index](const Json & cand, std::string & err) { return applyJson(index, cand, err); };
		form.build(formPanel, schemaFor(r), OT::resourceToJson(r.res), advancedCheck->isChecked(), kUiScale);
	}

	void EditorApp::showTextForm(int index)
	{
		const float s = kUiScale;
		formPanel->removeAllWidgets();
		const std::string & bytes = doc->resources[size_t(index)].res.bytes;
		const bool crlf = bytes.find("\r\n") != std::string::npos;
		std::string text;
		for (size_t i = 0; i < bytes.size(); ++i)
			if (!(bytes[i] == '\r' && i + 1 < bytes.size() && bytes[i + 1] == '\n')) text.push_back(bytes[i]);
		auto area = tgui::TextArea::create();
		area->setPosition({4 * s, 4 * s});
		area->setSize({(kFormW - 24) * s, (kPaneH - 80) * s});
		area->setTextSize(unsigned(12 * s));
		area->setText(tgui::String(OT::latin1ToUtf8(text)));
		applyTextAreaTheme(area, Theme::active());
		formPanel->add(area);
		auto apply = tgui::Button::create("Apply text");
		apply->setPosition({4 * s, (kPaneH - 70) * s});
		apply->setSize({120 * s, 26 * s});
		apply->setTextSize(unsigned(13 * s));
		applyButtonTheme(apply, Theme::active());
		auto weak = area.get();
		apply->onPress([this, index, weak, crlf] {
			std::string latin;
			if (!OT::utf8ToLatin1(weak->getText().toStdString(), latin))
			{
				setStatus("The text has characters outside Latin-1.", true);
				return;
			}
			std::string out;
			for (char c : latin)
			{
				if (c == '\n' && crlf) out += "\r\n";
				else out.push_back(c);
			}
			doc->resources[size_t(index)].res.bytes = out;
			markDirty();
			setStatus("Text applied.");
			if (preview == Preview::Bitmap && previewObjmap == index) setPreviewBitmap(previewBitmap, previewObjmap);
		});
		formPanel->add(apply);
		formPanel->setContentSize({(kFormW - 20) * s, (kPaneH - 40) * s});
	}

	void EditorApp::showRawForm(int index)
	{
		const OT::OspResource & r = doc->resources[size_t(index)];
		const std::string & b = r.res.bytes;
		if (b.size() > 1 && uint8_t(b[0]) == 1 && (b.size() - 1) % 4 == 0)
		{
			// Binary CResFile resource of unknown layout (PGDF, PZDF): its
			// 4-byte slots, named by position.
			Json slots = Json::array();
			for (size_t p = 1; p + 4 <= b.size(); p += 4) slots.push(Json(int32_t(le32(b, p))));
			Json docJson = Json::object().set("slots", std::move(slots));
			form.onCommit = [this, index](const Json & cand, std::string & err) {
				std::string out(1, char(1));
				for (const Json & v : cand["slots"].elements())
				{
					if (!v.isInt() || v.asInt() < INT32_MIN || v.asInt() > INT32_MAX) { err = "slots: expected 32-bit integers"; return false; }
					const uint32_t u = uint32_t(int32_t(v.asInt()));
					for (int k = 0; k < 4; ++k) out.push_back(char((u >> (8 * k)) & 0xFF));
				}
				doc->resources[size_t(index)].res.bytes = out;
				markDirty();
				return true;
			};
			form.build(formPanel, "RAW", docJson, true, kUiScale);
			return;
		}
		const float s = kUiScale;
		formPanel->removeAllWidgets();
		std::string hex;
		for (size_t i = 0; i < b.size() && i < 4096; ++i)
		{
			char buf[8];
			std::snprintf(buf, sizeof(buf), "%02x%s", uint8_t(b[i]), (i % 16 == 15) ? "\n" : " ");
			hex += buf;
		}
		if (b.size() > 4096) hex += "\n... (" + std::to_string(b.size()) + " bytes)";
		auto area = tgui::TextArea::create();
		area->setPosition({4 * s, 4 * s});
		area->setSize({(kFormW - 24) * s, (kPaneH - 50) * s});
		area->setTextSize(unsigned(12 * s));
		area->setText(hex);
		area->setReadOnly(true);
		applyTextAreaTheme(area, Theme::active());
		formPanel->add(area);
		formPanel->setContentSize({(kFormW - 20) * s, (kPaneH - 40) * s});
	}

	void EditorApp::showBitmapForm(int index)
	{
		const float s = kUiScale;
		formPanel->removeAllWidgets();
		const std::string & b = doc->resources[size_t(index)].res.bytes;
		const unsigned bpp = b.size() >= 16 ? unsigned(uint8_t(b[14])) : 0;
		const uint32_t used = le32(b, 32);
		std::string info = "Windows DIB " + std::to_string(int32_t(le32(b, 4))) + " x "
		                 + std::to_string(std::abs(int32_t(le32(b, 8)))) + ", " + std::to_string(bpp) + " bpp, "
		                 + std::to_string(used ? used : (bpp <= 8 ? (1u << bpp) : 0)) + " palette colours.\n\n"
		                 + "Import replaces the pixels and maps them onto this bitmap's palette (exact colours\n"
		                 + "first, nearest otherwise); a different size gets a fresh header. OSP stores it as\n"
		                 + "PNG plus the original header and palette, so an unedited export is exact.";
		auto l = tgui::Label::create(info);
		l->setPosition({6 * s, 6 * s});
		l->setTextSize(unsigned(13 * s));
		l->getRenderer()->setTextColor(Theme::active().labelText);
		formPanel->add(l);
		auto mk = [&](const char * text, float x, std::function<void()> fn) {
			auto bt = tgui::Button::create(text);
			bt->setPosition({x * s, 120 * s});
			bt->setSize({130 * s, 26 * s});
			bt->setTextSize(unsigned(13 * s));
			applyButtonTheme(bt, Theme::active());
			bt->onPress(fn);
			formPanel->add(bt);
		};
		mk("Import PNG...", 6, [this] { actionImportPng(); });
		mk("Export PNG...", 146, [this] { actionExportPng(); });
		formPanel->setContentSize({(kFormW - 20) * s, 200 * s});
	}

	void EditorApp::showSoundForm(int index)
	{
		const float s = kUiScale;
		formPanel->removeAllWidgets();
		const WaveInfo w = waveInfo(doc->resources[size_t(index)].res.bytes);
		char buf[200];
		std::snprintf(buf, sizeof(buf), "RIFF WAVE: %u Hz, %u bit, %s, %.2f s.\nStored in OSP as sounds/%d.wav (verbatim).",
		              w.rate, w.bits, w.channels == 1 ? "mono" : "stereo", w.seconds(), doc->resources[size_t(index)].res.id);
		auto l = tgui::Label::create(buf);
		l->setPosition({6 * s, 6 * s});
		l->setTextSize(unsigned(13 * s));
		l->getRenderer()->setTextColor(Theme::active().labelText);
		formPanel->add(l);
		auto play = tgui::Button::create("Play");
		play->setPosition({6 * s, 60 * s});
		play->setSize({90 * s, 26 * s});
		play->setTextSize(unsigned(13 * s));
		applyButtonTheme(play, Theme::active());
		play->onPress([this, index] { playSound(index); });
		formPanel->add(play);
	}

	void EditorApp::playSound(int index)
	{
		if (!doc || index < 0) return;
		const std::string & b = doc->resources[size_t(index)].res.bytes;
		auto buf = std::make_unique<sf::SoundBuffer>();
		if (!buf->loadFromMemory(b.data(), b.size()))
		{
			setStatus("Cannot decode this WAVE.", true);
			return;
		}
		sound.reset();
		soundBuffer = std::move(buf);
		sound = std::make_unique<sf::Sound>(*soundBuffer);
		sound->play();
		setStatus("Playing WAVE " + std::to_string(doc->resources[size_t(index)].res.id));
	}

	// =====================================================================
	// Preview
	// =====================================================================
	void EditorApp::clearPreviewTools()
	{
		previewTools->removeAllWidgets();
		previewTools->setSize({kPreviewW * kUiScale, kToolsH * kUiScale});
		canvas->setVisible(true);
		eyedropperActive = false;
	}

	void EditorApp::setPreviewBitmap(int bitmapIndex, int objmapIndex)
	{
		clearPreviewTools();
		preview = Preview::Bitmap;
		previewBitmap = bitmapIndex;
		previewObjmap = objmapIndex;
		previewTexValid = false;
		blocks.clear();
		if (selectedBlock >= 0 && objmapIndex < 0) selectedBlock = -1;
		if (bitmapIndex >= 0)
		{
			sf::Image img;
			if (decodeDib(doc->resources[size_t(bitmapIndex)].res.bytes, img)) previewTexValid = previewTex.loadFromImage(img);
		}
		if (objmapIndex >= 0)
		{
			const std::string & t = doc->resources[size_t(objmapIndex)].res.bytes;
			OT::parsePluginObjmap(t.data(), int(t.size()), blocks);
		}
		if (selectedBlock >= int(blocks.size())) selectedBlock = -1;
		buildBlockTools();
	}

	void EditorApp::buildBlockTools()
	{
		const float s = kUiScale;
		const Theme & th = Theme::active();
		previewTools->removeAllWidgets();
		auto zoom = tgui::ComboBox::create();
		zoom->setPosition({6 * s, 4 * s});
		zoom->setSize({90 * s, 24 * s});
		zoom->setTextSize(unsigned(13 * s));
		for (const char * z : {"Fit", "1x", "2x", "3x", "4x"}) zoom->addItem(z);
		zoom->setSelectedItemByIndex(size_t(zoomChoice));
		applyComboBoxTheme(zoom, th);
		auto zweak = zoom.get();
		zoom->onItemSelect([this, zweak] { zoomChoice = std::max(0, zweak->getSelectedItemIndex()); });
		previewTools->add(zoom);
		auto mkBtn = [&](const char * text, float x, float y, float w, std::function<void()> fn) {
			auto b = tgui::Button::create(text);
			b->setPosition({x * s, y * s});
			b->setSize({w * s, 24 * s});
			b->setTextSize(unsigned(12 * s));
			applyButtonTheme(b, th);
			b->onPress(fn);
			previewTools->add(b);
			return b;
		};
		if (previewBitmap >= 0)
		{
			mkBtn("Import PNG", 102, 4, 90, [this] { actionImportPng(); });
			mkBtn("Export PNG", 196, 4, 90, [this] { actionExportPng(); });
		}
		if (blocks.empty())
		{
			auto l = tgui::Label::create(previewBitmap < 0 ? "No bitmap for this resource." : "No OBJMAP for this bitmap.");
			l->setPosition({6 * s, 34 * s});
			l->setTextSize(unsigned(12 * s));
			l->getRenderer()->setTextColor(th.noteText);
			previewTools->add(l);
			return;
		}
		auto list = tgui::ListBox::create();
		list->setPosition({6 * s, 32 * s});
		list->setSize({230 * s, 94 * s});
		list->setTextSize(unsigned(12 * s));
		list->setItemHeight(unsigned(18 * s));
		for (size_t k = 0; k < blocks.size(); ++k)
		{
			const OT::ObjmapBlock & b = blocks[k];
			std::string label = std::to_string(k) + " " + b.name;
			if (b.id >= 0) label += " id" + std::to_string(b.id);
			if (b.pattern >= 0) label += " p" + std::to_string(b.pattern);
			list->addItem(label);
		}
		if (selectedBlock >= 0) list->setSelectedItemByIndex(size_t(selectedBlock));
		applyListBoxTheme(list, th);
		auto lweak = list.get();
		list->onItemSelect([this, lweak] {
			const int k = lweak->getSelectedItemIndex();
			if (k < 0 || k == selectedBlock) return;
			selectedBlock = k;
			// Rebuilding inside the callback would delete the list: defer
			// by rebuilding only the rect fields.
			for (auto & w : previewTools->getWidgets())
				if (w->getWidgetName().starts_with("rect")) w->setVisible(true);
			const OT::ObjmapRect & r = blocks[size_t(k)].rect;
			const int vals[4] = {r.left, r.top, r.right, r.bottom};
			for (int i = 0; i < 4; ++i)
				if (auto e = previewTools->get<tgui::EditBox>("rect" + std::to_string(i))) e->setText(std::to_string(vals[i]));
			setStatus("Part " + std::to_string(k) + ": " + blocks[size_t(k)].name);
		});
		previewTools->add(list);
		const char * names[4] = {"L", "T", "R", "B"};
		const OT::ObjmapRect r = selectedBlock >= 0 ? blocks[size_t(selectedBlock)].rect : OT::ObjmapRect{};
		const int vals[4] = {r.left, r.top, r.right, r.bottom};
		for (int i = 0; i < 4; ++i)
		{
			auto l = tgui::Label::create(names[i]);
			l->setPosition({(246 + 52 * (i % 2)) * s, (34 + 28 * (i / 2)) * s});
			l->setTextSize(unsigned(12 * s));
			l->getRenderer()->setTextColor(th.labelText);
			previewTools->add(l, "rectl" + std::to_string(i));
			auto e = tgui::EditBox::create();
			e->setPosition({(258 + 52 * (i % 2)) * s, (32 + 28 * (i / 2)) * s});
			e->setSize({38 * s, 22 * s});
			e->setTextSize(unsigned(12 * s));
			e->setText(std::to_string(vals[i]));
			applyEditBoxTheme(e, th);
			previewTools->add(e, "rect" + std::to_string(i));
		}
		mkBtn("Apply rect", 246, 90, 100, [this] {
			if (selectedBlock < 0) { setStatus("Select a part first.", true); return; }
			int v[4];
			for (int i = 0; i < 4; ++i)
			{
				auto e = previewTools->get<tgui::EditBox>("rect" + std::to_string(i));
				int64_t n = 0;
				if (!e || !parseUserInt(e->getText().toStdString(), n) || n < -100000 || n > 100000)
				{
					setStatus("Rect values must be integers.", true);
					return;
				}
				v[i] = int(n);
			}
			if (v[2] < v[0] || v[3] < v[1]) { setStatus("Rect right/bottom must not be less than left/top.", true); return; }
			OT::ObjmapRect nr;
			nr.left = v[0]; nr.top = v[1]; nr.right = v[2]; nr.bottom = v[3];
			applyBlockRect(nr);
		});
	}

	bool EditorApp::applyBlockRect(const OT::ObjmapRect & rect)
	{
		if (previewObjmap < 0 || selectedBlock < 0) return false;
		std::string & text = doc->resources[size_t(previewObjmap)].res.bytes;
		// Find the selectedBlock-th DefParts block and its "Rect" line.
		size_t pos = 0;
		for (int k = 0; k <= selectedBlock; ++k)
		{
			pos = text.find("DefParts", k == 0 ? 0 : pos + 8);
			if (pos == std::string::npos) return false;
		}
		const size_t next = text.find("DefParts", pos + 8);
		const size_t r = text.find("Rect ", pos);
		if (r == std::string::npos || (next != std::string::npos && r > next))
		{
			setStatus("This part has no Rect line to edit.", true);
			return false;
		}
		size_t end = r + 5;
		while (end < text.size() && text[end] != '\r' && text[end] != '\n') ++end;
		text.replace(r, end - r, "Rect " + std::to_string(rect.left) + "," + std::to_string(rect.top) + ","
		                         + std::to_string(rect.right) + "," + std::to_string(rect.bottom));
		markDirty();
		const int keep = selectedBlock;
		setPreviewBitmap(previewBitmap, previewObjmap);
		selectedBlock = keep;
		buildBlockTools();
		if (selected == previewObjmap) showTextForm(selected);
		setStatus("Updated the rect of part " + std::to_string(keep) + ".");
		return true;
	}

	void EditorApp::setPreviewGrid(int index)
	{
		clearPreviewTools();
		preview = Preview::Grid;
		gridResource = index;
		previewTexValid = true;
		tileTexValid = false;
		const float s = kUiScale;
		const Theme & th = Theme::active();
		const bool binf = doc->resources[size_t(index)].res.codec == ResourceCodec::BkgndInfo;
		if (binf)
		{
			const int b = bitmapIndexForId(1000);
			sf::Image img;
			if (b >= 0 && decodeDib(doc->resources[size_t(b)].res.bytes, img)) tileTexValid = tileTex.loadFromImage(img);
		}
		auto l = tgui::Label::create(binf ? "BINF background tiles (8 units x 2 rows per cell).\nClick a cell to paint the tile value below."
		                                  : "FINF build mask: red = unbuildable, green = buildable.\nClick a unit to toggle it.");
		l->setPosition({6 * s, 4 * s});
		l->setTextSize(unsigned(12 * s));
		l->getRenderer()->setTextColor(th.labelText);
		previewTools->add(l);
		if (binf)
		{
			auto lab = tgui::Label::create("Paint value");
			lab->setPosition({6 * s, 48 * s});
			lab->setTextSize(unsigned(12 * s));
			lab->getRenderer()->setTextColor(th.labelText);
			previewTools->add(lab);
			auto e = tgui::EditBox::create();
			e->setPosition({90 * s, 46 * s});
			e->setSize({80 * s, 22 * s});
			e->setTextSize(unsigned(12 * s));
			e->setText(std::to_string(paintValue));
			applyEditBoxTheme(e, th);
			auto weak = e.get();
			e->onReturnOrUnfocus([this, weak] {
				int64_t n = 0;
				if (parseUserInt(weak->getText().toStdString(), n) && n >= INT32_MIN && n <= INT32_MAX) paintValue = int32_t(n);
				else setStatus("Paint value must be an integer.", true);
			});
			previewTools->add(e);
		}
	}

	void EditorApp::setPreviewPalette(int index)
	{
		clearPreviewTools();
		preview = Preview::Palette;
		gridResource = index;
		auto l = tgui::Label::create("Palette as the engine builds it (256 colours, 16 per row).\nEdit entries in the Fields pane.");
		l->setPosition({6 * kUiScale, 4 * kUiScale});
		l->setTextSize(unsigned(12 * kUiScale));
		l->getRenderer()->setTextColor(Theme::active().labelText);
		previewTools->add(l);
	}

	void EditorApp::setPreviewSounds()
	{
		clearPreviewTools();
		preview = Preview::Sounds;
		const float s = kUiScale;
		const Theme & th = Theme::active();
		// The sound list takes the whole pane.
		canvas->setVisible(false);
		previewTools->setSize({kPreviewW * s, kPaneH * s});
		auto list = tgui::ListBox::create();
		list->setPosition({6 * s, 4 * s});
		list->setSize({(kPreviewW - 110) * s, (kPaneH - 12) * s});
		list->setAutoScroll(false);
		list->setTextSize(unsigned(12 * s));
		list->setItemHeight(unsigned(18 * s));
		int sel = -1, row = 0;
		for (size_t i = 0; i < doc->resources.size(); ++i)
		{
			const OT::OspResource & r = doc->resources[i];
			if (upperType(r.res) != "WAVE") continue;
			const WaveInfo w = waveInfo(r.res.bytes);
			char buf[96];
			std::snprintf(buf, sizeof(buf), "WAVE %d   %.2f s, %u Hz", r.res.id, w.seconds(), w.rate);
			list->addItem(buf, std::to_string(i));
			if (int(i) == selected) sel = row;
			++row;
		}
		if (sel >= 0) list->setSelectedItemByIndex(size_t(sel));
		applyListBoxTheme(list, th);
		previewTools->add(list);
		auto weak = list.get();
		auto mk = [&](const char * text, float y, std::function<void()> fn) {
			auto b = tgui::Button::create(text);
			b->setPosition({(kPreviewW - 98) * s, y * s});
			b->setSize({90 * s, 26 * s});
			b->setTextSize(unsigned(13 * s));
			applyButtonTheme(b, th);
			b->onPress(fn);
			previewTools->add(b);
		};
		mk("Play", 4, [this, weak] {
			int64_t idx = -1;
			if (parseUserInt(weak->getSelectedItemId().toStdString(), idx)) playSound(int(idx));
			else setStatus("Select a sound first.", true);
		});
		mk("Stop", 34, [this] { if (sound) sound->stop(); });
		list->onDoubleClick([this, weak] {
			int64_t idx = -1;
			if (parseUserInt(weak->getSelectedItemId().toStdString(), idx)) playSound(int(idx));
		});
	}

	void EditorApp::setPreviewLegacy(int sheet)
	{
		clearPreviewTools();
		selected = -1;
		formPanel->removeAllWidgets();
		formHeader->setText("OSP v2 sprite sheet");
		preview = Preview::Legacy;
		legacySheet = sheet;
		previewBitmap = -1;
		previewObjmap = -1;
		OT::OspSpriteSheet * sheets[4] = {&doc->toolbox, &doc->ingame, &doc->occupants, &doc->walking};
		OT::OspSpriteSheet & sh = *sheets[sheet];
		previewTexValid = sh.image.getSize().x > 0 && previewTex.loadFromImage(sh.image);
		blocks = sh.blocks;
		selectedBlock = -1;
		const float s = kUiScale;
		auto mk = [&](const char * text, float x, std::function<void()> fn) {
			auto b = tgui::Button::create(text);
			b->setPosition({x * s, 4 * s});
			b->setSize({100 * s, 24 * s});
			b->setTextSize(unsigned(12 * s));
			applyButtonTheme(b, Theme::active());
			b->onPress(fn);
			previewTools->add(b);
		};
		mk("Import PNG", 6, [this] { actionImportPng(); });
		mk("Export PNG", 112, [this] { actionExportPng(); });
		mk("Pick Mask", 218, [this] {
			eyedropperActive = true;
			setStatus("Eyedropper: click a pixel to set the transparent colour (Clear Mask first if one is set).");
		});
		mk("Clear Mask", 324, [this] {
			OT::OspSpriteSheet * sheets2[4] = {&doc->toolbox, &doc->ingame, &doc->occupants, &doc->walking};
			sheets2[legacySheet]->mask.reset();
			sheets2[legacySheet]->maskHow = OT::MaskDiscovery::None;
			markDirty();
			setStatus("Cleared the transparent colour.");
		});
	}

	void EditorApp::renderPreview()
	{
		if (!canvas) return;
		const Theme & th = Theme::active();
		canvas->clear(th.canvasBg);
		const sf::Vector2f cs = canvas->getSize();
		const float margin = 8.f;
		if (!doc || preview == Preview::None || preview == Preview::Sounds)
		{
			canvas->display();
			return;
		}
		if ((preview == Preview::Bitmap || preview == Preview::Legacy) && previewTexValid)
		{
			const sf::Vector2u sz = previewTex.getSize();
			float scale = zoomChoice > 0 ? float(zoomChoice)
			                             : std::min({(cs.x - 2 * margin) / float(sz.x), (cs.y - 2 * margin) / float(sz.y), 4.f});
			previewScale = scale;
			previewX = margin;
			previewY = margin;
			sf::Sprite sp(previewTex);
			sp.setScale({scale, scale});
			sp.setPosition({previewX, previewY});
			canvas->draw(sp);
			for (size_t k = 0; k < blocks.size(); ++k)
			{
				const OT::ObjmapRect & r = blocks[k].rect;
				if (r.width() <= 0 || r.height() <= 0) continue;
				sf::RectangleShape rr({r.width() * scale, r.height() * scale});
				rr.setPosition({previewX + r.left * scale, previewY + r.top * scale});
				const bool sel = int(k) == selectedBlock;
				rr.setFillColor(sel ? sf::Color(255, 220, 0, 70) : sf::Color::Transparent);
				rr.setOutlineColor(sel ? sf::Color(255, 220, 0) : th.rectOutline);
				rr.setOutlineThickness(sel ? 2.f : 1.f);
				canvas->draw(rr);
			}
		}
		else if (preview == Preview::Grid && gridResource >= 0)
		{
			const OT::DecodedResource & r = doc->resources[size_t(gridResource)].res;
			int rows = 0, cols = 0;
			const std::vector<int32_t> * cells = nullptr;
			bool binf = r.codec == ResourceCodec::BkgndInfo;
			int tileW = 64, tileH = 72;
			if (binf)
			{
				const OT::BkgndInfoDef & d = std::get<OT::BkgndInfoDef>(r.value);
				rows = d.rows; cols = d.cols; cells = &d.cells;
				if (tileTexValid)
				{
					tileW = int(tileTex.getSize().x);
					// Tile height from WORLDDEF (bkgndTileHeight) when present.
					for (const OT::OspResource & o : doc->resources)
						if (o.res.codec == ResourceCodec::WorldRecord && o.res.type == "WORLDDEF")
						{
							const Json & j = std::get<Json>(o.res.value);
							tileH = int(j["bkgndTileHeight"].asInt(72));
							tileW = int(j["bkgndTileWidth"].asInt(tileW));
						}
				}
			}
			else
			{
				const OT::FloorInfoMaskDef & d = std::get<OT::FloorInfoMaskDef>(r.value);
				rows = d.height; cols = d.width; cells = &d.cells;
			}
			if (rows > 0 && cols > 0 && cells)
			{
				const float cell = std::min((cs.x - 2 * margin) / float(cols), (cs.y - 2 * margin) / float(rows));
				const float ch = binf ? cell * float(tileH) / float(std::max(1, tileW)) : cell;
				const float cellH = binf ? std::min(ch, (cs.y - 2 * margin) / float(rows)) : cell;
				previewScale = cell;
				previewX = margin;
				previewY = margin;
				for (int y = 0; y < rows; ++y)
					for (int x = 0; x < cols; ++x)
					{
						const int32_t v = (*cells)[size_t(y) * size_t(cols) + size_t(x)];
						if (binf && tileTexValid)
						{
							sf::Sprite sp(tileTex, sf::IntRect({0, int(v & 0xFFFF) * tileH}, {tileW, tileH}));
							sp.setScale({cell / float(tileW), cellH / float(tileH)});
							sp.setPosition({margin + x * cell, margin + y * cellH});
							canvas->draw(sp);
						}
						else
						{
							sf::RectangleShape rc({cell, cellH});
							rc.setPosition({margin + x * cell, margin + y * cellH});
							rc.setFillColor(binf ? hashColor(v) : (v ? sf::Color(170, 50, 50) : sf::Color(60, 150, 70)));
							canvas->draw(rc);
						}
					}
				gridCellH = cellH;
			}
		}
		else if (preview == Preview::Palette && gridResource >= 0)
		{
			const OT::PaletteDef & p = std::get<OT::PaletteDef>(doc->resources[size_t(gridResource)].res.value);
			const auto rgb = p.rgb();
			const float cell = std::min((cs.x - 2 * margin), (cs.y - 2 * margin)) / 16.f;
			for (int k = 0; k < 256; ++k)
			{
				sf::RectangleShape rc({cell - 1, cell - 1});
				rc.setPosition({margin + (k % 16) * cell, margin + (k / 16) * cell});
				rc.setFillColor(sf::Color(rgb[size_t(k)][0], rgb[size_t(k)][1], rgb[size_t(k)][2]));
				canvas->draw(rc);
			}
		}
		canvas->display();
	}

	void EditorApp::onCanvasClick(tgui::Vector2f pos)
	{
		if (!doc) return;
		if (preview == Preview::Bitmap || preview == Preview::Legacy)
		{
			if (!previewTexValid || previewScale <= 0) return;
			const float px = (pos.x - previewX) / previewScale, py = (pos.y - previewY) / previewScale;
			if (preview == Preview::Legacy && eyedropperActive)
			{
				eyedropperActive = false;
				OT::OspSpriteSheet * sheets[4] = {&doc->toolbox, &doc->ingame, &doc->occupants, &doc->walking};
				OT::OspSpriteSheet & sh = *sheets[legacySheet];
				const sf::Vector2u sz = sh.image.getSize();
				if (px < 0 || py < 0 || unsigned(px) >= sz.x || unsigned(py) >= sz.y) { setStatus("Click within the sprite.", true); return; }
				if (sh.mask) { setStatus("Clear Mask first, then Pick Mask to sample a color.", true); return; }
				const sf::Color c = sh.image.getPixel({unsigned(px), unsigned(py)});
				sh.mask = c;
				sh.maskHow = OT::MaskDiscovery::Manual;
				sh.image.createMaskFromColor(c);
				previewTexValid = previewTex.loadFromImage(sh.image);
				markDirty();
				char buf[64];
				std::snprintf(buf, sizeof(buf), "Set transparent color to (%d, %d, %d).", c.r, c.g, c.b);
				setStatus(buf);
				return;
			}
			for (int k = int(blocks.size()) - 1; k >= 0; --k)
			{
				const OT::ObjmapRect & r = blocks[size_t(k)].rect;
				if (px >= r.left && px < r.right && py >= r.top && py < r.bottom)
				{
					selectedBlock = k;
					buildBlockTools();
					setStatus("Part " + std::to_string(k) + ": " + blocks[size_t(k)].name);
					return;
				}
			}
			return;
		}
		if (preview == Preview::Grid && gridResource >= 0 && previewScale > 0)
		{
			OT::DecodedResource & r = doc->resources[size_t(gridResource)].res;
			const int x = int((pos.x - previewX) / previewScale);
			const int y = int((pos.y - previewY) / std::max(0.01f, gridCellH));
			if (r.codec == ResourceCodec::BkgndInfo)
			{
				OT::BkgndInfoDef & d = std::get<OT::BkgndInfoDef>(r.value);
				if (x < 0 || y < 0 || x >= d.cols || y >= d.rows) return;
				d.cells[size_t(y) * size_t(d.cols) + size_t(x)] = paintValue;
				setStatus("BINF row " + std::to_string(y) + ", col " + std::to_string(x) + " = " + std::to_string(paintValue));
			}
			else
			{
				OT::FloorInfoMaskDef & d = std::get<OT::FloorInfoMaskDef>(r.value);
				if (x < 0 || y < 0 || x >= d.width || y >= d.height) return;
				int32_t & v = d.cells[size_t(y) * size_t(d.width) + size_t(x)];
				v = v ? 0 : 1;
				setStatus("FINF row " + std::to_string(y) + ", unit " + std::to_string(x) + (v ? " unbuildable" : " buildable"));
			}
			markDirty();
			if (selected == gridResource) form.setDoc(OT::resourceToJson(r));
		}
	}

	// =====================================================================
	// Actions
	// =====================================================================
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
			setStatus("Failed to load plugin: " + path, true);
			return false;
		}
		OT::OspPlugin p;
		OT::OspResult cr = OT::convertT2pToOsp(plugin, p);
		if (!cr.ok) { setStatus("Convert failed: " + cr.message, true); return false; }
		openDocument(std::move(p), "", "Loaded " + path + ". Save As writes an .osp-plugin; Export writes a .t2p.");
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
		const bool synthesized = p.resources.empty();
		OT::synthesizeOspResources(p);
		openDocument(std::move(p), path, synthesized ? "Opened " + path + " (OSP v1/v2: identity and tables only; no .t2p export)"
		                                             : "Opened " + path);
		return true;
	}

	void EditorApp::openDocument(OT::OspPlugin && p, const std::string & ospPath, const std::string & message)
	{
		doc = std::move(p);
		loadedFromPath = ospPath;
		dirty = false;
		selected = -1;
		selectedBlock = -1;
		preview = Preview::None;
		clearPreviewTools();
		formPanel->removeAllWidgets();
		rebuildTree();
		const int def = findResourceIndex("TNDF", 1) >= 0 ? findResourceIndex("TNDF", 1)
		              : findResourceIndex("MVDF", 1) >= 0 ? findResourceIndex("MVDF", 1)
		              : findResourceIndex("OODF", 1) >= 0 ? findResourceIndex("OODF", 1)
		              : findResourceIndex("WORLDDEF", 1) >= 0 ? findResourceIndex("WORLDDEF", 1)
		              : findResourceIndex("MODF", 128) >= 0 ? findResourceIndex("MODF", 128)
		              : findResourceIndex("HEDF", 128) >= 0 ? findResourceIndex("HEDF", 128)
		              : findResourceIndex("ADDF", 128) >= 0 ? findResourceIndex("ADDF", 128)
		              : findResourceIndex("ATTR", 128);
		if (def >= 0) selectResource(def);
		setStatus(message);
		updateTitle();
	}

	void EditorApp::actionSave()
	{
		if (!doc) { setStatus("Nothing to save.", true); return; }
		if (loadedFromPath.empty()) { actionSaveAs(); return; }
		OT::OspResult r = OT::saveOspPlugin(loadedFromPath, *doc, true);
		if (!r.ok) { setStatus("Save failed: " + r.message, true); return; }
		dirty = false;
		setStatus("Saved to " + loadedFromPath);
		updateTitle();
	}

	void EditorApp::actionSaveAs()
	{
		if (!doc) { setStatus("Nothing to save.", true); return; }
		std::string path;
		std::string def = doc->attr.name.empty() ? std::string("NewPlugin") : doc->attr.name;
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

	bool EditorApp::exportT2pTo(const std::string & path)
	{
		if (!doc) { setStatus("Nothing to export.", true); return false; }
		std::string err;
		if (!OT::writeT2pFile(*doc, path, &err))
		{
			setStatus("Export failed: " + err, true);
			return false;
		}
		setStatus("Exported " + path);
		return true;
	}

	void EditorApp::actionExportT2p()
	{
		if (!doc) { setStatus("Nothing to export.", true); return; }
		std::string dir;
		Result r = OT::NativeFileDialog::openDirectory("Export .t2p into folder", dir);
		if (r != Result::Selected) { if (r == Result::Unavailable) setStatus("No directory dialog available.", true); return; }
		std::string name = doc->sourceFile.empty() ? (doc->attr.name.empty() ? std::string("Plugin") : doc->attr.name) + ".t2p"
		                                           : doc->sourceFile;
		exportT2pTo(dir + "/" + name);
	}

	void EditorApp::newTenant()
	{
		OT::OspPlugin p;
		p.sourceFormat = "scratch";
		p.pluginClass = OT::PluginDesc::TenantPlugin;
		p.attr.valid = true;
		p.attr.id = OT::FourCC::fromFileOrder("NERH");
		p.attr.dependID = OT::FourCC::fromFileOrder("****");
		p.attr.name = "New Tenant";
		p.attr.gameLevel = 1;
		p.attr.attrib = 0x7;
		p.attr.itemType = 1399;
		OT::synthesizeOspResources(p);
		openDocument(std::move(p), "", "New Tenant created. Edit its fields, import sprites, then Save As.");
		dirty = true;
		updateTitle();
	}

	void EditorApp::closeDoc()
	{
		doc.reset();
		loadedFromPath.clear();
		dirty = false;
		selected = -1;
		preview = Preview::None;
		clearPreviewTools();
		formPanel->removeAllWidgets();
		formHeader->setText("No plugin loaded.");
		tree->removeAllItems();
		updateTitle();
	}

	void EditorApp::confirmThen(const std::function<void()> & action)
	{
		if (!doc || !dirty) { action(); return; }
		pendingAction = action;
		if (confirmBox)
		{
			gui.remove(confirmBox);
			confirmBox = nullptr;
		}
		confirmBox = tgui::MessageBox::create("Unsaved changes", "Save changes before continuing?", {"Save", "Discard", "Cancel"});
		applyMessageBoxTheme(confirmBox, Theme::active());
		confirmBox->setPosition({"(parent.width - width) / 2", "(parent.height - height) / 2"});
		gui.add(confirmBox);
		confirmBox->onButtonPress([this](const tgui::String & button) {
			tgui::MessageBox::Ptr box = confirmBox;
			confirmBox = nullptr;
			if (box) gui.remove(box);
			if (button == "Cancel") { pendingAction = nullptr; return; }
			if (button == "Save")
			{
				actionSave();
				if (dirty) { pendingAction = nullptr; return; }
			}
			std::function<void()> run = pendingAction;
			pendingAction = nullptr;
			if (run) run();
		});
	}

	void EditorApp::actionImportPng()
	{
		if (!doc) { setStatus("Open or create a plugin first.", true); return; }
		std::string path;
		Result r = OT::NativeFileDialog::openPngFile("Import PNG", path);
		if (r != Result::Selected) { if (r == Result::Unavailable) setStatus("No file dialog available.", true); return; }
		sf::Image img;
		if (!img.loadFromFile(path)) { setStatus("Failed to load PNG: " + path, true); return; }
		if (preview == Preview::Legacy)
		{
			OT::OspSpriteSheet * sheets[4] = {&doc->toolbox, &doc->ingame, &doc->occupants, &doc->walking};
			OT::OspSpriteSheet & sh = *sheets[legacySheet];
			const char * rel[4] = {"sprites/100.png", "sprites/1000.png", "sprites/1100.png", "sprites/200.png"};
			sh.image = img;
			sh.present = true;
			sh.relPath = rel[legacySheet];
			if (sh.mask) sh.image.createMaskFromColor(*sh.mask);
			previewTexValid = previewTex.loadFromImage(sh.image);
			markDirty();
			setStatus("Imported " + path);
			return;
		}
		if (previewBitmap < 0) { setStatus("Select a bitmap first.", true); return; }
		std::string dib, err;
		if (!OT::replaceDibPixels(doc->resources[size_t(previewBitmap)].res.bytes, img, dib, &err))
		{
			setStatus("Import failed: " + err, true);
			return;
		}
		doc->resources[size_t(previewBitmap)].res.bytes = dib;
		markDirty();
		setPreviewBitmap(previewBitmap, previewObjmap);
		rebuildTree();
		setStatus("Imported " + path + " (mapped onto the bitmap's palette).");
	}

	void EditorApp::actionExportPng()
	{
		sf::Image img;
		if (preview == Preview::Legacy)
		{
			OT::OspSpriteSheet * sheets[4] = {&doc->toolbox, &doc->ingame, &doc->occupants, &doc->walking};
			img = sheets[legacySheet]->image;
		}
		else if (previewBitmap < 0 || !decodeDib(doc->resources[size_t(previewBitmap)].res.bytes, img))
		{
			setStatus("No bitmap to export.", true);
			return;
		}
		std::string def = (doc->attr.name.empty() ? std::string("bitmap") : doc->attr.name) + ".png";
		std::string path;
		Result r = OT::NativeFileDialog::savePngFile("Export PNG", def, path);
		if (r != Result::Selected) { if (r == Result::Unavailable) setStatus("No file dialog available.", true); return; }
		if (!img.saveToFile(path)) { setStatus("Failed to write PNG: " + path, true); return; }
		setStatus("Exported " + path);
	}
}
