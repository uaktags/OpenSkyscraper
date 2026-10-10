/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// YootTower layout of the toolbox (game-parity.md S4): like the original
/// Tools window, a grid of category buttons (each plugin's OBJMAP 100
/// `Category` icon, pattern category x 10 + 0 / 1) above the selected
/// category's name and its items, sorted by T2ToolDef::GetSortKey
/// (category << 16 | toolNo). Item icons are the plugin's own OBJMAP 100
/// patterns variation x 10 + 0 / 1 (normal / selected).
#include "ToolboxWindow.h"

#include "Application.h"
#include "Game.h"
#include "PluginObjmap.h"
#include "YootCatalog.h"

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <TGUI/Widgets/Label.hpp>

#include <map>

using namespace OT;

namespace {
    constexpr int kCell   = 36;   ///< rendered cell (ToolboxWindow kCellW/H)
    constexpr int kPadX   = 5;
    constexpr int kTop    = 30;   ///< ToolboxWindow kItemsTopY
    constexpr int kCols   = 3;
    constexpr int kLabelH = 16;
    constexpr int kOtherCategory = 1000;  ///< prototypes without plugin data

    /// One OBJMAP 100 icon (RT_BITMAP 100 rect for `pattern`), cached.
    bool pluginIcon(Application * app, const std::string & plugin, int pattern, sf::Image & out)
    {
        static std::map<std::pair<std::string, int>, sf::Image> cache;
        static std::map<std::pair<std::string, int>, bool> missing;
        const std::pair<std::string, int> key(plugin, pattern);
        auto hit = cache.find(key);
        if (hit != cache.end()) { out = hit->second; return true; }
        if (missing.count(key)) return false;
        missing[key] = true;
        AbstractPlugin * p = app->pluginManager.get(plugin);
        const WindowsPEExecutable::Resource * om = (p && p->ensureLoaded()) ? p->getResource("OBJMAP", 100) : nullptr;
        std::vector<ObjmapBlock> blocks;
        if (!om || !parsePluginObjmap(om->data, om->length, blocks)) return false;
        const ObjmapBlock * block = nullptr;
        for (const ObjmapBlock & b : blocks)
            if (b.pattern == pattern && b.rect.width() > 0 && b.rect.height() > 0) { block = &b; break; }
        sf::Texture sheet;
        if (!block || !app->bitmaps.loadFromPlugin(plugin, 100, sheet)) return false;
        const sf::Image full = sheet.copyToImage();
        sf::Image cell({static_cast<unsigned>(block->rect.width()), static_cast<unsigned>(block->rect.height())});
        if (!cell.copy(full, {0, 0}, sf::IntRect({block->rect.left, block->rect.top}, {block->rect.width(), block->rect.height()})))
            return false;
        missing.erase(key);
        cache[key] = cell;
        out = cell;
        return true;
    }
}

bool ToolboxWindow::yootLayoutActive() const
{
    return game->activeRules.ruleset() == BuildRules::Ruleset::YootTower;
}

bool ToolboxWindow::makePluginIconButton(const std::string & plugin, int pattern, ButtonState & state, tgui::Button::Ptr & out)
{
    sf::Image normal, selected;
    if (!pluginIcon(app, plugin, pattern, normal)) return false;
    if (!pluginIcon(app, plugin, pattern + 1, selected) || selected.getSize() != normal.getSize()) selected = normal;
    // Two-row sheet: makeButton takes row 1 as the checked state.
    const sf::Vector2u size = normal.getSize();
    sf::Image sheet({size.x, size.y * 2});
    if (!sheet.copy(normal, {0, 0}) || !sheet.copy(selected, {0, size.y})) return false;
    sf::Texture texture;
    if (!texture.loadFromImage(sheet)) return false;
    out = makeButton(kCell, kCell, texture, 0, state, static_cast<int>(size.x), static_cast<int>(size.y));
    return true;
}

std::vector<YootToolboxGroup> ToolboxWindow::collectYootGroups() const
{
    std::vector<YootToolboxEntry> entries;
    for (size_t i = 0; i < game->itemFactory.prototypes.size(); ++i) {
        Item::AbstractPrototype * p = game->itemFactory.prototypes[i];
        if (game->activeRules.minRatingToBuild(p->id) > game->rating) continue;
        YootToolboxEntry e;
        e.prototypeId = p->id;
        if (p->yoot) {
            e.category = p->yoot->category;
            e.categoryName = p->yoot->categoryName;
            e.toolNo = p->yoot->toolNo;
        } else {
            e.category = kOtherCategory;
            e.categoryName = "Other";
            e.toolNo = (int)i;
        }
        entries.push_back(e);
    }
    return yootToolboxGroups(entries);
}

int ToolboxWindow::yootLayoutHeight(const std::vector<YootToolboxGroup> & groups) const
{
    const int categoryRows = ((int)groups.size() + kCols - 1) / kCols;
    int items = 0;
    for (const YootToolboxGroup & g : groups)
        if (g.category == yootCategory) items = (int)g.prototypeIds.size();
    const int itemRows = (items + kCols - 1) / kCols;
    return categoryRows * kCell + kLabelH + itemRows * kCell;
}

void ToolboxWindow::addYootItems(const std::vector<YootToolboxGroup> & groups)
{
    auto & protos = game->itemFactory.prototypesById;
    // Categories.
    for (size_t i = 0; i < groups.size(); ++i) {
        const YootToolboxGroup & g = groups[i];
        ButtonState state;
        tgui::Button::Ptr button;
        bool made = false;
        for (const std::string & id : g.prototypeIds) {
            auto p = protos.find(id);
            if (p == protos.end() || !p->second->yoot) continue;
            if ((made = makePluginIconButton(p->second->yoot->plugin, g.category * 10, state, button))) break;
        }
        if (!made) {
            // No plugin icon: the first item's icon stands for the group.
            auto p = protos.find(g.prototypeIds.front());
            if (p == protos.end()) continue;
            button = makeItemButton(p->second, state);
        }
        button->setPosition((kPadX + kCell * ((int)i % kCols)) * app->uiScale, (kTop + kCell * ((int)i / kCols)) * app->uiScale);
        button->setToolTip(tgui::Label::create(g.name));
        window->add(button);
        buttons.insert(button);
        buttonStates[button] = state;
        const std::string toolname = "category-" + std::to_string(g.category);
        toolButtons[toolname] = button;
        const int category = g.category;
        button->onPress([this, category] {
            // Rebuilding inside the press handler would delete the widget
            // that is firing; update() does it on the next frame.
            yootCategory = category;
            yootReloadPending = true;
        });
    }

    // The selected category's title and items.
    const int categoryRows = ((int)groups.size() + kCols - 1) / kCols;
    int y = kTop + categoryRows * kCell;
    const YootToolboxGroup * selected = nullptr;
    for (const YootToolboxGroup & g : groups)
        if (g.category == yootCategory) selected = &g;
    auto label = tgui::Label::create(selected ? selected->name : "");
    label->setTextSize(10 * app->uiScale);
    label->getRenderer()->setTextColor(sf::Color(230, 230, 230));
    label->setPosition(kPadX * app->uiScale, (y + 1) * app->uiScale);
    window->add(label);
    y += kLabelH;
    if (!selected) return;
    for (size_t i = 0; i < selected->prototypeIds.size(); ++i) {
        auto p = protos.find(selected->prototypeIds[i]);
        if (p == protos.end()) continue;
        Item::AbstractPrototype * proto = p->second;
        ButtonState state;
        tgui::Button::Ptr button = makeItemButton(proto, state);
        button->setPosition((kPadX + kCell * ((int)i % kCols)) * app->uiScale, (y + kCell * ((int)i / kCols)) * app->uiScale);
        button->setToolTip(tgui::Label::create(proto->name));
        window->add(button);
        buttons.insert(button);
        const std::string toolname = "item-" + proto->id;
        toolButtons[toolname] = button;
        buttonStates[button] = state;
        button->onPress([this, toolname] { onToolButtonPress(toolname.c_str()); });
    }
}

void ToolboxWindow::followYootTool(const std::string & protoId)
{
    if (protoId.empty()) return;
    auto p = game->itemFactory.prototypesById.find(protoId);
    if (p == game->itemFactory.prototypesById.end() || !p->second) return;
    const int category = p->second->yoot ? p->second->yoot->category : kOtherCategory;
    if (category == yootCategory) return;
    yootCategory = category;
    yootReloadPending = true;
}
