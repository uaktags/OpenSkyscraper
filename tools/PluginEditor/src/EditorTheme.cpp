/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "EditorTheme.h"

#include <cstdlib>
#include <string>

namespace editors
{
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
			t.border      = sf::Color(118, 124, 136);
			t.labelText   = sf::Color(30, 34, 42);
			t.sectionText = sf::Color(20, 90, 160);
			t.noteText    = sf::Color(110, 110, 120);
			t.editBg      = sf::Color(255, 255, 255);
			t.statusText  = sf::Color(40, 44, 52);
			t.btnBg       = sf::Color(62, 110, 178);
			t.canvasBg    = sf::Color(220, 222, 228);
			t.placeholder = sf::Color(200, 202, 210);
			t.rectOutline = sf::Color(200, 40, 40, 220);

			// Full-widget palette: every renderer property the theme helpers
			// touch is set, so no TGUI light-theme default can show through.
			t.editText          = sf::Color(25, 28, 34);
			t.editTextFocused   = sf::Color(15, 18, 24);
			t.editCaret         = sf::Color(20, 24, 30);
			t.editSelectionBg   = sf::Color(60, 110, 180);
			t.editSelectionText = sf::Color(255, 255, 255);
			t.editBgHover       = sf::Color(245, 247, 250);
			t.editBgFocused     = sf::Color(255, 255, 255);
			t.editBorder        = sf::Color(118, 124, 136);
			t.editBorderHover   = sf::Color(110, 118, 132);
			t.editBorderFocused = sf::Color(20, 90, 160);
			t.buttonText        = sf::Color(250, 251, 253);
			t.buttonTextHover   = sf::Color(255, 255, 255);
			t.buttonTextDown    = sf::Color(255, 255, 255);
			t.buttonBgHover     = sf::Color(60, 108, 172);
			t.buttonBgDown      = sf::Color(52, 94, 152);
			t.buttonBorder      = sf::Color(52, 94, 152);
			t.scrollTrack       = sf::Color(228, 230, 236);
			t.scrollThumb       = sf::Color(122, 130, 144);
			t.scrollThumbHover  = sf::Color(104, 112, 128);
			t.scrollArrow       = sf::Color(60, 64, 72);
			t.scrollArrowBg     = sf::Color(206, 210, 218);
			t.checkMark         = sf::Color(255, 255, 255);
			t.errorText         = sf::Color(190, 40, 40);
		}
		else  // dark (default)
		{
			t.windowBg    = sf::Color(24, 26, 32);
			t.panelBg     = sf::Color(28, 31, 38);
			t.titleBar    = sf::Color(42, 46, 54);
			t.border      = sf::Color(100, 108, 122);
			t.labelText   = sf::Color(210, 216, 224);
			t.sectionText = sf::Color(120, 170, 230);
			t.noteText    = sf::Color(140, 140, 150);
			t.editBg      = sf::Color(40, 44, 52);
			t.statusText  = sf::Color(180, 186, 194);
			t.btnBg       = sf::Color(48, 90, 140);
			t.canvasBg    = sf::Color(32, 35, 42);
			t.placeholder = sf::Color(50, 54, 62);
			t.rectOutline = sf::Color(80, 200, 120, 200);

			// Full-widget palette: every renderer property the theme helpers
			// touch is set, so no TGUI light-theme default can show through.
			t.editText          = sf::Color(232, 236, 242);
			t.editTextFocused   = sf::Color(240, 244, 250);
			t.editCaret         = sf::Color(230, 235, 242);
			t.editSelectionBg   = sf::Color(70, 110, 170);
			t.editSelectionText = sf::Color(245, 248, 252);
			t.editBgHover       = sf::Color(44, 48, 57);
			t.editBgFocused     = sf::Color(48, 53, 62);
			t.editBorder        = sf::Color(110, 120, 134);
			t.editBorderHover   = sf::Color(124, 134, 148);
			t.editBorderFocused = sf::Color(120, 170, 230);
			t.buttonText        = sf::Color(238, 242, 248);
			t.buttonTextHover   = sf::Color(250, 252, 255);
			t.buttonTextDown    = sf::Color(255, 255, 255);
			t.buttonBgHover     = sf::Color(60, 108, 164);
			t.buttonBgDown      = sf::Color(38, 72, 116);
			t.buttonBorder      = sf::Color(70, 110, 160);
			t.scrollTrack       = sf::Color(32, 36, 44);
			t.scrollThumb       = sf::Color(112, 120, 134);
			t.scrollThumbHover  = sf::Color(136, 144, 158);
			t.scrollArrow       = sf::Color(200, 206, 214);
			t.scrollArrowBg     = sf::Color(48, 52, 62);
			t.checkMark         = sf::Color(245, 248, 252);
			t.errorText         = sf::Color(235, 90, 90);
		}
		return t;
	}

	namespace
	{
		void applyButtonMapping(tgui::ButtonRenderer * r, const Theme & th);
		std::shared_ptr<tgui::RendererData> makeButtonRendererData(const Theme & th);
		std::shared_ptr<tgui::RendererData> makeScrollbarRendererData(const Theme & th);
		std::shared_ptr<tgui::RendererData> makeListBoxRendererData(const Theme & th);
	}

	namespace
	{
		// ---------------------------------------------------------------------
		// Theme helpers — apply the full palette to each widget renderer.
		// Nested renderers (list box, scrollbars, dialog buttons) are handed
		// RendererData built from the same palette, so no property falls back
		// to a TGUI light-theme default.
		// ---------------------------------------------------------------------
		/// Shared button mapping. Applied in place to a widget's renderer (the
		/// property signals keep the widget's cached values in sync) and to a
		/// fresh renderer for the nested ButtonRendererData of a MessageBox.
		void applyButtonMapping(tgui::ButtonRenderer * r, const Theme & th)
		{
			r->setBackgroundColor(th.btnBg);
			r->setBackgroundColorHover(th.buttonBgHover);
			r->setBackgroundColorDown(th.buttonBgDown);
			r->setBackgroundColorDownHover(th.buttonBgDown);
			r->setBackgroundColorDownFocused(th.buttonBgDown);
			r->setBackgroundColorFocused(th.btnBg);
			r->setTextColor(th.buttonText);
			r->setTextColorHover(th.buttonTextHover);
			r->setTextColorDown(th.buttonTextDown);
			r->setTextColorDownHover(th.buttonTextDown);
			r->setTextColorDownFocused(th.buttonTextDown);
			r->setTextColorFocused(th.buttonText);
			r->setBorderColor(th.buttonBorder);
			r->setBorderColorHover(th.buttonBorder);
			r->setBorderColorFocused(th.buttonBorder);
			r->setBorderColorDown(th.buttonBorder);
			r->setBorderColorDownHover(th.buttonBorder);
			r->setBorderColorDownFocused(th.buttonBorder);
			r->setBorders({1, 1, 1, 1});
			r->setRoundedBorderRadius(3);
		}

		std::shared_ptr<tgui::RendererData> makeButtonRendererData(const Theme & th)
		{
			auto r = std::make_shared<tgui::ButtonRenderer>();
			applyButtonMapping(r.get(), th);
			return r->getData();
		}

		std::shared_ptr<tgui::RendererData> makeScrollbarRendererData(const Theme & th)
		{
			tgui::ScrollbarRenderer r;
			r.setTrackColor(th.scrollTrack);
			r.setTrackColorHover(th.scrollTrack);
			r.setThumbColor(th.scrollThumb);
			r.setThumbColorHover(th.scrollThumbHover);
			r.setArrowBackgroundColor(th.scrollArrowBg);
			r.setArrowBackgroundColorHover(th.scrollArrowBg);
			r.setArrowColor(th.scrollArrow);
			r.setArrowColorHover(th.scrollArrow);
			return r.getData();
		}

		std::shared_ptr<tgui::RendererData> makeListBoxRendererData(const Theme & th)
		{
			tgui::ListBoxRenderer r;
			r.setBackgroundColor(th.editBg);
			r.setBackgroundColorHover(th.editBgHover);
			r.setTextColor(th.editText);
			r.setTextColorHover(th.editText);
			r.setSelectedBackgroundColor(th.editSelectionBg);
			r.setSelectedBackgroundColorHover(th.editSelectionBg);
			r.setSelectedTextColor(th.editSelectionText);
			r.setSelectedTextColorHover(th.editSelectionText);
			r.setBorderColor(th.editBorder);
			r.setBorders({1, 1, 1, 1});
			r.setScrollbar(makeScrollbarRendererData(th));
			return r.getData();
		}

		/// MenuBar — also covers its popup menus, which share this renderer.
		/// ChildWindow chrome only: geometry (borders, title-bar height, client
		/// size) stays at the call site.
		/// CheckBox and RadioButton share the RadioButtonRenderer properties.
		/// Button — in-place mapping so TGUI's cached renderer values stay in sync.
		/// ScrollablePanel — only the scrollbars; the call site keeps deciding
		/// the panel background (the inspector uses Transparent).
	}

	void applyMenuTheme(tgui::MenuBar::Ptr menu, const Theme & th)
	{
		auto * r = menu->getRenderer();
		r->setBackgroundColor(th.titleBar);
		r->setTextColor(th.labelText);
		r->setSelectedBackgroundColor(th.btnBg);
		r->setSelectedTextColor(th.buttonText);
		r->setSeparatorColor(th.border);
	}

	void applyChildWindowTheme(tgui::ChildWindow::Ptr w, const Theme & th, const sf::Color & clientBg)
	{
		auto * r = w->getRenderer();
		r->setBackgroundColor(clientBg);
		r->setTitleBarColor(th.titleBar);
		r->setBorderColor(th.border);
		r->setBorderColorFocused(th.editBorderFocused);
		r->setTitleColor(th.labelText);
	}

	void applyEditBoxTheme(tgui::EditBox::Ptr e, const Theme & th)
	{
		auto * r = e->getRenderer();
		r->setBackgroundColor(th.editBg);
		r->setBackgroundColorHover(th.editBgHover);
		r->setBackgroundColorFocused(th.editBgFocused);
		r->setTextColor(th.editText);
		r->setTextColorFocused(th.editTextFocused);
		r->setCaretColor(th.editCaret);
		r->setCaretColorHover(th.editCaret);
		r->setSelectedTextColor(th.editSelectionText);
		r->setSelectedTextBackgroundColor(th.editSelectionBg);
		r->setBorderColor(th.editBorder);
		r->setBorderColorHover(th.editBorderHover);
		r->setBorderColorFocused(th.editBorderFocused);
		r->setBorders({1, 1, 1, 1});
		r->setRoundedBorderRadius(3);
	}

	void applyComboBoxTheme(tgui::ComboBox::Ptr c, const Theme & th)
	{
		auto * r = c->getRenderer();
		r->setBackgroundColor(th.editBg);
		r->setBorderColor(th.editBorder);
		r->setTextColor(th.editText);
		r->setDefaultTextColor(th.noteText);
		r->setArrowColor(th.labelText);
		r->setArrowColorHover(th.labelText);
		r->setArrowBackgroundColor(th.editBg);
		r->setArrowBackgroundColorHover(th.editBgHover);
		r->setListBox(makeListBoxRendererData(th));
	}

	void applyCheckBoxTheme(tgui::CheckBox::Ptr c, const Theme & th)
	{
		auto * r = c->getRenderer();
		r->setTextColor(th.labelText);
		r->setTextColorHover(th.labelText);
		r->setTextColorChecked(th.labelText);
		r->setTextColorCheckedHover(th.labelText);
		r->setBackgroundColor(th.editBg);
		r->setBackgroundColorHover(th.editBgHover);
		r->setBackgroundColorChecked(th.btnBg);
		r->setBackgroundColorCheckedHover(th.buttonBgHover);
		r->setBorderColor(th.editBorder);
		r->setBorderColorHover(th.editBorder);
		r->setBorderColorChecked(th.editBorder);
		r->setBorderColorCheckedHover(th.editBorder);
		r->setBorderColorFocused(th.editBorderFocused);
		r->setBorderColorCheckedFocused(th.editBorderFocused);
		r->setCheckColor(th.checkMark);
		r->setCheckColorHover(th.checkMark);
	}

	void applyButtonTheme(tgui::Button::Ptr b, const Theme & th)
	{
		applyButtonMapping(b->getRenderer(), th);
	}

	void applyScrollablePanelTheme(tgui::ScrollablePanel::Ptr p, const Theme & th)
	{
		p->getRenderer()->setScrollbar(makeScrollbarRendererData(th));
	}

	void applyMessageBoxTheme(tgui::MessageBox::Ptr m, const Theme & th)
	{
		auto * r = m->getRenderer();
		r->setBackgroundColor(th.panelBg);
		r->setTitleBarColor(th.titleBar);
		r->setBorderColor(th.border);
		r->setBorderColorFocused(th.editBorderFocused);
		r->setTitleColor(th.labelText);
		r->setTextColor(th.labelText);
		r->setButton(makeButtonRendererData(th));
	}


	void applyListBoxTheme(tgui::ListBox::Ptr l, const Theme & th)
	{
		l->getRenderer()->setData(makeListBoxRendererData(th));
	}

	void applyTextAreaTheme(tgui::TextArea::Ptr t, const Theme & th)
	{
		auto * r = t->getRenderer();
		r->setBackgroundColor(th.editBg);
		r->setTextColor(th.editText);
		r->setSelectedTextColor(th.editSelectionText);
		r->setSelectedTextBackgroundColor(th.editSelectionBg);
		r->setCaretColor(th.editCaret);
		r->setBorderColor(th.editBorder);
		r->setBorders({1, 1, 1, 1});
		r->setScrollbar(makeScrollbarRendererData(th));
	}

	void applyTreeViewTheme(tgui::TreeView::Ptr t, const Theme & th)
	{
		auto * r = t->getRenderer();
		r->setBackgroundColor(th.editBg);
		r->setTextColor(th.editText);
		r->setTextColorHover(th.editText);
		r->setSelectedTextColor(th.editSelectionText);
		r->setSelectedTextColorHover(th.editSelectionText);
		r->setSelectedBackgroundColor(th.editSelectionBg);
		r->setSelectedBackgroundColorHover(th.editSelectionBg);
		r->setBackgroundColorHover(th.editBgHover);
		r->setBorderColor(th.editBorder);
		r->setBorders({1, 1, 1, 1});
		r->setScrollbar(makeScrollbarRendererData(th));
	}

	tgui::Label::Ptr makeTooltip(const std::string & text, float s)
	{
		auto tip = tgui::Label::create(text);
		tip->setTextSize(int(13 * s));
		tip->getRenderer()->setBackgroundColor(sf::Color(250, 245, 200));
		tip->getRenderer()->setTextColor(sf::Color(30, 30, 30));
		tip->getRenderer()->setBorders({1, 1, 1, 1});
		return tip;
	}
}
