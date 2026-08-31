/* Copyright (c) 2026 OpenSkyscraper contributors */
/// Shared typed-accessor implementations for AbstractPlugin. The body is
/// identical to the former Plugin methods; it now lives here so both the PE
/// Plugin and the folder-based DecompiledPlugin get it through inheritance
/// instead of a copy.
#include "AbstractPlugin.h"
#include "WindowsPEExecutable.h"

namespace OT
{
	bool AbstractPlugin::getAttr(PluginAttr & out) const
	{
		const WindowsPEExecutable::Resource * r = getResource("ATTR", 128);
		if (!r) return false;
		return parsePluginAttr(r->data, r->length, out);
	}

	bool AbstractPlugin::getDesc(PluginDesc & out) const
	{
		const WindowsPEExecutable::Resource * r = getResource("DESC", 128);
		if (!r) return false;
		return parsePluginDesc(r->data, r->length, out);
	}

	bool AbstractPlugin::getDpdf(PluginDpdf & out) const
	{
		const WindowsPEExecutable::Resource * r = getResource("DPDF", 128);
		if (!r) return false;
		return parsePluginDpdf(r->data, r->length, out);
	}

	bool AbstractPlugin::getPldf(PluginPldf & out) const
	{
		const WindowsPEExecutable::Resource * r = getResource("PLDF", 128);
		if (!r) return false;
		return parsePluginPldf(r->data, r->length, out);
	}

	bool AbstractPlugin::getMvdf(PluginMvdf & out) const
	{
		// MVDF is a category-level resource at id 1 (not the item-level id 128
		// used by ATTR/DESC/DPDF/PLDF). Present on Mover plugins only.
		const WindowsPEExecutable::Resource * r = getResource("MVDF", 1);
		if (!r) return false;
		return parsePluginMvdf(r->data, r->length, out);
	}
}
