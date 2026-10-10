/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Generic form for one resource's Json document (PluginResourceJson.h).
///
/// Every member becomes a widget: integers an edit box (with the validated
/// range from FieldSchema), enums a dropdown, bitmasks checkboxes, 0/1 flags
/// a checkbox, minutes an edit box with an hh:mm hint, strings an edit box
/// (long text a text area), rects one row of four boxes. Lists of records
/// are master/detail tables with Add, Remove, Up and Down; lists of scalars
/// are rows of boxes (growable when the schema says so). Fields the engine
/// ignores or whose meaning is unknown are "advanced": hidden unless the
/// advanced toggle is on, then shown by their key.
///
/// Every edit builds a candidate document and hands it to `onCommit`, which
/// re-encodes it (resourceFromJson). A rejected edit is reverted and the
/// error reported through `onStatus`.
#pragma once
#include "FieldSchema.h"
#include "ResourceJson.h"

#include <TGUI/Backend/SFML-Graphics.hpp>
#include <TGUI/TGUI.hpp>

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace editors
{
	class ResourceForm
	{
	public:
		/// Validates + applies a candidate document. Returns false with
		/// `error` set to reject it.
		std::function<bool(const OT::Json & candidate, std::string & error)> onCommit;
		/// Status line (message, isError).
		std::function<void(const std::string &, bool)> onStatus;
		/// Called after a list row is selected (path of the list, row).
		std::function<void(const std::string &, int)> onRowSelected;

		/// Builds the form into `panel` (cleared first).
		void build(tgui::ScrollablePanel::Ptr panel, const std::string & schema, const OT::Json & doc,
		           bool showAdvanced, float uiScale);
		/// Rebuilds after a structural change; call once per frame.
		void rebuildIfPending();
		void requestRebuild() { pendingRebuild_ = true; }
		const OT::Json & doc() const { return doc_; }
		/// Replaces the document (e.g. after a preview edit) and rebuilds.
		void setDoc(const OT::Json & doc) { doc_ = doc; pendingRebuild_ = true; }
		/// Selected row of a list ("variations", "members"...), -1 if none.
		int selectedRow(const std::string & listPath) const;
		void selectRow(const std::string & listPath, int row) { selected_[listPath] = row; pendingRebuild_ = true; }
		/// Applies `value` at a dotted path ("tool.width", "variations[0].price")
		/// through onCommit, as if typed. Used by the --set CLI.
		bool setPath(const std::string & path, const std::string & value, std::string & error);

	private:
		struct PathEl
		{
			std::string key;
			int index = -1;
		};
		using Path = std::vector<PathEl>;

		tgui::ScrollablePanel::Ptr panel_;
		std::string schema_;
		OT::Json doc_;
		bool advanced_ = false;
		float s_ = 1.f;
		float y_ = 0;
		int hiddenAdvanced_ = 0;
		bool pendingRebuild_ = false;
		std::map<std::string, int> selected_;

		static std::string pathKey(const Path & p);     // "variations[].price" (metadata key)
		static std::string pathString(const Path & p);  // "variations[2].price"
		static OT::Json * at(OT::Json & root, const Path & p);
		const FieldMeta * meta(const Path & p) const;
		bool isAdvanced(const Path & p) const;
		std::string caption(const Path & p) const;

		bool commit(const OT::Json & candidate, bool structural);
		bool commitValue(const Path & p, OT::Json value, bool structural);

		void renderObject(const OT::Json & obj, const Path & p, int depth);
		void renderMember(const OT::Json & v, const Path & p, int depth);
		void renderScalar(const OT::Json & v, const Path & p, int depth);
		void renderRect(const OT::Json & v, const Path & p, int depth);
		void renderScalarList(const OT::Json & v, const Path & p, int depth);
		void renderRecordList(const OT::Json & v, const Path & p, int depth);
		void renderGridList(const OT::Json & v, const Path & p, int depth);

		// widget helpers
		tgui::Label::Ptr addLabel(const std::string & text, float x, float w, const sf::Color & c, int size = 13);
		void addCaption(const Path & p, int depth);
		tgui::EditBox::Ptr addEdit(float x, float w, const std::string & text, bool readOnly);
		tgui::Button::Ptr addButton(const std::string & text, float x, float w);
		float indent(int depth) const { return 6.f + 14.f * float(depth); }
		float captionExtra_ = 0;           // extra height of a wrapped caption
		void nextRow();
	};

	/// Parses an integer typed by the user ("42", "-1", "0x1F"). False on junk.
	bool parseUserInt(const std::string & text, int64_t & out);
	/// "600" -> "10:00".
	std::string minutesToClock(int64_t m);
}
