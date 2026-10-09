/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Plugin Editor colour themes and the helpers that apply them to TGUI
/// widgets. Shared by EditorApp and ResourceForm.
#pragma once
#include <SFML/Graphics/Color.hpp>

#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/TGUI.hpp>

#include <string>

namespace editors
{
	/// UI color palette. Two themes: a dark editor theme (default) and a
	/// bright theme for screenshots / accessibility / light-preference users.
	/// Selected at startup via the OSP_EDITOR_THEME env var
	/// ("light" | "dark" | "auto"; auto == dark).
	/// Both palettes are applied in full to every widget renderer (see the
	/// apply* helpers), so no TGUI default color remains in either theme.
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

		sf::Color editText;            ///< EditBox / ListBox / ComboBox text
		sf::Color editTextFocused;     ///< EditBox text while focused
		sf::Color editCaret;           ///< EditBox caret (normal + focused)
		sf::Color editSelectionBg;     ///< EditBox / ListBox selection background
		sf::Color editSelectionText;   ///< EditBox / ListBox selected text
		sf::Color editBgHover;         ///< EditBox / ComboBox / ListBox background, hovered
		sf::Color editBgFocused;       ///< EditBox background while focused
		sf::Color editBorder;          ///< EditBox / ComboBox / ListBox border
		sf::Color editBorderHover;     ///< EditBox border, hovered
		sf::Color editBorderFocused;   ///< EditBox / CheckBox border, focused
		sf::Color buttonText;          ///< Button / MenuBar selection / MessageBox label
		sf::Color buttonTextHover;     ///< Button label, hovered
		sf::Color buttonTextDown;      ///< Button label, pressed
		sf::Color buttonBgHover;       ///< Button / checked CheckBox background, hovered
		sf::Color buttonBgDown;        ///< Button background, pressed
		sf::Color buttonBorder;        ///< Button border (all states)
		sf::Color scrollTrack;         ///< Scrollbar track
		sf::Color scrollThumb;         ///< Scrollbar thumb
		sf::Color scrollThumbHover;    ///< Scrollbar thumb, hovered
		sf::Color scrollArrow;         ///< Scrollbar arrow glyph
		sf::Color scrollArrowBg;       ///< Scrollbar arrow background
		sf::Color checkMark;           ///< tick glyph on a checked CheckBox
		sf::Color errorText;           ///< invalid field / error status
	};

	void applyMenuTheme(tgui::MenuBar::Ptr menu, const Theme & th);
	void applyChildWindowTheme(tgui::ChildWindow::Ptr w, const Theme & th, const sf::Color & clientBg);
	void applyEditBoxTheme(tgui::EditBox::Ptr e, const Theme & th);
	void applyComboBoxTheme(tgui::ComboBox::Ptr c, const Theme & th);
	void applyCheckBoxTheme(tgui::CheckBox::Ptr c, const Theme & th);
	void applyButtonTheme(tgui::Button::Ptr b, const Theme & th);
	void applyScrollablePanelTheme(tgui::ScrollablePanel::Ptr p, const Theme & th);
	void applyMessageBoxTheme(tgui::MessageBox::Ptr m, const Theme & th);
	void applyListBoxTheme(tgui::ListBox::Ptr l, const Theme & th);
	void applyTextAreaTheme(tgui::TextArea::Ptr t, const Theme & th);
	void applyTreeViewTheme(tgui::TreeView::Ptr t, const Theme & th);

	/// A hover tooltip label in the shared yellow style.
	tgui::Label::Ptr makeTooltip(const std::string & text, float uiScale);
}
