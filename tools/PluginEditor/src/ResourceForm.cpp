/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "ResourceForm.h"

#include "EditorTheme.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>

namespace editors
{
	using OT::Json;

	namespace
	{
		constexpr float kLabelW = 190.f;
		constexpr float kFieldX = 200.f;
		constexpr float kFieldW = 250.f;
		constexpr float kRowH = 30.f;
		constexpr float kContentW = 470.f;

		/// Default rows for lists that may be empty in a plugin.
		const Json * rowTemplate(const std::string & schema, const std::string & key)
		{
			static std::map<std::string, Json> t;
			if (t.empty())
			{
				Json m = Json::object();
				m.set("name", Json("Customer")).set("demandType", Json(-1)).set("startSilhouetteType", Json(0))
				 .set("endSilhouetteType", Json(0)).set("optionType", Json(-1)).set("percent", Json(100)).set("numOfPeople", Json(1));
				t["MemberTable:members"] = m;
				Json w = Json::object();
				w.set("text", Json("Hello!")).set("sexMask", Json(3)).set("ageMask", Json(255)).set("flags", Json(0)).set("special", Json(0));
				t["Words:words"] = w;
				Json e = Json::object();
				e.set("position", Json(0)).set("pattern", Json(1)).set("silhouette", Json(0)).set("animeID", Json(0));
				t["PeopleAnime:elems"] = e;
				Json s = Json::object();
				s.set("silhouetteType", Json(300)).set("width", Json(1)).set("routeType", Json(0)).set("bitmapID", Json(300)).set("name", Json(""));
				t["Silhouettes:entries"] = s;
				Json pp = Json::object();
				pp.set("peopleType", Json(1)).set("attribute", Json(0)).set("silhouetteType", Json(300)).set("stressMax", Json(300))
				  .set("stressBlueLimit", Json(100)).set("stressYellowLimit", Json(230));
				t["People:entries"] = pp;
				t["StringTable:strings"] = Json("");
				t["StringBlock:strings"] = Json("");
				t["Movie:extra"] = Json("");
				t["TemplateDef:inMoney"] = Json(0);
				t["WORLDDEF:cashAmounts"] = Json(0);
				t["FBEV:values"] = Json(0);
				t["CLEV:imageIDs"] = Json(0);
			}
			auto it = t.find(schema + ":" + key);
			return it == t.end() ? nullptr : &it->second;
		}

		bool isRectObject(const Json & v)
		{
			return v.isObject() && v.size() == 4 && v.has("left") && v.has("top") && v.has("right") && v.has("bottom");
		}

		bool isPointObject(const Json & v)
		{
			return v.isObject() && v.size() == 2 && v.has("x") && v.has("y");
		}

		std::string rowSummary(const Json & row, size_t i)
		{
			std::string s = std::to_string(i) + ":";
			if (row.isObject())
			{
				for (const Json::Member & m : row.members())
					if (m.second.isString() && !m.second.asString().empty())
					{
						s += " " + m.second.asString();
						if (s.size() > 48) s = s.substr(0, 45) + "...";
						return s;
					}
				int n = 0;
				for (const Json::Member & m : row.members())
				{
					if (!m.second.isInt() && !m.second.isString()) continue;
					s += " " + m.first + "=" + (m.second.isInt() ? std::to_string(m.second.asInt()) : m.second.asString());
					if (++n == 3) break;
				}
			}
			else if (row.isInt()) s += " " + std::to_string(row.asInt());
			return s;
		}

		std::string toCrLf(const std::string & s)
		{
			std::string o;
			for (char c : s)
			{
				if (c == '\n') o += "\r\n";
				else if (c != '\r') o.push_back(c);
			}
			return o;
		}

		std::string fromCrLf(const std::string & s)
		{
			std::string o;
			for (size_t i = 0; i < s.size(); ++i)
				if (!(s[i] == '\r' && i + 1 < s.size() && s[i + 1] == '\n')) o.push_back(s[i]);
			return o;
		}
	}

	bool parseUserInt(const std::string & text, int64_t & out)
	{
		std::string t;
		for (char c : text)
			if (c != ' ' && c != '\t' && c != '_') t.push_back(c);
		if (t.empty()) return false;
		errno = 0;
		char * end = nullptr;
		const bool neg = t[0] == '-';
		const char * body = t.c_str() + (neg ? 1 : 0);
		const int base = (body[0] == '0' && (body[1] == 'x' || body[1] == 'X')) ? 16 : 10;
		const unsigned long long v = std::strtoull(body, &end, base);
		if (errno || !end || *end != '\0' || end == body) return false;
		if (v > 0xFFFFFFFFFFull) return false;
		out = neg ? -int64_t(v) : int64_t(v);
		return true;
	}

	std::string minutesToClock(int64_t m)
	{
		char buf[16];
		std::snprintf(buf, sizeof(buf), "%02lld:%02lld", (long long)(m / 60), (long long)(m % 60));
		return buf;
	}

	// ---------------------------------------------------------------------
	// Paths and metadata
	// ---------------------------------------------------------------------
	std::string ResourceForm::pathKey(const Path & p)
	{
		std::string s;
		for (const PathEl & e : p)
		{
			if (e.index >= 0) { s += "[]"; continue; }
			if (!s.empty()) s += ".";
			s += e.key;
		}
		return s;
	}

	std::string ResourceForm::pathString(const Path & p)
	{
		std::string s;
		for (const PathEl & e : p)
		{
			if (e.index >= 0) { s += "[" + std::to_string(e.index) + "]"; continue; }
			if (!s.empty()) s += ".";
			s += e.key;
		}
		return s;
	}

	Json * ResourceForm::at(Json & root, const Path & p)
	{
		Json * j = &root;
		for (const PathEl & e : p)
		{
			if (e.index >= 0)
			{
				if (!j->isArray() || size_t(e.index) >= j->size()) return nullptr;
				j = &j->elements()[size_t(e.index)];
			}
			else
			{
				j = j->find(e.key);
				if (!j) return nullptr;
			}
		}
		return j;
	}

	const FieldMeta * ResourceForm::meta(const Path & p) const
	{
		return findFieldMeta(schema_, pathKey(p));
	}

	bool ResourceForm::isAdvanced(const Path & p) const
	{
		if (const FieldMeta * m = meta(p)) return m->advanced;
		for (const PathEl & e : p)
			if (e.index < 0 && isAdvancedKey(e.key)) return true;
		return false;
	}

	std::string ResourceForm::caption(const Path & p) const
	{
		if (const FieldMeta * m = meta(p))
			if (!m->label.empty()) return m->label;
		for (auto it = p.rbegin(); it != p.rend(); ++it)
			if (it->index < 0) return prettyKey(it->key);
		return "value";
	}

	int ResourceForm::selectedRow(const std::string & listPath) const
	{
		auto it = selected_.find(listPath);
		return it == selected_.end() ? -1 : it->second;
	}

	// ---------------------------------------------------------------------
	// Commit
	// ---------------------------------------------------------------------
	bool ResourceForm::commit(const Json & candidate, bool structural)
	{
		std::string err;
		if (!onCommit || onCommit(candidate, err))
		{
			doc_ = candidate;
			if (structural) pendingRebuild_ = true;
			return true;
		}
		if (onStatus) onStatus(err.empty() ? std::string("Invalid value") : err, true);
		pendingRebuild_ = true;  // put the widgets back to the stored values
		return false;
	}

	bool ResourceForm::commitValue(const Path & p, Json value, bool structural)
	{
		Json candidate = doc_;
		Json * slot = at(candidate, p);
		if (!slot) return false;
		if (*slot == value) return true;
		*slot = std::move(value);
		const bool ok = commit(candidate, structural);
		if (ok && onStatus) onStatus("Set " + pathString(p), false);
		return ok;
	}

	bool ResourceForm::setPath(const std::string & path, const std::string & value, std::string & error)
	{
		Path p;
		size_t i = 0;
		while (i < path.size())
		{
			if (path[i] == '.') { ++i; continue; }
			if (path[i] == '[')
			{
				const size_t close = path.find(']', i);
				if (close == std::string::npos) { error = "bad path"; return false; }
				PathEl e;
				e.index = std::atoi(path.substr(i + 1, close - i - 1).c_str());
				p.push_back(e);
				i = close + 1;
				continue;
			}
			size_t j = i;
			while (j < path.size() && path[j] != '.' && path[j] != '[') ++j;
			PathEl e;
			e.key = path.substr(i, j - i);
			p.push_back(e);
			i = j;
		}
		Json * cur = at(doc_, p);
		if (!cur) { error = "no field " + path; return false; }
		Json v;
		if (cur->isInt() || cur->isBool())
		{
			int64_t n = 0;
			if (!parseUserInt(value, n)) { error = path + ": not an integer"; return false; }
			v = cur->isBool() ? Json(n != 0) : Json((long long)n);
		}
		else if (cur->isString()) v = Json(value);
		else { error = path + ": only scalar fields can be set"; return false; }
		Json candidate = doc_;
		*at(candidate, p) = v;
		if (onCommit && !onCommit(candidate, error)) return false;
		doc_ = candidate;
		pendingRebuild_ = true;
		return true;
	}

	// ---------------------------------------------------------------------
	// Build
	// ---------------------------------------------------------------------
	void ResourceForm::build(tgui::ScrollablePanel::Ptr panel, const std::string & schema, const Json & doc,
	                         bool showAdvanced, float uiScale)
	{
		panel_ = panel;
		schema_ = schema;
		doc_ = doc;
		advanced_ = showAdvanced;
		s_ = uiScale;
		pendingRebuild_ = true;
		rebuildIfPending();
		panel_->setVerticalScrollbarValue(0);
	}

	void ResourceForm::rebuildIfPending()
	{
		if (!pendingRebuild_ || !panel_) return;
		pendingRebuild_ = false;
		const unsigned scroll = panel_->getVerticalScrollbarValue();
		panel_->removeAllWidgets();
		y_ = 6;
		hiddenAdvanced_ = 0;
		if (doc_.isObject()) renderObject(doc_, {}, 0);
		if (hiddenAdvanced_ > 0)
		{
			y_ += 6;
			addLabel(std::to_string(hiddenAdvanced_) + " advanced field(s) hidden (unused by the engine or of unknown "
			         "meaning). Tick \"Advanced\" to show them.", 6, kContentW - 12, Theme::active().noteText, 12);
			y_ += 34;
		}
		panel_->setContentSize({kContentW * s_, (y_ + 20) * s_});
		panel_->setVerticalScrollbarValue(scroll);
	}

	tgui::Label::Ptr ResourceForm::addLabel(const std::string & text, float x, float w, const sf::Color & c, int size)
	{
		auto l = tgui::Label::create(text);
		l->setPosition({x * s_, (y_ + 4) * s_});
		l->setTextSize(unsigned(float(size) * s_));
		l->setMaximumTextWidth(w * s_);
		l->getRenderer()->setTextColor(c);
		panel_->add(l);
		return l;
	}

	void ResourceForm::addCaption(const Path & p, int depth)
	{
		const bool adv = isAdvanced(p);
		auto l = addLabel(caption(p), indent(depth), kLabelW - indent(depth),
		                  adv ? Theme::active().noteText : Theme::active().labelText);
		std::string tip;
		if (const FieldMeta * m = meta(p)) tip = m->tip;
		std::string where = pathString(p);
		if (adv) where += "  (advanced)";
		tip = tip.empty() ? where : tip + "\n" + where;
		if (const FieldMeta * m = meta(p))
			if (m->hasRange) tip += "\nrange " + std::to_string(m->min) + " .. " + std::to_string(m->max);
		l->setToolTip(makeTooltip(tip, s_));
		// A caption that wraps pushes the next row down.
		captionExtra_ = std::max(0.f, l->getSize().y / s_ - 24.f);
	}

	void ResourceForm::nextRow()
	{
		y_ += kRowH + captionExtra_;
		captionExtra_ = 0;
	}

	tgui::EditBox::Ptr ResourceForm::addEdit(float x, float w, const std::string & text, bool readOnly)
	{
		auto e = tgui::EditBox::create();
		e->setPosition({x * s_, y_ * s_});
		e->setSize({w * s_, 24 * s_});
		e->setTextSize(unsigned(13 * s_));
		e->setText(tgui::String(text));
		e->setReadOnly(readOnly);
		applyEditBoxTheme(e, Theme::active());
		panel_->add(e);
		return e;
	}

	tgui::Button::Ptr ResourceForm::addButton(const std::string & text, float x, float w)
	{
		auto b = tgui::Button::create(text);
		b->setPosition({x * s_, y_ * s_});
		b->setSize({w * s_, 24 * s_});
		b->setTextSize(unsigned(12 * s_));
		applyButtonTheme(b, Theme::active());
		panel_->add(b);
		return b;
	}

	void ResourceForm::renderObject(const Json & obj, const Path & p, int depth)
	{
		for (const Json::Member & m : obj.members())
		{
			Path q = p;
			q.push_back({m.first, -1});
			if (isAdvanced(q) && !advanced_)
			{
				++hiddenAdvanced_;
				continue;
			}
			renderMember(m.second, q, depth);
		}
	}

	void ResourceForm::renderMember(const Json & v, const Path & p, int depth)
	{
		if (isRectObject(v) || isPointObject(v)) { renderRect(v, p, depth); return; }
		if (v.isObject())
		{
			addLabel(caption(p), indent(depth), kContentW, Theme::active().sectionText, 14);
			nextRow();
			renderObject(v, p, depth + 1);
			return;
		}
		if (v.isArray())
		{
			const std::string key = p.back().key;
			const Json * tpl = rowTemplate(schema_, key);
			const Json * first = v.size() ? &v.at(0) : tpl;
			if (first && first->isObject()) renderRecordList(v, p, depth);
			else if (first && first->isArray()) renderGridList(v, p, depth);
			else renderScalarList(v, p, depth);
			return;
		}
		renderScalar(v, p, depth);
	}

	void ResourceForm::renderScalar(const Json & v, const Path & p, int depth)
	{
		const Theme & th = Theme::active();
		const FieldMeta * m = meta(p);
		const bool ro = m && m->readOnly;
		addCaption(p, depth);
		if (v.isBool() || (v.isInt() && m && m->boolean))
		{
			auto c = tgui::CheckBox::create();
			c->setPosition({kFieldX * s_, (y_ + 3) * s_});
			c->setSize({18 * s_, 18 * s_});
			c->setChecked(v.asBool());
			c->setEnabled(!ro);
			applyCheckBoxTheme(c, th);
			const bool asBool = v.isBool();
			c->onChange([this, p, asBool](bool on) { commitValue(p, asBool ? Json(on) : Json(on ? 1 : 0), false); });
			panel_->add(c);
			nextRow();
			return;
		}
		if (v.isInt() && m && !m->options.empty())
		{
			auto c = tgui::ComboBox::create();
			c->setPosition({kFieldX * s_, y_ * s_});
			c->setSize({kFieldW * s_, 24 * s_});
			c->setTextSize(unsigned(13 * s_));
			c->setItemsToDisplay(12);
			bool found = false;
			for (const auto & o : m->options)
			{
				c->addItem(std::to_string(o.first) + "  " + o.second, std::to_string(o.first));
				found = found || o.first == v.asInt();
			}
			if (!found) c->addItem(std::to_string(v.asInt()) + "  (unlisted)", std::to_string(v.asInt()));
			c->setSelectedItemById(std::to_string(v.asInt()));
			c->setEnabled(!ro);
			applyComboBoxTheme(c, th);
			auto weak = c.get();
			c->onItemSelect([this, p, weak] {
				int64_t n = 0;
				if (parseUserInt(weak->getSelectedItemId().toStdString(), n)) commitValue(p, Json((long long)n), false);
			});
			panel_->add(c);
			nextRow();
			return;
		}
		if (v.isString() && m && !m->tagOptions.empty())
		{
			auto c = tgui::ComboBox::create();
			c->setPosition({kFieldX * s_, y_ * s_});
			c->setSize({kFieldW * s_, 24 * s_});
			c->setTextSize(unsigned(13 * s_));
			bool found = false;
			for (const std::string & o : m->tagOptions)
			{
				c->addItem(o.empty() ? std::string("(none)") : o, o.empty() ? std::string("-") : o);
				found = found || o == v.asString();
			}
			if (!found) c->addItem(v.asString() + "  (unlisted)", v.asString().empty() ? std::string("-") : v.asString());
			c->setSelectedItemById(v.asString().empty() ? std::string("-") : v.asString());
			c->setEnabled(!ro);
			applyComboBoxTheme(c, th);
			auto weak = c.get();
			c->onItemSelect([this, p, weak] {
				std::string id = weak->getSelectedItemId().toStdString();
				if (id == "-") id.clear();
				commitValue(p, Json(id), false);
			});
			panel_->add(c);
			nextRow();
			return;
		}
		if (v.isString())
		{
			const std::string & s = v.asString();
			const bool crlf = s.find("\r\n") != std::string::npos;
			if ((m && m->multiline) || s.find('\n') != std::string::npos || s.size() > 60)
			{
				y_ += 22;
				auto t = tgui::TextArea::create();
				t->setPosition({indent(depth) * s_, y_ * s_});
				t->setSize({(kContentW - indent(depth) - 10) * s_, 66 * s_});
				t->setTextSize(unsigned(12 * s_));
				t->setText(tgui::String(fromCrLf(s)));
				t->setReadOnly(ro);
				applyTextAreaTheme(t, th);
				auto weak = t.get();
				t->onUnfocus([this, p, weak, crlf] {
					std::string text = weak->getText().toStdString();
					if (crlf) text = toCrLf(text);
					commitValue(p, Json(text), false);
				});
				panel_->add(t);
				y_ += 72;
				return;
			}
			auto e = addEdit(kFieldX, kFieldW, s, ro);
			auto weak = e.get();
			e->onReturnOrUnfocus([this, p, weak] { commitValue(p, Json(weak->getText().toStdString()), false); });
			nextRow();
			return;
		}
		if (!v.isInt())
		{
			addLabel("null", kFieldX, kFieldW, th.noteText);
			nextRow();
			return;
		}
		// Integers.
		const bool bits = m && !m->bits.empty();
		const bool minutes = m && m->minutes;
		char hex[24];
		std::snprintf(hex, sizeof(hex), "0x%llX", (unsigned long long)uint32_t(v.asInt()));
		auto e = addEdit(kFieldX, minutes || bits ? 110.f : kFieldW, bits ? std::string(hex) : std::to_string(v.asInt()), ro);
		if (minutes) addLabel("= " + minutesToClock(v.asInt()), kFieldX + 116, 100, th.noteText);
		auto weak = e.get();
		const FieldMeta range = m ? *m : FieldMeta{};
		e->onReturnOrUnfocus([this, p, weak, range, bits] {
			int64_t n = 0;
			const std::string text = weak->getText().toStdString();
			if (!parseUserInt(text, n))
			{
				if (onStatus) onStatus(pathString(p) + ": \"" + text + "\" is not an integer", true);
				pendingRebuild_ = true;
				return;
			}
			const int64_t lo = range.hasRange ? range.min : int64_t(INT32_MIN);
			const int64_t hi = range.hasRange ? range.max : int64_t(UINT32_MAX);
			if (n < lo || n > hi)
			{
				if (onStatus) onStatus(pathString(p) + ": " + std::to_string(n) + " is outside " + std::to_string(lo)
				                       + " .. " + std::to_string(hi), true);
				pendingRebuild_ = true;
				return;
			}
			commitValue(p, Json((long long)n), bits || range.minutes);
		});
		nextRow();
		if (bits)
		{
			const uint32_t cur = uint32_t(v.asInt());
			int col = 0;
			for (const auto & b : m->bits)
			{
				auto c = tgui::CheckBox::create(b.second);
				c->setPosition({(indent(depth) + 10 + 225.f * float(col)) * s_, y_ * s_});
				c->setSize({16 * s_, 16 * s_});
				c->setTextSize(unsigned(12 * s_));
				c->setChecked((cur & b.first) == b.first);
				c->setEnabled(!ro);
				applyCheckBoxTheme(c, th);
				const uint32_t bit = b.first;
				c->onChange([this, p, bit](bool on) {
					Json * j = at(doc_, p);
					if (!j) return;
					uint32_t val = uint32_t(j->asInt());
					val = on ? (val | bit) : (val & ~bit);
					commitValue(p, Json(val), true);
				});
				panel_->add(c);
				if (++col == 2) { col = 0; y_ += 24; }
			}
			if (col) y_ += 24;
			uint32_t known = 0;
			for (const auto & b : m->bits) known |= b.first;
			if (cur & ~known)
			{
				char other[64];
				std::snprintf(other, sizeof(other), "other bits set: 0x%X (meaning unknown)", cur & ~known);
				addLabel(other, indent(depth) + 10, kContentW, th.noteText, 12);
				y_ += 22;
			}
			y_ += 4;
		}
	}

	void ResourceForm::renderRect(const Json & v, const Path & p, int depth)
	{
		addCaption(p, depth);
		const bool point = isPointObject(v);
		const char * const rectKeys[4] = {"left", "top", "right", "bottom"};
		const char * const pointKeys[2] = {"x", "y"};
		const int n = point ? 2 : 4;
		const FieldMeta * m = meta(p);
		const bool ro = m && m->readOnly;
		for (int k = 0; k < n; ++k)
		{
			const char * key = point ? pointKeys[k] : rectKeys[k];
			const float x = kFieldX + float(k) * 63.f;
			auto e = addEdit(x, 58, std::to_string(v[key].asInt()), ro);
			e->setToolTip(makeTooltip(key, s_));
			Path q = p;
			q.push_back({key, -1});
			auto weak = e.get();
			e->onReturnOrUnfocus([this, q, weak] {
				int64_t val = 0;
				if (!parseUserInt(weak->getText().toStdString(), val) || val < INT32_MIN || val > INT32_MAX)
				{
					if (onStatus) onStatus(pathString(q) + ": expected a 32-bit integer", true);
					pendingRebuild_ = true;
					return;
				}
				commitValue(q, Json((long long)val), false);
			});
		}
		nextRow();
	}

	void ResourceForm::renderScalarList(const Json & v, const Path & p, int depth)
	{
		const Theme & th = Theme::active();
		const FieldMeta * m = meta(p);
		const bool variable = (m && m->variableList) || rowTemplate(schema_, p.back().key);
		const size_t maxItems = m ? m->maxItems : 0;
		const bool ro = m && m->readOnly;
		addCaption(p, depth);
		const bool strings = (v.size() && v.at(0).isString()) || (!v.size() && rowTemplate(schema_, p.back().key)
		                                                               && rowTemplate(schema_, p.back().key)->isString());
		if (variable)
		{
			auto add = addButton("+", kFieldX + kFieldW - 56, 26);
			auto rem = addButton("-", kFieldX + kFieldW - 26, 26);
			add->setEnabled(!ro && (maxItems == 0 || v.size() < maxItems));
			rem->setEnabled(!ro && v.size() > 0);
			add->onPress([this, p] {
				Json c = doc_;
				Json * list = at(c, p);
				if (!list) return;
				const Json * tpl = rowTemplate(schema_, p.back().key);
				list->push(list->size() ? list->elements().back() : (tpl ? *tpl : Json(0)));
				commit(c, true);
			});
			rem->onPress([this, p] {
				Json c = doc_;
				Json * list = at(c, p);
				if (!list || !list->size()) return;
				list->elements().pop_back();
				commit(c, true);
			});
		}
		addLabel("[" + std::to_string(v.size()) + "]", kFieldX, 60, th.noteText, 12);
		nextRow();
		if (strings)
		{
			for (size_t i = 0; i < v.size(); ++i)
			{
				Path q = p;
				q.push_back({"", int(i)});
				addLabel(std::to_string(i), indent(depth) + 4, 30, th.noteText, 12);
				auto e = addEdit(indent(depth) + 34, kContentW - indent(depth) - 44, v.at(i).asString(), ro);
				auto weak = e.get();
				e->onReturnOrUnfocus([this, q, weak] { commitValue(q, Json(weak->getText().toStdString()), false); });
				nextRow();
			}
			return;
		}
		const int perRow = 6;
		const float w = 66.f;
		for (size_t i = 0; i < v.size(); ++i)
		{
			const int col = int(i % perRow);
			if (i && col == 0) nextRow();
			Path q = p;
			q.push_back({"", int(i)});
			auto e = addEdit(indent(depth) + 4 + float(col) * (w + 4), w, std::to_string(v.at(i).asInt()), ro);
			e->setToolTip(makeTooltip(pathString(q), s_));
			auto weak = e.get();
			e->onReturnOrUnfocus([this, q, weak] {
				int64_t n = 0;
				if (!parseUserInt(weak->getText().toStdString(), n))
				{
					if (onStatus) onStatus(pathString(q) + ": not an integer", true);
					pendingRebuild_ = true;
					return;
				}
				commitValue(q, Json((long long)n), false);
			});
		}
		if (v.size()) nextRow();
	}

	void ResourceForm::renderRecordList(const Json & v, const Path & p, int depth)
	{
		const Theme & th = Theme::active();
		const FieldMeta * m = meta(p);
		const size_t maxItems = m ? m->maxItems : 0;
		const std::string ps = pathString(p);
		int sel = selectedRow(ps);
		if (sel < 0 || sel >= int(v.size())) sel = v.size() ? 0 : -1;
		selected_[ps] = sel;

		addLabel(caption(p) + "  (" + std::to_string(v.size()) + " rows)", indent(depth), kContentW, th.sectionText, 14);
		nextRow();
		auto list = tgui::ListBox::create();
		list->setPosition({indent(depth) * s_, y_ * s_});
		const float rows = float(std::min<size_t>(std::max<size_t>(v.size(), 1), 7));
		list->setSize({(kContentW - indent(depth) - 80) * s_, (rows * 20 + 6) * s_});
		list->setTextSize(unsigned(12 * s_));
		list->setItemHeight(unsigned(20 * s_));
		for (size_t i = 0; i < v.size(); ++i) list->addItem(rowSummary(v.at(i), i));
		if (sel >= 0) list->setSelectedItemByIndex(size_t(sel));
		applyListBoxTheme(list, th);
		auto weak = list.get();
		list->onItemSelect([this, ps, weak] {
			const int idx = weak->getSelectedItemIndex();
			if (idx < 0 || idx == selectedRow(ps)) return;
			selected_[ps] = idx;
			pendingRebuild_ = true;
			if (onRowSelected) onRowSelected(ps, idx);
		});
		panel_->add(list);

		const float bx = kContentW - 74;
		const float y0 = y_;
		auto mk = [&](const char * label, float dy) {
			const float save = y_;
			y_ = y0 + dy;
			auto b = addButton(label, bx, 66);
			y_ = save;
			return b;
		};
		auto add = mk("Add", 0);
		auto rem = mk("Remove", 26);
		auto up = mk("Up", 52);
		auto down = mk("Down", 78);
		const Json * tpl = rowTemplate(schema_, p.back().key);
		add->setEnabled((v.size() || tpl) && (maxItems == 0 || v.size() < maxItems));
		rem->setEnabled(v.size() > 0);
		up->setEnabled(sel > 0);
		down->setEnabled(sel >= 0 && sel + 1 < int(v.size()));
		add->onPress([this, p, ps] {
			Json c = doc_;
			Json * l = at(c, p);
			if (!l) return;
			const int s = selectedRow(ps);
			const Json * t = rowTemplate(schema_, p.back().key);
			Json row = (s >= 0 && size_t(s) < l->size()) ? l->at(size_t(s)) : (t ? *t : Json::object());
			const size_t pos = s >= 0 ? size_t(s) + 1 : l->size();
			l->elements().insert(l->elements().begin() + long(pos), row);
			if (commit(c, true)) selected_[ps] = int(pos);
		});
		rem->onPress([this, p, ps] {
			Json c = doc_;
			Json * l = at(c, p);
			const int s = selectedRow(ps);
			if (!l || s < 0 || size_t(s) >= l->size()) return;
			l->elements().erase(l->elements().begin() + s);
			if (commit(c, true)) selected_[ps] = std::max(0, s - 1);
		});
		auto move = [this, p, ps](int delta) {
			Json c = doc_;
			Json * l = at(c, p);
			const int s = selectedRow(ps);
			if (!l || s < 0 || s + delta < 0 || size_t(s + delta) >= l->size()) return;
			std::swap(l->elements()[size_t(s)], l->elements()[size_t(s + delta)]);
			if (commit(c, true)) selected_[ps] = s + delta;
		};
		up->onPress([move] { move(-1); });
		down->onPress([move] { move(1); });
		y_ += std::max(rows * 20 + 12, 108.f);

		if (sel >= 0)
		{
			Path q = p;
			q.push_back({"", sel});
			addLabel("Row " + std::to_string(sel), indent(depth + 1), kContentW, th.noteText, 12);
			y_ += 24;
			const Json & row = v.at(size_t(sel));
			if (row.isObject()) renderObject(row, q, depth + 1);
			else renderMember(row, q, depth + 1);
			y_ += 6;
		}
	}

	void ResourceForm::renderGridList(const Json & v, const Path & p, int depth)
	{
		const Theme & th = Theme::active();
		addCaption(p, depth);
		const size_t cols = v.size() ? v.at(0).size() : 0;
		if (p.back().key == "grid" || v.size() > 300)
		{
			addLabel(std::to_string(v.size()) + " x " + std::to_string(cols) + "  (edit in the preview)", kFieldX,
			         kFieldW, th.noteText, 12);
			nextRow();
			return;
		}
		nextRow();
		for (size_t i = 0; i < v.size(); ++i)
		{
			std::string text;
			for (size_t k = 0; k < v.at(i).size(); ++k)
				text += (k ? ", " : "") + std::to_string(v.at(i).at(k).asInt());
			Path q = p;
			q.push_back({"", int(i)});
			addLabel(std::to_string(i), indent(depth) + 4, 34, th.noteText, 12);
			auto e = addEdit(indent(depth) + 38, kContentW - indent(depth) - 48, text, false);
			auto weak = e.get();
			const size_t n = v.at(i).size();
			e->onReturnOrUnfocus([this, q, weak, n] {
				Json row = Json::array();
				std::string t = weak->getText().toStdString(), cell;
				t += ",";
				for (char c : t)
				{
					if (c != ',') { cell.push_back(c); continue; }
					int64_t val = 0;
					if (!parseUserInt(cell, val))
					{
						if (onStatus) onStatus(pathString(q) + ": expected comma-separated integers", true);
						pendingRebuild_ = true;
						return;
					}
					row.push(Json((long long)val));
					cell.clear();
				}
				if (row.size() != n)
				{
					if (onStatus) onStatus(pathString(q) + ": expected " + std::to_string(n) + " values", true);
					pendingRebuild_ = true;
					return;
				}
				commitValue(q, row, false);
			});
			nextRow();
		}
	}
}
