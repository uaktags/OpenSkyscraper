#include "TimeWindowWatch.h"
#include <TGUI/TGUI.hpp>
#include "OpenGL.h"
#include <cmath>

using namespace OT;

void TimeWindowWatch::RenderElement(tgui::Widget* widget, float time)
{
	// TODO: Reimplement watch rendering using TGUI
	// For now, this is a stub to allow compilation

	// Original Rocket code commented out for reference:
	/*
	Vector2f offset = element->GetAbsoluteOffset();
	Vector2f size   = element->GetBox().GetSize() / 2;

	double radius_m = std::min<float>(size.x, size.y);
	double radius_h = 0.6 * radius_m;

	double angle_h = 2 * M_PI * time/12;
	double angle_m = 2 * M_PI * time;

	glPushMatrix();
	glTranslatef(offset.x + size.x, offset.y + size.y, 0);
	glColor3f(0, 0, 0);
	glBegin(GL_LINES);
	glVertex2f(0, 0);
	glVertex2f(radius_h * sin(angle_h), -radius_h * cos(angle_h));
	glVertex2f(0, 0);
	glVertex2f(radius_m * sin(angle_m), -radius_m * cos(angle_m));
	glEnd();
	glPopMatrix();
	*/
}
