#pragma once
#include <TGUI/TGUI.hpp>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace OT {

	class TimeWindowWatch
	{
	public:
		TimeWindowWatch() {}
		virtual ~TimeWindowWatch() {}

		// Stub methods for TGUI migration - watch rendering will be reimplemented later
		void RenderElement(tgui::Widget* widget, float time);
	};

	class TimeWindowWatchInstancer
	{
	public:
		TimeWindowWatch* InstanceDecorator() {
			return new TimeWindowWatch();
		}
		void ReleaseDecorator(TimeWindowWatch* decorator) {
			delete decorator;
		}
		void Release() {
			delete this;
		}
	};
}
